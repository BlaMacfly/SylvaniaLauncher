//! Client download (segmented/resumable when the size is known, SHA-256
//! check when known) + extraction + Config.wtf generation, on a worker thread.
//!
//! Port of the Qt `DownloadDialog` logic. The UI side (`ui::download`) only
//! renders the [`Snapshot`]s this worker sends and forwards the cancel request.

use crate::config::Config;
use crate::constants;
use crate::i18n::I18n;
use crate::paths;
use crate::util::{self, archive};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError};
use std::time::{Duration, Instant};

/// Temporary archive name, created inside the destination folder.
pub const ARCHIVE_NAME: &str = "SylvaniaClient_temp.zip";

/// What to download and how to configure the client afterwards.
#[derive(Debug, Clone)]
pub struct DownloadParams {
    pub url: String,
    pub destination: PathBuf,
    /// 0 = unknown (single stream); otherwise segmented + size-checked.
    pub expected_size: u64,
    /// Empty = unknown (no hash comparison).
    pub expected_sha256: String,
    /// Address written into the generated Config.wtf (realmList).
    pub realmlist_address: String,
    /// "fr" or "en" (Config.wtf locale).
    pub language: String,
}

impl DownloadParams {
    /// WoW 3.3.5a client (Qt `configureForClient`).
    pub fn for_client(destination: PathBuf, realmlist_address: Option<String>, language: &str) -> Self {
        let address = realmlist_address
            .filter(|a| !a.trim().is_empty())
            .unwrap_or_else(|| constants::DEFAULT_REALMLIST.to_string());
        DownloadParams {
            url: constants::CLIENT_URL.to_string(),
            destination,
            expected_size: constants::CLIENT_EXPECTED_SIZE,
            expected_sha256: constants::CLIENT_SHA256.to_lowercase(),
            realmlist_address: address,
            language: language.to_string(),
        }
    }

    pub fn archive_path(&self) -> PathBuf {
        self.destination.join(ARCHIVE_NAME)
    }
}

/// Everything the dialog displays (all strings already translated).
#[derive(Debug, Clone, Default)]
pub struct Snapshot {
    pub status: String,
    /// Dialog progress bar: None = indeterminate.
    pub percent: Option<u8>,
    pub speed: String,
    pub size: String,
    pub eta: String,
    /// Mirror for the main-window primary button (Qt `progressChanged`):
    /// Some(0-100) while downloading, None = indeterminate (verify/extract).
    pub button_percent: Option<u8>,
}

/// Terminal result of a download.
#[derive(Debug, Clone)]
pub struct Outcome {
    pub ok: bool,
    /// Already translated.
    pub message: String,
    /// The Qt dialog reported this failure with a critical box
    /// (`failDownload` / network errors) rather than a plain warning.
    pub critical: bool,
    /// Folder containing the client exe (searched recursively), on success.
    pub client_dir: Option<PathBuf>,
}

impl Outcome {
    pub fn cancelled(i18n: &I18n) -> Self {
        Outcome { ok: false, message: i18n.t("Téléchargement annulé"), critical: false, client_dir: None }
    }
}

#[derive(Debug, Clone)]
pub enum DlEvent {
    Update(Snapshot),
    Finished(Outcome),
}

/// Handle on the background download thread.
pub struct DownloadTask {
    rx: Receiver<DlEvent>,
    cancel: Arc<AtomicBool>,
    i18n: I18n,
    finished: bool,
}

impl DownloadTask {
    pub fn start(params: DownloadParams, i18n: I18n) -> Self {
        let (tx, rx) = mpsc::channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let worker_cancel = cancel.clone();
        let worker_i18n = i18n.clone();
        let spawned = std::thread::Builder::new()
            .name("client-download".into())
            .spawn(move || Worker::new(params, worker_i18n, tx, worker_cancel).run_to_end());
        if let Err(e) = spawned {
            log::warn!("Impossible de lancer le téléchargement: {e}");
        }
        DownloadTask { rx, cancel, i18n, finished: false }
    }

    /// Drains pending events (non-blocking). A worker that died without a
    /// result is reported as a failure.
    pub fn poll(&mut self) -> Vec<DlEvent> {
        let mut out = Vec::new();
        if self.finished {
            return out;
        }
        loop {
            match self.rx.try_recv() {
                Ok(ev) => {
                    if matches!(ev, DlEvent::Finished(_)) {
                        self.finished = true;
                    }
                    out.push(ev);
                    if self.finished {
                        break;
                    }
                }
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => {
                    self.finished = true;
                    out.push(DlEvent::Finished(Outcome {
                        ok: false,
                        message: self.i18n.t("Erreur interne."),
                        critical: true,
                        client_dir: None,
                    }));
                    break;
                }
            }
        }
        out
    }

    /// Asks the worker to stop; it removes the temporary archive itself.
    pub fn cancel(&self) {
        self.cancel.store(true, Ordering::Relaxed);
    }
}

// --- Worker -------------------------------------------------------------------

/// Internal failure kinds (mirrors how the Qt dialog reported them).
enum Fail {
    Cancelled,
    /// `failDownload`: critical box; optionally removes a file.
    Critical(String, Option<PathBuf>),
    /// Reported only through `downloadFinished(false, …)`.
    Plain(String),
}

enum SegError {
    Cancelled,
    /// Server ignored the Range header (HTTP 200): fall back to one stream.
    NoRange,
    Net(String),
}

const UI_THROTTLE: Duration = Duration::from_millis(100);

struct Worker {
    p: DownloadParams,
    i18n: I18n,
    tx: Sender<DlEvent>,
    cancel: Arc<AtomicBool>,
    snap: Snapshot,
    last_sent: Instant,
    // Speed / ETA (Qt updateSpeed).
    speed_timer: Instant,
    last_bytes: u64,
    total_bytes: u64,
}

impl Worker {
    fn new(p: DownloadParams, i18n: I18n, tx: Sender<DlEvent>, cancel: Arc<AtomicBool>) -> Self {
        let snap = Snapshot {
            status: i18n.t("Préparation du téléchargement..."),
            percent: Some(0),
            speed: i18n.t("Vitesse: --"),
            size: i18n.t("Taille: --"),
            eta: i18n.t("Temps restant: --"),
            button_percent: None,
        };
        Worker {
            p,
            i18n,
            tx,
            cancel,
            snap,
            last_sent: Instant::now(),
            speed_timer: Instant::now(),
            last_bytes: 0,
            total_bytes: 0,
        }
    }

    fn t(&self, s: &str) -> String {
        self.i18n.t(s)
    }

    fn fmt(&self, bytes: u64) -> String {
        util::format_bytes(&self.i18n, bytes)
    }

    fn cancelled(&self) -> bool {
        self.cancel.load(Ordering::Relaxed)
    }

    /// Sends the current snapshot (throttled unless `force`).
    fn send(&mut self, force: bool) {
        if force || self.last_sent.elapsed() >= UI_THROTTLE {
            self.last_sent = Instant::now();
            let _ = self.tx.send(DlEvent::Update(self.snap.clone()));
        }
    }

    fn run_to_end(mut self) {
        self.send(true);
        let archive = self.p.archive_path();
        let outcome = match self.run() {
            Ok(client_dir) => Outcome {
                ok: true,
                message: self.t("Téléchargement et extraction terminés!"),
                critical: false,
                client_dir,
            },
            Err(Fail::Cancelled) => {
                let _ = std::fs::remove_file(&archive);
                Outcome::cancelled(&self.i18n)
            }
            Err(Fail::Critical(message, remove)) => {
                if let Some(f) = remove {
                    let _ = std::fs::remove_file(f);
                }
                Outcome { ok: false, message, critical: true, client_dir: None }
            }
            Err(Fail::Plain(message)) => Outcome { ok: false, message, critical: false, client_dir: None },
        };
        let _ = self.tx.send(DlEvent::Finished(outcome));
    }

    /// Qt `startDownload` → strategy → `verifyIntegrity` → extraction.
    fn run(&mut self) -> Result<Option<PathBuf>, Fail> {
        let dest = self.p.destination.clone();
        if dest.as_os_str().is_empty() {
            return Err(Fail::Plain(self.t("Destination non spécifiée")));
        }
        if !dest.is_dir() && std::fs::create_dir_all(&dest).is_err() {
            return Err(Fail::Critical(
                self.t("Impossible de créer le dossier de destination.\nVérifiez vos droits d'administrateur."),
                None,
            ));
        }
        if !is_writable(&dest) {
            return Err(Fail::Critical(
                self.t("Impossible d'écrire dans le dossier de destination.\nVérifiez vos droits d'administrateur."),
                None,
            ));
        }

        // Free-space gate before anything is transferred: archive + extracted
        // tree coexist during installation, hence the multiplier.
        let mut disk_checked = false;
        if self.p.expected_size > 0 {
            disk_checked = true;
            self.check_disk_space(required_space(self.p.expected_size), None)?;
        }

        self.speed_timer = Instant::now();
        if self.p.expected_size > 0 {
            self.segmented()?;
        } else {
            self.single_stream(disk_checked)?;
        }
        if self.cancelled() {
            return Err(Fail::Cancelled);
        }
        let archive = self.p.archive_path();
        self.verify(&archive)?;
        self.extract(&archive)
    }

    fn check_disk_space(&self, required: u64, remove: Option<&Path>) -> Result<(), Fail> {
        if required == 0 {
            return Ok(());
        }
        if let Some(available) = free_disk_space(&self.p.destination)
            && available < required
        {
            return Err(Fail::Critical(
                self.i18n.tf(
                    "Espace disque insuffisant: %1 libres, %2 requis (archive + extraction).",
                    &[&self.fmt(available), &self.fmt(required)],
                ),
                remove.map(Path::to_path_buf),
            ));
        }
        Ok(())
    }

    /// Qt `updateSpeed`: speed + ETA refreshed once per second.
    fn update_speed(&mut self, received: u64) {
        let elapsed = self.speed_timer.elapsed().as_millis() as u64;
        if elapsed > 1000 {
            let bps = received.saturating_sub(self.last_bytes) * 1000 / elapsed;
            self.last_bytes = received;
            self.speed_timer = Instant::now();
            self.snap.speed = self.i18n.tf("Vitesse: %1/s", &[&self.fmt(bps)]);
            if bps > 0 && self.total_bytes > 0 {
                let remaining = self.total_bytes.saturating_sub(received) / bps;
                self.snap.eta = self.i18n.tf("Temps restant: %1", &[&util::format_duration(remaining)]);
            }
        }
    }

    /// Progress labels while bytes are flowing (`total` 0 = unknown).
    fn show_progress(&mut self, received: u64, total: u64) {
        self.total_bytes = total;
        if total > 0 {
            let pct = percent_of(received, total);
            self.snap.percent = Some(pct);
            self.snap.button_percent = Some(pct);
            self.snap.status = self.i18n.tf("Téléchargement en cours... %1%", &[&pct]);
            self.snap.size = self.i18n.tf("Taille: %1 / %2", &[&self.fmt(received), &self.fmt(total)]);
        } else {
            // Size unknown (no Content-Length): busy bar + received bytes.
            self.snap.percent = None;
            self.snap.button_percent = None;
            self.snap.status = self.t("Téléchargement...");
            self.snap.size = self.i18n.tf("Taille: %1 / %2", &[&self.fmt(received), &"--"]);
        }
        self.update_speed(received);
        self.send(false);
    }

    fn net_error(&self, e: &reqwest::Error) -> String {
        if e.is_timeout() {
            return self.t("Délai de connexion dépassé");
        }
        if e.is_connect() {
            let detail = format!("{e:?}").to_lowercase();
            if detail.contains("dns") || detail.contains("resolve") || detail.contains("no such host") {
                return self.t("Serveur introuvable");
            }
            return self.t("Connexion refusée par le serveur");
        }
        self.t("Erreur réseau: ") + &e.to_string()
    }

    // --- Single streamed download (unknown size) ------------------------------

    fn single_stream(&mut self, disk_checked: bool) -> Result<(), Fail> {
        let path = self.p.archive_path();
        // Written straight to disk as it arrives, never buffered in memory.
        let mut file = std::fs::File::create(&path)
            .map_err(|_| Fail::Critical(self.t("Impossible de créer le fichier de téléchargement"), None))?;

        self.snap.status = self.t("Connexion au serveur...");
        self.send(true);

        let mut resp = http_client()
            .get(&self.p.url)
            .send()
            .map_err(|e| Fail::Critical(self.net_error(&e), Some(path.clone())))?;
        if !resp.status().is_success() {
            return Err(Fail::Critical(self.t("Erreur réseau: ") + &format!("HTTP {}", resp.status()), Some(path)));
        }
        let total = resp.content_length().unwrap_or(0);
        // Size was unknown: gate on the server-announced size (done once).
        if !disk_checked && total > 0 {
            self.check_disk_space(required_space(total), Some(&path))?;
        }

        self.last_bytes = 0;
        self.speed_timer = Instant::now();
        let mut buf = vec![0u8; 256 * 1024];
        let mut received = 0u64;
        loop {
            if self.cancelled() {
                return Err(Fail::Cancelled);
            }
            let n = resp
                .read(&mut buf)
                .map_err(|e| Fail::Critical(self.t("Erreur réseau: ") + &e.to_string(), Some(path.clone())))?;
            if n == 0 {
                break;
            }
            file.write_all(&buf[..n]).map_err(|_| {
                Fail::Critical(
                    self.t(
                        "Erreur d'écriture sur le disque (espace insuffisant ?). Le téléchargement a été interrompu.",
                    ),
                    Some(path.clone()),
                )
            })?;
            received += n as u64;
            self.show_progress(received, total);
        }
        file.flush().map_err(|_| {
            Fail::Critical(
                self.t("Erreur d'écriture sur le disque (espace insuffisant ?). Le téléchargement a été interrompu."),
                Some(path.clone()),
            )
        })?;
        drop(file);

        self.snap.status = self.t("Vérification de l'intégrité du fichier...");
        self.snap.percent = None;
        self.snap.button_percent = None;
        self.send(true);
        Ok(())
    }

    // --- Segmented, resumable download (known size + byte ranges) -------------

    fn segmented(&mut self) -> Result<(), Fail> {
        let path = self.p.archive_path();
        let expected = self.p.expected_size;
        let mut offset = std::fs::metadata(&path).map(|m| m.len()).unwrap_or(0);
        if offset > expected {
            offset = 0; // garbage -> restart fresh
        }

        // Keeps already-downloaded bytes; we then append from there.
        let mut file = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(&path)
            .map_err(|_| Fail::Critical(self.t("Impossible de créer le fichier de téléchargement"), None))?;
        file.set_len(offset)
            .and_then(|_| file.seek(SeekFrom::Start(offset)))
            .map_err(|_| Fail::Critical(self.t("Impossible de préparer le fichier de téléchargement"), None))?;

        if offset < expected {
            // Seed the speed baseline at the resume offset.
            self.last_bytes = offset;
            self.speed_timer = Instant::now();
            self.snap.status = self.t("Connexion au serveur...");
            self.send(true);

            let client = http_client();
            let mut retries = 0u32;
            let mut seg: Vec<u8> = Vec::new();
            while offset < expected {
                if self.cancelled() {
                    return Err(Fail::Cancelled);
                }
                let (start, end) = segment_range(offset, expected, constants::DOWNLOAD_SEGMENT_BYTES);
                seg.clear();
                let result = self.fetch_segment(&client, start, end, offset, &mut seg).and_then(|()| {
                    // A short body is a failed segment: never commit partial data.
                    if seg.len() as u64 == end - start + 1 {
                        Ok(())
                    } else {
                        Err(SegError::Net(format!("{} / {}", seg.len(), end - start + 1)))
                    }
                });
                match result {
                    Ok(()) => {
                        file.write_all(&seg).map_err(|_| {
                            Fail::Critical(
                                self.t("Erreur d'écriture sur le disque (espace insuffisant ?). Le téléchargement a été interrompu."),
                                None,
                            )
                        })?;
                        offset += seg.len() as u64;
                        retries = 0;
                        self.show_progress(offset, expected);
                    }
                    Err(SegError::Cancelled) => return Err(Fail::Cancelled),
                    Err(SegError::NoRange) => {
                        log::warn!("Le serveur ignore les requêtes Range: téléchargement en un flux");
                        drop(file);
                        return self.single_stream(true);
                    }
                    Err(SegError::Net(why)) => {
                        // Re-fetch the SAME segment (bounded retries); partial
                        // segments are never written, so nothing is duplicated.
                        retries += 1;
                        if retries > constants::DOWNLOAD_SEGMENT_RETRIES {
                            // Archive kept on disk: the next attempt resumes.
                            return Err(Fail::Critical(
                                self.i18n.tf("Erreur réseau pendant le téléchargement : %1", &[&why]),
                                None,
                            ));
                        }
                        self.snap.status = self.t("Reprise du téléchargement…");
                        self.send(true);
                        self.sleep_cancellable(Duration::from_millis(1500))?;
                    }
                }
            }
        }
        file.flush().ok();
        drop(file);

        // Cheap final size sanity before hashing.
        if std::fs::metadata(&path).map(|m| m.len()).unwrap_or(0) != expected {
            return Err(Fail::Critical(
                self.t("Fichier corrompu: taille inattendue. Le fichier a été supprimé."),
                Some(path),
            ));
        }

        // Qt showVerifyingState.
        self.snap.percent = None;
        self.snap.button_percent = None;
        self.snap.status = self.t("Vérification de l'intégrité (SHA-256)…");
        self.snap.speed = self.t("Cela peut prendre quelques minutes pour un gros client.");
        self.snap.size.clear();
        self.snap.eta.clear();
        self.send(true);
        Ok(())
    }

    /// GETs `bytes=start-end` into `seg` (buffered until complete).
    fn fetch_segment(
        &mut self,
        client: &reqwest::blocking::Client,
        start: u64,
        end: u64,
        committed: u64,
        seg: &mut Vec<u8>,
    ) -> Result<(), SegError> {
        let mut resp = client
            .get(&self.p.url)
            .header(reqwest::header::RANGE, format!("bytes={start}-{end}"))
            .send()
            .map_err(|e| SegError::Net(e.to_string()))?;
        let status = resp.status();
        if status == reqwest::StatusCode::OK {
            return Err(SegError::NoRange);
        }
        if status != reqwest::StatusCode::PARTIAL_CONTENT {
            return Err(SegError::Net(format!("HTTP {status}")));
        }
        let want = (end - start + 1) as usize;
        seg.reserve(want);
        let mut buf = vec![0u8; 256 * 1024];
        while seg.len() < want {
            if self.cancelled() {
                return Err(SegError::Cancelled);
            }
            let n = resp.read(&mut buf).map_err(|e| SegError::Net(e.to_string()))?;
            if n == 0 {
                break;
            }
            let take = n.min(want - seg.len());
            seg.extend_from_slice(&buf[..take]);
            // The UI tracks committed + in-flight bytes.
            let live = committed + seg.len() as u64;
            self.show_progress(live, self.p.expected_size);
        }
        Ok(())
    }

    fn sleep_cancellable(&self, d: Duration) -> Result<(), Fail> {
        let end = Instant::now() + d;
        while Instant::now() < end {
            if self.cancelled() {
                return Err(Fail::Cancelled);
            }
            std::thread::sleep(Duration::from_millis(50));
        }
        Ok(())
    }

    // --- Integrity + extraction -------------------------------------------------

    fn verify(&mut self, path: &Path) -> Result<(), Fail> {
        // 1) Expected size, when known. Checked before hashing — cheap and final.
        if self.p.expected_size > 0 {
            let actual = std::fs::metadata(path).map(|m| m.len()).unwrap_or(0);
            if actual != self.p.expected_size {
                return Err(Fail::Critical(
                    self.i18n.tf(
                        "Fichier corrompu: taille inattendue (%1 au lieu de %2). Le fichier a été supprimé.",
                        &[&self.fmt(actual), &self.fmt(self.p.expected_size)],
                    ),
                    Some(path.to_path_buf()),
                ));
            }
        }
        // 2) SHA-256, when known (the endpoint publishes none yet).
        if self.p.expected_sha256.is_empty() {
            log::warn!("Aucun hash attendu configuré - extraction sans comparaison.");
            return Ok(());
        }
        let actual = util::http::sha256_file(path).unwrap_or_default();
        if self.cancelled() {
            return Err(Fail::Cancelled);
        }
        log::info!("SHA-256 du fichier téléchargé: {actual}");
        if actual.is_empty() || !actual.eq_ignore_ascii_case(&self.p.expected_sha256) {
            return Err(Fail::Critical(
                self.t("Fichier corrompu: le hash SHA-256 ne correspond pas. Le fichier a été supprimé."),
                Some(path.to_path_buf()),
            ));
        }
        Ok(())
    }

    fn extract(&mut self, path: &Path) -> Result<Option<PathBuf>, Fail> {
        self.snap.status = self.t("Extraction des fichiers en cours...");
        self.snap.speed = self.t("Veuillez patienter...");
        self.snap.size.clear();
        self.snap.eta.clear();
        self.snap.percent = Some(0);
        self.snap.button_percent = None;
        self.send(true);

        let cancel = self.cancel.clone();
        let dest = self.p.destination.clone();
        let result = archive::extract_zip(path, &dest, &cancel, |done, total| {
            self.snap.percent = Some(percent_of(done as u64, total.max(1) as u64));
            self.send(false);
        });
        if let Err(e) = result {
            if self.cancelled() {
                return Err(Fail::Cancelled);
            }
            // The archive is kept so the user can retry.
            return Err(Fail::Plain(self.i18n.tf("Erreur lors de l'extraction : %1", &[&e])));
        }

        self.snap.percent = Some(100);
        self.snap.button_percent = Some(100);
        self.snap.status = self.t("Extraction terminée!");
        self.send(true);

        // Remove temp archive, then generate Config.wtf.
        let _ = std::fs::remove_file(path);
        let client_dir = find_client_exe(&dest).and_then(|exe| exe.parent().map(Path::to_path_buf));
        let target = client_dir.clone().unwrap_or_else(|| dest.clone());
        if let Err(e) = write_config_wtf(&target, &self.p.language, &self.p.realmlist_address) {
            log::warn!("Impossible d'écrire Config.wtf: {e}");
        }
        Ok(client_dir)
    }
}

// --- Helpers --------------------------------------------------------------------

fn http_client() -> reqwest::blocking::Client {
    util::http::client(None)
}

fn required_space(size: u64) -> u64 {
    (size as f64 * constants::CLIENT_DISK_SPACE_FACTOR) as u64
}

fn percent_of(done: u64, total: u64) -> u8 {
    if total == 0 {
        return 0;
    }
    ((done.min(total) as u128 * 100) / total as u128) as u8
}

/// Inclusive byte range of the next segment starting at `offset`.
fn segment_range(offset: u64, total: u64, segment: u64) -> (u64, u64) {
    let end = (offset + segment).min(total) - 1;
    (offset, end)
}

/// Writability probe: creates (and deletes) a temp file in `dir`.
fn is_writable(dir: &Path) -> bool {
    tempfile::NamedTempFile::new_in(dir).is_ok()
}

/// Free bytes available to the user on the volume holding `dir`.
#[cfg(windows)]
pub fn free_disk_space(dir: &Path) -> Option<u64> {
    use std::os::windows::ffi::OsStrExt;
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetDiskFreeSpaceExW(
            directory: *const u16,
            free_to_caller: *mut u64,
            total: *mut u64,
            total_free: *mut u64,
        ) -> i32;
    }
    let wide: Vec<u16> = dir.as_os_str().encode_wide().chain(std::iter::once(0)).collect();
    let mut available = 0u64;
    // SAFETY: `wide` is NUL-terminated and outlives the call; the out pointer
    // is valid, the two optional ones are null.
    let ok = unsafe { GetDiskFreeSpaceExW(wide.as_ptr(), &mut available, std::ptr::null_mut(), std::ptr::null_mut()) };
    (ok != 0).then_some(available)
}

#[cfg(not(windows))]
pub fn free_disk_space(_dir: &Path) -> Option<u64> {
    None
}

/// Breadth-first search for the client exe under `root` (case-insensitive,
/// symlinks not followed), so the shallowest match wins.
pub fn find_client_exe(root: &Path) -> Option<PathBuf> {
    let target = constants::CLIENT_EXE_NAME.to_lowercase();
    let mut queue = std::collections::VecDeque::from([root.to_path_buf()]);
    while let Some(dir) = queue.pop_front() {
        let Ok(rd) = std::fs::read_dir(&dir) else { continue };
        let mut subdirs = Vec::new();
        for entry in rd.flatten() {
            let Ok(ft) = entry.file_type() else { continue };
            if ft.is_file() && entry.file_name().to_string_lossy().to_lowercase() == target {
                return Some(entry.path());
            }
            if ft.is_dir() {
                subdirs.push(entry.path());
            }
        }
        subdirs.sort();
        queue.extend(subdirs);
    }
    None
}

/// Content of the generated WTF/Config.wtf (Qt `generateConfigWtf`), with
/// `\n` line endings.
pub fn config_wtf_content(language: &str, realmlist_address: &str) -> String {
    let locale = if language == "en" { "enUS" } else { "frFR" };
    let host = Config::host_from_address(realmlist_address);
    let lines: [(&str, &str); 56] = [
        ("locale", locale),
        ("hwDetect", "0"),
        ("gxRefresh", "60"),
        ("gxMultisampleQuality", "0.000000"),
        ("gxWindow", "1"),
        ("videoOptionsVersion", "3"),
        ("movie", "0"),
        ("Gamma", "1.000000"),
        ("readTOS", "1"),
        ("readEULA", "1"),
        ("readTerminationWithoutNotice", "1"),
        ("showToolsUI", "1"),
        ("Sound_OutputDriverName", "System Default"),
        ("Sound_MusicVolume", "0.40000000596046"),
        ("Sound_AmbienceVolume", "0.60000002384186"),
        ("componentTextureLevel", "9"),
        ("farclip", "1277"),
        ("projectedTextures", "1"),
        ("weatherDensity", "3"),
        ("accounttype", "LK"),
        ("mouseSpeed", "1.2000000476837"),
        ("gameTip", "16"),
        ("uiScale", "0.75999999046326"),
        ("useUiScale", "1"),
        ("checkAddonVersion", "0"),
        ("Sound_VoiceChatInputDriverName", "Réglage du système"),
        ("Sound_VoiceChatOutputDriverName", "Réglage du système"),
        ("movieSubtitle", "1"),
        ("Sound_SFXVolume", "0.5"),
        ("dbCompress", "0"),
        ("gxResolution", "1920x1080"),
        ("textureFilteringMode", "5"),
        ("groundEffectDist", "140"),
        ("environmentDetail", "1.5"),
        ("timingTestError", "0"),
        ("windowResizeLock", "1"),
        ("particleDensity", "1"),
        ("mapShadows", "0"),
        ("vertexShaders", "0"),
        ("pixelShaders", "0"),
        ("M2UseShaders", "0"),
        ("M2Faster", "3"),
        ("shadowinstancing", "0"),
        ("objectFade", "0"),
        ("timingMethod", "2"),
        ("ffxSpecial", "0"),
        ("groundEffectDensity", "64"),
        ("realmList", &host),
        ("realmName", "The Kingdom of Sylvania"),
        ("specular", "1"),
        ("Sound_MasterVolume", "0.60000002384186"),
        ("Sound_ZoneMusicNoDelay", "1"),
        ("gxTripleBuffer", "1"),
        ("gxMultisample", "8"),
        ("shadowLevel", "0"),
        ("extShadowQuality", "5"),
    ];
    let mut out = String::new();
    for (k, v) in lines {
        out.push_str(&format!("SET {k} \"{v}\"\n"));
    }
    out
}

/// Writes `<client_dir>/WTF/Config.wtf` atomically. Like the Qt text-mode
/// write, lines end with CRLF on Windows.
pub fn write_config_wtf(client_dir: &Path, language: &str, realmlist_address: &str) -> std::io::Result<()> {
    let mut content = config_wtf_content(language, realmlist_address);
    if cfg!(windows) {
        content = content.replace('\n', "\r\n");
    }
    paths::write_atomic(&client_dir.join("WTF").join("Config.wtf"), content.as_bytes())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn config_wtf_has_locale_realm_and_all_lines() {
        let c = config_wtf_content("en", "set realmlist \"logon.example.org\"");
        assert!(c.starts_with("SET locale \"enUS\"\n"));
        assert!(c.contains("SET realmList \"logon.example.org\"\n"));
        assert!(c.contains("SET Sound_VoiceChatInputDriverName \"Réglage du système\"\n"));
        assert!(c.ends_with("SET extShadowQuality \"5\"\n"));
        assert_eq!(c.lines().count(), 56);
        assert!(config_wtf_content("fr", "x").starts_with("SET locale \"frFR\""));
    }

    #[test]
    fn segment_ranges_are_inclusive_and_clamped() {
        assert_eq!(segment_range(0, 100, 32), (0, 31));
        assert_eq!(segment_range(96, 100, 32), (96, 99));
        assert_eq!(percent_of(50, 200), 25);
        assert_eq!(percent_of(5, 0), 0);
        assert_eq!(percent_of(300, 200), 100);
    }

    #[test]
    fn finds_client_exe_in_subfolder() {
        let dir = tempfile::tempdir().unwrap();
        let sub = dir.path().join("World of Warcraft 3.3.5a");
        std::fs::create_dir_all(sub.join("Data")).unwrap();
        std::fs::write(sub.join("WOW.EXE"), b"").unwrap();
        assert_eq!(find_client_exe(dir.path()), Some(sub.join("WOW.EXE")));
        assert_eq!(find_client_exe(&sub.join("Data")), None);
    }

    #[test]
    fn writes_config_wtf_under_wtf_dir() {
        let dir = tempfile::tempdir().unwrap();
        write_config_wtf(dir.path(), "fr", "set realmlist a.b").unwrap();
        let s = std::fs::read_to_string(dir.path().join("WTF/Config.wtf")).unwrap();
        assert!(s.contains("SET realmList \"a.b\""));
    }
}
