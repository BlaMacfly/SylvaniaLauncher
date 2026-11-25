//! Server (realmlist) management window — port of the Qt RealmlistWindow and
//! its ServerEditDialog (shown here as a modal inside the same OS window).

use crate::app::{AppEvent, Shared};
use crate::config::RealmlistEntry;
use crate::constants::{BUTTON_HEIGHT_SECONDARY, SPACE_MD, SPACE_SM};
use crate::theme::{self, hex};
use crate::ui::{child_window, widgets};
use crate::util::msg;
use eframe::egui::{self, Align, CornerRadius, Layout, RichText, Stroke, Ui};

/// Must match `app::window_id(WindowKind::Servers)`.
const WINDOW_ID: &str = "servers";

/// Add / edit form (Qt ServerEditDialog).
struct EditState {
    /// None = new entry.
    index: Option<usize>,
    name: String,
    address: String,
}

pub struct RealmlistWindow {
    selected: Option<usize>,
    edit: Option<EditState>,
}

impl RealmlistWindow {
    pub fn new(shared: &Shared) -> Self {
        RealmlistWindow { selected: Some(shared.config.active_realmlist_index()), edit: None }
    }

    /// Qt loadRealmlistEntries: after any change the selection follows the
    /// active server.
    fn reload(&mut self, shared: &Shared) {
        self.selected = Some(shared.config.active_realmlist_index());
    }

    /// Renders the window (via `ui::child_window`). Returns false once closed.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> bool {
        let title = shared.t("🌐 Gestion des Serveurs - Sylvania Launcher");
        let mut close = false;
        let open = child_window(ctx, WINDOW_ID, &title, [600.0, 500.0], [500.0, 400.0], |ui| {
            close = self.contents(ui, shared);
        });
        open && !close
    }

    /// Returns true when the window should close (after Apply).
    fn contents(&mut self, ui: &mut Ui, shared: &mut Shared) -> bool {
        let entries = shared.config.realmlist_entries();
        let active = shared.config.active_realmlist_index();
        if self.selected.is_some_and(|s| s >= entries.len()) {
            self.selected = None;
        }

        ui.spacing_mut().item_spacing.y = 15.0;

        // Current server label.
        let current = match entries.get(active) {
            Some(e) => shared.tf("Serveur actuel: %1", &[&e.name]),
            None => shared.t("Aucun serveur configuré"),
        };
        ui.vertical_centered(|ui| {
            ui.label(RichText::new(current).size(14.0).color(theme::GOLD));
        });

        // Server list takes the space left above the button row.
        let list_h = (ui.available_height() - BUTTON_HEIGHT_SECONDARY - ui.spacing().item_spacing.y).max(60.0);
        egui::ScrollArea::vertical().max_height(list_h).auto_shrink([false, false]).show(ui, |ui| {
            ui.spacing_mut().item_spacing.y = 10.0;
            for (i, e) in entries.iter().enumerate() {
                self.server_row(ui, shared, e, i, i == active);
            }
        });

        let mut close = false;
        let mut action: Option<Action> = None;
        ui.horizontal(|ui| {
            if widgets::dialog_button(ui, &shared.t("➕ Ajouter")).clicked() {
                action = Some(Action::Add);
            }
            if widgets::dialog_button(ui, &shared.t("✏️ Modifier")).clicked() {
                action = Some(Action::Edit);
            }
            if widgets::dialog_button(ui, &shared.t("🗑️ Supprimer")).clicked() {
                action = Some(Action::Delete);
            }
            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                if widgets::dialog_button(ui, &shared.t("✅ Appliquer")).clicked() {
                    action = Some(Action::Apply);
                }
            });
        });

        match action {
            Some(Action::Add) => {
                self.edit = Some(EditState { index: None, name: String::new(), address: String::new() })
            }
            Some(Action::Edit) => match self.selected.and_then(|i| entries.get(i).map(|e| (i, e))) {
                Some((i, e)) => {
                    self.edit = Some(EditState { index: Some(i), name: e.name.clone(), address: e.address.clone() })
                }
                None => msg::warning(&shared.t("Erreur"), &shared.t("Veuillez sélectionner un serveur à modifier.")),
            },
            Some(Action::Delete) => self.delete_selected(shared),
            Some(Action::Apply) => {
                if let Some(i) = self.selected {
                    shared.config.switch_realmlist(i);
                    shared.emit(AppEvent::RealmlistChanged);
                    self.reload(shared);
                    msg::info(&shared.t("Succès"), &shared.t("Serveur modifié avec succès!"));
                }
                close = true;
            }
            None => {}
        }

        self.edit_modal(ui, shared);
        close
    }

    fn server_row(&mut self, ui: &mut Ui, shared: &Shared, e: &RealmlistEntry, index: usize, is_active: bool) {
        let (fill, border) = if is_active { (hex("#3a3a3a"), theme::GOLD) } else { (hex("#2a2a2a"), hex("#5a4a2d")) };
        egui::Frame::new()
            .fill(fill)
            .stroke(Stroke::new(2.0, border))
            .corner_radius(CornerRadius::same(10))
            .inner_margin(egui::Margin::same(10))
            .show(ui, |ui| {
                ui.set_width(ui.available_width());
                ui.horizontal(|ui| {
                    let checked = self.selected == Some(index);
                    if ui.radio(checked, "").clicked() {
                        self.selected = Some(index);
                    }
                    ui.vertical(|ui| {
                        ui.spacing_mut().item_spacing.y = 2.0;
                        ui.label(theme::bold(&e.name, 16.0).color(theme::GOLD));
                        ui.label(RichText::new(&e.address).size(12.0).color(theme::TEXT_MUTED));
                    });
                    if is_active {
                        ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                            ui.label(theme::bold(shared.t("✓ Actif"), 13.0).color(hex("#4CAF50")));
                        });
                    }
                });
            });
    }

    fn delete_selected(&mut self, shared: &mut Shared) {
        let Some(sel) = self.selected else {
            msg::warning(&shared.t("Erreur"), &shared.t("Veuillez sélectionner un serveur à supprimer."));
            return;
        };
        let mut entries = shared.config.realmlist_entries();
        if entries.len() <= 1 {
            msg::warning(&shared.t("Erreur"), &shared.t("Vous devez garder au moins un serveur."));
            return;
        }
        if !msg::confirm(&shared.t("Confirmer"), &shared.t("Êtes-vous sûr de vouloir supprimer ce serveur?")) {
            return;
        }
        if sel >= entries.len() {
            return;
        }
        entries.remove(sel);
        // Adjust the active index if needed.
        let mut active = shared.config.active_realmlist_index();
        if sel <= active && active > 0 {
            active -= 1;
        }
        shared.config.set_realmlist_entries(&entries);
        // Also refreshes the per-entry "active" flags.
        shared.config.set_active_realmlist_index(active.min(entries.len() - 1));
        shared.emit(AppEvent::RealmlistChanged);
        self.reload(shared);
    }

    /// Qt ServerEditDialog, as a modal over the list.
    fn edit_modal(&mut self, ui: &mut Ui, shared: &mut Shared) {
        let Some(edit) = self.edit.as_mut() else { return };
        let mut result: Option<bool> = None; // Some(true) = OK, Some(false) = cancel
        let modal = egui::Modal::new(egui::Id::unique("sylvania-server-edit")).show(ui.ctx(), |ui| {
            ui.set_width(400.0);
            ui.spacing_mut().item_spacing.y = 12.0;
            ui.label(theme::bold(shared.t("Configuration du Serveur"), 15.0).color(theme::GOLD));
            let field_label = |ui: &mut Ui, s: String| {
                ui.label(theme::bold(s, 13.0).color(theme::GOLD));
            };
            field_label(ui, shared.t("Nom du serveur:"));
            ui.add(
                egui::TextEdit::singleline(&mut edit.name)
                    .hint_text(shared.t("Ex: Sylvania"))
                    .desired_width(f32::INFINITY)
                    .margin(egui::Margin::same(SPACE_SM as i8)),
            );
            field_label(ui, shared.t("Adresse (realmlist):"));
            ui.add(
                egui::TextEdit::singleline(&mut edit.address)
                    .hint_text(shared.t("Ex: set realmlist sylvania-servergame.com"))
                    .desired_width(f32::INFINITY)
                    .margin(egui::Margin::same(SPACE_SM as i8)),
            );
            ui.add_space(SPACE_MD - 12.0);
            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                ui.spacing_mut().item_spacing.x = 15.0;
                if widgets::dialog_button(ui, &shared.t("Annuler")).clicked() {
                    result = Some(false);
                }
                if widgets::dialog_button(ui, &shared.t("OK")).clicked() {
                    result = Some(true);
                }
            });
        });
        if result.is_none() && modal.should_close() {
            result = Some(false);
        }

        match result {
            Some(true) => {
                let name = edit.name.trim().to_string();
                let address = edit.address.trim().to_string();
                if name.is_empty() || address.is_empty() {
                    msg::warning(&shared.t("Erreur"), &shared.t("Tous les champs sont requis."));
                    return;
                }
                let index = edit.index;
                self.edit = None;
                let mut entries = shared.config.realmlist_entries();
                match index {
                    Some(i) if i < entries.len() => {
                        entries[i].name = name;
                        entries[i].address = address;
                    }
                    Some(_) => return,
                    None => entries.push(RealmlistEntry { name, address, active: false }),
                }
                shared.config.set_realmlist_entries(&entries);
                shared.emit(AppEvent::RealmlistChanged);
                self.reload(shared);
            }
            Some(false) => self.edit = None,
            None => {}
        }
    }
}

enum Action {
    Add,
    Edit,
    Delete,
    Apply,
}
