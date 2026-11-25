//! Reminder service: polls due reminders, fires toasts + sound.
//! STUB — implemented by the notes port.

use crate::app::Shared;

pub struct ReminderService {}

impl ReminderService {
    pub fn new() -> Self {
        ReminderService {}
    }

    /// Called every frame by the App; rate-limits itself internally
    /// (constants::REMINDER_CHECK_MS) and reports missed reminders once at startup.
    pub fn tick(&mut self, _shared: &mut Shared) {}
}
