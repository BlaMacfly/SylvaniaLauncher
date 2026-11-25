//! Reminder service (port of the Qt `ReminderService`): polls the notes for
//! due reminders every [`REMINDER_CHECK_MS`], fires a Windows toast + the
//! notification sound for each, then marks it done so it never fires twice.
//! Reminders that came due while the launcher was closed are reported once,
//! on the first tick, grouped in a single "missed" notification.

use crate::app::{AppEvent, Shared};
use crate::constants::REMINDER_CHECK_MS;
use crate::core::notes::{Note, NotesStore, left_chars, now_local};
use crate::i18n::I18n;
use chrono::NaiveDateTime;
use std::time::{Duration, Instant};

pub struct ReminderService {
    /// `None` until the first tick (startup "missed reminders" report).
    last_check: Option<Instant>,
}

impl Default for ReminderService {
    fn default() -> Self {
        Self::new()
    }
}

impl ReminderService {
    pub fn new() -> Self {
        ReminderService { last_check: None }
    }

    /// Called every frame by the App; rate-limits itself internally
    /// (constants::REMINDER_CHECK_MS) and reports missed reminders once at startup.
    pub fn tick(&mut self, shared: &mut Shared) {
        let notifications = match self.last_check {
            None => {
                self.last_check = Some(Instant::now());
                missed_notification(&shared.i18n, &take_due(&mut shared.notes, now_local()))
                    .into_iter()
                    .collect()
            }
            Some(last) if last.elapsed() >= Duration::from_millis(REMINDER_CHECK_MS) => {
                self.last_check = Some(Instant::now());
                take_due(&mut shared.notes, now_local())
                    .iter()
                    .map(|n| due_notification(&shared.i18n, n))
                    .collect()
            }
            Some(_) => Vec::new(),
        };
        if notifications.is_empty() {
            return;
        }
        for (title, body) in &notifications {
            crate::notify::toast(title, body);
            shared.sounds.play("notification");
        }
        // Lets an open notes window refresh (reminder_done changed).
        shared.emit(AppEvent::NotesChanged);
    }
}

/// Returns the reminders due at `now` and marks them done.
fn take_due(store: &mut NotesStore, now: NaiveDateTime) -> Vec<Note> {
    let due = store.due_reminders(now);
    if !due.is_empty() {
        let ids: Vec<String> = due.iter().map(|n| n.id.clone()).collect();
        store.mark_reminders_done(&ids);
    }
    due
}

/// Toast for one reminder: "Rappel : <title>" + the first 200 characters.
fn due_notification(i18n: &I18n, note: &Note) -> (String, String) {
    (
        i18n.tf("Rappel : %1", &[&note.title]),
        left_chars(&note.content, 200).to_string(),
    )
}

/// One grouped toast for the reminders missed while the launcher was closed.
fn missed_notification(i18n: &I18n, missed: &[Note]) -> Option<(String, String)> {
    if missed.is_empty() {
        return None;
    }
    let lines: Vec<String> = missed
        .iter()
        .map(|n| {
            let when = n
                .due_at
                .map(|d| d.format("%d/%m %H:%M").to_string())
                .unwrap_or_default();
            format!("• {} ({when})", n.title)
        })
        .collect();
    Some((
        i18n.tf("Rappels manqués (%1)", &[&missed.len()]),
        i18n.tf(
            "Échéances passées pendant que le launcher était fermé :\n%1",
            &[&lines.join("\n")],
        ),
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn dt(s: &str) -> NaiveDateTime {
        NaiveDateTime::parse_from_str(s, "%Y-%m-%d %H:%M:%S").unwrap()
    }

    #[test]
    fn missed_reminders_are_grouped_and_marked_done() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(
            dir.path().join("notes.json"),
            r#"[{"id":"1","title":"Raid","due_at":"2026-11-20T20:30:00"},
                {"id":"2","title":"Foire","due_at":"2026-11-21T08:05:00"},
                {"id":"3","title":"Plus tard","due_at":"2099-01-01T00:00:00"}]"#,
        )
        .unwrap();
        let mut store = NotesStore::open(dir.path().to_path_buf());
        let now = dt("2026-11-25 12:00:00");
        let missed = take_due(&mut store, now);
        let (title, body) = missed_notification(&I18n::new("fr"), &missed).unwrap();
        assert_eq!(title, "Rappels manqués (2)");
        assert_eq!(
            body,
            "Échéances passées pendant que le launcher était fermé :\n• Raid (20/11 20:30)\n• Foire (21/11 08:05)"
        );
        // Persisted: never fires again, even after a restart.
        assert!(take_due(&mut store, now).is_empty());
        let mut reloaded = NotesStore::open(dir.path().to_path_buf());
        assert!(take_due(&mut reloaded, now).is_empty());
        assert_eq!(missed_notification(&I18n::new("fr"), &[]), None);
    }

    #[test]
    fn due_notification_truncates_content() {
        let note = Note {
            title: "T".into(),
            content: "é".repeat(300),
            ..Default::default()
        };
        let (title, body) = due_notification(&I18n::new("en"), &note);
        assert_eq!(title, "Reminder: T");
        assert_eq!(body.chars().count(), 200);
    }
}
