//! User interface. The main window lives in [`main_window`]; every secondary
//! window (Qt dialogs) is a real OS window opened with [`child_window`].

pub mod addons;
pub mod download;
pub mod main_window;
pub mod notes;
pub mod realmlist;
pub mod settings;
pub mod widgets;

use crate::constants::SPACE_MD;
use crate::theme;
use eframe::egui;
use std::sync::{Arc, OnceLock};

/// Window icon shared by every viewport.
pub fn app_icon() -> Arc<egui::IconData> {
    static ICON: OnceLock<Arc<egui::IconData>> = OnceLock::new();
    ICON.get_or_init(|| {
        let icon = image::load_from_memory(crate::assets::ICON_PNG)
            .map(|i| i.to_rgba8())
            .map(|i| egui::IconData { width: i.width(), height: i.height(), rgba: i.into_raw() })
            .unwrap_or_default();
        Arc::new(icon)
    })
    .clone()
}

/// Shows a secondary OS window (immediate viewport) with the dark launcher
/// style, rendering `add` inside a padded, scroll-free central panel.
///
/// `id` must be unique and stable per window. Returns `false` once the user
/// asked to close it (title-bar ✕); the caller then drops its state.
pub fn child_window(
    ctx: &egui::Context,
    id: &str,
    title: &str,
    size: [f32; 2],
    min_size: [f32; 2],
    add: impl FnMut(&mut egui::Ui),
) -> bool {
    let mut add = add;
    let builder = egui::ViewportBuilder::default()
        .with_title(title)
        .with_inner_size(size)
        .with_min_inner_size(min_size)
        .with_icon(app_icon());
    ctx.show_viewport_immediate(egui::ViewportId::from_hash_of(id), builder, |ctx, _class| {
        egui::CentralPanel::default()
            .frame(egui::Frame::new().fill(theme::WINDOW_BG).inner_margin(SPACE_MD))
            .show(ctx, |ui| add(ui));
        !ctx.input(|i| i.viewport().close_requested())
    })
}

/// Requests focus for a child window (e.g. when its button is clicked again).
pub fn focus_child_window(ctx: &egui::Context, id: &str) {
    ctx.send_viewport_cmd_to(egui::ViewportId::from_hash_of(id), egui::ViewportCommand::Focus);
}
