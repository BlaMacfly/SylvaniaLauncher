//! STUB — implemented by its port. Contract: keep `new`, `show`
//! (and `on_event` for the notes window) with these signatures.

use crate::app::Shared;
use eframe::egui;

pub struct AddonsWindow {}

impl AddonsWindow {
    pub fn new(shared: &Shared) -> Self {
        let _ = shared;
        AddonsWindow {}
    }

    /// Renders the window (via `ui::child_window`). Returns false once closed.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> bool {
        let _ = (ctx, shared);
        false
    }
}
