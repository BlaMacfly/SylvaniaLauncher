//! HD patch download + installation, run on a worker thread.
//! STUB — implemented by the settings / HD patch port. The API below is the
//! contract used by the main window: keep these signatures.

use crate::i18n::I18n;
use std::path::{Path, PathBuf};

/// Progress reported by [`HdPatchTask::poll`].
#[derive(Debug, Clone)]
pub enum HdEvent {
    /// `percent` None = indeterminate (busy). `status` is already translated.
    Progress { percent: Option<u8>, status: String },
    /// Terminal event. `message` is already translated.
    Finished { ok: bool, message: String },
}

pub struct HdPatchTask {}

impl HdPatchTask {
    /// Starts download + install into `wow_path` on a background thread.
    pub fn start(wow_path: PathBuf, i18n: I18n) -> Self {
        let _ = (wow_path, i18n);
        HdPatchTask {}
    }

    /// Drains pending events (non-blocking).
    pub fn poll(&mut self) -> Vec<HdEvent> {
        Vec::new()
    }

    /// False once a Finished event has been produced.
    pub fn is_running(&self) -> bool {
        false
    }

    /// Heuristic: is the HD patch already present in this client?
    pub fn is_installed(wow_path: &Path) -> bool {
        let _ = wow_path;
        false
    }
}
