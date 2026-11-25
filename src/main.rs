//! Sylvania Launcher - World of Warcraft 3.3.5a launcher (Rust / egui).

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod app;
mod assets;
mod config;
mod constants;
mod core;
mod i18n;
mod notify;
mod paths;
mod sound;
mod stats;
mod theme;
mod ui;
mod util;

use eframe::egui;

fn main() -> eframe::Result<()> {
    init_logging();

    #[cfg(windows)]
    set_app_user_model_id();

    let viewport = egui::ViewportBuilder::default()
        .with_title(constants::WINDOW_TITLE)
        .with_inner_size([constants::MAIN_WINDOW_WIDTH, constants::MAIN_WINDOW_HEIGHT])
        .with_min_inner_size([
            constants::MAIN_WINDOW_MIN_WIDTH,
            constants::MAIN_WINDOW_MIN_HEIGHT,
        ])
        .with_icon(ui::app_icon());

    let options = eframe::NativeOptions {
        viewport,
        // Window size/position are restored by eframe's own persistence.
        persist_window: true,
        centered: true,
        ..Default::default()
    };
    eframe::run_native(
        "Sylvania Launcher",
        options,
        Box::new(|cc| Ok(Box::new(app::App::new(cc)))),
    )
}

/// Logs to `logs/launcher.log` (warnings and above, plus info in debug builds).
fn init_logging() {
    struct FileLogger(std::sync::Mutex<Option<std::fs::File>>);
    impl log::Log for FileLogger {
        fn enabled(&self, m: &log::Metadata) -> bool {
            m.target().starts_with("sylvania") && m.level() <= max_level()
        }
        fn log(&self, r: &log::Record) {
            if !self.enabled(r.metadata()) {
                return;
            }
            use std::io::Write;
            if let Ok(mut f) = self.0.lock()
                && let Some(f) = f.as_mut()
            {
                let _ = writeln!(
                    f,
                    "{} [{}] {}",
                    chrono::Local::now().format("%Y-%m-%d %H:%M:%S"),
                    r.level(),
                    r.args()
                );
            }
        }
        fn flush(&self) {}
    }
    fn max_level() -> log::Level {
        if cfg!(debug_assertions) {
            log::Level::Debug
        } else {
            log::Level::Warn
        }
    }
    let file = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(paths::logs_dir().join("launcher.log"))
        .ok();
    let logger = Box::leak(Box::new(FileLogger(std::sync::Mutex::new(file))));
    if log::set_logger(logger).is_ok() {
        log::set_max_level(max_level().to_level_filter());
    }
}

/// Explicit AppUserModelID for taskbar grouping.
#[cfg(windows)]
fn set_app_user_model_id() {
    #[link(name = "shell32")]
    unsafe extern "system" {
        fn SetCurrentProcessExplicitAppUserModelID(app_id: *const u16) -> i32;
    }
    let id: Vec<u16> = "sylvania.launcher"
        .encode_utf16()
        .chain(std::iter::once(0))
        .collect();
    // SAFETY: `id` is a valid NUL-terminated UTF-16 string that outlives the call.
    unsafe {
        SetCurrentProcessExplicitAppUserModelID(id.as_ptr());
    }
}
