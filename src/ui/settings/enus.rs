//! English language pack (enUS) download, run on a worker thread: streamed
//! download into `<wow>/Data`, SHA-256 (logged), extraction into `<wow>/Data`,
//! archive removal. Replaces the Qt `DownloadDialog` used with
//! `setSkipConfigWtf(true)`: Config.wtf is left untouched.

use crate::constants;
use crate::core::hd_patch::network_error_message;
use crate::i18n::I18n;
use crate::util::{archive, format_bytes, format_duration, http};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::time::{Duration, Instant};

/// Temporary archive name inside the destination (same as Qt).
const ARCHIVE_NAME: &str = "SylvaniaClient_temp.zip";
const PROGRESS_THROTTLE: Duration = Duration::from_millis(150);

/// Snapshot of the task shown in the settings window (already translated).
#[derive(Debug, Clone, Default)]
pub struct Progress {
    /// 0..=1, None = indeterminate.
    pub fraction: Option<f32>,
    pub status: String,
    /// Size / speed / remaining time line.
    pub details: String,
    /// Cancel only makes sense while downloading (extraction is never
    /// interrupted, so the client is not left half-written).
    pub cancellable: bool,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Outcome {
    Installed,
    Cancelled,
    Failed(String),
}

enum Msg {
    Progress(Progress),
    Done(Outcome),
}

pub struct EnUsTask {
    rx: Receiver<Msg>,
    cancel: Arc<AtomicBool>,
    progress: Progress,
}

impl EnUsTask {
    /// Starts downloading the pack into `data_dir` (`<wow>/Data`).
    pub fn start(data_dir: PathBuf, i18n: I18n) -> Self {
        let (tx, rx) = mpsc::channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let worker_cancel = Arc::clone(&cancel);
        let progress = Progress {
            status: i18n.t("Connexion au serveur..."),
            cancellable: true,
            ..Default::default()
        };
        let worker_tx = tx.clone();
        let worker_i18n = i18n.clone();
        let spawned = std::thread::Builder::new()
            .name("enus-pack".into())
            .spawn(move || {
                let outcome = run(&data_dir, &worker_i18n, &worker_cancel, &worker_tx);
                let _ = worker_tx.send(Msg::Done(outcome));
            });
        if let Err(e) = spawned {
            let _ = tx.send(Msg::Done(Outcome::Failed(format!(
                "{}: {e}",
                i18n.t("Erreur")
            ))));
        }
        EnUsTask {
            rx,
            cancel,
            progress,
        }
    }

    /// Applies pending updates; returns the outcome once the task is over.
    pub fn poll(&mut self) -> Option<Outcome> {
        let mut outcome = None;
        for msg in self.rx.try_iter() {
            match msg {
                Msg::Progress(p) => self.progress = p,
                Msg::Done(o) => outcome = Some(o),
            }
        }
        outcome
    }

    pub fn progress(&self) -> &Progress {
        &self.progress
    }

    pub fn cancel(&self) {
        self.cancel.store(true, Ordering::Relaxed);
    }
}

impl Drop for EnUsTask {
    /// Closing the settings window aborts a download still in flight.
    fn drop(&mut self) {
        self.cancel();
    }
}

/// Download speed / ETA, refreshed about once per second.
struct SpeedMeter {
    since: Instant,
    bytes_at: u64,
    text: String,
}

impl SpeedMeter {
    fn update(&mut self, i18n: &I18n, received: u64, total: Option<u64>) {
        let elapsed = self.since.elapsed();
        if elapsed < Duration::from_secs(1) {
            return;
        }
        let per_sec =
            (received.saturating_sub(self.bytes_at) as f64 / elapsed.as_secs_f64()) as u64;
        self.since = Instant::now();
        self.bytes_at = received;
        self.text = i18n.tf("Vitesse: %1/s", &[&format_bytes(i18n, per_sec)]);
        if let Some(total) = total.filter(|_| per_sec > 0) {
            let remaining = total.saturating_sub(received) / per_sec;
            self.text.push_str("   ");
            self.text
                .push_str(&i18n.tf("Temps restant: %1", &[&format_duration(remaining)]));
        }
    }
}

fn run(dest: &Path, i18n: &I18n, cancel: &AtomicBool, tx: &Sender<Msg>) -> Outcome {
    let send = |p: Progress| {
        let _ = tx.send(Msg::Progress(p));
    };
    if std::fs::create_dir_all(dest).is_err() {
        return Outcome::Failed(i18n.t(
            "Impossible de créer le dossier de destination.\nVérifiez vos droits d'administrateur.",
        ));
    }
    if tempfile::tempfile_in(dest).is_err() {
        return Outcome::Failed(
            i18n.t("Impossible d'écrire dans le dossier de destination.\nVérifiez vos droits d'administrateur."),
        );
    }

    // 1. Download (cancellable)
    let archive_path = dest.join(ARCHIVE_NAME);
    let mut meter = SpeedMeter {
        since: Instant::now(),
        bytes_at: 0,
        text: String::new(),
    };
    let mut last_sent: Option<Instant> = None;
    let result = http::download_to_file(
        constants::ENUS_PACK_URL,
        &archive_path,
        cancel,
        |received, total| {
            let total = total.filter(|t| *t > 0);
            meter.update(i18n, received, total);
            let due = last_sent.is_none_or(|t| t.elapsed() >= PROGRESS_THROTTLE)
                || total == Some(received);
            if !due {
                return;
            }
            last_sent = Some(Instant::now());
            let (fraction, status, size) = match total {
                Some(total) => {
                    let percent = (received.saturating_mul(100) / total).min(100);
                    (
                        Some(received as f32 / total as f32),
                        i18n.tf("Téléchargement en cours... %1%", &[&percent]),
                        i18n.tf(
                            "Taille: %1 / %2",
                            &[&format_bytes(i18n, received), &format_bytes(i18n, total)],
                        ),
                    )
                }
                None => (
                    None,
                    i18n.t("Téléchargement..."),
                    format_bytes(i18n, received),
                ),
            };
            let details = if meter.text.is_empty() {
                size
            } else {
                format!("{size}   {}", meter.text)
            };
            send(Progress {
                fraction,
                status,
                details,
                cancellable: true,
            });
        },
    );
    match result {
        Ok(_) => {}
        Err(http::DownloadError::Cancelled) => return Outcome::Cancelled,
        Err(http::DownloadError::Http(e)) => {
            let _ = std::fs::remove_file(&archive_path);
            return Outcome::Failed(network_error_message(i18n, &e));
        }
        Err(http::DownloadError::Io(e)) => {
            let _ = std::fs::remove_file(&archive_path);
            return Outcome::Failed(format!(
                "{} ({e})",
                i18n.t("Impossible de créer le fichier de téléchargement")
            ));
        }
    }

    // 2. Integrity (no published hash for this endpoint: logged only)
    send(Progress {
        status: i18n.t("Vérification de l'intégrité du fichier..."),
        ..Default::default()
    });
    match http::sha256_file(&archive_path) {
        Ok(h) => log::info!("Pack enUS SHA-256: {h}"),
        Err(e) => log::warn!("SHA-256 du pack enUS impossible: {e}"),
    }

    // 3. Extraction into Data (never interrupted)
    let status = i18n.t("Extraction des fichiers en cours...");
    let wait = i18n.t("Veuillez patienter...");
    send(Progress {
        status: status.clone(),
        details: wait.clone(),
        ..Default::default()
    });
    let never = AtomicBool::new(false);
    let mut last_percent = None;
    let extracted = archive::extract_zip(&archive_path, dest, &never, |done, total| {
        let percent = done * 100 / total.max(1);
        if last_percent != Some(percent) {
            last_percent = Some(percent);
            let fraction = Some(done as f32 / total.max(1) as f32);
            send(Progress {
                fraction,
                status: status.clone(),
                details: wait.clone(),
                cancellable: false,
            });
        }
    });
    let _ = std::fs::remove_file(&archive_path);
    match extracted {
        Ok(()) => {
            send(Progress {
                fraction: Some(1.0),
                status: i18n.t("Extraction terminée!"),
                ..Default::default()
            });
            Outcome::Installed
        }
        Err(e) => Outcome::Failed(i18n.tf("Erreur lors de l'extraction : %1", &[&e])),
    }
}
