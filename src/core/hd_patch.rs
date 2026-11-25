//! HD patch download + installation, run on a worker thread (port of the Qt
//! `HdPatchManager`).
//!
//! Pipeline: download the ZIP from [`constants::HD_PATCH_URL`] into a temp
//! folder → SHA-256 (logged; enforced when [`HD_PATCH_SHA256`] is filled in)
//! → pure-Rust extraction → locate the client root inside the extracted tree
//! ([`find_extracted_root`]) → move `Data`, `Interface`, `WoW.exe`, DXVK…
//! into the client, overwriting → write `WTF/Config.wtf` → remove the temp
//! folder.
//!
//! The public API below is the contract used by the main window: keep these
//! signatures.

use crate::constants;
use crate::i18n::I18n;
use crate::util::{archive, http};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::time::{Duration, Instant};

/// Progress reported by [`HdPatchTask::poll`].
#[derive(Debug, Clone)]
pub enum HdEvent {
    /// `percent` None = indeterminate (busy). `status` is already translated.
    Progress { percent: Option<u8>, status: String },
    /// Terminal event. `message` is already translated.
    Finished { ok: bool, message: String },
}

/// Expected SHA-256 of the HD archive (lowercase hex). The endpoint publishes
/// no hash yet: empty = computed and logged only, like the Qt version.
const HD_PATCH_SHA256: &str = "";

/// Folder name of the client root inside the official archive.
const EXTRACTED_ROOT_NAME: &str = "Le Client WoW HD";

/// Breadth-first search depth for [`find_extracted_root`].
const FIND_ROOT_MAX_DEPTH: usize = 4;

/// Items moved from the extracted root into the client (folders, then files).
const ITEMS_TO_MIGRATE: [&str; 6] = ["Data", "Interface", "World of Warcraft.app", "d3d9.dll", "dxvk.conf", "WoW.exe"];

/// Minimum delay between two download progress events.
const PROGRESS_THROTTLE: Duration = Duration::from_millis(150);

pub struct HdPatchTask {
    rx: Receiver<HdEvent>,
    running: bool,
    cancel: Arc<AtomicBool>,
}

impl HdPatchTask {
    /// Starts download + install into `wow_path` on a background thread.
    pub fn start(wow_path: PathBuf, i18n: I18n) -> Self {
        let (tx, rx) = mpsc::channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let worker_cancel = Arc::clone(&cancel);
        let temp_dir = std::env::temp_dir().join("SylvaniaLauncher_HD");
        let spawned = std::thread::Builder::new().name("hd-patch".into()).spawn({
            let tx = tx.clone();
            let i18n = i18n.clone();
            move || {
                let worker = Worker { wow: wow_path, temp: temp_dir, i18n, tx: tx.clone(), cancel: worker_cancel };
                let (ok, message) = match worker.run() {
                    Ok(msg) => (true, msg),
                    Err(msg) => (false, msg),
                };
                worker.cleanup();
                let _ = tx.send(HdEvent::Finished { ok, message });
            }
        });
        if let Err(e) = spawned {
            log::error!("Thread du Patch HD non démarré: {e}");
            let _ = tx.send(HdEvent::Finished { ok: false, message: format!("{}: {e}", i18n.t("Erreur")) });
        }
        HdPatchTask { rx, running: true, cancel }
    }

    /// Drains pending events (non-blocking).
    pub fn poll(&mut self) -> Vec<HdEvent> {
        let events: Vec<HdEvent> = self.rx.try_iter().collect();
        if events.iter().any(|e| matches!(e, HdEvent::Finished { .. })) {
            self.running = false;
        }
        events
    }

    /// False once a Finished event has been produced.
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Heuristic: is the HD patch already present in this client?
    /// `patch-w.mpq` / `patch-5.mpq` (enabled or renamed `.disabled`) are the
    /// most representative files of the pack.
    pub fn is_installed(wow_path: &Path) -> bool {
        if wow_path.as_os_str().is_empty() {
            return false;
        }
        ["patch-w.mpq", "patch-w.mpq.disabled", "patch-5.mpq", "patch-5.mpq.disabled"]
            .iter()
            .any(|f| wow_path.join("Data").join(f).exists())
    }
}

impl Drop for HdPatchTask {
    /// Like the Qt destructor: abort a download still in flight. Extraction
    /// and migration are left to finish so the client is never half-written.
    fn drop(&mut self) {
        self.cancel.store(true, Ordering::Relaxed);
    }
}

/// Translated, user-facing message for a network error string coming from
/// [`http::download_to_file`] (same wording as the Qt `onDownloadError`).
pub fn network_error_message(i18n: &I18n, err: &str) -> String {
    let lower = err.to_lowercase();
    if lower.contains("connection refused") || lower.contains("10061") || lower.contains("actively refused") {
        i18n.t("Connexion refusée par le serveur")
    } else if lower.contains("dns")
        || lower.contains("no such host")
        || lower.contains("failed to lookup")
        || lower.contains("name or service not known")
        || lower.contains("11001")
    {
        i18n.t("Serveur introuvable")
    } else if lower.contains("timed out") || lower.contains("timeout") {
        i18n.t("Délai de connexion dépassé")
    } else {
        format!("{}{err}", i18n.t("Erreur réseau: "))
    }
}

/// Locates the client root inside the extracted tree. The archive may wrap
/// "Le Client WoW HD" in extra folders or ship its contents directly, so
/// search (breadth-first, depth-limited, stopping at the first match so the
/// multi-GB `Data` tree is never walked) rather than assume a path.
pub fn find_extracted_root(extract_path: &Path) -> Option<PathBuf> {
    let looks_like_root = |dir: &Path| {
        dir.join("Data").is_dir() || ["WoW.exe", "wow.exe", "Wow.exe"].iter().any(|f| dir.join(f).is_file())
    };
    let mut queue = std::collections::VecDeque::from([(extract_path.to_path_buf(), 0usize)]);
    while let Some((dir, depth)) = queue.pop_front() {
        let canonical = dir.join(EXTRACTED_ROOT_NAME);
        if canonical.is_dir() {
            return Some(canonical);
        }
        if looks_like_root(&dir) {
            return Some(dir);
        }
        if depth >= FIND_ROOT_MAX_DEPTH {
            continue;
        }
        let Ok(entries) = std::fs::read_dir(&dir) else { continue };
        let mut subs: Vec<PathBuf> = entries
            .filter_map(Result::ok)
            .filter(|e| e.file_type().is_ok_and(|t| t.is_dir()))
            .map(|e| e.path())
            .collect();
        subs.sort(); // QDir::entryList order (by name)
        queue.extend(subs.into_iter().map(|p| (p, depth + 1)));
    }
    None
}

/// Moves `src` into `dest` recursively, overwriting existing files and
/// merging folders (robocopy `/E /IS /MOVE`). Symlinks are skipped, so the
/// copy can never be redirected outside `dest`. Returns the failures.
pub fn move_tree(src: &Path, dest: &Path) -> Vec<String> {
    let mut errors = Vec::new();
    if let Err(e) = std::fs::create_dir_all(dest) {
        errors.push(format!("{}: {e}", dest.display()));
        return errors;
    }
    let entries = match std::fs::read_dir(src) {
        Ok(it) => it,
        Err(e) => {
            errors.push(format!("{}: {e}", src.display()));
            return errors;
        }
    };
    for entry in entries.filter_map(Result::ok) {
        let Ok(kind) = entry.file_type() else { continue };
        let from = entry.path();
        let to = dest.join(entry.file_name());
        if kind.is_symlink() {
            log::warn!("Lien symbolique ignoré: {}", from.display());
        } else if kind.is_dir() {
            if to.is_file() {
                errors.push(format!("{}: {}", to.display(), "un fichier porte déjà ce nom"));
                continue;
            }
            errors.extend(move_tree(&from, &to));
        } else if let Err(e) = move_file(&from, &to) {
            errors.push(format!("{}: {e}", to.display()));
        }
    }
    errors
}

/// Moves one file, overwriting `to`: rename when on the same volume, copy
/// otherwise (the temp folder is often on another drive than the client).
fn move_file(from: &Path, to: &Path) -> std::io::Result<()> {
    if to.is_dir() {
        return Err(std::io::Error::other("un dossier porte déjà ce nom"));
    }
    if std::fs::rename(from, to).is_ok() {
        return Ok(());
    }
    copy_overwrite(from, to)?;
    let _ = std::fs::remove_file(from);
    Ok(())
}

/// Copies `from` over `to`, clearing a read-only destination first.
fn copy_overwrite(from: &Path, to: &Path) -> std::io::Result<()> {
    match std::fs::copy(from, to) {
        Ok(_) => Ok(()),
        Err(first) => {
            if !to.is_file() {
                return Err(first);
            }
            std::fs::remove_file(to)?;
            std::fs::copy(from, to).map(|_| ())
        }
    }
}

/// `WTF/Config.wtf` written after installation (same content as the Qt
/// version, byte for byte — including its double-encoded voice-chat names).
const CONFIG_WTF: &str = "SET locale \"frFR\"\n\
SET hwDetect \"0\"\n\
SET gxRefresh \"60\"\n\
SET gxMultisampleQuality \"0.000000\"\n\
SET gxWindow \"1\"\n\
SET videoOptionsVersion \"3\"\n\
SET movie \"0\"\n\
SET Gamma \"1.000000\"\n\
SET readTOS \"1\"\n\
SET readEULA \"1\"\n\
SET readTerminationWithoutNotice \"1\"\n\
SET showToolsUI \"1\"\n\
SET Sound_OutputDriverName \"System Default\"\n\
SET Sound_MusicVolume \"0.40000000596046\"\n\
SET Sound_AmbienceVolume \"0.60000002384186\"\n\
SET componentTextureLevel \"9\"\n\
SET farclip \"1277\"\n\
SET projectedTextures \"1\"\n\
SET weatherDensity \"3\"\n\
SET accounttype \"LK\"\n\
SET mouseSpeed \"1.2000000476837\"\n\
SET gameTip \"16\"\n\
SET uiScale \"0.75999999046326\"\n\
SET useUiScale \"1\"\n\
SET checkAddonVersion \"0\"\n\
SET Sound_VoiceChatInputDriverName \"RÃ©glage du systÃ¨me\"\n\
SET Sound_VoiceChatOutputDriverName \"RÃ©glage du systÃ¨me\"\n\
SET movieSubtitle \"1\"\n\
SET Sound_SFXVolume \"0.5\"\n\
SET dbCompress \"0\"\n\
SET gxResolution \"1920x1080\"\n\
SET textureFilteringMode \"5\"\n\
SET groundEffectDist \"140\"\n\
SET environmentDetail \"1.5\"\n\
SET timingTestError \"0\"\n\
SET windowResizeLock \"1\"\n\
SET particleDensity \"1\"\n\
SET mapShadows \"0\"\n\
SET vertexShaders \"0\"\n\
SET pixelShaders \"0\"\n\
SET M2UseShaders \"0\"\n\
SET M2Faster \"3\"\n\
SET shadowinstancing \"0\"\n\
SET objectFade \"0\"\n\
SET timingMethod \"2\"\n\
SET ffxSpecial \"0\"\n\
SET groundEffectDensity \"64\"\n\
SET realmList \"sylvania-servergame.com\"\n\
SET realmName \"The Kingdom of Sylvania\"\n\
SET specular \"1\"\n\
SET Sound_MasterVolume \"0.60000002384186\"\n\
SET Sound_ZoneMusicNoDelay \"1\"\n\
SET gxTripleBuffer \"1\"\n\
SET gxMultisample \"8\"\n\
SET shadowLevel \"0\"\n\
SET extShadowQuality \"5\"\n";

/// Writes `WTF/Config.wtf` atomically. An existing file is first copied to
/// `Config.wtf.bak` so the player's own settings are never lost.
pub fn write_config_wtf(wow: &Path) -> std::io::Result<()> {
    let wtf = wow.join("WTF");
    std::fs::create_dir_all(&wtf)?;
    let config = wtf.join("Config.wtf");
    if config.is_file()
        && let Err(e) = std::fs::copy(&config, wtf.join("Config.wtf.bak"))
    {
        log::warn!("Sauvegarde de Config.wtf impossible: {e}");
    }
    crate::paths::write_atomic(&config, CONFIG_WTF.as_bytes())
}

struct Worker {
    wow: PathBuf,
    temp: PathBuf,
    i18n: I18n,
    tx: Sender<HdEvent>,
    cancel: Arc<AtomicBool>,
}

impl Worker {
    fn progress(&self, percent: Option<u8>, status: String) {
        let _ = self.tx.send(HdEvent::Progress { percent, status });
    }

    fn t(&self, fr: &str) -> String {
        self.i18n.t(fr)
    }

    /// Ok(success message) / Err(error message), both translated.
    fn run(&self) -> Result<String, String> {
        if std::fs::create_dir_all(&self.temp).is_err() {
            return Err(self.t("Erreur de permission : Impossible de créer le dossier temporaire."));
        }
        if !self.wow.is_dir() || tempfile::tempfile_in(&self.wow).is_err() {
            return Err(self.t(
                "Erreur de permission : Impossible d'écrire dans le dossier WoW. Vérifiez vos droits d'administrateur.",
            ));
        }

        // 1. Download
        let zip_path = self.temp.join("patch-hd.zip");
        self.progress(Some(0), self.t("Démarrage du téléchargement..."));
        self.download(&zip_path)?;

        // 2. Hash
        self.progress(Some(100), self.t("Téléchargement terminé. Vérification..."));
        self.verify(&zip_path)?;
        self.progress(Some(100), self.t("Vérification terminée. Extraction..."));

        // 3. Extraction (not cancellable: see Drop)
        let extract_path = self.temp.join("extracted");
        if extract_path.exists() {
            let _ = std::fs::remove_dir_all(&extract_path);
        }
        self.extract(&zip_path, &extract_path)?;
        let _ = std::fs::remove_file(&zip_path); // free disk space early
        self.progress(Some(100), self.t("Extraction terminée. Migration des fichiers..."));

        // 4. Migration
        let Some(source) = find_extracted_root(&extract_path) else {
            return Err(self.t("Contenu du Patch HD introuvable dans l'archive."));
        };
        let errors = self.migrate(&source);

        // 5. Config.wtf
        if let Err(e) = write_config_wtf(&self.wow) {
            log::warn!("Écriture de Config.wtf impossible: {e}");
        }

        if errors.is_empty() {
            Ok(self.t("Patch HD installé avec succès."))
        } else {
            for e in &errors {
                log::warn!("Migration du Patch HD: {e}");
            }
            Err(self.i18n.tf(
                "Patch HD installé, mais %1 fichier(s) n'ont pas pu être copiés (jeu ouvert ?).\n%2",
                &[&errors.len(), &errors.first().cloned().unwrap_or_default()],
            ))
        }
    }

    fn download(&self, zip_path: &Path) -> Result<(), String> {
        let mut last_percent: Option<u8> = None;
        let mut last_sent = Instant::now();
        let result = http::download_to_file(constants::HD_PATCH_URL, zip_path, &self.cancel, |received, total| {
            match total.filter(|t| *t > 0) {
                Some(total) => {
                    let percent = ((received.saturating_mul(100)) / total).min(100) as u8;
                    if last_percent != Some(percent) {
                        last_percent = Some(percent);
                        self.progress(
                            Some(percent),
                            self.i18n.tf("Téléchargement du Patch HD... %1%", &[&percent]),
                        );
                    }
                }
                None if last_sent.elapsed() >= PROGRESS_THROTTLE => {
                    last_sent = Instant::now();
                    let size = crate::util::format_bytes(&self.i18n, received);
                    self.progress(None, format!("{} {size}", self.t("Téléchargement...")));
                }
                None => {}
            }
        });
        match result {
            Ok(_) => Ok(()),
            Err(http::DownloadError::Cancelled) => Err(self.t("Téléchargement annulé")),
            Err(http::DownloadError::Http(e)) => {
                let _ = std::fs::remove_file(zip_path);
                Err(network_error_message(&self.i18n, &e))
            }
            Err(http::DownloadError::Io(e)) => {
                let _ = std::fs::remove_file(zip_path);
                Err(format!("{} ({e})", self.t("Impossible de créer le fichier temporaire")))
            }
        }
    }

    fn verify(&self, zip_path: &Path) -> Result<(), String> {
        match http::sha256_file(zip_path) {
            Ok(hash) => {
                log::info!("HD Patch downloaded file SHA-256: {hash}");
                if !HD_PATCH_SHA256.is_empty() && !hash.eq_ignore_ascii_case(HD_PATCH_SHA256) {
                    let _ = std::fs::remove_file(zip_path);
                    return Err(self.t(
                        "Fichier corrompu: le hash SHA-256 ne correspond pas. Le fichier a été supprimé.",
                    ));
                }
            }
            Err(e) => log::warn!("SHA-256 du Patch HD impossible: {e}"),
        }
        Ok(())
    }

    fn extract(&self, zip_path: &Path, extract_path: &Path) -> Result<(), String> {
        let status = self.t("Extraction des fichiers HD en cours...");
        self.progress(None, status.clone());
        let never = AtomicBool::new(false);
        let mut last_percent = None;
        archive::extract_zip(zip_path, extract_path, &never, |done, total| {
            let percent = (done * 100 / total.max(1)).min(100) as u8;
            if last_percent != Some(percent) {
                last_percent = Some(percent);
                self.progress(Some(percent), status.clone());
            }
        })
        .map_err(|e| format!("{}{e}", self.t("Échec de l'extraction: ")))
    }

    /// Moves the pack items into the client. Returns the failures.
    fn migrate(&self, source: &Path) -> Vec<String> {
        // Leftover of older packs.
        let old_patch_menu = self.wow.join("PatchMenu.exe");
        if old_patch_menu.is_file() {
            let _ = std::fs::remove_file(&old_patch_menu);
        }

        let mut errors = Vec::new();
        let total = ITEMS_TO_MIGRATE.len();
        for (i, item) in ITEMS_TO_MIGRATE.iter().enumerate() {
            // DXVK ships disabled in the pack: force-enable it on install.
            let mut src = source.join(item);
            if *item == "d3d9.dll" || *item == "dxvk.conf" {
                let disabled = source.join(format!("{item}.disabled"));
                if disabled.exists() {
                    src = disabled;
                }
            }
            let dest = self.wow.join(item);
            let Ok(meta) = std::fs::symlink_metadata(&src) else { continue };
            if meta.is_dir() {
                errors.extend(move_tree(&src, &dest));
            } else if meta.is_file() {
                if let Err(e) = copy_overwrite(&src, &dest) {
                    errors.push(format!("{}: {e}", dest.display()));
                }
            } else {
                continue;
            }
            let percent = ((i + 1) * 100 / total) as u8;
            self.progress(Some(percent), format!("{}{item}", self.t("Migration: ")));
        }
        errors
    }

    fn cleanup(&self) {
        if self.temp.exists()
            && let Err(e) = std::fs::remove_dir_all(&self.temp)
        {
            log::warn!("Nettoyage du dossier temporaire impossible: {e}");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn touch(p: &Path) {
        fs::create_dir_all(p.parent().unwrap()).unwrap();
        fs::write(p, b"x").unwrap();
    }

    #[test]
    fn finds_canonical_root_wrapped_in_folders() {
        let tmp = tempfile::tempdir().unwrap();
        let root = tmp.path().join("Patch-HD").join(EXTRACTED_ROOT_NAME);
        fs::create_dir_all(root.join("Data")).unwrap();
        assert_eq!(find_extracted_root(tmp.path()), Some(root));
    }

    #[test]
    fn finds_root_by_markers() {
        let tmp = tempfile::tempdir().unwrap();
        // Contents shipped directly.
        fs::create_dir_all(tmp.path().join("Data")).unwrap();
        assert_eq!(find_extracted_root(tmp.path()), Some(tmp.path().to_path_buf()));

        let tmp2 = tempfile::tempdir().unwrap();
        touch(&tmp2.path().join("a").join("b").join("WoW.exe"));
        assert_eq!(find_extracted_root(tmp2.path()), Some(tmp2.path().join("a").join("b")));
    }

    #[test]
    fn root_search_is_depth_limited() {
        let tmp = tempfile::tempdir().unwrap();
        let mut deep = tmp.path().to_path_buf();
        for i in 0..=FIND_ROOT_MAX_DEPTH {
            deep = deep.join(format!("d{i}"));
        }
        fs::create_dir_all(deep.join("Data")).unwrap();
        assert_eq!(find_extracted_root(tmp.path()), None);
    }

    #[test]
    fn move_tree_merges_and_overwrites() {
        let tmp = tempfile::tempdir().unwrap();
        let src = tmp.path().join("src");
        let dest = tmp.path().join("dest");
        touch(&src.join("patch-d.mpq"));
        touch(&src.join("frFR").join("patch-frFR-f.mpq"));
        fs::create_dir_all(dest.join("frFR")).unwrap();
        fs::write(dest.join("patch-d.mpq"), b"old").unwrap();
        fs::write(dest.join("frFR").join("keep.mpq"), b"k").unwrap();

        assert!(move_tree(&src, &dest).is_empty());
        assert_eq!(fs::read(dest.join("patch-d.mpq")).unwrap(), b"x");
        assert!(dest.join("frFR").join("patch-frFR-f.mpq").is_file());
        assert!(dest.join("frFR").join("keep.mpq").is_file());
        assert!(!src.join("patch-d.mpq").exists());
    }

    #[test]
    fn is_installed_checks_marker_files() {
        let tmp = tempfile::tempdir().unwrap();
        assert!(!HdPatchTask::is_installed(tmp.path()));
        assert!(!HdPatchTask::is_installed(Path::new("")));
        touch(&tmp.path().join("Data").join("patch-5.mpq.disabled"));
        assert!(HdPatchTask::is_installed(tmp.path()));
    }

    #[test]
    fn config_wtf_backs_up_existing_file() {
        let tmp = tempfile::tempdir().unwrap();
        let wtf = tmp.path().join("WTF");
        fs::create_dir_all(&wtf).unwrap();
        fs::write(wtf.join("Config.wtf"), b"SET locale \"enUS\"\n").unwrap();
        write_config_wtf(tmp.path()).unwrap();
        assert_eq!(fs::read(wtf.join("Config.wtf.bak")).unwrap(), b"SET locale \"enUS\"\n");
        let written = fs::read_to_string(wtf.join("Config.wtf")).unwrap();
        assert!(written.starts_with("SET locale \"frFR\"\n"));
        assert!(written.ends_with("SET extShadowQuality \"5\"\n"));
        assert_eq!(written.lines().count(), 56);
    }

    #[test]
    fn network_errors_are_classified() {
        let i = I18n::new("fr");
        assert_eq!(network_error_message(&i, "operation timed out"), "Délai de connexion dépassé");
        assert_eq!(network_error_message(&i, "Connection refused (os error 10061)"), "Connexion refusée par le serveur");
        assert_eq!(network_error_message(&i, "dns error: failed to lookup address"), "Serveur introuvable");
        assert_eq!(network_error_message(&i, "HTTP 500"), "Erreur réseau: HTTP 500");
    }
}
