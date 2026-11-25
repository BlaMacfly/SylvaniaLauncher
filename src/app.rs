//! Application root: shared state, cross-window events and the eframe App
//! that owns the main window, the secondary windows, reminders and the tray.

use crate::config::Config;
use crate::core::notes::NotesStore;
use crate::core::reminders::ReminderService;
use crate::i18n::I18n;
use crate::notify::Tray;
use crate::sound::Sounds;
use crate::ui;
use eframe::egui;

/// Something one window tells the rest of the app. Pushed into
/// [`Shared::events`] during a frame, dispatched by [`App`] after it.
#[derive(Debug, Clone, PartialEq)]
pub enum AppEvent {
    /// Background picked in Settings: apply it live on the main window.
    BackgroundChanged(String),
    /// Settings saved (path, sound, …): refresh client detection.
    SettingsChanged,
    /// Server list or active server changed.
    RealmlistChanged,
    /// UI language switched (FR ⇄ EN).
    LanguageChanged,
    /// Notes changed outside the notes window (e.g. a reminder fired).
    NotesChanged,
    /// Open (or focus) a secondary window.
    Open(WindowKind),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WindowKind {
    Settings,
    Notes,
    Servers,
    Addons,
}

/// State shared by every window.
pub struct Shared {
    pub config: Config,
    pub i18n: I18n,
    pub sounds: Sounds,
    pub notes: NotesStore,
    /// Events raised this frame (see [`AppEvent`]).
    pub events: Vec<AppEvent>,
}

impl Shared {
    /// Translate a French source string.
    pub fn t(&self, fr: &str) -> String {
        self.i18n.t(fr)
    }
    /// Translate + substitute `%1`, `%2`, …
    pub fn tf(&self, fr: &str, args: &[&dyn std::fmt::Display]) -> String {
        self.i18n.tf(fr, args)
    }
    pub fn emit(&mut self, ev: AppEvent) {
        self.events.push(ev);
    }
}

pub struct App {
    shared: Shared,
    main: ui::main_window::MainWindow,
    settings: Option<ui::settings::SettingsWindow>,
    notes: Option<ui::notes::NotesWindow>,
    servers: Option<ui::realmlist::RealmlistWindow>,
    addons: Option<ui::addons::AddonsWindow>,
    reminders: ReminderService,
    tray: Tray,
}

impl App {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let ctx = &cc.egui_ctx;
        crate::theme::install_fonts(ctx);
        crate::theme::apply_global_style(ctx);
        egui_extras::install_image_loaders(ctx);

        let config = Config::load();
        let i18n = I18n::new(&config.language());
        let sounds = Sounds::new(config.sound_enabled());
        let mut shared = Shared { config, i18n, sounds, notes: NotesStore::load(), events: Vec::new() };
        let main = ui::main_window::MainWindow::new(ctx, &mut shared);
        let tray = Tray::new(ctx);
        App {
            shared,
            main,
            settings: None,
            notes: None,
            servers: None,
            addons: None,
            reminders: ReminderService::new(),
            tray,
        }
    }

    fn open(&mut self, ctx: &egui::Context, kind: WindowKind) {
        let s = &self.shared;
        match kind {
            WindowKind::Settings if self.settings.is_none() => {
                self.settings = Some(ui::settings::SettingsWindow::new(s))
            }
            WindowKind::Notes if self.notes.is_none() => self.notes = Some(ui::notes::NotesWindow::new(s)),
            WindowKind::Servers if self.servers.is_none() => {
                self.servers = Some(ui::realmlist::RealmlistWindow::new(s))
            }
            WindowKind::Addons if self.addons.is_none() => self.addons = Some(ui::addons::AddonsWindow::new(s)),
            // Already open: bring it to the front.
            _ => ui::focus_child_window(ctx, window_id(kind)),
        }
    }

    fn dispatch_events(&mut self, ctx: &egui::Context) {
        // Events may raise further events; bound the loop defensively.
        for _ in 0..8 {
            let events = std::mem::take(&mut self.shared.events);
            if events.is_empty() {
                break;
            }
            for ev in events {
                if let AppEvent::Open(kind) = ev {
                    self.open(ctx, kind);
                    continue;
                }
                self.main.on_event(&ev, ctx, &mut self.shared);
                if let Some(w) = self.notes.as_mut() {
                    w.on_event(&ev, &mut self.shared);
                }
            }
        }
    }
}

/// Stable viewport id of each secondary window (see ui::child_window).
pub fn window_id(kind: WindowKind) -> &'static str {
    match kind {
        WindowKind::Settings => "settings",
        WindowKind::Notes => "notes",
        WindowKind::Servers => "servers",
        WindowKind::Addons => "addons",
    }
}

impl eframe::App for App {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let ctx = ui.ctx().clone();

        if self.tray.take_restore_request() {
            ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(false));
            ctx.send_viewport_cmd(egui::ViewportCommand::Focus);
        }

        self.reminders.tick(&mut self.shared);
        self.main.show(ui, &mut self.shared);

        let shared = &mut self.shared;
        if let Some(w) = self.settings.as_mut() {
            if !w.show(&ctx, shared) {
                self.settings = None;
            }
        }
        if let Some(w) = self.notes.as_mut() {
            if !w.show(&ctx, shared) {
                self.notes = None;
            }
        }
        if let Some(w) = self.servers.as_mut() {
            if !w.show(&ctx, shared) {
                self.servers = None;
            }
        }
        if let Some(w) = self.addons.as_mut() {
            if !w.show(&ctx, shared) {
                self.addons = None;
            }
        }

        self.dispatch_events(&ctx);

        // Keep timers (play-time poll, reminders) ticking while idle.
        ctx.request_repaint_after(std::time::Duration::from_secs(1));
    }

    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        self.main.on_exit(&mut self.shared);
        self.shared.config.save();
    }
}
