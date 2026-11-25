//! Shared widgets: the 3-level button system (primary / secondary / tertiary,
//! vertical 3-stop gradient, hover / pressed / disabled / focus states — same
//! look as the Qt ButtonStyles), themed panels and a few dialog helpers.

use crate::constants::*;
use crate::theme::{self, Palette, ThemeTokens};
use eframe::egui::{self, Color32, CornerRadius, Response, Sense, Stroke, Ui, Vec2};
use std::collections::HashMap;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Level {
    /// One per screen (Jouer / Installer). 16 px bold.
    Primary,
    /// Navigation and common actions. 12 px bold.
    Secondary,
    /// Small toggles. 10 px bold.
    Tertiary,
}

impl Level {
    fn min_height(self) -> f32 {
        match self {
            Level::Primary => BUTTON_HEIGHT_PRIMARY,
            Level::Secondary => BUTTON_HEIGHT_SECONDARY,
            Level::Tertiary => BUTTON_HEIGHT_TERTIARY,
        }
    }
    fn font_size(self) -> f32 {
        match self {
            Level::Primary => 16.0,
            Level::Secondary => 12.0,
            Level::Tertiary => 10.0,
        }
    }
}

/// A themed button. `min_width` 0 = fit the text.
pub fn level_button(
    ui: &mut Ui,
    text: &str,
    level: Level,
    tokens: &ThemeTokens,
    enabled: bool,
    min_width: f32,
) -> Response {
    let (grad, border, text_color) = match level {
        Level::Primary => (tokens.primary, tokens.primary_border, tokens.primary_text),
        _ => (
            tokens.secondary,
            tokens.secondary_border,
            tokens.secondary_text,
        ),
    };
    let galley = ui.painter().layout_no_wrap(
        text.to_string(),
        egui::FontId::new(
            level.font_size(),
            egui::FontFamily::Name(theme::BOLD.into()),
        ),
        text_color,
    );
    let size = Vec2::new(
        (galley.size().x + 2.0 * SPACE_MD + 4.0).max(min_width),
        level.min_height().max(galley.size().y + 2.0 * SPACE_SM),
    );
    let sense = if enabled {
        Sense::click()
    } else {
        Sense::hover()
    };
    let (rect, mut response) = ui.allocate_exact_size(size, sense);
    if enabled {
        response = response.on_hover_cursor(egui::CursorIcon::PointingHand);
    }

    if ui.is_rect_visible(rect) {
        let painter = ui.painter();
        let radius = CornerRadius::same(BUTTON_RADIUS as u8);
        let (stops, stroke_color, label_color) = if !enabled {
            let g = Color32::from_rgb(0x3a, 0x3a, 0x3a);
            (
                [g, g, g],
                Color32::from_rgb(0x55, 0x55, 0x55),
                Color32::from_rgb(0x8a, 0x8a, 0x8a),
            )
        } else if response.is_pointer_button_down_on() {
            ([grad[2], grad[2], grad[2]], border, text_color)
        } else if response.hovered() {
            ([border, grad[0], grad[1]], border, text_color)
        } else {
            (grad, border, text_color)
        };
        let stroke_color = if enabled && response.has_focus() {
            tokens.accent
        } else {
            stroke_color
        };

        let tex = gradient_texture(ui.ctx(), stops);
        painter.add(
            egui::epaint::RectShape::filled(rect, radius, Color32::WHITE).with_texture(
                tex,
                egui::Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)),
            ),
        );
        painter.rect_stroke(
            rect.shrink(1.0),
            radius,
            Stroke::new(2.0, stroke_color),
            egui::StrokeKind::Inside,
        );
        let pos = rect.center() - galley.size() / 2.0;
        painter.galley_with_override_text_color(pos, galley, label_color);
    }
    response
}

/// Secondary-level button with the default dark tokens (secondary windows).
pub fn dialog_button(ui: &mut Ui, text: &str) -> Response {
    level_button(
        ui,
        text,
        Level::Secondary,
        &ThemeTokens::default(),
        true,
        0.0,
    )
}

/// 1×3 texture holding a vertical gradient, cached per colour triple.
fn gradient_texture(ctx: &egui::Context, stops: [Color32; 3]) -> egui::TextureId {
    #[derive(Clone, Default)]
    struct Cache(HashMap<[Color32; 3], egui::TextureHandle>);

    let id = egui::Id::unique("sylvania-gradient-cache");
    if let Some(t) = ctx.data(|d| {
        d.get_temp::<Cache>(id)
            .and_then(|c| c.0.get(&stops).map(|t| t.id()))
    }) {
        return t;
    }
    let mut pixels = Vec::with_capacity(12);
    for c in stops {
        pixels.extend_from_slice(&c.to_srgba_unmultiplied());
    }
    let image = egui::ColorImage::from_rgba_unmultiplied([1, 3], &pixels);
    let handle = ctx.load_texture(
        format!("gradient-{stops:?}"),
        image,
        egui::TextureOptions::LINEAR,
    );
    let tid = handle.id();
    ctx.data_mut(|d| {
        d.get_temp_mut_or_default::<Cache>(id)
            .0
            .insert(stops, handle);
    });
    tid
}

/// Frame of an info panel on the main window (rounded, palette colours).
pub fn panel_frame(palette: &Palette) -> egui::Frame {
    egui::Frame::new()
        .fill(palette.panel_bg)
        .stroke(Stroke::new(2.0, palette.panel_border))
        .corner_radius(CornerRadius::same(10))
        .inner_margin(egui::Margin::symmetric(SPACE_LG as i8, SPACE_MD as i8))
}

/// Gold section title used in secondary windows.
pub fn section_title(ui: &mut Ui, text: &str) {
    ui.label(theme::bold(text, 14.0).color(theme::GOLD));
}
