//! Month calendar drawn by hand (egui has no calendar widget in this build):
//! month navigation, Monday-first weekday header, decorated / selected / today
//! days. Used for the notes day filter and the reminder date picker.

use crate::theme::{self, GOLD, TEXT_MUTED};
use chrono::{Datelike, Duration, NaiveDate};
use eframe::egui::{
    self, Align2, Color32, CornerRadius, FontFamily, FontId, Rect, RichText, Sense, Stroke, StrokeKind, Ui, Vec2,
    pos2, vec2,
};

const FR_MONTHS: [&str; 12] = [
    "Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre",
    "Décembre",
];
const EN_MONTHS: [&str; 12] = [
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
    "December",
];
const FR_DAYS: [&str; 7] = ["Lu", "Ma", "Me", "Je", "Ve", "Sa", "Di"];
const EN_DAYS: [&str; 7] = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"];

const GRID_BG: Color32 = Color32::from_rgb(0x1f, 0x1f, 0x1f);
const GRID_LINE: Color32 = Color32::from_rgb(0x2e, 0x2e, 0x2e);
const DAY_TEXT: Color32 = Color32::from_rgb(0xe8, 0xe8, 0xe8);
const WEEKEND_TEXT: Color32 = Color32::from_rgb(0xe0, 0x70, 0x70);
const OTHER_MONTH_TEXT: Color32 = Color32::from_rgb(0x5a, 0x5a, 0x5a);
const HOVER_BG: Color32 = Color32::from_rgb(0x33, 0x33, 0x33);
const DARK_TEXT: Color32 = Color32::from_rgb(0x1a, 0x1a, 0x1a);

/// Decoration of one day (Qt: bold gold text for created notes, gold
/// background for reminders — the reminder look wins).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum DayMark {
    #[default]
    None,
    Created,
    Reminder,
}

/// The month currently displayed.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MonthView {
    pub year: i32,
    pub month: u32,
}

impl MonthView {
    pub fn of(day: NaiveDate) -> Self {
        MonthView { year: day.year(), month: day.month() }
    }

    fn first_day(self) -> NaiveDate {
        NaiveDate::from_ymd_opt(self.year, self.month, 1).unwrap_or_default()
    }

    fn shift(&mut self, delta: i32) {
        let idx = self.year * 12 + self.month as i32 - 1 + delta;
        self.year = idx.div_euclid(12);
        self.month = idx.rem_euclid(12) as u32 + 1;
    }

    /// The 42 days (6 weeks, Monday first) shown for this month.
    pub fn grid_days(self) -> impl Iterator<Item = NaiveDate> {
        let first = self.first_day();
        let start = first - Duration::days(i64::from(first.weekday().num_days_from_monday()));
        (0..42).map(move |i| start + Duration::days(i))
    }
}

/// "Novembre 2026" / "November 2026".
pub fn month_title(en: bool, view: MonthView) -> String {
    let names = if en { &EN_MONTHS } else { &FR_MONTHS };
    format!("{} {}", names[(view.month as usize - 1) % 12], view.year)
}

/// Draws a month calendar `7 × cell.x` wide. Returns the day clicked, if any
/// (clicking a day of an adjacent month also switches to that month).
pub fn month_calendar(
    ui: &mut Ui,
    en: bool,
    view: &mut MonthView,
    selected: Option<NaiveDate>,
    today: NaiveDate,
    cell: Vec2,
    mark: impl Fn(NaiveDate) -> DayMark,
) -> Option<NaiveDate> {
    let width = cell.x * 7.0;
    let base_id = ui.scope_id().with("month-calendar");

    // Header: ◀  Month Year  ▶
    let (header, _) = ui.allocate_exact_size(vec2(width, 28.0), Sense::hover());
    let arrow = vec2(28.0, 28.0);
    let prev = Rect::from_min_size(header.min, arrow);
    let next = Rect::from_min_size(pos2(header.max.x - arrow.x, header.min.y), arrow);
    let arrow_btn = |s: &str| egui::Button::new(RichText::new(s).color(GOLD).size(14.0)).frame(false);
    if ui.put(prev, arrow_btn("◀")).on_hover_cursor(egui::CursorIcon::PointingHand).clicked() {
        view.shift(-1);
    }
    if ui.put(next, arrow_btn("▶")).on_hover_cursor(egui::CursorIcon::PointingHand).clicked() {
        view.shift(1);
    }
    ui.painter().text(
        header.center(),
        Align2::CENTER_CENTER,
        month_title(en, *view),
        FontId::new(14.0, FontFamily::Name(theme::BOLD.into())),
        GOLD,
    );

    // Weekday header.
    let (days_row, _) = ui.allocate_exact_size(vec2(width, 20.0), Sense::hover());
    let names = if en { &EN_DAYS } else { &FR_DAYS };
    for (i, name) in names.iter().enumerate() {
        let center = pos2(days_row.min.x + (i as f32 + 0.5) * cell.x, days_row.center().y);
        let color = if i >= 5 { WEEKEND_TEXT.gamma_multiply(0.8) } else { TEXT_MUTED };
        ui.painter().text(center, Align2::CENTER_CENTER, *name, FontId::proportional(11.0), color);
    }

    // Day grid.
    let (grid, _) = ui.allocate_exact_size(vec2(width, cell.y * 6.0), Sense::hover());
    let painter = ui.painter().clone();
    painter.rect_filled(grid, CornerRadius::same(4), GRID_BG);
    let mut clicked = None;
    for (i, day) in view.grid_days().enumerate() {
        let (row, col) = (i / 7, i % 7);
        let rect = Rect::from_min_size(grid.min + vec2(col as f32 * cell.x, row as f32 * cell.y), cell);
        let resp = ui
            .interact(rect, base_id.with(i), Sense::click())
            .on_hover_cursor(egui::CursorIcon::PointingHand);
        let in_month = day.month() == view.month;
        let m = mark(day);

        let fill = match m {
            DayMark::Reminder => Some(GOLD),
            _ if resp.hovered() => Some(HOVER_BG),
            _ if selected == Some(day) => Some(Color32::from_rgb(0x4a, 0x42, 0x20)),
            _ => None,
        };
        if let Some(f) = fill {
            let f = if in_month { f } else { f.gamma_multiply(0.45) };
            painter.rect_filled(rect.shrink(1.0), CornerRadius::same(3), f);
        }
        painter.rect_stroke(rect, CornerRadius::ZERO, Stroke::new(1.0, GRID_LINE), StrokeKind::Inside);

        let mut color = match m {
            DayMark::Reminder => DARK_TEXT,
            DayMark::Created => GOLD,
            DayMark::None if col >= 5 => WEEKEND_TEXT,
            DayMark::None => DAY_TEXT,
        };
        if !in_month {
            color = if m == DayMark::None { OTHER_MONTH_TEXT } else { color.gamma_multiply(0.55) };
        }
        let font = if m == DayMark::None {
            FontId::proportional(12.0)
        } else {
            FontId::new(12.5, FontFamily::Name(theme::BOLD.into()))
        };
        painter.text(rect.center(), Align2::CENTER_CENTER, day.day().to_string(), font, color);

        if selected == Some(day) {
            painter.rect_stroke(rect.shrink(1.0), CornerRadius::same(3), Stroke::new(2.0, Color32::WHITE), StrokeKind::Inside);
        } else if day == today {
            let c = if m == DayMark::Reminder { DARK_TEXT } else { GOLD };
            painter.rect_stroke(rect.shrink(2.0), CornerRadius::same(3), Stroke::new(1.0, c), StrokeKind::Inside);
        }

        if resp.clicked() {
            clicked = Some(day);
            if !in_month {
                *view = MonthView::of(day);
            }
        }
    }
    clicked
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn grid_starts_on_monday_and_navigation_wraps_years() {
        let mut v = MonthView { year: 2026, month: 11 };
        let days: Vec<_> = v.grid_days().collect();
        assert_eq!(days.len(), 42);
        // 1 Nov 2026 is a Sunday → grid starts Monday 26 Oct.
        assert_eq!(days[0], NaiveDate::from_ymd_opt(2026, 10, 26).unwrap());
        assert_eq!(days[6], NaiveDate::from_ymd_opt(2026, 11, 1).unwrap());
        v.shift(2);
        assert_eq!(v, MonthView { year: 2027, month: 1 });
        v.shift(-13);
        assert_eq!(v, MonthView { year: 2025, month: 12 });
        assert_eq!(month_title(false, v), "Décembre 2025");
        assert_eq!(month_title(true, v), "December 2025");
    }
}
