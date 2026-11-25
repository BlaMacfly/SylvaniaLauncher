//! Shared helpers: HTTP downloads, zip extraction, message boxes, formatting.

pub mod archive;
pub mod http;

use crate::i18n::I18n;

/// "12.34 Mo" style size (units translated like the Qt version).
pub fn format_bytes(i18n: &I18n, bytes: u64) -> String {
    let units = ["o", "Ko", "Mo", "Go"];
    let mut size = bytes as f64;
    let mut idx = 0;
    while size >= 1024.0 && idx < 3 {
        size /= 1024.0;
        idx += 1;
    }
    format!("{size:.2} {}", i18n.t(units[idx]))
}

/// "42s", "3m 12s", "1h 05m".
pub fn format_duration(seconds: u64) -> String {
    if seconds < 60 {
        format!("{seconds}s")
    } else if seconds < 3600 {
        format!("{}m {}s", seconds / 60, seconds % 60)
    } else {
        format!("{}h {}m", seconds / 3600, (seconds % 3600) / 60)
    }
}

/// Native message boxes (blocking, like QMessageBox::exec).
pub mod msg {
    use rfd::{MessageButtons, MessageDialog, MessageDialogResult, MessageLevel};

    fn show(level: MessageLevel, title: &str, text: &str) {
        MessageDialog::new()
            .set_level(level)
            .set_title(title)
            .set_description(text)
            .show();
    }
    pub fn info(title: &str, text: &str) {
        show(MessageLevel::Info, title, text);
    }
    pub fn warning(title: &str, text: &str) {
        show(MessageLevel::Warning, title, text);
    }
    pub fn error(title: &str, text: &str) {
        show(MessageLevel::Error, title, text);
    }
    /// Yes/No question; true = Yes.
    pub fn confirm(title: &str, text: &str) -> bool {
        matches!(
            MessageDialog::new()
                .set_level(MessageLevel::Warning)
                .set_title(title)
                .set_description(text)
                .set_buttons(MessageButtons::YesNo)
                .show(),
            MessageDialogResult::Yes
        )
    }
}
