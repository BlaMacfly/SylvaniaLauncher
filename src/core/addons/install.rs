//! Filesystem + network side of addon management: trusted-URL check, staging
//! directory, download, extraction, deployment into Interface/AddOns,
//! uninstallation, `.toc` reading and the "Installés" scan.
//!
//! The blocking parts run on worker threads ([`spawn_install`],
//! [`spawn_scan`]); results come back over `mpsc` channels.

use super::registry::InstalledAddon;
use crate::constants::{APP_VERSION, SERVER_HOST};
use std::collections::{HashSet, VecDeque};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::time::Duration;

/// Depth limit when looking for an expected folder in an extracted archive.
pub const FIND_MAX_DEPTH: usize = 4;

// --- Validation -----------------------------------------------------------------

/// Only https downloads from the Sylvania host are accepted (the manifest
/// could be tampered with, or be an old embedded copy).
pub fn is_trusted_url(url: &str) -> bool {
    reqwest::Url::parse(url)
        .map(|u| u.scheme() == "https" && u.host_str() == Some(SERVER_HOST))
        .unwrap_or(false)
}

/// Id sanitised for temp file/folder names (`[^A-Za-z0-9_.-]` → `_`).
pub fn safe_id(id: &str) -> String {
    let s: String = id
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || matches!(c, '_' | '.' | '-') {
                c
            } else {
                '_'
            }
        })
        .collect();
    if s.is_empty() { "addon".into() } else { s }
}

/// A folder name that can safely be joined to the AddOns directory: a single
/// plain component (no separator, no drive, no `..`).
pub fn is_safe_folder_name(name: &str) -> bool {
    !name.is_empty()
        && name != "."
        && !name.contains("..")
        && !name.contains(['/', '\\', ':', '\0'])
        && Path::new(name).components().count() == 1
}

/// True if `<addons_dir>/<name>` is an existing directory (unsafe names: false).
pub fn folder_exists(addons_dir: &Path, name: &str) -> bool {
    is_safe_folder_name(name) && addons_dir.join(name).is_dir()
}

// --- Directory helpers ------------------------------------------------------------

/// Sub-directories of `dir` (no symlinks), sorted case-insensitively.
fn sub_dirs(dir: &Path) -> Vec<String> {
    let mut out: Vec<String> = std::fs::read_dir(dir)
        .into_iter()
        .flatten()
        .flatten()
        .filter(|e| e.file_type().map(|t| t.is_dir()).unwrap_or(false))
        .filter_map(|e| e.file_name().into_string().ok())
        .collect();
    out.sort_by_key(|s| s.to_lowercase());
    out
}

/// Breadth-first, depth-limited search for a directory named `name` below
/// `root` (`root/name` is depth 0).
pub fn find_folder_in(root: &Path, name: &str, max_depth: usize) -> Option<PathBuf> {
    let mut queue = VecDeque::from([(root.to_path_buf(), 0usize)]);
    while let Some((dir, depth)) = queue.pop_front() {
        let candidate = dir.join(name);
        if candidate.is_dir() {
            return Some(candidate);
        }
        if depth >= max_depth {
            continue;
        }
        for sub in sub_dirs(&dir) {
            queue.push_back((dir.join(sub), depth + 1));
        }
    }
    None
}

/// Recursive copy (files overwritten; symlinks skipped).
pub fn copy_dir_recursive(src: &Path, dst: &Path) -> std::io::Result<()> {
    std::fs::create_dir_all(dst)?;
    for entry in std::fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        let target = dst.join(entry.file_name());
        if ty.is_dir() {
            copy_dir_recursive(&entry.path(), &target)?;
        } else if ty.is_file() {
            std::fs::copy(entry.path(), &target)?;
        }
    }
    Ok(())
}

/// Moves `src` into `dest_parent` (replacing an existing copy). Falls back to
/// copy + delete when rename fails (temp dir on another volume).
pub fn move_dir_into(src: &Path, dest_parent: &Path) -> bool {
    let Some(name) = src.file_name() else {
        return false;
    };
    let dest = dest_parent.join(name);
    if dest.symlink_metadata().is_ok() {
        let _ = std::fs::remove_dir_all(&dest);
    }
    if std::fs::rename(src, &dest).is_ok() {
        return true;
    }
    if copy_dir_recursive(src, &dest).is_ok() {
        let _ = std::fs::remove_dir_all(src);
        return true;
    }
    false
}

/// Locates the directory of the extracted tree that actually holds the addon
/// folders (an expected folder is used as a hint, so archives wrapping
/// everything in an extra top folder still work), then deploys EVERY
/// top-level folder it contains into `addons_dir` — that is what installs
/// multi-folder addons completely (DBM ships 13 folders, Details 9…).
/// Returns the deployed folder names.
pub fn deploy_extracted(
    extract_root: &Path,
    expected: &[String],
    addons_dir: &Path,
) -> Vec<String> {
    let root = expected
        .iter()
        .filter(|f| is_safe_folder_name(f))
        .find_map(|f| find_folder_in(extract_root, f, FIND_MAX_DEPTH))
        .and_then(|found| found.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| extract_root.to_path_buf());
    sub_dirs(&root)
        .into_iter()
        .filter(|sub| move_dir_into(&root.join(sub), addons_dir))
        .collect()
}

/// Total size of the regular files below `path` (symlinks not followed).
pub fn dir_size(path: &Path) -> u64 {
    let mut total = 0;
    let mut stack = vec![path.to_path_buf()];
    while let Some(dir) = stack.pop() {
        for entry in std::fs::read_dir(&dir).into_iter().flatten().flatten() {
            let Ok(ty) = entry.file_type() else { continue };
            if ty.is_dir() {
                stack.push(entry.path());
            } else if ty.is_file() {
                total += entry.metadata().map(|m| m.len()).unwrap_or(0);
            }
        }
    }
    total
}

/// Deletes `<addons_dir>/<folder>` for each folder. Returns the folders that
/// could not be removed (unsafe names are never touched and count as failures).
pub fn remove_folders(addons_dir: &Path, folders: &[String]) -> Vec<String> {
    let mut failures = Vec::new();
    for folder in folders {
        if !is_safe_folder_name(folder) {
            failures.push(folder.clone());
            continue;
        }
        let path = addons_dir.join(folder);
        let Ok(meta) = path.symlink_metadata() else {
            continue;
        };
        let res = if meta.is_dir() {
            std::fs::remove_dir_all(&path)
        } else {
            std::fs::remove_file(&path)
        };
        if let Err(e) = res {
            log::warn!("Suppression de {} impossible : {e}", path.display());
            failures.push(folder.clone());
        }
    }
    failures
}

// --- .toc ---------------------------------------------------------------------------

/// Removes WoW colour codes (`|cAARRGGBB` and `|r`).
pub fn strip_color_codes(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut rest = s;
    while let Some(pos) = rest.find('|') {
        out.push_str(&rest[..pos]);
        let tail = &rest[pos..];
        let b = tail.as_bytes();
        if b.len() >= 10 && b[1] == b'c' && b[2..10].iter().all(u8::is_ascii_hexdigit) {
            rest = &tail[10..];
        } else if let Some(after) = tail.strip_prefix("|r") {
            rest = after;
        } else {
            out.push('|');
            rest = &tail[1..];
        }
    }
    out.push_str(rest);
    out
}

/// Value of `## <key>:` in a .toc document (case-insensitive key), colour
/// codes stripped; empty if absent.
pub fn parse_toc_value(content: &str, key: &str) -> String {
    let prefix = format!("## {key}:").to_lowercase();
    content
        .lines()
        .map(|l| l.trim_start_matches('\u{feff}').trim())
        .find(|l| l.to_lowercase().starts_with(&prefix))
        .map(|l| {
            strip_color_codes(l[prefix.len()..].trim())
                .trim()
                .to_string()
        })
        .unwrap_or_default()
}

/// [`parse_toc_value`] on a file (missing/unreadable file: empty).
pub fn read_toc_value(toc_path: &Path, key: &str) -> String {
    std::fs::read(toc_path)
        .map(|b| parse_toc_value(&String::from_utf8_lossy(&b), key))
        .unwrap_or_default()
}

// --- "Installés" scan ------------------------------------------------------------

/// One row of the "Installés" tab.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InstalledEntry {
    /// Display name (registry name/id, or .toc Title, or folder name).
    pub name: String,
    pub version: String,
    pub size: u64,
    /// Folders deleted together with this row.
    pub folders: Vec<String>,
    /// Registry id ("" for manual installs).
    pub registry_id: String,
}

impl InstalledEntry {
    pub fn by_launcher(&self) -> bool {
        !self.registry_id.is_empty()
    }
}

/// Lists `addons_dir`: first the addons installed by the launcher (grouped,
/// deleted as one), then every remaining folder (manual installs).
/// `None` if the directory does not exist.
pub fn scan_installed(
    addons_dir: &Path,
    registry: &[InstalledAddon],
) -> Option<Vec<InstalledEntry>> {
    if !addons_dir.is_dir() {
        return None;
    }
    let mut rows = Vec::new();
    let mut covered = HashSet::new();

    for a in registry {
        let present: Vec<String> = a
            .folders
            .iter()
            .filter(|f| folder_exists(addons_dir, f))
            .cloned()
            .collect();
        if present.is_empty() {
            continue;
        }
        let size = present.iter().map(|f| dir_size(&addons_dir.join(f))).sum();
        covered.extend(present.iter().cloned());
        rows.push(InstalledEntry {
            name: if a.name.is_empty() {
                a.id.clone()
            } else {
                a.name.clone()
            },
            version: a.version.clone(),
            size,
            folders: present,
            registry_id: a.id.clone(),
        });
    }

    for sub in sub_dirs(addons_dir) {
        if covered.contains(&sub) || !is_safe_folder_name(&sub) {
            continue;
        }
        let dir = addons_dir.join(&sub);
        let toc = dir.join(format!("{sub}.toc"));
        let title = read_toc_value(&toc, "Title");
        rows.push(InstalledEntry {
            name: if title.is_empty() { sub.clone() } else { title },
            version: read_toc_value(&toc, "Version"),
            size: dir_size(&dir),
            folders: vec![sub],
            registry_id: String::new(),
        });
    }
    Some(rows)
}

/// [`scan_installed`] on a worker thread.
pub fn spawn_scan(
    addons_dir: PathBuf,
    registry: Vec<InstalledAddon>,
) -> mpsc::Receiver<Option<Vec<InstalledEntry>>> {
    let (tx, rx) = mpsc::channel();
    std::thread::Builder::new()
        .name("addons-scan".into())
        .spawn(move || {
            let _ = tx.send(scan_installed(&addons_dir, &registry));
        })
        .ok();
    rx
}

// --- Install pipeline ----------------------------------------------------------------

/// Temp directory of one install: `<tmp>/SylvaniaLauncher_Addons/<safe id>`.
#[derive(Debug, Clone)]
pub struct Staging {
    pub dir: PathBuf,
    pub zip: PathBuf,
}

impl Staging {
    /// Creates the staging directory and the (empty) archive file.
    pub fn create(id: &str) -> std::io::Result<Self> {
        let sid = safe_id(id);
        let dir = std::env::temp_dir()
            .join("SylvaniaLauncher_Addons")
            .join(&sid);
        if dir.exists() {
            let _ = std::fs::remove_dir_all(&dir);
        }
        std::fs::create_dir_all(&dir)?;
        let zip = dir.join(format!("{sid}.zip"));
        std::fs::File::create(&zip)?;
        Ok(Staging { dir, zip })
    }

    pub fn extract_root(&self) -> PathBuf {
        self.dir.join("extracted")
    }

    pub fn remove(&self) {
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InstallError {
    Network,
    NotZip,
    Extract,
    NoFolders,
    Cancelled,
}

#[derive(Debug)]
pub enum InstallEvent {
    /// Download progress in percent (only when the size is known).
    Downloading(Option<u8>),
    /// Extraction progress in [0, 1].
    Extracting(f32),
    /// Deployed folder names, or the failure.
    Finished(Result<Vec<String>, InstallError>),
}

/// A running install; dropping it does not stop it, [`InstallJob::cancel`] does.
pub struct InstallJob {
    pub rx: mpsc::Receiver<InstallEvent>,
    cancel: Arc<AtomicBool>,
}

impl InstallJob {
    pub fn cancel(&self) {
        self.cancel.store(true, Ordering::Relaxed);
    }
}

/// Downloads `url` (redirects only within the trusted host), checks it is a
/// ZIP, extracts it and deploys its folders into `addons_dir`. The staging
/// directory is always removed at the end.
pub fn spawn_install(
    url: String,
    expected: Vec<String>,
    staging: Staging,
    addons_dir: PathBuf,
) -> InstallJob {
    let (tx, rx) = mpsc::channel();
    let cancel = Arc::new(AtomicBool::new(false));
    let flag = cancel.clone();
    let worker = move || {
        let result = run_install(&url, &expected, &staging, &addons_dir, &flag, &tx);
        staging.remove();
        let _ = tx.send(InstallEvent::Finished(result));
    };
    if let Err(e) = std::thread::Builder::new()
        .name("addons-install".into())
        .spawn(worker)
    {
        log::error!("Thread d'installation d'addon impossible : {e}");
        let (tx, rx2) = mpsc::channel();
        let _ = tx.send(InstallEvent::Finished(Err(InstallError::Extract)));
        return InstallJob { rx: rx2, cancel };
    }
    InstallJob { rx, cancel }
}

fn run_install(
    url: &str,
    expected: &[String],
    staging: &Staging,
    addons_dir: &Path,
    cancel: &AtomicBool,
    tx: &mpsc::Sender<InstallEvent>,
) -> Result<Vec<String>, InstallError> {
    let mut last_pct = None;
    download_trusted(url, &staging.zip, cancel, |received, total| {
        let pct = total
            .filter(|t| *t > 0)
            .map(|t| (received.saturating_mul(100) / t).min(100) as u8);
        if pct != last_pct || received == 0 {
            last_pct = pct;
            let _ = tx.send(InstallEvent::Downloading(pct));
        }
    })
    .map_err(|e| {
        log::warn!("Téléchargement de l'addon {url} : {e}");
        if cancel.load(Ordering::Relaxed) {
            InstallError::Cancelled
        } else {
            InstallError::Network
        }
    })?;

    if !crate::util::archive::is_zip_file(&staging.zip) {
        return Err(InstallError::NotZip);
    }

    let _ = tx.send(InstallEvent::Extracting(0.0));
    let root = staging.extract_root();
    crate::util::archive::extract_zip(&staging.zip, &root, cancel, |done, total| {
        if total > 0 && (done % 16 == 0 || done == total) {
            let _ = tx.send(InstallEvent::Extracting(done as f32 / total as f32));
        }
    })
    .map_err(|e| {
        log::warn!("Extraction de {} : {e}", staging.zip.display());
        if cancel.load(Ordering::Relaxed) {
            InstallError::Cancelled
        } else {
            InstallError::Extract
        }
    })?;

    if cancel.load(Ordering::Relaxed) {
        return Err(InstallError::Cancelled);
    }
    let deployed = deploy_extracted(&root, expected, addons_dir);
    if deployed.is_empty() {
        Err(InstallError::NoFolders)
    } else {
        Ok(deployed)
    }
}

/// Streams `url` to `dest`. Like the Qt SameOriginRedirectPolicy, redirects
/// are only followed while they stay on https + the trusted host.
fn download_trusted(
    url: &str,
    dest: &Path,
    cancel: &AtomicBool,
    mut on_progress: impl FnMut(u64, Option<u64>),
) -> Result<u64, String> {
    let policy = reqwest::redirect::Policy::custom(|attempt| {
        if attempt.previous().len() >= 10 {
            attempt.error("trop de redirections")
        } else if attempt.url().scheme() == "https" && attempt.url().host_str() == Some(SERVER_HOST)
        {
            attempt.follow()
        } else {
            attempt.stop()
        }
    });
    let client = reqwest::blocking::Client::builder()
        .user_agent(format!("SylvaniaLauncher/{APP_VERSION}"))
        .connect_timeout(Duration::from_secs(20))
        .timeout(None)
        .redirect(policy)
        .build()
        .map_err(|e| e.to_string())?;
    let mut resp = client.get(url).send().map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status()));
    }
    let total = resp.content_length();
    let mut file = std::fs::File::create(dest).map_err(|e| e.to_string())?;
    let mut buf = vec![0u8; 64 * 1024];
    let mut received = 0u64;
    on_progress(0, total);
    loop {
        if cancel.load(Ordering::Relaxed) {
            return Err("cancelled".into());
        }
        let n = resp.read(&mut buf).map_err(|e| e.to_string())?;
        if n == 0 {
            break;
        }
        file.write_all(&buf[..n]).map_err(|e| e.to_string())?;
        received += n as u64;
        on_progress(received, total);
    }
    file.flush().map_err(|e| e.to_string())?;
    Ok(received)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn mkdirs(root: &Path, rels: &[&str]) {
        for r in rels {
            std::fs::create_dir_all(root.join(r)).unwrap();
        }
    }

    #[test]
    fn trusted_urls() {
        assert!(is_trusted_url(
            "https://sylvania-servergame.com/download_addon.php?file=x.zip"
        ));
        assert!(is_trusted_url("https://SYLVANIA-SERVERGAME.COM/a.zip"));
        assert!(!is_trusted_url("http://sylvania-servergame.com/a.zip"));
        assert!(!is_trusted_url("https://evil.com/a.zip"));
        assert!(!is_trusted_url(
            "https://sylvania-servergame.com.evil.com/a.zip"
        ));
        assert!(!is_trusted_url(
            "https://evil.sylvania-servergame.com/a.zip"
        ));
        assert!(!is_trusted_url(
            "https://sylvania-servergame.com@evil.com/a.zip"
        ));
        assert!(!is_trusted_url("ftp://sylvania-servergame.com/a.zip"));
        assert!(!is_trusted_url("not a url"));
        assert!(!is_trusted_url(""));
    }

    #[test]
    fn folder_name_safety() {
        assert!(is_safe_folder_name("DBM-Core"));
        assert!(is_safe_folder_name("_NPCScan-Overlay"));
        assert!(is_safe_folder_name("Prat-3.0"));
        for bad in [
            "",
            ".",
            "..",
            "../x",
            "a/b",
            "a\\b",
            "C:",
            "C:\\Windows",
            "/etc",
            "x..y",
        ] {
            assert!(!is_safe_folder_name(bad), "{bad}");
        }
        assert_eq!(safe_id("Details! Damage/Meter"), "Details__Damage_Meter");
        assert_eq!(safe_id(""), "addon");
    }

    #[test]
    fn toc_parsing() {
        let toc = "\u{feff}## Interface: 30300\r\n##  Title: ignored\r\n## title: |cff1784d1ElvUI|r |cffffffffCore|r\r\n## Version: 6.08 \r\n## Notes: x\n";
        assert_eq!(parse_toc_value(toc, "Title"), "ElvUI Core");
        assert_eq!(parse_toc_value(toc, "Version"), "6.08");
        assert_eq!(parse_toc_value(toc, "Interface"), "30300");
        assert_eq!(parse_toc_value(toc, "Author"), "");
        assert_eq!(strip_color_codes("a|cFFzz|rb|"), "a|cFFzzb|");

        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("X.toc");
        std::fs::write(&path, "## Title: X Addon\n").unwrap();
        assert_eq!(read_toc_value(&path, "Title"), "X Addon");
        assert_eq!(read_toc_value(&dir.path().join("missing.toc"), "Title"), "");
    }

    #[test]
    fn find_folder_respects_depth() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();
        mkdirs(
            root,
            &["wrap/inner/DBM-Core", "a/b/c/d/e/Deep", "a/b/c/d/Ok"],
        );
        assert_eq!(
            find_folder_in(root, "DBM-Core", 4),
            Some(root.join("wrap/inner/DBM-Core"))
        );
        assert_eq!(find_folder_in(root, "Ok", 4), Some(root.join("a/b/c/d/Ok")));
        assert_eq!(find_folder_in(root, "Deep", 4), None);
        assert_eq!(
            find_folder_in(root, "Deep", 5),
            Some(root.join("a/b/c/d/e/Deep"))
        );
        assert_eq!(find_folder_in(root, "Missing", 4), None);
    }

    #[test]
    fn deploy_moves_every_sibling_and_overwrites() {
        let dir = tempfile::tempdir().unwrap();
        let extracted = dir.path().join("extracted");
        let addons = dir.path().join("AddOns");
        mkdirs(
            &extracted,
            &[
                "DBM-master/DBM-Core",
                "DBM-master/DBM-GUI",
                "DBM-master/DBM-Naxx",
            ],
        );
        std::fs::write(
            extracted.join("DBM-master/DBM-Core/DBM-Core.toc"),
            "## Title: DBM\n",
        )
        .unwrap();
        mkdirs(&addons, &["DBM-Core/old-stuff"]);

        let deployed = deploy_extracted(&extracted, &["DBM-Core".into()], &addons);
        assert_eq!(deployed, vec!["DBM-Core", "DBM-GUI", "DBM-Naxx"]);
        assert!(addons.join("DBM-Core/DBM-Core.toc").is_file());
        assert!(!addons.join("DBM-Core/old-stuff").exists());
        assert!(addons.join("DBM-Naxx").is_dir());

        // Expected folder not found: top level of the archive is deployed.
        let extracted2 = dir.path().join("extracted2");
        mkdirs(&extracted2, &["Other"]);
        assert_eq!(
            deploy_extracted(&extracted2, &["Nope".into()], &addons),
            vec!["Other"]
        );
        let empty = dir.path().join("empty");
        mkdirs(&empty, &[""]);
        assert!(deploy_extracted(&empty, &["X".into()], &addons).is_empty());
    }

    #[test]
    fn scan_and_remove() {
        let dir = tempfile::tempdir().unwrap();
        let addons = dir.path().join("AddOns");
        assert!(scan_installed(&addons, &[]).is_none());
        mkdirs(&addons, &["ElvUI", "ElvUI_OptionsUI", "Manual", "NoToc"]);
        std::fs::write(
            addons.join("Manual/Manual.toc"),
            "## Title: |cff00ff00Manual|r Addon\n## Version: 2.1\n",
        )
        .unwrap();
        std::fs::write(addons.join("ElvUI/file.lua"), vec![0u8; 1000]).unwrap();
        std::fs::write(addons.join("ElvUI_OptionsUI/o.lua"), vec![0u8; 24]).unwrap();

        let reg = vec![InstalledAddon {
            id: "elvui".into(),
            name: "ElvUI".into(),
            version: "6.08".into(),
            folders: vec![
                "ElvUI".into(),
                "ElvUI_OptionsUI".into(),
                "ElvUI_Gone".into(),
                "../evil".into(),
            ],
            source: "recommended".into(),
        }];
        let rows = scan_installed(&addons, &reg).unwrap();
        assert_eq!(rows.len(), 3);
        assert_eq!(rows[0].folders, vec!["ElvUI", "ElvUI_OptionsUI"]);
        assert_eq!(rows[0].size, 1024);
        assert!(rows[0].by_launcher());
        assert_eq!(
            (rows[1].name.as_str(), rows[1].version.as_str()),
            ("Manual Addon", "2.1")
        );
        assert_eq!(rows[2].name, "NoToc");
        assert!(!rows[2].by_launcher());

        let failures = remove_folders(&addons, &["ElvUI".into(), "Absent".into(), "..".into()]);
        assert_eq!(failures, vec![".."]);
        assert!(!addons.join("ElvUI").exists());
        assert!(dir.path().exists());
    }
}
