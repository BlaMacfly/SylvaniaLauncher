//! Post-it notes window (port of the Qt `NotesWindow`): calendar pane on the
//! left (days with a note or a reminder are decorated, clicking a day filters
//! the grid), coloured post-it grid on the right, view / edit dialogs with a
//! per-note reminder (date + time).

mod calendar;

use crate::app::{self, AppEvent, Shared, WindowKind};
use crate::constants::{SPACE_LG, SPACE_MD, SPACE_SM, SPACE_XS};
use crate::core::notes::{Note, NotesStore, left_chars, now_local};
use crate::theme::{self, GOLD, TEXT_MUTED, ThemeTokens};
use crate::ui::widgets::{self, Level};
use crate::util::msg;
use calendar::{DayMark, MonthView, month_calendar};
use chrono::{Duration, Local, NaiveDate, NaiveDateTime, Timelike};
use eframe::egui::{
    self, Align, Align2, Color32, CornerRadius, FontFamily, FontId, Layout, Rect, RichText, Sense,
    Stroke, StrokeKind, Ui, UiBuilder, Vec2, pos2, vec2,
};
use std::collections::HashMap;
use std::sync::Arc;

/// Post-it colours (stored in the note's `category`, like the Qt version).
const COLORS: [&str; 6] = [
    "#FFE066", // Yellow
    "#FF9F80", // Orange/Coral
    "#80D4FF", // Light Blue
    "#98FB98", // Pale Green
    "#DDA0DD", // Plum/Purple
    "#FFB6C1", // Light Pink
];

const WINDOW_SIZE: [f32; 2] = [1100.0, 680.0];
const WINDOW_MIN_SIZE: [f32; 2] = [1000.0, 620.0];
const CALENDAR_PANE_W: f32 = 300.0;
const CARD_MIN_W: f32 = 230.0;
const CARD_H: f32 = 230.0;
const EDITOR_W: f32 = 420.0;
const VIEWER_W: f32 = 500.0;

const TEXT_DARK: Color32 = Color32::from_rgb(0x33, 0x33, 0x33);
const OVERDUE: Color32 = Color32::from_rgb(0xb0, 0x00, 0x20);
const DUE: Color32 = Color32::from_rgb(0x55, 0x55, 0x55);
const BORDER_BROWN: Color32 = Color32::from_rgb(0x5a, 0x4a, 0x2d);

/// The colour a note is drawn with (unknown category → yellow).
fn post_it_color(note: &Note) -> &'static str {
    COLORS
        .iter()
        .copied()
        .find(|c| *c == note.category)
        .unwrap_or(COLORS[0])
}

/// egui's fonts have no glyph for the emoji variation selector (U+FE0F).
fn clean(s: String) -> String {
    if s.contains('\u{FE0F}') {
        s.replace('\u{FE0F}', "")
    } else {
        s
    }
}

fn bold_font(size: f32) -> FontId {
    FontId::new(size, FontFamily::Name(theme::BOLD.into()))
}

fn layout_text(
    ui: &Ui,
    text: String,
    font: FontId,
    color: Color32,
    width: f32,
    max_rows: usize,
) -> Arc<egui::Galley> {
    let mut job =
        egui::text::LayoutJob::single_section(text, egui::TextFormat::simple(font, color));
    job.wrap = egui::text::TextWrapping {
        max_width: width,
        max_rows,
        break_anywhere: false,
        overflow_character: Some('…'),
    };
    ui.painter().layout_job(job)
}

/// Default due date for a new reminder: in one hour, on the minute.
fn default_due() -> NaiveDateTime {
    let t = now_local() + Duration::hours(1);
    t.with_second(0).unwrap_or(t)
}

/// Work that needs a blocking native dialog: run after the viewport is drawn.
enum Deferred {
    ConfirmDelete(String),
    EmptyNoteWarning,
    Import,
    Export,
}

enum CardAction {
    View(Note),
    Edit(Note),
    Delete(String),
}

/// State of the add / edit dialog.
struct Editor {
    /// `None` = new note.
    id: Option<String>,
    title: String,
    content: String,
    color: String,
    reminder: bool,
    due: NaiveDateTime,
    picker_month: MonthView,
    /// Focus the title field on the first frame.
    focus_title: bool,
}

impl Editor {
    fn new_note() -> Self {
        let due = default_due();
        Editor {
            id: None,
            title: String::new(),
            content: String::new(),
            color: COLORS[0].to_string(),
            reminder: false,
            due,
            picker_month: MonthView::of(due.date()),
            focus_title: true,
        }
    }

    fn edit(note: &Note) -> Self {
        let due = note.due_at.unwrap_or_else(default_due);
        Editor {
            id: Some(note.id.clone()),
            title: note.title.clone(),
            content: note.content.clone(),
            // Legacy notes keep their category unless a colour is picked.
            color: if note.category.is_empty() {
                COLORS[0].to_string()
            } else {
                note.category.clone()
            },
            reminder: note.has_reminder(),
            due,
            picker_month: MonthView::of(due.date()),
            focus_title: true,
        }
    }
}

pub struct NotesWindow {
    /// Day filter (`None` = show everything).
    filter: Option<NaiveDate>,
    month: MonthView,
    /// Store revision the caches below were built from (`None` = stale).
    cache_rev: Option<u64>,
    visible: Vec<Note>,
    marks: HashMap<NaiveDate, DayMark>,
    editor: Option<Editor>,
    viewer: Option<Note>,
    deferred: Vec<Deferred>,
}

impl NotesWindow {
    pub fn new(shared: &Shared) -> Self {
        let mut w = NotesWindow {
            filter: None,
            month: MonthView::of(Local::now().date_naive()),
            cache_rev: None,
            visible: Vec::new(),
            marks: HashMap::new(),
            editor: None,
            viewer: None,
            deferred: Vec::new(),
        };
        w.refresh(&shared.notes);
        w
    }

    /// Renders the window (via `ui::child_window`). Returns false once closed.
    pub fn show(&mut self, ctx: &egui::Context, shared: &mut Shared) -> bool {
        self.refresh(&shared.notes);
        let title = clean(shared.t("📝 Notes - Sylvania Launcher"));
        let open = super::child_window(
            ctx,
            app::window_id(WindowKind::Notes),
            &title,
            WINDOW_SIZE,
            WINDOW_MIN_SIZE,
            |ui| self.contents(ui, shared),
        );
        if !self.deferred.is_empty() {
            self.run_deferred(shared);
            ctx.request_repaint();
        }
        open
    }

    /// Reacts to app events (e.g. AppEvent::NotesChanged → reload).
    pub fn on_event(&mut self, ev: &AppEvent, shared: &mut Shared) {
        if matches!(ev, AppEvent::NotesChanged) {
            self.cache_rev = None;
            self.refresh(&shared.notes);
        }
    }

    /// Rebuilds the visible list and the calendar marks when the store changed.
    fn refresh(&mut self, store: &NotesStore) {
        if self.cache_rev == Some(store.revision()) {
            return;
        }
        self.cache_rev = Some(store.revision());
        let notes = match self.filter {
            Some(day) => store.notes_on_day(day),
            None => store.get_all_notes(false),
        };
        self.visible = notes.into_iter().cloned().collect();

        // Created-note days first, reminder days override (stronger highlight).
        self.marks.clear();
        for note in store.get_all_notes(false) {
            if let Some(created) = note.created_at {
                self.marks.entry(created.date()).or_insert(DayMark::Created);
            }
        }
        for note in store.notes_with_reminders() {
            if let Some(due) = note.due_at {
                self.marks.insert(due.date(), DayMark::Reminder);
            }
        }
        if let Some(v) = &self.viewer {
            self.viewer = store.get_note_by_id(&v.id).cloned();
        }
    }

    fn set_filter(&mut self, filter: Option<NaiveDate>, store: &NotesStore) {
        self.filter = filter;
        self.cache_rev = None;
        self.refresh(store);
    }

    fn contents(&mut self, ui: &mut Ui, shared: &mut Shared) {
        let full = ui.available_rect_before_wrap();
        let left = Rect::from_min_size(full.min, vec2(CALENDAR_PANE_W, full.height()));
        let right = Rect::from_min_max(pos2(left.right() + SPACE_MD, full.top()), full.max);
        ui.scope_builder(UiBuilder::new().max_rect(left), |ui| {
            self.calendar_pane(ui, shared)
        });
        ui.scope_builder(UiBuilder::new().max_rect(right), |ui| {
            self.notes_pane(ui, shared)
        });

        let ctx = ui.ctx().clone();
        self.editor_dialog(&ctx, shared);
        self.viewer_dialog(&ctx, shared);
    }

    // --- Left pane: calendar + day filter -------------------------------------

    fn calendar_pane(&mut self, ui: &mut Ui, shared: &mut Shared) {
        ui.label(theme::bold(clean(shared.t("📅 Calendrier")), 16.0).color(GOLD));

        let today = Local::now().date_naive();
        // Frame margins + 1 px stroke on each side.
        let inner_w = CALENDAR_PANE_W - 2.0 * SPACE_SM - 2.0;
        let mut picked = None;
        egui::Frame::new()
            .fill(Color32::from_rgb(0x24, 0x24, 0x24))
            .corner_radius(CornerRadius::same(8))
            .stroke(Stroke::new(1.0, BORDER_BROWN))
            .inner_margin(SPACE_SM)
            .show(ui, |ui| {
                let marks = &self.marks;
                picked = month_calendar(
                    ui,
                    shared.i18n.is_en(),
                    &mut self.month,
                    self.filter,
                    today,
                    vec2(inner_w / 7.0, 34.0),
                    |d| marks.get(&d).copied().unwrap_or_default(),
                );
            });
        if let Some(day) = picked {
            self.set_filter(Some(day), &shared.notes);
        }

        let info = match self.filter {
            Some(day) => shared.tf(
                "%1 : %2 note(s)",
                &[&day.format("%d/%m/%Y"), &self.visible.len()],
            ),
            None => shared.t("Cliquez sur un jour pour filtrer."),
        };
        ui.add(egui::Label::new(RichText::new(info).color(TEXT_MUTED).size(12.0)).wrap());

        let show_all = shared.t("Afficher toutes les notes");
        let tokens = ThemeTokens::default();
        if widgets::level_button(
            ui,
            &show_all,
            Level::Secondary,
            &tokens,
            self.filter.is_some(),
            CALENDAR_PANE_W,
        )
        .clicked()
        {
            self.set_filter(None, &shared.notes);
        }
    }

    // --- Right pane: header + post-it grid ------------------------------------

    fn notes_pane(&mut self, ui: &mut Ui, shared: &mut Shared) {
        ui.horizontal(|ui| {
            ui.label(theme::bold(clean(shared.t("📌 Mes Notes")), 24.0).color(GOLD));
            let n = self.visible.len();
            let count = if self.filter.is_some() {
                shared.tf("%1 notes (filtrées)", &[&n])
            } else {
                shared.tf("%1 notes", &[&n])
            };
            ui.label(RichText::new(count).color(TEXT_MUTED).size(14.0));

            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                let tokens = ThemeTokens::default();
                let add = shared.t("+ Nouvelle Note");
                if widgets::level_button(ui, &add, Level::Primary, &tokens, true, 150.0).clicked() {
                    self.editor = Some(Editor::new_note());
                }
                if widgets::dialog_button(ui, &clean(shared.t("📤 Exporter"))).clicked() {
                    self.deferred.push(Deferred::Export);
                }
                if widgets::dialog_button(ui, &clean(shared.t("📥 Importer"))).clicked() {
                    self.deferred.push(Deferred::Import);
                }
            });
        });
        ui.add_space(SPACE_SM);

        egui::ScrollArea::vertical()
            .auto_shrink([false, false])
            .show(ui, |ui| {
                ui.add_space(SPACE_XS);
                let gap = SPACE_MD;
                // Leave room for the post-it shadows on the right.
                let width = ui.available_width() - SPACE_SM;
                let cols = (((width + gap) / (CARD_MIN_W + gap)).floor() as usize).max(1);
                let card_w = ((width - gap * (cols as f32 - 1.0)) / cols as f32).floor();
                let now = now_local();
                let mut action = None;
                for row in self.visible.chunks(cols) {
                    ui.horizontal(|ui| {
                        ui.add_space(SPACE_XS);
                        ui.spacing_mut().item_spacing.x = gap;
                        for note in row {
                            if let Some(a) = post_it(ui, note, vec2(card_w, CARD_H), shared, now) {
                                action = Some(a);
                            }
                        }
                    });
                    ui.add_space(gap - ui.spacing().item_spacing.y);
                }
                match action {
                    Some(CardAction::View(note)) => self.viewer = Some(note),
                    Some(CardAction::Edit(note)) => self.editor = Some(Editor::edit(&note)),
                    Some(CardAction::Delete(id)) => self.deferred.push(Deferred::ConfirmDelete(id)),
                    None => {}
                }
            });
    }

    // --- Add / edit dialog ------------------------------------------------------

    fn editor_dialog(&mut self, ctx: &egui::Context, shared: &mut Shared) {
        let Some(ed) = self.editor.as_mut() else {
            return;
        };
        let mut save = false;
        let mut cancel = false;
        let en = shared.i18n.is_en();
        let label = |s: &str| RichText::new(shared.t(s)).color(GOLD).size(13.0);

        let modal = egui::Modal::new(egui::Id::unique("sylvania-notes-editor"))
            .frame(
                egui::Frame::new()
                    .fill(theme::WINDOW_BG)
                    .stroke(Stroke::new(1.0, BORDER_BROWN))
                    .corner_radius(CornerRadius::same(10))
                    .inner_margin(SPACE_LG),
            )
            .show(ctx, |ui| {
                ui.set_width(EDITOR_W);
                let heading = if ed.id.is_none() {
                    "Nouvelle Note"
                } else {
                    "Modifier la Note"
                };
                ui.label(theme::bold(shared.t(heading), 18.0).color(GOLD));
                ui.add_space(SPACE_XS);

                ui.label(label("Titre:"));
                let title_edit = ui.add(
                    egui::TextEdit::singleline(&mut ed.title)
                        .hint_text(shared.t("Titre de la note..."))
                        .desired_width(f32::INFINITY)
                        .margin(vec2(10.0, 8.0)),
                );
                if std::mem::take(&mut ed.focus_title) {
                    title_edit.request_focus();
                }

                ui.label(label("Contenu:"));
                egui::ScrollArea::vertical()
                    .id_salt("note-content")
                    .max_height(190.0)
                    .show(ui, |ui| {
                        ui.add(
                            egui::TextEdit::multiline(&mut ed.content)
                                .hint_text(shared.t("Écrivez votre note ici..."))
                                .desired_rows(8)
                                .desired_width(f32::INFINITY)
                                .margin(vec2(10.0, 8.0)),
                        );
                    });

                // Reminder: checkbox + date picker + hour/minute.
                ui.horizontal(|ui| {
                    ui.checkbox(&mut ed.reminder, label("Rappel:"));
                    ui.add_enabled_ui(ed.reminder, |ui| {
                        let date_btn = ui.button(
                            RichText::new(format!("📅 {}", ed.due.format("%d/%m/%Y"))).size(13.0),
                        );
                        if date_btn.clicked() {
                            ed.picker_month = MonthView::of(ed.due.date());
                        }
                        egui::Popup::from_toggle_button_response(&date_btn)
                            .close_behavior(egui::PopupCloseBehavior::CloseOnClickOutside)
                            .show(|ui| {
                                let today = Local::now().date_naive();
                                let picked = month_calendar(
                                    ui,
                                    en,
                                    &mut ed.picker_month,
                                    Some(ed.due.date()),
                                    today,
                                    vec2(34.0, 28.0),
                                    |_| DayMark::None,
                                );
                                if let Some(day) = picked {
                                    ed.due = day.and_time(ed.due.time());
                                    ui.close();
                                }
                            });

                        let (mut h, mut m) = (ed.due.hour(), ed.due.minute());
                        let two_digits = |v: f64, _| format!("{:02}", v as u32);
                        let dh = ui.add(
                            egui::DragValue::new(&mut h)
                                .range(0..=23)
                                .speed(0.1)
                                .custom_formatter(two_digits),
                        );
                        ui.label(RichText::new(":").size(13.0));
                        let dm = ui.add(
                            egui::DragValue::new(&mut m)
                                .range(0..=59)
                                .speed(0.2)
                                .custom_formatter(two_digits),
                        );
                        if (dh.changed() || dm.changed())
                            && let Some(t) = ed.due.date().and_hms_opt(h, m, ed.due.second())
                        {
                            ed.due = t;
                        }
                    });
                });

                ui.label(label("Couleur:"));
                ui.horizontal(|ui| {
                    for color in COLORS {
                        let (rect, resp) =
                            ui.allocate_exact_size(Vec2::splat(35.0), Sense::click());
                        let resp = resp.on_hover_cursor(egui::CursorIcon::PointingHand);
                        let selected = ed.color == color;
                        let stroke = if selected || resp.hovered() {
                            Stroke::new(3.0, GOLD)
                        } else {
                            Stroke::new(2.0, Color32::from_rgb(0x55, 0x55, 0x55))
                        };
                        ui.painter()
                            .circle(rect.center(), 16.0, theme::hex(color), stroke);
                        if resp.clicked() {
                            ed.color = color.to_string();
                        }
                    }
                });
                ui.add_space(SPACE_SM);

                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    if widgets::dialog_button(ui, &shared.t("Sauvegarder")).clicked() {
                        save = true;
                    }
                    let cancel_btn = egui::Button::new(
                        RichText::new(shared.t("Annuler"))
                            .color(Color32::WHITE)
                            .size(13.0),
                    )
                    .fill(Color32::from_rgb(0x3a, 0x3a, 0x3a))
                    .stroke(Stroke::new(1.0, Color32::from_rgb(0x5a, 0x5a, 0x5a)))
                    .corner_radius(CornerRadius::same(5))
                    .min_size(vec2(100.0, 36.0));
                    if ui.add(cancel_btn).clicked() {
                        cancel = true;
                    }
                });
            });

        // Escape cancels; a click outside is ignored (no accidental data loss).
        if modal.should_close() && !modal.backdrop_response.clicked() {
            cancel = true;
        }
        if save {
            self.save_editor(shared);
        } else if cancel {
            self.editor = None;
        }
    }

    fn save_editor(&mut self, shared: &mut Shared) {
        let Some(ed) = &self.editor else { return };
        let title = ed.title.trim();
        let content = ed.content.trim();
        if title.is_empty() && content.is_empty() {
            self.deferred.push(Deferred::EmptyNoteWarning);
            return;
        }
        let title = if title.is_empty() {
            let mut t = left_chars(content, 30).to_string();
            if content.chars().count() > 30 {
                t.push_str("...");
            }
            t
        } else {
            title.to_string()
        };
        let due = ed.reminder.then_some(ed.due);
        let notes = &mut shared.notes;
        match &ed.id {
            None => {
                notes.create_note(&title, content, &ed.color, &[], due);
            }
            Some(id) => {
                notes.update_note(id, Some(&title), Some(content), Some(&ed.color), &[]);
                // Only touch the reminder when it actually changed, so an
                // unrelated edit never re-arms an already-notified reminder.
                if notes.get_note_by_id(id).is_some_and(|n| n.due_at != due) {
                    notes.set_note_due_date(id, due);
                }
            }
        }
        self.editor = None;
        self.refresh(&shared.notes);
    }

    // --- Read-only view dialog --------------------------------------------------

    fn viewer_dialog(&mut self, ctx: &egui::Context, shared: &mut Shared) {
        if self.editor.is_some() {
            return;
        }
        let Some(note) = &self.viewer else { return };
        let mut close = false;
        let mut edit = false;
        let dark_button = |text: String, fill: Color32| {
            egui::Button::new(RichText::new(clean(text)).color(Color32::WHITE).size(12.0))
                .fill(fill)
                .corner_radius(CornerRadius::same(5))
                .min_size(vec2(90.0, 35.0))
        };

        let modal = egui::Modal::new(egui::Id::unique("sylvania-notes-viewer"))
            .frame(
                egui::Frame::new()
                    .fill(theme::hex(post_it_color(note)))
                    .corner_radius(CornerRadius::same(8))
                    .inner_margin(SPACE_LG),
            )
            .show(ctx, |ui| {
                ui.set_width(VIEWER_W);
                ui.add(egui::Label::new(theme::bold(&note.title, 22.0).color(TEXT_DARK)).wrap());
                if let Some(due) = note.due_at {
                    let text = clean(shared.tf("⏰ Rappel: %1", &[&due.format("%d/%m/%Y %H:%M")]));
                    ui.label(theme::bold(text, 12.0).color(TEXT_DARK));
                }
                let (line, _) =
                    ui.allocate_exact_size(vec2(ui.available_width(), 2.0), Sense::hover());
                ui.painter()
                    .rect_filled(line, CornerRadius::ZERO, TEXT_DARK);

                egui::ScrollArea::vertical()
                    .id_salt("note-view")
                    .min_scrolled_height(260.0)
                    .max_height(380.0)
                    .auto_shrink([false, true])
                    .show(ui, |ui| {
                        ui.add(
                            egui::Label::new(
                                RichText::new(&note.content).color(TEXT_DARK).size(14.0),
                            )
                            .wrap(),
                        );
                    });

                ui.add_space(SPACE_SM);
                ui.horizontal(|ui| {
                    let created = note
                        .created_at
                        .map(|c| c.format("%d/%m/%Y %H:%M").to_string())
                        .unwrap_or_default();
                    ui.label(
                        RichText::new(shared.tf("Créée le: %1", &[&created]))
                            .color(TEXT_DARK)
                            .size(11.0),
                    );
                    ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                        if ui
                            .add(dark_button(
                                shared.t("Fermer"),
                                Color32::from_rgba_unmultiplied(60, 60, 60, 200),
                            ))
                            .clicked()
                        {
                            close = true;
                        }
                        if ui
                            .add(dark_button(
                                shared.t("✏️ Modifier"),
                                Color32::from_rgba_unmultiplied(80, 80, 80, 200),
                            ))
                            .clicked()
                        {
                            edit = true;
                        }
                    });
                });
            });

        if edit {
            self.editor = Some(Editor::edit(note));
            self.viewer = None;
        } else if close || modal.should_close() {
            self.viewer = None;
        }
    }

    // --- Blocking native dialogs --------------------------------------------------

    fn run_deferred(&mut self, shared: &mut Shared) {
        for job in std::mem::take(&mut self.deferred) {
            match job {
                Deferred::ConfirmDelete(id) => {
                    if msg::confirm(&shared.t("Supprimer"), &shared.t("Supprimer cette note ?")) {
                        shared.notes.delete_note(&id);
                    }
                }
                Deferred::EmptyNoteWarning => {
                    msg::warning(
                        &shared.t("Erreur"),
                        &shared.t("La note ne peut pas être vide."),
                    );
                }
                Deferred::Export => {
                    let name =
                        format!("notes_export_{}.json", Local::now().format("%Y%m%d_%H%M%S"));
                    let Some(path) = rfd::FileDialog::new()
                        .set_title(shared.t("Exporter les notes"))
                        .set_file_name(name)
                        .add_filter("JSON", &["json"])
                        .save_file()
                    else {
                        continue;
                    };
                    match shared.notes.export_notes(Some(&path)) {
                        Some(p) => msg::info(
                            &shared.t("Succès"),
                            &shared.tf("Notes exportées vers :\n%1", &[&p.display()]),
                        ),
                        None => msg::error(
                            &shared.t("Erreur"),
                            &shared.t("Impossible d'exporter les notes."),
                        ),
                    }
                }
                Deferred::Import => {
                    let Some(path) = rfd::FileDialog::new()
                        .set_title(shared.t("Importer des notes"))
                        .add_filter("JSON", &["json"])
                        .pick_file()
                    else {
                        continue;
                    };
                    if shared.notes.import_notes(&path, true) {
                        msg::info(
                            &shared.t("Succès"),
                            &shared.t("Notes importées avec succès."),
                        );
                    } else {
                        msg::error(
                            &shared.t("Erreur"),
                            &shared.t("Import impossible : fichier invalide ou trop volumineux (10 Mo max)."),
                        );
                    }
                }
            }
        }
        self.refresh(&shared.notes);
    }
}

/// Draws one post-it and returns what the user clicked on it.
fn post_it(
    ui: &mut Ui,
    note: &Note,
    size: Vec2,
    shared: &Shared,
    now: NaiveDateTime,
) -> Option<CardAction> {
    let (rect, card) = ui.allocate_exact_size(size, Sense::click());
    let card = card.on_hover_cursor(egui::CursorIcon::PointingHand);
    let painter = ui.painter().clone();
    let radius = CornerRadius::same(5);

    let shadow = egui::epaint::Shadow {
        offset: [3, 3],
        blur: 15,
        spread: 0,
        color: Color32::from_black_alpha(100),
    };
    painter.add(shadow.as_shape(rect, radius));
    painter.rect_filled(rect, radius, theme::hex(post_it_color(note)));
    if card.contains_pointer() {
        painter.rect_stroke(rect, radius, Stroke::new(2.0, GOLD), StrokeKind::Inside);
    }

    let inner = rect.shrink2(vec2(SPACE_MD, SPACE_SM + 2.0));

    // Delete button (top-right ×).
    let del_rect = Rect::from_min_size(pos2(inner.right() - 25.0, inner.top()), Vec2::splat(25.0));
    let del = ui
        .interact(
            del_rect,
            ui.scope_id().with(("note-delete", &note.id)),
            Sense::click(),
        )
        .on_hover_cursor(egui::CursorIcon::PointingHand)
        .on_hover_text(shared.t("Supprimer cette note"));
    let del_fill = if del.hovered() {
        Color32::from_rgba_unmultiplied(255, 50, 50, 220)
    } else {
        Color32::from_rgba_unmultiplied(200, 50, 50, 180)
    };
    painter.circle_filled(del_rect.center(), 12.5, del_fill);
    painter.text(
        del_rect.center(),
        Align2::CENTER_CENTER,
        "×",
        bold_font(16.0),
        Color32::WHITE,
    );

    // Title (2 lines max).
    let title = if note.title.is_empty() {
        shared.t("Sans titre")
    } else {
        note.title.clone()
    };
    let title_g = layout_text(
        ui,
        title,
        bold_font(14.0),
        TEXT_DARK,
        inner.width() - 25.0 - SPACE_XS,
        2,
    );
    let mut y = inner.top() + title_g.size().y.max(25.0) + SPACE_XS;
    painter.galley(inner.min, title_g, TEXT_DARK);

    // Reminder line.
    if let Some(due) = note.due_at {
        let overdue = !note.reminder_done && due < now;
        let text = shared.tf("⏰ %1", &[&due.format("%d/%m/%Y %H:%M")]);
        let g = layout_text(
            ui,
            text,
            bold_font(11.0),
            if overdue { OVERDUE } else { DUE },
            inner.width(),
            1,
        );
        let h = g.size().y;
        painter.galley(pos2(inner.left(), y), g, TEXT_DARK);
        y += h + SPACE_XS;
    }

    // Bottom row: character count + edit button.
    let edit_g = layout_text(
        ui,
        clean(shared.t("✏️ Modifier")),
        FontId::proportional(11.0),
        Color32::WHITE,
        f32::INFINITY,
        1,
    );
    let edit_size = vec2(edit_g.size().x + 20.0, 28.0);
    let edit_rect = Rect::from_min_size(
        pos2(inner.right() - edit_size.x, inner.bottom() - edit_size.y),
        edit_size,
    );
    let edit = ui
        .interact(
            edit_rect,
            ui.scope_id().with(("note-edit", &note.id)),
            Sense::click(),
        )
        .on_hover_cursor(egui::CursorIcon::PointingHand);
    let edit_fill = if edit.hovered() {
        Color32::from_rgba_unmultiplied(120, 120, 120, 220)
    } else {
        Color32::from_rgba_unmultiplied(100, 100, 100, 180)
    };
    painter.rect_filled(edit_rect, radius, edit_fill);
    painter.galley(
        edit_rect.center() - edit_g.size() / 2.0,
        edit_g,
        Color32::WHITE,
    );

    let count = shared.tf("%1 car.", &[&note.content.chars().count()]);
    painter.text(
        pos2(inner.left(), edit_rect.center().y),
        Align2::LEFT_CENTER,
        count,
        FontId::proportional(10.0),
        TEXT_DARK,
    );

    // Content preview, elided to the space left.
    let mut preview = left_chars(&note.content, 250).to_string();
    if note.content.chars().count() > 250 {
        preview.push_str("...");
    }
    let font = FontId::proportional(12.0);
    let row_h = ui.fonts_mut(|f| f.row_height(&font));
    let room = edit_rect.top() - SPACE_XS - y;
    let rows = (room / row_h).floor().max(0.0) as usize;
    if rows > 0 && !preview.is_empty() {
        let g = layout_text(ui, preview, font, TEXT_DARK, inner.width(), rows);
        painter.galley(pos2(inner.left(), y), g, TEXT_DARK);
    }

    if del.clicked() {
        Some(CardAction::Delete(note.id.clone()))
    } else if edit.clicked() {
        Some(CardAction::Edit(note.clone()))
    } else if card.clicked() {
        Some(CardAction::View(note.clone()))
    } else {
        None
    }
}
