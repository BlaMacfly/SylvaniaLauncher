//! Settings window (port of the Qt `SettingsDialog`): client folder, cache,
//! AddOns folder, English pack download, appearance, sound and the HD patch
//! manager (master ON/OFF + per-feature toggles that rename files, never
//! delete them).
//!
//! Like the Qt dialog, nothing is written until OK (except the live
//! background preview, reverted on Cancel / close).

mod enus;
mod features;

use crate::app::{AppEvent, Shared};
use crate::constants::{self, SPACE_MD, SPACE_SM, SPACE_XS};
use crate::theme::{self, ThemeTokens, hex};
use crate::ui::widgets::{self, Level};
use crate::util::msg;
use eframe::egui::{
    self, Align, Color32, CursorIcon, Layout, Response, RichText, Stroke, Ui, vec2,
};
use enus::{EnUsTask, Outcome};
use features::FeatureDef;
use std::path::{Path, PathBuf};
use std::time::Duration;

const WINDOW_SIZE: [f32; 2] = [620.0, 770.0];
const WINDOW_MIN_SIZE: [f32; 2] = [520.0, 480.0];
const FOOTER_HEIGHT: f32 = 44.0;
const TOGGLE_WIDTH: f32 = 95.0;
const PATCH_LIST_MAX_HEIGHT: f32 = 300.0;

/// One row of the HD patch list.
struct PatchRow {
    def: &'static FeatureDef,
    present: bool,
    on: bool,
}

/// HD patch section, built only when a client folder is configured.
struct PatchPanel {
    /// Client the states were read from (and are applied to).
    wow: PathBuf,
    rows: Vec<PatchRow>,
    master: bool,
}

impl PatchPanel {
    fn load(wow: PathBuf) -> Self {
        let rows: Vec<PatchRow> = features::FEATURES
            .iter()
            .map(|def| {
                let present = features::is_present(&wow, def);
                PatchRow {
                    def,
                    present,
                    on: present && features::is_enabled(&wow, def),
                }
            })
            .collect();
        let master = rows.iter().any(|r| r.on);
        PatchPanel { wow, rows, master }
    }

    fn set_master(&mut self, on: bool) {
        self.master = on;
        for row in self.rows.iter_mut().filter(|r| r.present) {
            row.on = features::master_target(row.def, on);
        }
    }

    fn toggle(&mut self, index: usize) {
        let row = &mut self.rows[index];
        row.on = !row.on;
        if let (true, Some(other)) = (row.on, row.def.exclusive_with)
            && let Some(o) = self.rows.iter_mut().find(|r| r.def.label == other)
        {
            o.on = false;
        }
    }

    /// Renames the files of every present feature. Returns the failures.
    fn apply(&self) -> Vec<String> {
        self.rows
            .iter()
            .filter(|r| r.present)
            .flat_map(|r| features::apply(&self.wow, r.def, r.on))
            .map(|(path, e)| {
                log::warn!("Patch HD: {}: {e}", path.display());
                let name = path
                    .file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_default();
                format!("{name} ({e})")
            })
            .collect()
    }
}

/// enUS pack download shown inside the window.
struct EnUsState {
    task: Option<EnUsTask>,
    /// Final line once the task is over.
    result: Option<(bool, String)>,
}

pub struct SettingsWindow {
    path: String,
    /// `<path>/Wow.exe` exists (gates cache / AddOns / enUS buttons).
    client_found: bool,
    sound: bool,
    background: String,
    random_bg: bool,
    patches: Option<PatchPanel>,
    enus: EnUsState,
    open: bool,
}

impl SettingsWindow {
    pub fn new(shared: &Shared) -> Self {
        let path = shared.config.wow_path();
        let background = shared.config.background();
        let background = if constants::BACKGROUNDS.contains(&background.as_str()) {
            background
        } else {
            constants::DEFAULT_BACKGROUND.to_string()
        };
        let patches = (!path.is_empty()).then(|| PatchPanel::load(PathBuf::from(&path)));
        SettingsWindow {
            client_found: client_found(&path),
            path,
            sound: shared.sounds.is_enabled(),
            background,
            random_bg: shared.config.random_background(),
            patches,
            enus: EnUsState {
                task: None,
                result: None,
            },
            open: true,
        }
    }

    /// Renders the window (via `ui::child_window`). Returns false once closed.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> bool {
        self.poll_enus(shared);
        if self.enus.task.is_some() {
            ctx.request_repaint_after(Duration::from_millis(100));
        }
        let title = shared.t("Réglages");
        let alive = super::child_window(
            ctx,
            crate::app::window_id(crate::app::WindowKind::Settings),
            &title,
            WINDOW_SIZE,
            WINDOW_MIN_SIZE,
            |ui| self.ui(ui, shared),
        );
        if !alive && self.open {
            // Title-bar ✕ = Cancel: revert the live background preview.
            shared.emit(AppEvent::BackgroundChanged(shared.config.background()));
            self.open = false;
        }
        self.open
    }

    fn ui(&mut self, ui: &mut Ui, shared: &mut Shared) {
        let height = (ui.available_height() - FOOTER_HEIGHT - SPACE_SM).max(120.0);
        egui::ScrollArea::vertical()
            .id_salt("settings-content")
            .max_height(height)
            .auto_shrink([false, false])
            .show(ui, |ui| {
                ui.spacing_mut().item_spacing.y = 10.0;
                self.install_section(ui, shared);
                self.addons_section(ui, shared);
                self.appearance_section(ui, shared);
                self.audio_section(ui, shared);
                if self.patches.is_some() {
                    self.patch_section(ui, shared);
                }
            });
        ui.add_space(SPACE_SM);
        ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
            if widgets::dialog_button(ui, &shared.t("Annuler")).clicked() {
                self.on_cancel(shared);
            }
            if widgets::level_button(
                ui,
                &shared.t("OK"),
                Level::Secondary,
                &ThemeTokens::default(),
                true,
                100.0,
            )
            .clicked()
            {
                self.on_ok(shared);
            }
        });
    }

    // --- Sections ---------------------------------------------------------------

    fn install_section(&mut self, ui: &mut Ui, shared: &mut Shared) {
        centered_title(ui, &shared.t("Installation de WoW"));
        ui.horizontal(|ui| {
            ui.add_sized(
                [130.0, 20.0],
                egui::Label::new(
                    RichText::new(shared.t("Dossier d'installation:")).color(Color32::WHITE),
                ),
            );
            let browse = shared.t("Parcourir");
            // Same metrics as widgets::level_button (Secondary = bold 12 px).
            let font = egui::FontId::new(12.0, egui::FontFamily::Name(theme::BOLD.into()));
            let browse_width = ui
                .painter()
                .layout_no_wrap(browse.clone(), font, Color32::WHITE)
                .size()
                .x
                + 2.0 * SPACE_MD
                + 4.0;
            let mut text: &str = &self.path;
            ui.add(
                egui::TextEdit::singleline(&mut text)
                    .text_color(theme::GOLD)
                    .desired_width((ui.available_width() - browse_width - SPACE_SM).max(80.0)),
            );
            if widgets::dialog_button(ui, &browse).clicked() {
                self.on_browse(shared);
            }
        });
        let width = ui.available_width();
        let clear = shared.t("Vider le cache");
        if widgets::level_button(
            ui,
            &clear,
            Level::Secondary,
            &ThemeTokens::default(),
            self.client_found,
            width,
        )
        .clicked()
        {
            self.on_clear_cache(shared);
        }
    }

    fn addons_section(&mut self, ui: &mut Ui, shared: &mut Shared) {
        centered_title(ui, &shared.t("AddOns"));
        let width = ui.available_width();
        let green = ThemeTokens {
            secondary: [hex("#4a6a4a"), hex("#3a5a3a"), hex("#2a4a2a")],
            secondary_border: hex("#5a7a5a"),
            secondary_text: Color32::WHITE,
            ..ThemeTokens::default()
        };
        let open = shared.t("Ouvrir le dossier AddOns");
        if widgets::level_button(
            ui,
            &open,
            Level::Secondary,
            &green,
            self.client_found,
            width,
        )
        .clicked()
        {
            self.on_open_addons(shared);
        }

        let blue = ThemeTokens {
            secondary: [hex("#2a3a4a"), hex("#1e2a3a"), hex("#151a2a")],
            secondary_border: hex("#3a4a5a"),
            secondary_text: hex("#7ec8e3"),
            ..ThemeTokens::default()
        };
        let enabled = self.client_found && self.enus.task.is_none();
        let label = shared.t("Télécharger le Pack Anglais (enUS)");
        let mut resp = widgets::level_button(ui, &label, Level::Secondary, &blue, enabled, width);
        if !self.client_found {
            resp = resp
                .on_hover_text(shared.t("Veuillez d'abord configurer le dossier WoW (Wow.exe)"));
        }
        if resp.clicked() {
            self.on_download_enus(shared);
        }
        self.enus_progress(ui, shared);
    }

    fn appearance_section(&mut self, ui: &mut Ui, shared: &mut Shared) {
        centered_title(ui, &shared.t("Apparence"));
        ui.horizontal(|ui| {
            ui.label(RichText::new(shared.t("Arrière-plan:")).color(Color32::WHITE));
            let before = self.background.clone();
            ui.add_enabled_ui(!self.random_bg, |ui| {
                egui::ComboBox::from_id_salt("settings-background")
                    .selected_text(self.background.clone())
                    .width(150.0)
                    .show_ui(ui, |ui| {
                        for bg in constants::BACKGROUNDS {
                            ui.selectable_value(&mut self.background, bg.to_string(), bg);
                        }
                    });
            });
            if self.background != before {
                shared.emit(AppEvent::BackgroundChanged(self.background.clone()));
            }

            let text = if self.random_bg { shared.t("🎲 Aléatoire : ON") } else { shared.t("🎲 Aléatoire : OFF") };
            let (fill, border, color) = if self.random_bg {
                (hex("#3a6b2f"), hex("#5a8c4f"), Color32::WHITE)
            } else {
                (hex("#42321d"), hex("#7a6a4d"), theme::GOLD)
            };
            let tip = shared.t(
                "ON : change l'arrière-plan à chaque ouverture du launcher.\nOFF : garde toujours le même arrière-plan.",
            );
            if toggle_like_button(ui, theme::bold(text, 11.0).color(color), fill, border, vec2(0.0, 26.0))
                .on_hover_text(tip)
                .clicked()
            {
                shared.sounds.play("button");
                self.random_bg = !self.random_bg;
            }
        });
    }

    fn audio_section(&mut self, ui: &mut Ui, shared: &mut Shared) {
        centered_title(ui, &shared.t("Audio"));
        let label = RichText::new(shared.t("Activer les sons du launcher")).color(Color32::WHITE);
        if ui.checkbox(&mut self.sound, label).clicked() {
            shared.sounds.play("button");
        }
    }

    fn patch_section(&mut self, ui: &mut Ui, shared: &mut Shared) {
        let Some(panel) = self.patches.as_mut() else {
            return;
        };
        ui.add_space(SPACE_XS);
        centered_title(ui, &shared.t("Gestion des Patchs HD"));

        // Master ON/OFF
        let (text, fill, border, color) = if panel.master {
            (
                shared.t("Patch HD : ACTIVÉ"),
                hex("#3a6a2f"),
                hex("#5a8c4f"),
                Color32::WHITE,
            )
        } else {
            (
                shared.t("Patch HD : DÉSACTIVÉ"),
                hex("#5a1a1a"),
                hex("#8a3a3a"),
                hex("#e0c0c0"),
            )
        };
        let tip = shared.t(
            "ACTIVÉ : active tous les patchs présents.\nDÉSACTIVÉ : éteint tous les patchs (client d'origine).",
        );
        let width = ui.available_width();
        if toggle_like_button(
            ui,
            theme::bold(text, 13.0).color(color),
            fill,
            border,
            vec2(width, 40.0),
        )
        .on_hover_text(tip)
        .clicked()
        {
            shared.sounds.play("button");
            panel.set_master(!panel.master);
        }

        ui.add(
            egui::Label::new(
                RichText::new(shared.t(
                    "Videz le cache après tout changement de patch (certains modifient des tables .dbc).",
                ))
                .size(10.0)
                .color(hex("#c08a4a")),
            )
            .wrap(),
        );

        // Per-feature toggles
        let danger_tip = shared.t(
            "Patch « Dangereux » : il remplace une table .dbc (risque de détection « unlike check » ou de crash selon le serveur).",
        );
        let absent_tip = shared.t("Ce patch n'est pas présent dans votre installation.");
        let (on_text, off_text, absent_text) =
            (shared.t("ACTIVÉ"), shared.t("ÉTEINT"), shared.t("Absent"));
        let mut clicked = None;
        egui::Frame::new()
            .stroke(Stroke::new(1.0, hex("#5a4a2d")))
            .corner_radius(5)
            .inner_margin(SPACE_SM)
            .show(ui, |ui| {
                egui::ScrollArea::vertical()
                    .id_salt("settings-hd-features")
                    .max_height(PATCH_LIST_MAX_HEIGHT)
                    .auto_shrink([false, true])
                    .show(ui, |ui| {
                        for (i, row) in panel.rows.iter().enumerate() {
                            ui.horizontal(|ui| {
                                let label =
                                    theme::bold(shared.t(row.def.label), 11.0).color(theme::GOLD);
                                let mut resp = ui.label(label);
                                if row.def.dangerous {
                                    resp |= ui
                                        .label(RichText::new("⚠").size(11.0).color(hex("#e0a040")));
                                    resp.on_hover_text(danger_tip.as_str());
                                }
                                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                                    ui.add_space(SPACE_XS);
                                    if !row.present {
                                        ui.add_enabled(false, feature_toggle(&absent_text, false))
                                            .on_disabled_hover_text(absent_tip.as_str());
                                    } else {
                                        let text = if row.on { &on_text } else { &off_text };
                                        if ui
                                            .add(feature_toggle(text, row.on))
                                            .on_hover_cursor(CursorIcon::PointingHand)
                                            .clicked()
                                        {
                                            clicked = Some(i);
                                        }
                                    }
                                });
                            });
                        }
                    });
            });
        if let Some(i) = clicked {
            shared.sounds.play("button");
            panel.toggle(i);
        }
    }

    fn enus_progress(&mut self, ui: &mut Ui, shared: &mut Shared) {
        if self.enus.task.is_none() && self.enus.result.is_none() {
            return;
        }
        let mut cancel = false;
        egui::Frame::new()
            .fill(hex("#151a2a"))
            .stroke(Stroke::new(1.0, hex("#3a4a5a")))
            .corner_radius(5)
            .inner_margin(SPACE_SM)
            .show(ui, |ui| {
                ui.set_width(ui.available_width());
                ui.label(
                    theme::bold(shared.t("Téléchargement du Pack Anglais"), 12.0)
                        .color(hex("#7ec8e3")),
                );
                if let Some(task) = &self.enus.task {
                    let p = task.progress();
                    let bar = match p.fraction {
                        Some(f) => egui::ProgressBar::new(f).show_percentage(),
                        None => egui::ProgressBar::new(0.0).animate(true),
                    };
                    ui.add(bar.fill(hex("#2e6a9a")));
                    ui.label(RichText::new(&p.status).color(Color32::WHITE));
                    if !p.details.is_empty() {
                        ui.label(
                            RichText::new(&p.details)
                                .size(11.0)
                                .color(theme::TEXT_MUTED),
                        );
                    }
                    if p.cancellable && widgets::dialog_button(ui, &shared.t("Annuler")).clicked() {
                        cancel = true;
                    }
                } else if let Some((ok, text)) = &self.enus.result {
                    let color = if *ok { hex("#7ccf6a") } else { hex("#e07070") };
                    ui.add(egui::Label::new(RichText::new(text).color(color)).wrap());
                }
            });
        if cancel {
            shared.sounds.play("button");
            if let Some(task) = &self.enus.task {
                task.cancel();
            }
        }
    }

    // --- Actions ----------------------------------------------------------------

    fn on_browse(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        let start = if self.path.is_empty() {
            dirs::home_dir().unwrap_or_default()
        } else {
            PathBuf::from(&self.path)
        };
        let Some(dir) = rfd::FileDialog::new()
            .set_title(shared.t("Sélectionner le dossier World of Warcraft"))
            .set_directory(start)
            .pick_folder()
        else {
            return;
        };
        let dir = dir.to_string_lossy().into_owned();
        if !client_found(&dir)
            && !msg::confirm(
                &shared.t("Confirmation"),
                &shared.t(
                    "Wow.exe n'a pas été trouvé dans ce dossier.\n\nVoulez-vous utiliser cet emplacement (par exemple pour une nouvelle installation) ?",
                ),
            )
        {
            return;
        }
        self.client_found = client_found(&dir);
        self.path = dir;
    }

    fn on_clear_cache(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        if self.path.is_empty() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Aucun dossier WoW configuré."),
            );
            return;
        }
        let cache = Path::new(&self.path).join("Cache");
        // symlink_metadata: never follow a link out of the client folder.
        if !std::fs::symlink_metadata(&cache).is_ok_and(|m| m.is_dir()) {
            msg::info(&shared.t("Info"), &shared.t("Aucun cache à vider."));
            return;
        }
        if !msg::confirm(
            &shared.t("Confirmer"),
            &shared.t(
                "Êtes-vous sûr de vouloir vider le cache WoW?\nCela supprimera le dossier Cache.",
            ),
        ) {
            return;
        }
        match std::fs::remove_dir_all(&cache) {
            Ok(()) => msg::info(&shared.t("Succès"), &shared.t("Cache vidé avec succès!")),
            Err(e) => {
                log::warn!("Vidage du cache impossible: {e}");
                msg::warning(
                    &shared.t("Erreur"),
                    &shared.t("Impossible de vider le cache."),
                );
            }
        }
    }

    fn on_open_addons(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        if self.path.is_empty() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Aucun dossier WoW configuré."),
            );
            return;
        }
        let addons = Path::new(&self.path).join("Interface").join("AddOns");
        if let Err(e) = std::fs::create_dir_all(&addons) {
            log::warn!("Création du dossier AddOns impossible: {e}");
        }
        open_in_file_manager(&addons);
    }

    fn on_download_enus(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        if self.path.is_empty() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.t("Aucun dossier WoW configuré."),
            );
            return;
        }
        let data = Path::new(&self.path).join("Data");
        self.enus = EnUsState {
            task: Some(EnUsTask::start(data, shared.i18n.clone())),
            result: None,
        };
    }

    fn poll_enus(&mut self, shared: &mut Shared) {
        let Some(task) = self.enus.task.as_mut() else {
            return;
        };
        let Some(outcome) = task.poll() else { return };
        self.enus.task = None;
        self.enus.result = Some(match outcome {
            Outcome::Installed => {
                let text = shared.t("Pack Anglais installé avec succès !");
                msg::info(&shared.t("Succès"), &text);
                (true, text)
            }
            Outcome::Cancelled => (false, shared.t("Téléchargement annulé")),
            Outcome::Failed(e) => {
                msg::error(&shared.t("Erreur de téléchargement"), &e);
                (false, e)
            }
        });
    }

    fn on_ok(&mut self, shared: &mut Shared) {
        // Apply (including the new sound state) FIRST, then play the
        // confirmation click, so re-enabling sound is audibly confirmed.
        shared.config.set_wow_path(&self.path);
        shared.sounds.set_enabled(self.sound);
        shared.config.set_sound_enabled(self.sound);
        shared.config.set_background(&self.background);
        shared.config.set_random_background(self.random_bg);
        let failures = self
            .patches
            .as_ref()
            .map(PatchPanel::apply)
            .unwrap_or_default();
        shared.config.save();
        shared.sounds.play("button");
        if !failures.is_empty() {
            msg::warning(
                &shared.t("Erreur"),
                &shared.tf(
                    "Certains patchs n'ont pas pu être modifiés (fermez le jeu puis réessayez) :\n%1",
                    &[&failures.join("\n")],
                ),
            );
        }
        shared.emit(AppEvent::SettingsChanged);
        self.open = false;
    }

    fn on_cancel(&mut self, shared: &mut Shared) {
        shared.sounds.play("button");
        // The background is previewed live: restore the saved one.
        shared.emit(AppEvent::BackgroundChanged(shared.config.background()));
        self.open = false;
    }
}

fn client_found(path: &str) -> bool {
    !path.is_empty() && Path::new(path).join(constants::CLIENT_EXE_NAME).is_file()
}

fn centered_title(ui: &mut Ui, text: &str) {
    ui.vertical_centered(|ui| widgets::section_title(ui, text));
}

/// Flat coloured button used for the ON/OFF toggles.
fn toggle_like_button(
    ui: &mut Ui,
    text: RichText,
    fill: Color32,
    border: Color32,
    min_size: egui::Vec2,
) -> Response {
    ui.add(
        egui::Button::new(text)
            .fill(fill)
            .stroke(Stroke::new(1.5, border))
            .corner_radius(5)
            .min_size(min_size),
    )
    .on_hover_cursor(CursorIcon::PointingHand)
}

/// Per-feature ACTIVÉ / ÉTEINT button (fixed width, like Qt).
fn feature_toggle(text: &str, on: bool) -> egui::Button<'static> {
    let (fill, border, color) = if on {
        (hex("#3a6a2f"), hex("#5a8c4f"), Color32::WHITE)
    } else {
        (hex("#4a1f1f"), hex("#7a3a3a"), hex("#ddbbbb"))
    };
    egui::Button::new(theme::bold(text, 11.0).color(color))
        .fill(fill)
        .stroke(Stroke::new(1.0, border))
        .corner_radius(5)
        .min_size(vec2(TOGGLE_WIDTH, 24.0))
}

fn open_in_file_manager(path: &Path) {
    #[cfg(windows)]
    let result = {
        // Explorer wants backslashes; config paths may hold forward slashes.
        let native = path.to_string_lossy().replace('/', "\\");
        std::process::Command::new("explorer").arg(native).spawn()
    };
    #[cfg(not(windows))]
    let result = std::process::Command::new("xdg-open").arg(path).spawn();
    if let Err(e) = result {
        log::warn!("Ouverture de {} impossible: {e}", path.display());
    }
}
