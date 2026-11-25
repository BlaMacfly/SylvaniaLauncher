//! Native desktop notifications (Windows toasts) + the system-tray icon that
//! restores the launcher when clicked.

/// Shows a Windows toast. Never blocks the UI: sent from a short-lived thread.
pub fn toast(title: &str, body: &str) {
    #[cfg(windows)]
    {
        let (title, body) = (title.to_string(), body.to_string());
        std::thread::spawn(move || {
            use tauri_winrt_notification::Toast;
            // The PowerShell AUMID is always registered, so the toast shows up
            // without installing a Start-menu shortcut for the launcher.
            if let Err(e) = Toast::new(Toast::POWERSHELL_APP_ID).title(&title).text1(&body).show() {
                log::warn!("Notification impossible: {e:?}");
            }
        });
    }
    #[cfg(not(windows))]
    {
        let _ = (title, body);
    }
}

static RESTORE_REQUESTED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

/// System-tray icon. Clicking it reports a "restore" request to the app.
pub struct Tray {
    #[cfg(windows)]
    _icon: Option<tray_icon::TrayIcon>,
}

impl Tray {
    /// Creates the tray icon; `ctx` is woken when the icon is clicked.
    pub fn new(ctx: &eframe::egui::Context) -> Self {
        #[cfg(windows)]
        {
            // With a handler installed the receiver channel stays empty, so the
            // handler records the click itself and wakes the UI.
            let ctx = ctx.clone();
            tray_icon::TrayIconEvent::set_event_handler(Some(move |ev: tray_icon::TrayIconEvent| {
                if matches!(
                    ev,
                    tray_icon::TrayIconEvent::Click { button: tray_icon::MouseButton::Left, .. }
                        | tray_icon::TrayIconEvent::DoubleClick { .. }
                ) {
                    RESTORE_REQUESTED.store(true, std::sync::atomic::Ordering::Relaxed);
                    ctx.request_repaint();
                }
            }));
            let icon = image::load_from_memory(crate::assets::ICON_PNG).ok().and_then(|i| {
                let i = i.resize(32, 32, image::imageops::FilterType::Lanczos3).to_rgba8();
                tray_icon::Icon::from_rgba(i.to_vec(), i.width(), i.height()).ok()
            });
            let mut b = tray_icon::TrayIconBuilder::new().with_tooltip("Sylvania Launcher");
            if let Some(icon) = icon {
                b = b.with_icon(icon);
            }
            Tray { _icon: b.build().map_err(|e| log::warn!("Icône de zone de notification: {e}")).ok() }
        }
        #[cfg(not(windows))]
        {
            let _ = ctx;
            Tray {}
        }
    }

    /// True if the tray icon was (double-)clicked since the last call.
    pub fn take_restore_request(&self) -> bool {
        RESTORE_REQUESTED.swap(false, std::sync::atomic::Ordering::Relaxed)
    }
}
