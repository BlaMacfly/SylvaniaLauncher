//! Rendering of the addon manager (colours ported from the Qt stylesheets).
//! Clicks are reported as [`Action`]s, executed by the window after the pass.

use super::{Action, AddonsWindow, Card, CardRef, ManifestStatus, StatusKind, Tab, format_size};
use crate::app::Shared;
use crate::constants::SPACE_SM;
use crate::core::addons::InstallState;
use crate::theme::{self, GOLD, ThemeTokens, hex};
use crate::ui::widgets::{Level, level_button};
use eframe::egui::{self, Align, Color32, CornerRadius, Layout, RichText, Sense, Stroke, Ui, vec2};

const PANE_BG: Color32 = Color32::from_rgb(0x15, 0x15, 0x25);
const PANE_BORDER: Color32 = Color32::from_rgb(0x3a, 0x3a, 0x5e);
const CARD_BG: Color32 = Color32::from_rgb(0x2a, 0x2a, 0x4e);
const CARD_HOVER: Color32 = Color32::from_rgb(0x32, 0x32, 0x5c);
const TAB_BG: Color32 = Color32::from_rgb(0x2a, 0x2a, 0x4e);
const TAB_SELECTED: Color32 = Color32::from_rgb(0x3a, 0x3a, 0x5e);
const TEXT_INFO: Color32 = Color32::from_rgb(0xaa, 0xaa, 0xaa);
const TEXT_BLUE: Color32 = Color32::from_rgb(0x7e, 0xc8, 0xe3);
const TEXT_GREEN: Color32 = Color32::from_rgb(0x55, 0xff, 0x55);
const TEXT_RED: Color32 = Color32::from_rgb(0xff, 0x55, 0x55);
const PROGRESS_BG: Color32 = Color32::from_rgb(0x1a, 0x1a, 0x2e);
const DISABLED_TEXT: Color32 = Color32::from_rgb(0x88, 0x88, 0x88);

const CLOSE_BUTTON_HEIGHT: f32 = 38.0;

#[derive(Clone, Copy)]
enum ButtonStyle {
    Install,
    Update,
    Uninstall,
    Neutral,
}

fn tokens(style: ButtonStyle) -> ThemeTokens {
    let (grad, border, text) = match style {
        ButtonStyle::Install => (["#4a7c3f", "#3a6a2f", "#2a5a1f"], "#5a8c4f", Color32::WHITE),
        ButtonStyle::Update => (["#2a6dd4", "#1e5ab8", "#15479a"], "#3a7de4", Color32::WHITE),
        ButtonStyle::Uninstall => (["#8a3a3a", "#6a2a2a", "#4a1a1a"], "#9a4a4a", hex("#f0c0c0")),
        ButtonStyle::Neutral => (["#4a4a6e", "#3a3a5e", "#2a2a4e"], "#5a5a7e", Color32::WHITE),
    };
    ThemeTokens {
        secondary: grad.map(hex),
        secondary_border: hex(border),
        secondary_text: text,
        ..ThemeTokens::default()
    }
}

fn button(ui: &mut Ui, text: &str, style: ButtonStyle, enabled: bool, width: f32) -> bool {
    level_button(ui, text, Level::Secondary, &tokens(style), enabled, width).clicked()
}

fn text(s: impl Into<String>, size: f32, color: Color32) -> RichText {
    RichText::new(s).size(size).color(color)
}

fn wrapped(ui: &mut Ui, rt: RichText) {
    ui.add(egui::Label::new(rt).wrap());
}

impl AddonsWindow {
    pub(super) fn render(&mut self, ui: &mut Ui, shared: &Shared, actions: &mut Vec<Action>) {
        ui.vertical_centered(|ui| {
            ui.label(theme::bold(shared.t("Gestionnaire d'Addons"), 20.0).color(GOLD));
        });
        ui.add_space(4.0);

        let pane_height = (ui.available_height() - CLOSE_BUTTON_HEIGHT - 3.0 * SPACE_SM).max(200.0);
        ui.scope(|ui| {
            ui.spacing_mut().item_spacing.y = 0.0;
            self.tab_bar(ui, shared, actions);
            let width = ui.available_width();
            ui.allocate_ui_with_layout(
                vec2(width, pane_height),
                Layout::top_down(Align::Min),
                |ui| {
                    egui::Frame::new()
                        .fill(PANE_BG)
                        .stroke(Stroke::new(1.0, PANE_BORDER))
                        .corner_radius(CornerRadius::same(5))
                        .inner_margin(12)
                        .show(ui, |ui| {
                            ui.set_min_size(ui.available_size());
                            ui.spacing_mut().item_spacing.y = SPACE_SM;
                            match self.tab {
                                Tab::Recommended => self.recommended_tab(ui, shared, actions),
                                Tab::Catalog => self.catalog_tab(ui, shared, actions),
                                Tab::Installed => self.installed_tab(ui, shared, actions),
                            }
                        });
                },
            );
        });

        ui.add_space(SPACE_SM);
        ui.vertical_centered(|ui| {
            if level_button(
                ui,
                &shared.t("Fermer"),
                Level::Secondary,
                &ThemeTokens::default(),
                true,
                120.0,
            )
            .clicked()
            {
                actions.push(Action::Close);
            }
        });
    }

    fn tab_bar(&self, ui: &mut Ui, shared: &Shared, actions: &mut Vec<Action>) {
        ui.horizontal(|ui| {
            ui.spacing_mut().item_spacing.x = 2.0;
            for (tab, label) in [
                (Tab::Recommended, "Recommandés"),
                (Tab::Catalog, "Catalogue"),
                (Tab::Installed, "Installés"),
            ] {
                let selected = self.tab == tab;
                let rt = if selected {
                    theme::bold(shared.t(label), 13.0).color(GOLD)
                } else {
                    text(shared.t(label), 13.0, TEXT_INFO)
                };
                let galley = egui::WidgetText::from(rt).into_galley(
                    ui,
                    Some(egui::TextWrapMode::Extend),
                    f32::INFINITY,
                    egui::TextStyle::Button,
                );
                let size = galley.size() + vec2(32.0, 16.0);
                let (rect, resp) = ui.allocate_exact_size(size, Sense::click());
                let resp = resp.on_hover_cursor(egui::CursorIcon::PointingHand);
                let fill = if selected { TAB_SELECTED } else { TAB_BG };
                let radius = CornerRadius {
                    nw: 5,
                    ne: 5,
                    sw: 0,
                    se: 0,
                };
                ui.painter().rect_filled(rect, radius, fill);
                ui.painter()
                    .galley(rect.center() - galley.size() / 2.0, galley, Color32::WHITE);
                if resp.clicked() {
                    actions.push(Action::SelectTab(tab));
                }
            }
        });
    }

    // --- Recommended / catalogue -------------------------------------------------

    fn recommended_tab(&self, ui: &mut Ui, shared: &Shared, actions: &mut Vec<Action>) {
        ui.vertical_centered(|ui| {
            wrapped(
                ui,
                text(
                    shared.t("Installez en un clic des addons recommandés par Sylvania."),
                    12.0,
                    TEXT_INFO,
                ),
            );
            let status = match self.manifest_status {
                ManifestStatus::Loading => Some("Chargement de la liste..."),
                ManifestStatus::Empty => Some("Aucun addon recommandé disponible."),
                ManifestStatus::Offline => Some("Liste hors-ligne (le serveur est injoignable)."),
                ManifestStatus::Ready => None,
            };
            if let Some(s) = status {
                wrapped(ui, text(shared.t(s), 12.0, TEXT_BLUE));
            }
        });
        egui::ScrollArea::vertical()
            .id_salt("addons-recommended")
            .auto_shrink(false)
            .show(ui, |ui| {
                ui.spacing_mut().item_spacing.y = 10.0;
                for (i, card) in self.recommended.iter().enumerate() {
                    self.card_ui(ui, shared, card, CardRef::Recommended(i), actions);
                }
            });
    }

    fn catalog_tab(&self, ui: &mut Ui, shared: &Shared, actions: &mut Vec<Action>) {
        ui.vertical_centered(|ui| {
            wrapped(
                ui,
                text(
                    shared.t(
                        "Sélectionnez les addons que vous souhaitez installer. Ils seront automatiquement placés dans le dossier de votre jeu.",
                    ),
                    12.0,
                    TEXT_INFO,
                ),
            );
        });
        egui::ScrollArea::vertical()
            .id_salt("addons-catalog")
            .auto_shrink(false)
            .show(ui, |ui| {
                ui.spacing_mut().item_spacing.y = 10.0;
                for (i, card) in self.catalog.iter().enumerate() {
                    self.card_ui(ui, shared, card, CardRef::Catalog(i), actions);
                }
            });
    }

    fn card_ui(
        &self,
        ui: &mut Ui,
        shared: &Shared,
        card: &Card,
        card_ref: CardRef,
        actions: &mut Vec<Action>,
    ) {
        let recommended = matches!(card_ref, CardRef::Recommended(_));
        let installing = self.active.as_ref().is_some_and(|a| a.card == card_ref);
        let button_width = if recommended { 120.0 } else { 110.0 };

        let margin = egui::Margin::symmetric(15, 12);
        let mut prepared = egui::Frame::new()
            .fill(CARD_BG)
            .corner_radius(CornerRadius::same(5))
            .inner_margin(margin)
            .begin(ui);
        {
            let ui = &mut prepared.content_ui;
            ui.set_width(ui.available_width());
            let text_width = (ui.available_width() - button_width - 2.0 * SPACE_SM).max(120.0);
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.set_width(text_width);
                    ui.spacing_mut().item_spacing.y = 3.0;
                    let t = &card.target;
                    let mut title = t.name.clone();
                    if recommended && !t.version.is_empty() {
                        title.push_str(&format!("  v{}", t.version));
                    }
                    wrapped(ui, theme::bold(title, 15.0).color(Color32::WHITE));
                    // Catalogue descriptions are in en.json; manifest ones are
                    // translated when they match a known string.
                    wrapped(ui, text(shared.t(&card.description), 12.0, TEXT_INFO));
                    if let Some(rt) = version_label(shared, &card.state) {
                        wrapped(ui, rt);
                    }
                    if let Some(p) = card.progress {
                        progress_bar(ui, p.0);
                    }
                    if let Some(s) = card.status {
                        let msg = match s.pct {
                            Some(p) => shared.tf(s.text, &[&p]),
                            None => shared.t(s.text),
                        };
                        let color = match s.kind {
                            StatusKind::Info => TEXT_BLUE,
                            StatusKind::Success => TEXT_GREEN,
                            StatusKind::Error => TEXT_RED,
                            StatusKind::Muted => TEXT_INFO,
                        };
                        wrapped(ui, text(msg, 11.0, color));
                    }
                });
                ui.vertical(|ui| {
                    ui.spacing_mut().item_spacing.y = 4.0;
                    let (label, style) = match card.state {
                        InstallState::UpdateAvailable { .. } => {
                            ("Mettre à jour", ButtonStyle::Update)
                        }
                        InstallState::Installed(_) | InstallState::CatalogInstalled => {
                            ("Réinstaller", ButtonStyle::Install)
                        }
                        InstallState::NotInstalled | InstallState::CatalogAbsent => {
                            ("Installer", ButtonStyle::Install)
                        }
                    };
                    if button(ui, &shared.t(label), style, !installing, button_width) {
                        actions.push(Action::Install(card_ref));
                    }
                    let can_uninstall = recommended
                        && matches!(
                            card.state,
                            InstallState::UpdateAvailable { .. } | InstallState::Installed(_)
                        );
                    if can_uninstall
                        && button(
                            ui,
                            &shared.t("Désinstaller"),
                            ButtonStyle::Uninstall,
                            !installing,
                            button_width,
                        )
                    {
                        actions.push(Action::Uninstall(card_ref));
                    }
                });
            });
        }
        let outer = prepared.content_ui.min_rect() + margin;
        if ui.rect_contains_pointer(outer) {
            prepared.frame.fill = CARD_HOVER;
        }
        prepared.end(ui);
    }

    // --- Installés ------------------------------------------------------------------

    fn installed_tab(&mut self, ui: &mut Ui, shared: &Shared, actions: &mut Vec<Action>) {
        wrapped(
            ui,
            text(
                shared.t(
                    "Addons présents dans Interface/AddOns. Sélectionnez-en un ou plusieurs (Ctrl/Maj) puis supprimez.",
                ),
                12.0,
                TEXT_INFO,
            ),
        );

        let list_height = (ui.available_height() - 36.0 - SPACE_SM).max(80.0);
        let width = ui.available_width();
        ui.allocate_ui_with_layout(
            vec2(width, list_height),
            Layout::top_down(Align::Min),
            |ui| {
                egui::Frame::new()
                    .fill(PANE_BG)
                    .stroke(Stroke::new(1.0, PANE_BORDER))
                    .corner_radius(CornerRadius::same(5))
                    .inner_margin(1)
                    .show(ui, |ui| {
                        ui.set_min_size(ui.available_size());
                        egui::ScrollArea::vertical()
                            .id_salt("addons-installed")
                            .auto_shrink(false)
                            .show(ui, |ui| {
                                ui.spacing_mut().item_spacing.y = 0.0;
                                self.installed_rows(ui, shared);
                            });
                    });
            },
        );

        ui.horizontal(|ui| {
            if button(ui, &shared.t("Actualiser"), ButtonStyle::Neutral, true, 0.0) {
                actions.push(Action::RefreshInstalled);
            }
            if self.installed.scan.is_some() {
                ui.add(egui::Spinner::new().color(GOLD));
            }
            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                if button(
                    ui,
                    &shared.t("Supprimer la sélection"),
                    ButtonStyle::Uninstall,
                    true,
                    0.0,
                ) {
                    actions.push(Action::DeleteSelected);
                }
            });
        });
    }

    fn installed_rows(&mut self, ui: &mut Ui, shared: &Shared) {
        let v = &mut self.installed;
        if v.scan.is_some() && v.rows.is_empty() {
            return;
        }
        let placeholder = if v.no_dir {
            Some("Aucun dossier AddOns trouvé. Configurez WoW dans les réglages.")
        } else if v.rows.is_empty() {
            Some("Aucun addon installé.")
        } else {
            None
        };
        if let Some(p) = placeholder {
            egui::Frame::new().inner_margin(8).show(ui, |ui| {
                ui.set_width(ui.available_width());
                wrapped(ui, text(shared.t(p), 13.0, DISABLED_TEXT));
            });
            return;
        }

        let mut clicked = None;
        for (i, row) in v.rows.iter().enumerate() {
            let mut line = row.name.clone();
            if !row.version.is_empty() {
                line.push_str(&format!("  v{}", row.version));
            }
            line.push_str(&format!("   ({})", format_size(shared, row.size)));
            if row.by_launcher() {
                if row.folders.len() > 1 {
                    line.push_str(&format!("   [{}]", row.folders.join(", ")));
                }
                line.push_str(&format!("   — {}", shared.t("installé par le launcher")));
            }

            let selected = v.selected.contains(&i);
            let color = if selected { GOLD } else { Color32::WHITE };
            let pad = vec2(8.0, 8.0);
            let wrap_width = ui.available_width() - 2.0 * pad.x;
            let galley = egui::WidgetText::from(text(line, 13.0, color)).into_galley(
                ui,
                Some(egui::TextWrapMode::Wrap),
                wrap_width,
                egui::TextStyle::Body,
            );
            let size = vec2(ui.available_width(), galley.size().y + 2.0 * pad.y);
            let (rect, resp) = ui.allocate_exact_size(size, Sense::click());
            let fill = if selected {
                TAB_SELECTED
            } else if resp.hovered() {
                Color32::from_rgb(0x22, 0x22, 0x3a)
            } else {
                Color32::TRANSPARENT
            };
            ui.painter().rect_filled(rect, 0, fill);
            ui.painter().galley(rect.min + pad, galley, color);
            ui.painter().hline(
                rect.x_range(),
                rect.bottom() - 0.5,
                Stroke::new(1.0, CARD_BG),
            );
            if resp.clicked() {
                clicked = Some(i);
            }
        }

        if let Some(i) = clicked {
            let m = ui.input(|inp| inp.modifiers);
            match v.anchor {
                Some(a) if m.shift => {
                    let range = a.min(i)..=a.max(i);
                    if !m.command {
                        v.selected.clear();
                    }
                    v.selected.extend(range);
                }
                _ if m.command => {
                    if !v.selected.remove(&i) {
                        v.selected.insert(i);
                    }
                    v.anchor = Some(i);
                }
                _ => {
                    v.selected.clear();
                    v.selected.insert(i);
                    v.anchor = Some(i);
                }
            }
        }
    }
}

/// Version line of a card (colour + weight like the Qt labels).
fn version_label(shared: &Shared, state: &InstallState) -> Option<RichText> {
    Some(match state {
        InstallState::UpdateAvailable {
            installed,
            available,
        } => theme::bold(
            shared.tf(
                "Mise à jour disponible (v%1 → v%2)",
                &[installed, available],
            ),
            11.0,
        )
        .color(GOLD),
        InstallState::Installed(v) if v.is_empty() => {
            theme::bold(shared.t("Installé"), 11.0).color(TEXT_GREEN)
        }
        InstallState::Installed(v) => {
            theme::bold(shared.tf("Installé (v%1)", &[v]), 11.0).color(TEXT_GREEN)
        }
        InstallState::NotInstalled => text(shared.t("Non installé"), 11.0, TEXT_INFO),
        InstallState::CatalogInstalled => text(shared.t("Déjà installé"), 11.0, TEXT_GREEN),
        InstallState::CatalogAbsent => return None,
    })
}

/// Thin gold progress bar (5 px). `None` = indeterminate (animated).
fn progress_bar(ui: &mut Ui, fraction: Option<f32>) {
    let (rect, _) = ui.allocate_exact_size(vec2(ui.available_width(), 5.0), Sense::hover());
    let painter = ui.painter();
    painter.rect_filled(rect, 2, PROGRESS_BG);
    let fill = match fraction {
        Some(f) => {
            let mut r = rect;
            r.set_width(rect.width() * f.clamp(0.0, 1.0));
            r
        }
        None => {
            let t = ui.input(|i| i.time) as f32;
            let seg = rect.width() * 0.3;
            let x = rect.left() + (t * 0.6).fract() * (rect.width() + seg) - seg;
            let mut r = rect;
            r.set_left(x.max(rect.left()));
            r.set_right((x + seg).min(rect.right()));
            ui.ctx().request_repaint();
            r
        }
    };
    if fill.width() > 0.0 {
        painter.rect_filled(fill, 2, GOLD);
    }
}
