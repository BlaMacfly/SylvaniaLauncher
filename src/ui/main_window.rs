//! Main window. STUB — implemented by the main window port.
//! Contract: keep `new`, `show`, `on_event`, `on_exit` with these signatures.

use crate::app::{AppEvent, Shared};
use eframe::egui;

pub struct MainWindow {}

impl MainWindow {
    pub fn new(ctx: &egui::Context, shared: &mut Shared) -> Self {
        let _ = (ctx, shared);
        MainWindow {}
    }

    /// Draws the whole main viewport (the root `ui` of the eframe App).
    pub fn show(&mut self, ui: &mut egui::Ui, shared: &mut Shared) {
        let _ = shared;
        ui.label("Sylvania Launcher");
    }

    /// Reacts to events raised by other windows.
    pub fn on_event(&mut self, ev: &AppEvent, ctx: &egui::Context, shared: &mut Shared) {
        let _ = (ev, ctx, shared);
    }

    /// App is closing: persist stats / play time.
    pub fn on_exit(&mut self, shared: &mut Shared) {
        let _ = shared;
    }
}
