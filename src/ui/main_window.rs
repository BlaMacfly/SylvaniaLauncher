//! Main window — port of the Qt MainWindow (+ WowButton states).
//!
//! Layout: header (logo + language toggle), server and stats panels, the one
//! primary 3-state button (Installer / Téléchargement / Jouer) with the shared
//! progress row, a status line, the secondary navigation bar and the footer.
//! Contract with `app.rs`: `new`, `show`, `on_event`, `on_exit`.

use crate::app::{AppEvent, Shared, WindowKind};
use crate::assets;
use crate::config::Config;
use crate::constants::*;
use crate::core::download::Outcome;
use crate::core::hd_patch::{HdEvent, HdPatchTask};
use crate::paths;
use crate::stats::Stats;
use crate::theme::{self, Theme};
use crate::ui::download::{BarStyle, DownloadDialog, progress_bar};
use crate::ui::widgets::{self, Level};
use crate::util::msg;
use eframe::egui::{self, Align, Color32, CornerRadius, Layout, RichText, Stroke, Ui, Vec2};
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, Receiver, TryRecvError};
use std::time::{Duration, Instant};

/// Qt WowButton::State.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum PrimaryState {
    /// Client exe not found → "INSTALLER".
    Install,
    /// Download / extraction in progress → progress label, disabled.
    Downloading,
    /// Client detected → "JOUER".
    Play,
}

/// Status line content, translated at render time.
#[derive(Clone, Debug)]
enum Status {
    Loading,
    NotDetected,
    Ready(String),
    Installing,
    Launched,
    Running,
    SessionEnded(i64),
}

/// Shared progress row (HD patch).
struct ProgressRow {
    label: String,
    /// None = indeterminate.
    percent: Option<u8>,
}

#[derive(Clone, Copy)]
enum Click {
    Primary,
    Lang,
    Open(WindowKind),
    Hd,
    Quit,
}

pub struct MainWindow {
    theme: Theme,
    background: Option<egui::TextureHandle>,
    /// Background being decoded off the UI thread (name, result).
    pending_bg: Option<(String, Receiver<Option<egui::ColorImage>>)>,
    logo: Option<egui::TextureHandle>,

    stats: Stats,
    primary: PrimaryState,
    status: Status,
    /// Active server (name, host).
    server: Option<(String, String)>,
    progress_row: Option<ProgressRow>,

    download: Option<DownloadDialog>,
    hd: Option<HdPatchTask>,

    // Play-time tracking.
    watch: ProcessWatch,
    wow_running: bool,
    session_start: Option<Instant>,
    /// Set when the game is launched from here: keeps the hidden (minimized)
    /// window ticking until the process shows up.
    launched_at: Option<Instant>,

    /// Height of the bottom block (action zone → footer) measured last frame.
    bottom_h: f32,
}

impl MainWindow {
    pub fn new(ctx: &egui::Context, shared: &mut Shared) -> Self {
        // Qt changeLanguage(initial): keep Config.wtf in sync with the language.
        sync_wtf_locale(&shared.config.wow_path(), &shared.config.language());

        // Background on launch: if the random toggle is ON, pick a fresh one
        // (different from last time); otherwise keep the saved background.
        if shared.config.random_background() {
            let bg = pick_random_background(&shared.config.background());
            shared.config.set_background(&bg);
        }
        let bg = shared.config.background();

        let mut w = MainWindow {
            theme: Theme::for_background(&bg),
            background: None,
            pending_bg: None,
            logo: load_logo(ctx),
            stats: Stats::load(),
            primary: PrimaryState::Install,
            status: Status::Loading,
            server: None,
            progress_row: None,
            download: None,
            hd: None,
            watch: ProcessWatch::new(),
            wow_running: false,
            session_start: None,
            launched_at: None,
            bottom_h: 220.0,
        };
        w.apply_theme(&bg);
        w.update_server_info(shared);
        w.refresh_primary_state(shared);
        w
    }

    /// Draws the whole main viewport (the root `ui` of the eframe App).
    pub fn show(&mut self, ui: &mut Ui, shared: &mut Shared) {
        let ctx = ui.ctx().clone();
        self.poll_background(&ctx);
        self.poll_hd(shared);
        self.poll_process(&ctx);

        self.paint_background(ui);
        let mut click = None;
        egui::Frame::new()
            .inner_margin(egui::Margin::symmetric(SPACE_LG as i8, SPACE_MD as i8))
            .show(ui, |ui| {
                ui.set_min_size(ui.available_size());
                click = self.layout(ui, &ctx, shared);
            });
        if let Some(c) = click {
            self.on_click(c, &ctx, shared);
        }

        // Download dialog (secondary OS window) + its outcome.
        if let Some(d) = self.download.as_mut()
            && let Some(outcome) = d.show(&ctx, shared)
        {
            let destination = d.destination().to_path_buf();
            self.download = None;
            self.on_download_complete(outcome, &destination, shared);
            self.refresh_primary_state(shared);
        }

        // The game covers / minimizes the launcher: keep the ui pass running
        // so the Wow.exe poll can see the game exit and restore the window.
        if self.wow_running
            || self
                .launched_at
                .is_some_and(|t| t.elapsed() < Duration::from_secs(120))
        {
            ctx.send_viewport_cmd(egui::ViewportCommand::RequestPaintWhileHidden);
        }
        if self.download.is_some()
            || self.hd.is_some()
            || self.pending_bg.is_some()
            || self.watch.pending()
        {
            ctx.request_repaint_after(Duration::from_millis(100));
        }
    }

    /// Reacts to events raised by other windows.
    pub fn on_event(&mut self, ev: &AppEvent, ctx: &egui::Context, shared: &mut Shared) {
        let _ = ctx;
        match ev {
            AppEvent::BackgroundChanged(name) => self.apply_theme(name),
            AppEvent::SettingsChanged | AppEvent::RealmlistChanged => {
                self.update_server_info(shared);
                self.refresh_primary_state(shared);
            }
            // Every label is translated at render time.
            AppEvent::LanguageChanged | AppEvent::NotesChanged | AppEvent::Open(_) => {}
        }
    }

    /// App is closing: persist stats / play time.
    pub fn on_exit(&mut self, shared: &mut Shared) {
        let _ = shared;
        if self.wow_running
            && let Some(start) = self.session_start.take()
        {
            self.stats.total_play_time += start.elapsed().as_secs() as i64;
        }
        self.stats.save();
    }

    // --- Theme / background ---------------------------------------------------

    /// Applies a background live: palette immediately, image decoded off the
    /// UI thread (the previous image stays until the new one is ready).
    fn apply_theme(&mut self, name: &str) {
        self.theme = Theme::for_background(name);
        let name = name.to_string();
        let (tx, rx) = mpsc::channel();
        let job = name.clone();
        let spawned = std::thread::Builder::new()
            .name("background-decode".into())
            .spawn(move || {
                let bytes =
                    assets::background(&job).or_else(|| assets::background(DEFAULT_BACKGROUND));
                let _ = tx.send(bytes.and_then(assets::decode_image));
            });
        if spawned.is_ok() {
            self.pending_bg = Some((name, rx));
        }
    }

    fn poll_background(&mut self, ctx: &egui::Context) {
        let Some((name, rx)) = &self.pending_bg else {
            return;
        };
        match rx.try_recv() {
            Ok(img) => {
                if let Some(img) = img {
                    self.background = Some(ctx.load_texture(
                        format!("background-{name}"),
                        img,
                        egui::TextureOptions::LINEAR,
                    ));
                }
                self.pending_bg = None;
            }
            Err(TryRecvError::Empty) => {}
            Err(TryRecvError::Disconnected) => self.pending_bg = None,
        }
    }

    /// Keep-aspect-by-expanding, centred (Qt paintEvent).
    fn paint_background(&self, ui: &Ui) {
        let rect = ui.max_rect();
        let painter = ui.painter();
        painter.rect_filled(rect, 0.0, Color32::from_rgb(0x10, 0x10, 0x18));
        if let Some(tex) = &self.background {
            let size = tex.size_vec2();
            if size.x > 0.0 && size.y > 0.0 {
                let scale = (rect.width() / size.x).max(rect.height() / size.y);
                let img = egui::Rect::from_center_size(rect.center(), size * scale);
                let uv = egui::Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0));
                painter.image(tex.id(), img, uv, Color32::WHITE);
            }
        }
    }

    // --- Layout ---------------------------------------------------------------

    fn layout(&mut self, ui: &mut Ui, ctx: &egui::Context, shared: &Shared) -> Option<Click> {
        let mut click = None;
        ui.spacing_mut().item_spacing = Vec2::splat(SPACE_MD);

        // 1. Header: logo left, language switch right.
        self.header(ui, shared, &mut click);

        // 2. Info panels take whatever the bottom block leaves.
        let spacing = ui.spacing().item_spacing.y;
        let panels_h = (ui.available_height() - self.bottom_h - spacing).max(120.0);
        self.panels(ui, shared, panels_h);

        // 3-6. Action zone, status, nav bar, footer (height measured for next frame).
        let rect = ui
            .scope(|ui| self.bottom(ui, shared, &mut click))
            .response
            .rect;
        if (rect.height() - self.bottom_h).abs() > 0.5 {
            self.bottom_h = rect.height();
            ctx.request_repaint();
        }
        click
    }

    fn header(&self, ui: &mut Ui, shared: &Shared, click: &mut Option<Click>) {
        ui.horizontal(|ui| {
            match &self.logo {
                Some(logo) => {
                    let s = logo.size_vec2();
                    let w = if s.y > 0.0 {
                        s.x * LOGO_HEIGHT / s.y
                    } else {
                        LOGO_HEIGHT
                    };
                    ui.image((logo.id(), Vec2::new(w, LOGO_HEIGHT)));
                }
                None => {
                    ui.allocate_space(Vec2::new(LOGO_HEIGHT, LOGO_HEIGHT));
                }
            }
            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                let label = if shared.config.language() == "en" {
                    "Lang: EN"
                } else {
                    "Lang: FR"
                };
                let resp = widgets::level_button(
                    ui,
                    label,
                    Level::Tertiary,
                    &self.theme.tokens,
                    true,
                    0.0,
                )
                .on_hover_text(shared.t("Changer la langue (FR/EN)"));
                if resp.clicked() {
                    *click = Some(Click::Lang);
                }
            });
        });
    }

    fn panels(&self, ui: &mut Ui, shared: &Shared, h: f32) {
        let pal = self.theme.palette;
        let accent = self.theme.tokens.accent;
        ui.horizontal(|ui| {
            ui.spacing_mut().item_spacing.x = SPACE_LG;
            let w = ((ui.available_width() - SPACE_LG) / 2.0).max(100.0);
            let size = Vec2::new(w, h);

            // Server panel.
            panel(ui, &pal, size, |ui| {
                ui.vertical_centered(|ui| {
                    ui.label(theme::bold(shared.t("Serveur"), 18.0).color(accent));
                });
                if let Some((name, host)) = &self.server {
                    ui.label(
                        theme::bold(shared.tf("Serveur: %1", &[name]), 13.0)
                            .color(pal.text_primary),
                    );
                    ui.label(
                        RichText::new(shared.tf("Realmlist: %1", &[host]))
                            .size(12.0)
                            .color(pal.text_secondary),
                    );
                }
            });

            // Stats panel.
            panel(ui, &pal, size, |ui| {
                ui.vertical_centered(|ui| {
                    ui.label(theme::bold(shared.t("Statistiques de Jeu"), 18.0).color(accent));
                });
                let total = self.displayed_play_time();
                self.stat_label(
                    ui,
                    shared.tf(
                        "Temps de jeu: %1h %2min",
                        &[&(total / 3600), &((total % 3600) / 60)],
                    ),
                );
                self.stat_label(ui, shared.tf("Lancements: %1", &[&self.stats.launch_count]));
                let last = match self.stats.last_session {
                    Some(d) => {
                        shared.t("Dernière session: ")
                            + &d.format(&qt_to_chrono_format(&shared.t("dd/MM/yyyy hh:mm")))
                                .to_string()
                    }
                    None => shared.t("Dernière session: --"),
                };
                self.stat_label(ui, last);
            });
        });
    }

    /// Bordered stat line of the stats panel.
    fn stat_label(&self, ui: &mut Ui, text: String) {
        let pal = &self.theme.palette;
        let bg = if self.theme.background == "Azeroth" {
            Color32::from_rgba_unmultiplied(255, 255, 255, 100)
        } else {
            Color32::from_rgba_unmultiplied(0, 0, 0, 100)
        };
        egui::Frame::new()
            .fill(bg)
            .stroke(Stroke::new(2.0, pal.panel_border))
            .corner_radius(CornerRadius::same(8))
            .inner_margin(egui::Margin::same(SPACE_SM as i8))
            .show(ui, |ui| {
                ui.set_min_width(ui.available_width());
                ui.vertical_centered(|ui| {
                    ui.label(RichText::new(text).size(13.0).color(pal.text_primary));
                });
            });
    }

    fn bottom(&self, ui: &mut Ui, shared: &Shared, click: &mut Option<Click>) {
        let pal = self.theme.palette;
        let tokens = self.theme.tokens;

        // Action zone: the ONE primary button + shared progress row.
        ui.vertical_centered(|ui| {
            ui.spacing_mut().item_spacing.y = SPACE_SM;
            let enabled = self.primary != PrimaryState::Downloading && self.hd.is_none();
            let label = self.primary_label(shared);
            if widgets::level_button(
                ui,
                &label,
                Level::Primary,
                &tokens,
                enabled,
                MAIN_WINDOW_MIN_WIDTH / 3.0,
            )
            .clicked()
            {
                *click = Some(Click::Primary);
            }
            if let Some(row) = &self.progress_row {
                ui.label(
                    RichText::new(&row.label)
                        .size(11.0)
                        .color(pal.text_secondary),
                );
                let style = BarStyle {
                    height: 10.0,
                    bg: Color32::from_rgba_unmultiplied(0, 0, 0, 120),
                    border: pal.panel_border,
                    fill: tokens.accent,
                    radius: 5,
                    show_text: false,
                };
                progress_bar(ui, row.percent, &style);
            }
        });

        // Status line.
        ui.vertical_centered(|ui| {
            egui::Frame::new()
                .inner_margin(egui::Margin::same(SPACE_XS as i8))
                .show(ui, |ui| {
                    ui.label(
                        RichText::new(self.status_text(shared))
                            .size(12.0)
                            .color(pal.text_primary),
                    );
                });
        });

        // Secondary navigation bar.
        ui.horizontal(|ui| {
            ui.spacing_mut().item_spacing.x = SPACE_SM;
            let mut nav = |ui: &mut Ui, text: &str, enabled: bool, c: Click| {
                if widgets::level_button(
                    ui,
                    &shared.t(text),
                    Level::Secondary,
                    &tokens,
                    enabled,
                    0.0,
                )
                .clicked()
                {
                    *click = Some(c);
                }
            };
            nav(ui, "RÉGLAGES", true, Click::Open(WindowKind::Settings));
            nav(ui, "ADDONS", true, Click::Open(WindowKind::Addons));
            nav(ui, "NOTES", true, Click::Open(WindowKind::Notes));
            nav(ui, "SERVEURS", true, Click::Open(WindowKind::Servers));
            nav(
                ui,
                "PATCH HD",
                self.hd.is_none() && self.download.is_none(),
                Click::Hd,
            );
            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                nav(ui, "QUITTER", true, Click::Quit);
            });
        });

        // Footer.
        ui.vertical_centered(|ui| {
            ui.label(
                RichText::new(shared.tf(
                    "© 2026 Sylvania Launcher v%1 — %2",
                    &[&APP_VERSION, &CLIENT_DISPLAY_NAME],
                ))
                .size(11.0)
                .color(pal.text_secondary),
            );
        });
    }

    /// Qt WowButton::updateLabel.
    fn primary_label(&self, shared: &Shared) -> String {
        match self.primary {
            PrimaryState::Install => shared.t("INSTALLER"),
            PrimaryState::Downloading => match self
                .download
                .as_ref()
                .and_then(DownloadDialog::button_progress)
            {
                Some(p) => shared.tf("TÉLÉCHARGEMENT… %1%", &[&p]),
                None => shared.t("INSTALLATION…"),
            },
            PrimaryState::Play => shared.t("JOUER"),
        }
    }

    fn status_text(&self, shared: &Shared) -> String {
        match &self.status {
            Status::Loading => shared.t("Chargement..."),
            Status::NotDetected => shared.tf(
                "Client %1 non détecté — cliquez sur INSTALLER.",
                &[&CLIENT_DISPLAY_NAME],
            ),
            Status::Ready(server) => shared.tf(
                "%1 est installé. Prêt à jouer sur %2!",
                &[&CLIENT_DISPLAY_NAME, server],
            ),
            Status::Installing => {
                shared.tf("Installation de %1 en cours...", &[&CLIENT_DISPLAY_NAME])
            }
            Status::Launched => shared.t("World of Warcraft lancé! Bon jeu!"),
            Status::Running => shared.t("World of Warcraft en cours d'exécution..."),
            Status::SessionEnded(mins) => {
                shared.tf("Session terminée! Temps joué: %1 min", &[mins])
            }
        }
    }

    /// Total play time, live while the game runs.
    fn displayed_play_time(&self) -> i64 {
        let session = match (self.wow_running, self.session_start) {
            (true, Some(start)) => start.elapsed().as_secs() as i64,
            _ => 0,
        };
        self.stats.total_play_time + session
    }

    // --- Actions ----------------------------------------------------------------

    fn on_click(&mut self, click: Click, ctx: &egui::Context, shared: &mut Shared) {
        match click {
            Click::Primary => match self.primary {
                PrimaryState::Play => {
                    shared.sounds.play("play");
                    self.play_game(ctx, shared);
                }
                PrimaryState::Install => {
                    shared.sounds.play("button");
                    self.start_client_install(shared);
                }
                PrimaryState::Downloading => {} // the button is disabled in this state
            },
            Click::Lang => {
                shared.sounds.play("button");
                let new_lang = if shared.config.language() == "fr" {
                    "en"
                } else {
                    "fr"
                };
                change_language(new_lang, shared);
            }
            Click::Open(kind) => {
                shared.sounds.play("button");
                shared.emit(AppEvent::Open(kind));
            }
            Click::Hd => self.on_hd_clicked(shared),
            Click::Quit => {
                shared.sounds.play("button");
                ctx.send_viewport_cmd(egui::ViewportCommand::Close);
            }
        }
    }

    fn update_server_info(&mut self, shared: &Shared) {
        self.server = shared
            .config
            .active_realmlist_entry()
            .map(|e| (e.name.clone(), Config::host_from_address(&e.address)));
    }

    /// Qt refreshPrimaryState: 3-state button from on-disk client detection.
    fn refresh_primary_state(&mut self, shared: &Shared) {
        if self.download.is_some() {
            self.primary = PrimaryState::Downloading;
            return;
        }
        if client_exe_path(&shared.config).is_none() {
            self.primary = PrimaryState::Install;
            self.status = Status::NotDetected;
            return;
        }
        self.primary = PrimaryState::Play;
        let server = shared
            .config
            .active_realmlist_entry()
            .map(|e| e.name)
            .unwrap_or_else(|| DEFAULT_REALM_NAME.to_string());
        self.status = Status::Ready(server);
    }

    fn play_game(&mut self, ctx: &egui::Context, shared: &mut Shared) {
        let Some(exe) = client_exe_path(&shared.config) else {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t(
                    "Exécutable du jeu non trouvé!\nVeuillez vérifier le chemin d'installation.",
                ),
            );
            self.refresh_primary_state(shared);
            return;
        };

        // Update realmlist before launching.
        if let Some(e) = shared.config.active_realmlist_entry() {
            shared.config.update_realmlist(&e.address);
        }

        self.stats.launch_count += 1;
        self.stats.last_session = Some(chrono::Local::now().naive_local());
        self.stats.save();

        let work_dir = exe.parent().map(Path::to_path_buf).unwrap_or_default();
        if spawn_detached(&exe, &work_dir) {
            self.status = Status::Launched;
            self.launched_at = Some(Instant::now());
            // Get out of the way while the game runs; restored when it exits.
            ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(true));
        } else {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Impossible de lancer World of Warcraft."),
            );
        }
    }

    fn start_client_install(&mut self, shared: &mut Shared) {
        let title = shared.tf(
            "Choisir l'emplacement d'installation — %1",
            &[&CLIENT_DISPLAY_NAME],
        );
        let mut dialog = rfd::FileDialog::new().set_title(&title);
        if let Some(home) = dirs::home_dir() {
            dialog = dialog.set_directory(home);
        }
        let Some(dir) = dialog.pick_folder() else {
            return;
        };

        self.primary = PrimaryState::Downloading;
        self.status = Status::Installing;
        self.download = Some(DownloadDialog::start(dir, shared));
    }

    /// Qt onDownloadComplete.
    fn on_download_complete(&mut self, outcome: Outcome, destination: &Path, shared: &mut Shared) {
        let title = shared.t("Erreur de téléchargement");
        if outcome.ok {
            msg::info(&shared.t("Téléchargement terminé"), &outcome.message);
            let dir = outcome.client_dir.or_else(|| {
                crate::core::download::find_client_exe(destination)
                    .and_then(|e| e.parent().map(Path::to_path_buf))
            });
            if let Some(dir) = dir {
                shared.config.set_wow_path(&dir.to_string_lossy());
                shared.emit(AppEvent::SettingsChanged);
            }
        } else if outcome.critical {
            msg::error(&title, &outcome.message);
        } else {
            msg::warning(&title, &outcome.message);
        }
    }

    // --- HD patch ---------------------------------------------------------------

    fn on_hd_clicked(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        let title = shared.t("Patch HD");

        let wow = shared.config.wow_path();
        if wow.is_empty() || client_exe_path(&shared.config).is_none() {
            msg::warning(
                &title,
                &shared.t(
                    "Vous devez d'abord installer ou configurer l'emplacement du client World of Warcraft avant d'installer le Patch HD.",
                ),
            );
            return;
        }
        if self.hd.as_ref().is_some_and(HdPatchTask::is_running) {
            msg::info(
                &title,
                &shared.t("L'installation du Patch HD est déjà en cours."),
            );
            return;
        }
        if HdPatchTask::is_installed(Path::new(&wow))
            && !msg::confirm(
                &shared.t("Patch HD Détecté"),
                &shared.t(
                    "Le Patch HD semble déjà installé sur votre client.\n\nVoulez-vous tout de même le réinstaller ou forcer une mise à jour ?",
                ),
            )
        {
            return;
        }

        // Play / HD buttons are disabled while `self.hd` is Some.
        self.progress_row = Some(ProgressRow {
            label: shared.t("Préparation de l'installation..."),
            percent: Some(0),
        });
        self.hd = Some(HdPatchTask::start(PathBuf::from(&wow), shared.i18n.clone()));
    }

    fn poll_hd(&mut self, shared: &Shared) {
        let Some(task) = self.hd.as_mut() else { return };
        let mut finished = None;
        for ev in task.poll() {
            match ev {
                HdEvent::Progress { percent, status } => {
                    self.progress_row = Some(ProgressRow {
                        label: status,
                        percent,
                    });
                }
                HdEvent::Finished { ok, message } => finished = Some((ok, message)),
            }
        }
        if finished.is_none() && task.is_running() {
            return;
        }
        self.hd = None;
        self.progress_row = None;
        if let Some((ok, message)) = finished {
            let title = shared.t("Patch HD");
            if ok {
                msg::info(&title, &shared.t("Patch HD installé avec succès."));
            } else {
                msg::warning(
                    &title,
                    &(shared.t("Erreur lors de l'installation : ") + &message),
                );
            }
        }
        self.refresh_primary_state(shared);
    }

    // --- Game process tracking ------------------------------------------------

    fn poll_process(&mut self, ctx: &egui::Context) {
        if let Some(running) = self.watch.tick() {
            self.handle_wow_running_state(running, ctx);
        }
    }

    /// Qt handleWowRunningState.
    fn handle_wow_running_state(&mut self, running: bool, ctx: &egui::Context) {
        if running && !self.wow_running {
            // WoW just started.
            self.wow_running = true;
            self.launched_at = None;
            self.session_start = Some(Instant::now());
            self.status = Status::Running;
        } else if !running && self.wow_running {
            // WoW just stopped.
            self.wow_running = false;
            if let Some(start) = self.session_start.take() {
                let secs = start.elapsed().as_secs() as i64;
                self.stats.total_play_time += secs;
                self.stats.save();
                self.status = Status::SessionEnded(secs / 60);
            }
            // Bring the launcher back once the game has closed.
            ctx.send_viewport_cmd(egui::ViewportCommand::Visible(true));
            ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(false));
            ctx.send_viewport_cmd(egui::ViewportCommand::Focus);
        }
        // running && wow_running: the play-time label is computed live.
    }
}

/// Info panel of fixed outer `size` (rounded frame, palette colours).
fn panel(ui: &mut Ui, pal: &theme::Palette, size: Vec2, add: impl FnOnce(&mut Ui)) {
    ui.allocate_ui_with_layout(size, Layout::top_down(Align::Min), |ui| {
        widgets::panel_frame(pal).show(ui, |ui| {
            // Inner size = outer − inner margins (LG / MD) − 2 px stroke.
            let inner = size - Vec2::new(2.0 * SPACE_LG + 4.0, 2.0 * SPACE_MD + 4.0);
            let inner = inner.max(Vec2::ZERO);
            ui.set_min_size(inner);
            ui.set_max_width(inner.x);
            ui.spacing_mut().item_spacing.y = SPACE_SM;
            add(ui);
        });
    });
}

// --- Free helpers --------------------------------------------------------------

/// Full path of the client exe, None if not found (Qt clientExePath).
fn client_exe_path(config: &Config) -> Option<PathBuf> {
    let wow = config.wow_path();
    if wow.is_empty() {
        return None;
    }
    let path = Path::new(&wow).join(CLIENT_EXE_NAME);
    path.is_file().then_some(path)
}

/// Qt pickRandomBackground.
fn pick_random_background(exclude: &str) -> String {
    use rand::Rng;
    if BACKGROUNDS.len() == 1 {
        return BACKGROUNDS[0].to_string();
    }
    let mut rng = rand::rng();
    loop {
        let chosen = BACKGROUNDS[rng.random_range(0..BACKGROUNDS.len())];
        if chosen != exclude {
            return chosen.to_string();
        }
    }
}

/// Logo pre-scaled on the CPU (2× the header height for HiDPI screens).
fn load_logo(ctx: &egui::Context) -> Option<egui::TextureHandle> {
    let img = image::load_from_memory(assets::LOGO_PNG).ok()?;
    let target = (LOGO_HEIGHT * 2.0) as u32;
    let img = if img.height() > target {
        img.resize(u32::MAX, target, image::imageops::FilterType::Lanczos3)
    } else {
        img
    };
    let rgba = img.to_rgba8();
    let color = egui::ColorImage::from_rgba_unmultiplied(
        [rgba.width() as usize, rgba.height() as usize],
        rgba.as_raw(),
    );
    Some(ctx.load_texture("logo", color, egui::TextureOptions::LINEAR))
}

/// Qt changeLanguage: config + UI language + client locale.
fn change_language(lang: &str, shared: &mut Shared) {
    shared.config.set_language(lang);
    shared.i18n.set_lang(lang);
    sync_wtf_locale(&shared.config.wow_path(), lang);
    shared.emit(AppEvent::LanguageChanged);
}

/// Rewrites the locale of `<wow>/WTF/Config.wtf` (atomic), if the file exists.
fn sync_wtf_locale(wow_path: &str, lang: &str) {
    if wow_path.is_empty() {
        return;
    }
    let path = Path::new(wow_path).join("WTF").join("Config.wtf");
    let Ok(bytes) = std::fs::read(&path) else {
        return;
    };
    let content = String::from_utf8_lossy(&bytes).into_owned();
    if content.is_empty() {
        return;
    }
    let updated = rewrite_locale(&content, lang);
    if updated != content
        && let Err(e) = paths::write_atomic(&path, updated.as_bytes())
    {
        log::warn!("Impossible de mettre à jour la locale de Config.wtf: {e}");
    }
}

/// Locale switch on a Config.wtf body (same rules as the Qt launcher).
fn rewrite_locale(content: &str, lang: &str) -> String {
    if lang == "en" {
        if content.contains("SET locale") {
            content.replace("SET locale \"frFR\"", "SET locale \"enUS\"")
        } else {
            let eol = if content.contains("\r\n") {
                "\r\n"
            } else {
                "\n"
            };
            format!("{content}{eol}SET locale \"enUS\"{eol}")
        }
    } else {
        content.replace("SET locale \"enUS\"", "SET locale \"frFR\"")
    }
}

/// Converts the Qt date pattern used by the translations ("dd/MM/yyyy hh:mm")
/// into a chrono format string.
fn qt_to_chrono_format(qt: &str) -> String {
    qt.replace("yyyy", "%Y")
        .replace("dd", "%d")
        .replace("MM", "%m")
        .replace("hh", "%H")
        .replace("mm", "%M")
}

/// QProcess::startDetached equivalent: own console-less process, cwd = exe dir.
fn spawn_detached(exe: &Path, work_dir: &Path) -> bool {
    use std::process::{Command, Stdio};
    let mut cmd = Command::new(exe);
    cmd.stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null());
    if !work_dir.as_os_str().is_empty() {
        cmd.current_dir(work_dir);
    }
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const DETACHED_PROCESS: u32 = 0x0000_0008;
        const CREATE_NEW_PROCESS_GROUP: u32 = 0x0000_0200;
        cmd.creation_flags(DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP);
    }
    match cmd.spawn() {
        Ok(_child) => true, // dropping the handle does not kill the game
        Err(e) => {
            log::warn!("Lancement de {} impossible: {e}", exe.display());
            false
        }
    }
}

/// Periodic, non-blocking "is Wow.exe running?" check (tasklist on a worker
/// thread, one at a time, every PLAY_TIME_CHECK_MS).
struct ProcessWatch {
    rx: Option<Receiver<bool>>,
    last: Instant,
}

impl ProcessWatch {
    fn new() -> Self {
        ProcessWatch {
            rx: None,
            last: Instant::now(),
        }
    }

    fn pending(&self) -> bool {
        self.rx.is_some()
    }

    /// Returns a fresh result when a check just completed.
    fn tick(&mut self) -> Option<bool> {
        if let Some(rx) = &self.rx {
            let r = rx.try_recv();
            return match r {
                Ok(running) => {
                    self.rx = None;
                    Some(running)
                }
                Err(TryRecvError::Empty) => None,
                Err(TryRecvError::Disconnected) => {
                    self.rx = None;
                    None
                }
            };
        }
        if self.last.elapsed() >= Duration::from_millis(PLAY_TIME_CHECK_MS) {
            self.last = Instant::now();
            self.rx = spawn_wow_check();
        }
        None
    }
}

#[cfg(windows)]
fn spawn_wow_check() -> Option<Receiver<bool>> {
    let (tx, rx) = mpsc::channel();
    std::thread::Builder::new()
        .name("wow-check".into())
        .spawn(move || {
            use std::os::windows::process::CommandExt;
            const CREATE_NO_WINDOW: u32 = 0x0800_0000;
            let out = std::process::Command::new("tasklist")
                .args(["/NH", "/FI", &format!("IMAGENAME eq {CLIENT_EXE_NAME}")])
                .stdin(std::process::Stdio::null())
                .creation_flags(CREATE_NO_WINDOW)
                .output();
            // On error nothing is sent: the tick is simply skipped.
            if let Ok(out) = out {
                let text = String::from_utf8_lossy(&out.stdout).to_lowercase();
                let _ = tx.send(text.contains(&CLIENT_EXE_NAME.to_lowercase()));
            }
        })
        .ok()?;
    Some(rx)
}

#[cfg(not(windows))]
fn spawn_wow_check() -> Option<Receiver<bool>> {
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn locale_rewrite_rules() {
        let fr = "SET gxWindow \"1\"\r\nSET locale \"frFR\"\r\n";
        let en = rewrite_locale(fr, "en");
        assert_eq!(en, "SET gxWindow \"1\"\r\nSET locale \"enUS\"\r\n");
        assert_eq!(rewrite_locale(&en, "fr"), fr);
        assert_eq!(
            rewrite_locale("SET a \"1\"\n", "en"),
            "SET a \"1\"\n\nSET locale \"enUS\"\n"
        );
        assert_eq!(rewrite_locale("SET a \"1\"\n", "fr"), "SET a \"1\"\n");
    }

    #[test]
    fn qt_date_pattern_conversion() {
        assert_eq!(qt_to_chrono_format("dd/MM/yyyy hh:mm"), "%d/%m/%Y %H:%M");
        assert_eq!(qt_to_chrono_format("MM/dd/yyyy hh:mm"), "%m/%d/%Y %H:%M");
    }

    #[test]
    fn random_background_differs_from_previous() {
        for _ in 0..50 {
            let bg = pick_random_background("Azeroth");
            assert_ne!(bg, "Azeroth");
            assert!(BACKGROUNDS.contains(&bg.as_str()));
        }
    }
}
