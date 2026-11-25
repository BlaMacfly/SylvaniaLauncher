//! Client download dialog (progress, speed, ETA, cancel): a secondary OS
//! window rendering the snapshots of [`crate::core::download::DownloadTask`].

use crate::app::Shared;
use crate::constants::{self, SPACE_LG, SPACE_MD};
use crate::core::download::{DlEvent, DownloadParams, DownloadTask, Outcome, Snapshot};
use crate::theme::{self, hex};
use crate::ui::{child_window, widgets};
use eframe::egui::{self, Align, Color32, CornerRadius, Layout, RichText, Sense, Stroke, Ui, Vec2};
use std::path::PathBuf;

/// Viewport id of the dialog (see `ui::child_window`).
const WINDOW_ID: &str = "client-download";

pub struct DownloadDialog {
    task: DownloadTask,
    snap: Snapshot,
    destination: PathBuf,
}

impl DownloadDialog {
    /// Starts downloading the client into `destination` (Qt: DownloadDialog +
    /// configureForClient + setLanguage + setRealmlistAddress).
    pub fn start(destination: PathBuf, shared: &Shared) -> Self {
        let address = shared.config.active_realmlist_entry().map(|e| e.address);
        let params =
            DownloadParams::for_client(destination.clone(), address, &shared.config.language());
        let snap = Snapshot {
            status: shared.t("Préparation du téléchargement..."),
            percent: Some(0),
            speed: shared.t("Vitesse: --"),
            size: shared.t("Taille: --"),
            eta: shared.t("Temps restant: --"),
            button_percent: None,
        };
        DownloadDialog {
            task: DownloadTask::start(params, shared.i18n.clone()),
            snap,
            destination,
        }
    }

    pub fn destination(&self) -> &std::path::Path {
        &self.destination
    }

    /// Progress mirrored on the main primary button (None = indeterminate).
    pub fn button_progress(&self) -> Option<u8> {
        self.snap.button_percent
    }

    /// Renders the dialog. Returns the outcome once the download is over,
    /// cancelled (button or title-bar ✕); the caller then drops the dialog.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> Option<Outcome> {
        for ev in self.task.poll() {
            match ev {
                DlEvent::Update(s) => self.snap = s,
                DlEvent::Finished(o) => return Some(o),
            }
        }

        let title = shared.tf(
            "Téléchargement du Client — %1",
            &[&constants::CLIENT_DISPLAY_NAME],
        );
        let cancel_text = shared.t("Annuler");
        let snap = &self.snap;
        let mut cancel = false;
        let open = child_window(
            ctx,
            WINDOW_ID,
            &title,
            [500.0, 250.0],
            [500.0, 250.0],
            |ui| {
                ui.add_space(SPACE_LG - SPACE_MD);
                ui.spacing_mut().item_spacing.y = SPACE_MD;
                ui.vertical_centered(|ui| {
                    ui.label(theme::bold(&snap.status, 14.0).color(theme::GOLD));
                });
                progress_bar(ui, snap.percent, &BarStyle::dialog());
                info_row(ui, snap);
                ui.with_layout(Layout::bottom_up(Align::Center), |ui| {
                    ui.add_space(SPACE_LG - SPACE_MD);
                    if widgets::dialog_button(ui, &cancel_text).clicked() {
                        cancel = true;
                    }
                });
            },
        );
        ctx.request_repaint_after(std::time::Duration::from_millis(100));

        if cancel || !open {
            self.task.cancel();
            return Some(Outcome::cancelled(&shared.i18n));
        }
        None
    }
}

/// Speed (left) · size (centre) · ETA (right), grey 12 px.
fn info_row(ui: &mut Ui, snap: &Snapshot) {
    let w = ui.available_width() / 3.0;
    let h = 18.0;
    let text = |s: &str| RichText::new(s).size(12.0).color(theme::TEXT_MUTED);
    ui.horizontal(|ui| {
        ui.spacing_mut().item_spacing.x = 0.0;
        ui.allocate_ui_with_layout(
            Vec2::new(w, h),
            Layout::left_to_right(Align::Center),
            |ui| {
                ui.label(text(&snap.speed));
            },
        );
        ui.allocate_ui_with_layout(
            Vec2::new(w, h),
            Layout::centered_and_justified(egui::Direction::LeftToRight),
            |ui| {
                ui.label(text(&snap.size));
            },
        );
        ui.allocate_ui_with_layout(
            Vec2::new(w, h),
            Layout::right_to_left(Align::Center),
            |ui| {
                ui.label(text(&snap.eta));
            },
        );
    });
}

/// Look of a [`progress_bar`].
pub(crate) struct BarStyle {
    pub height: f32,
    pub bg: Color32,
    pub border: Color32,
    pub fill: Color32,
    pub radius: u8,
    /// Draw "NN%" centred on the bar (determinate only).
    pub show_text: bool,
}

impl BarStyle {
    /// Download dialog bar (Qt: #2a2a4e, gold chunk, centred percentage).
    pub fn dialog() -> Self {
        BarStyle {
            height: 25.0,
            bg: hex("#2a2a4e"),
            border: hex("#3a3a5e"),
            fill: theme::GOLD,
            radius: 5,
            show_text: true,
        }
    }
}

/// Full-width progress bar. `percent` None = indeterminate (a chunk sweeping
/// back and forth, like Qt's busy QProgressBar).
pub(crate) fn progress_bar(ui: &mut Ui, percent: Option<u8>, style: &BarStyle) -> egui::Response {
    let (rect, response) = ui.allocate_exact_size(
        Vec2::new(ui.available_width(), style.height),
        Sense::hover(),
    );
    if !ui.is_rect_visible(rect) {
        return response;
    }
    let painter = ui.painter();
    let radius = CornerRadius::same(style.radius);
    painter.rect_filled(rect, radius, style.bg);
    let inner = rect.shrink(1.0);
    let chunk_radius = CornerRadius::same(style.radius.saturating_sub(1));
    match percent {
        Some(p) => {
            let w = inner.width() * f32::from(p.min(100)) / 100.0;
            if w > 0.5 {
                let chunk = egui::Rect::from_min_size(inner.min, Vec2::new(w, inner.height()));
                painter.rect_filled(chunk, chunk_radius, style.fill);
            }
            if style.show_text {
                painter.text(
                    rect.center(),
                    egui::Align2::CENTER_CENTER,
                    format!("{p}%"),
                    egui::FontId::proportional(12.0),
                    Color32::WHITE,
                );
            }
        }
        None => {
            let t = ui.input(|i| i.time) as f32;
            let w = inner.width() * 0.3;
            // Triangle wave 0..1..0 over 2 s.
            let phase = (t * 0.5).fract();
            let k = if phase < 0.5 {
                phase * 2.0
            } else {
                2.0 - phase * 2.0
            };
            let x = inner.left() + (inner.width() - w) * k;
            let chunk =
                egui::Rect::from_min_size(egui::pos2(x, inner.top()), Vec2::new(w, inner.height()));
            painter.rect_filled(chunk, chunk_radius, style.fill);
            ui.ctx().request_repaint();
        }
    }
    painter.rect_stroke(
        rect,
        radius,
        Stroke::new(1.0, style.border),
        egui::StrokeKind::Inside,
    );
    response
}
