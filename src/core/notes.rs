//! Notes storage (`data/notes.json` + `data/note_categories.json`), ported
//! from the Qt `NotesManager`.
//!
//! The on-disk format is byte-compatible with what the Qt launcher wrote:
//! compact JSON arrays, object keys in alphabetical order (QJsonObject sorts
//! them), dates as `Qt::ISODate` (`yyyy-MM-ddTHH:mm:ss`, local time, no
//! offset), invalid dates as `""`. v3.0 reminder keys (`due_at`,
//! `reminder_done`) are only written when a note carries a reminder.
//!
//! Like the Qt version, a note's post-it colour is stored in `category`.

use crate::constants::MAX_IMPORT_BYTES;
use crate::paths;
use chrono::{Local, NaiveDate, NaiveDateTime, NaiveTime, TimeZone, Timelike, Utc};
use serde_json::{Map, Value};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

const NOTES_FILE: &str = "notes.json";
const CATEGORIES_FILE: &str = "note_categories.json";
const DEFAULT_CATEGORY_COLOR: &str = "#4CAF50";

/// `Qt::ISODate` output format (seconds precision, no offset for local time).
const QT_ISO_FORMAT: &str = "%Y-%m-%dT%H:%M:%S";

// --- Qt date helpers ----------------------------------------------------------

/// Serializes like `QDateTime::toString(Qt::ISODate)`; `None` (invalid) → `""`.
pub fn format_qt_iso(dt: Option<NaiveDateTime>) -> String {
    dt.map(|d| d.format(QT_ISO_FORMAT).to_string())
        .unwrap_or_default()
}

/// Parses like `QDateTime::fromString(s, Qt::ISODate)`, returning local time.
///
/// Accepts `yyyy-MM-dd`, then optionally `T` (or a space) + `HH:mm[:ss[.zzz]]`,
/// then optionally `Z` or a `±HH[:mm]` offset (converted to local time).
/// Anything malformed yields `None` ("no date"), never a panic.
pub fn parse_qt_iso(s: &str) -> Option<NaiveDateTime> {
    let s = s.trim();
    if s.len() < 10 || !s.is_char_boundary(10) {
        return None;
    }
    let date = NaiveDate::parse_from_str(&s[..10], "%Y-%m-%d").ok()?;
    let rest = &s[10..];
    if rest.is_empty() {
        return Some(date.and_time(NaiveTime::MIN));
    }
    let rest = rest.strip_prefix('T').or_else(|| rest.strip_prefix(' '))?;

    // Optional time-zone suffix.
    let (time_part, offset_secs) = if let Some(t) = rest.strip_suffix('Z') {
        (t, Some(0))
    } else if let Some(pos) = rest.rfind(['+', '-']) {
        (&rest[..pos], Some(parse_offset(&rest[pos..])?))
    } else {
        (rest, None)
    };

    // HH:mm[:ss[.zzz | ,zzz]]
    let (hms, frac) = match time_part.find(['.', ',']) {
        Some(p) => (&time_part[..p], Some(&time_part[p + 1..])),
        None => (time_part, None),
    };
    let mut fields = hms.split(':');
    let hour = parse_fixed_digits(fields.next()?, 2)?;
    let minute = parse_fixed_digits(fields.next()?, 2)?;
    let second = match fields.next() {
        Some(f) => parse_fixed_digits(f, 2)?,
        None => 0,
    };
    if fields.next().is_some() {
        return None;
    }
    let millis = match frac {
        Some(f) if !f.is_empty() && f.bytes().all(|b| b.is_ascii_digit()) => {
            let digits: String = f.chars().chain("000".chars()).take(3).collect();
            digits.parse::<u32>().ok()?
        }
        Some(_) => return None,
        None => 0,
    };

    let naive = if hour == 24 && minute == 0 && second == 0 && millis == 0 {
        // ISO 8601 "end of day" = next midnight (Qt accepts it too).
        date.succ_opt()?.and_time(NaiveTime::MIN)
    } else {
        date.and_time(NaiveTime::from_hms_milli_opt(hour, minute, second, millis)?)
    };

    match offset_secs {
        None => Some(naive),
        Some(off) => {
            let utc = naive - chrono::Duration::seconds(i64::from(off));
            Some(
                Utc.from_utc_datetime(&utc)
                    .with_timezone(&Local)
                    .naive_local(),
            )
        }
    }
}

fn parse_fixed_digits(s: &str, len: usize) -> Option<u32> {
    if s.len() == len && s.bytes().all(|b| b.is_ascii_digit()) {
        s.parse().ok()
    } else {
        None
    }
}

/// `+HH`, `+HHmm`, `+HH:mm` (or `-`) → seconds east of UTC.
fn parse_offset(s: &str) -> Option<i32> {
    let sign = match s.as_bytes().first()? {
        b'+' => 1,
        b'-' => -1,
        _ => return None,
    };
    let body = s[1..].replace(':', "");
    let (h, m) = match body.len() {
        2 => (parse_fixed_digits(&body, 2)?, 0),
        4 => (
            parse_fixed_digits(&body[..2], 2)?,
            parse_fixed_digits(&body[2..], 2)?,
        ),
        _ => return None,
    };
    if h > 23 || m > 59 {
        return None;
    }
    Some(sign * (h as i32 * 3600 + m as i32 * 60))
}

/// Current local time truncated to the second (what Qt::ISODate keeps).
pub fn now_local() -> NaiveDateTime {
    let now = Local::now().naive_local();
    now.with_nanosecond(0).unwrap_or(now)
}

/// `QString::left(n)` on characters.
pub fn left_chars(s: &str, n: usize) -> &str {
    match s.char_indices().nth(n) {
        Some((i, _)) => &s[..i],
        None => s,
    }
}

// Lenient accessors mirroring QJsonValue::toString()/toBool()/toArray().
fn str_of(obj: &Map<String, Value>, key: &str) -> Option<String> {
    obj.get(key).and_then(Value::as_str).map(str::to_string)
}
fn bool_of(obj: &Map<String, Value>, key: &str) -> bool {
    obj.get(key).and_then(Value::as_bool).unwrap_or(false)
}

// --- Data structures ----------------------------------------------------------

/// One note. `due_at == None` = no reminder.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Note {
    pub id: String,
    pub title: String,
    pub content: String,
    /// Post-it colour (`#RRGGBB`) for notes created by the notes window.
    pub category: String,
    pub tags: Vec<String>,
    pub created_at: Option<NaiveDateTime>,
    pub updated_at: Option<NaiveDateTime>,
    pub is_favorite: bool,
    pub is_archived: bool,
    pub due_at: Option<NaiveDateTime>,
    /// True once the reminder was notified.
    pub reminder_done: bool,
}

impl Note {
    pub fn has_reminder(&self) -> bool {
        self.due_at.is_some()
    }

    /// JSON object with keys in QJsonObject (alphabetical) order.
    pub fn to_json(&self) -> Value {
        let mut obj = Map::new();
        obj.insert("category".into(), Value::String(self.category.clone()));
        obj.insert("content".into(), Value::String(self.content.clone()));
        obj.insert(
            "created_at".into(),
            Value::String(format_qt_iso(self.created_at)),
        );
        if self.due_at.is_some() {
            obj.insert("due_at".into(), Value::String(format_qt_iso(self.due_at)));
        }
        obj.insert("id".into(), Value::String(self.id.clone()));
        obj.insert("is_archived".into(), Value::Bool(self.is_archived));
        obj.insert("is_favorite".into(), Value::Bool(self.is_favorite));
        if self.due_at.is_some() {
            obj.insert("reminder_done".into(), Value::Bool(self.reminder_done));
        }
        obj.insert(
            "tags".into(),
            Value::Array(self.tags.iter().cloned().map(Value::String).collect()),
        );
        obj.insert("title".into(), Value::String(self.title.clone()));
        obj.insert(
            "updated_at".into(),
            Value::String(format_qt_iso(self.updated_at)),
        );
        Value::Object(obj)
    }

    /// Lenient parse (wrong types → defaults, like QJsonValue accessors).
    pub fn from_json(obj: &Map<String, Value>) -> Self {
        let tags = obj
            .get("tags")
            .and_then(Value::as_array)
            .map(|a| {
                a.iter()
                    .map(|t| t.as_str().unwrap_or_default().to_string())
                    .collect()
            })
            .unwrap_or_default();
        let date = |key: &str| obj.get(key).and_then(Value::as_str).and_then(parse_qt_iso);
        Note {
            id: str_of(obj, "id").unwrap_or_default(),
            title: str_of(obj, "title").unwrap_or_default(),
            content: str_of(obj, "content").unwrap_or_default(),
            category: str_of(obj, "category").unwrap_or_else(|| "general".into()),
            tags,
            created_at: date("created_at"),
            updated_at: date("updated_at"),
            is_favorite: bool_of(obj, "is_favorite"),
            is_archived: bool_of(obj, "is_archived"),
            // A malformed due_at simply means "no reminder".
            due_at: date("due_at"),
            reminder_done: bool_of(obj, "reminder_done"),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct NoteCategory {
    pub id: String,
    pub name: String,
    pub color: String,
}

impl NoteCategory {
    fn new(id: &str, name: &str, color: &str) -> Self {
        NoteCategory {
            id: id.into(),
            name: name.into(),
            color: color.into(),
        }
    }

    pub fn to_json(&self) -> Value {
        let mut obj = Map::new();
        obj.insert("color".into(), Value::String(self.color.clone()));
        obj.insert("id".into(), Value::String(self.id.clone()));
        obj.insert("name".into(), Value::String(self.name.clone()));
        Value::Object(obj)
    }

    pub fn from_json(obj: &Map<String, Value>) -> Self {
        NoteCategory {
            id: str_of(obj, "id").unwrap_or_default(),
            name: str_of(obj, "name").unwrap_or_default(),
            color: str_of(obj, "color").unwrap_or_else(|| DEFAULT_CATEGORY_COLOR.into()),
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct NotesStats {
    pub total_notes: usize,
    pub active_notes: usize,
    pub favorite_notes: usize,
    pub archived_notes: usize,
    pub category_stats: BTreeMap<String, usize>,
}

/// Pre-lowercased search fields, parallel to `notes` (Qt "H2" index).
#[derive(Clone, Debug)]
struct SearchEntry {
    title: String,
    content: String,
    tags: Vec<String>,
}

impl SearchEntry {
    fn of(note: &Note) -> Self {
        SearchEntry {
            title: note.title.to_lowercase(),
            content: note.content.to_lowercase(),
            tags: note.tags.iter().map(|t| t.to_lowercase()).collect(),
        }
    }
}

// --- Store --------------------------------------------------------------------

pub struct NotesStore {
    notes: Vec<Note>,
    categories: Vec<NoteCategory>,
    index: Vec<SearchEntry>,
    data_dir: PathBuf,
    /// Bumped on every in-memory change, so views know when to refresh.
    revision: u64,
}

impl NotesStore {
    /// Loads notes and categories from disk (Qt-compatible JSON format),
    /// migrating notes from pre-AppData locations first.
    pub fn load() -> Self {
        let dir = paths::data_dir();
        let mut old = Vec::new();
        if let Some(home) = dirs::home_dir() {
            old.push(
                home.join("Documents")
                    .join("Sylvania Launcher")
                    .join("data")
                    .join(NOTES_FILE),
            );
        }
        if let Ok(cwd) = std::env::current_dir() {
            old.push(cwd.join("data").join(NOTES_FILE));
        }
        migrate_old_notes(&dir, &old);
        Self::open(dir)
    }

    /// Loads the store from `data_dir` (no migration). Creates the default
    /// categories when none exist, like the Qt constructor.
    pub fn open(data_dir: PathBuf) -> Self {
        let mut store = NotesStore {
            notes: Vec::new(),
            categories: Vec::new(),
            index: Vec::new(),
            data_dir,
            revision: 0,
        };
        store.load_notes();
        store.load_categories();
        if store.categories.is_empty() {
            store.create_default_categories();
        }
        store
    }

    pub fn revision(&self) -> u64 {
        self.revision
    }

    fn notes_path(&self) -> PathBuf {
        self.data_dir.join(NOTES_FILE)
    }

    fn categories_path(&self) -> PathBuf {
        self.data_dir.join(CATEGORIES_FILE)
    }

    fn create_default_categories(&mut self) {
        self.categories = vec![
            NoteCategory::new("general", "Général", "#4CAF50"),
            NoteCategory::new("wow", "World of Warcraft", "#FF9800"),
            NoteCategory::new("guides", "Guides", "#2196F3"),
            NoteCategory::new("todo", "À faire", "#F44336"),
            NoteCategory::new("ideas", "Idées", "#9C27B0"),
        ];
        self.save_categories();
    }

    fn load_notes(&mut self) {
        self.notes.clear();
        let path = self.notes_path();
        if let Some(Value::Array(items)) = read_json_capped(&path) {
            self.notes = items
                .iter()
                .filter_map(Value::as_object)
                .map(Note::from_json)
                .collect();
            log::debug!("Chargé {} notes", self.notes.len());
        }
        self.rebuild_index();
    }

    fn load_categories(&mut self) {
        self.categories.clear();
        if let Some(Value::Array(items)) = read_json_capped(&self.categories_path()) {
            self.categories = items
                .iter()
                .filter_map(Value::as_object)
                .map(NoteCategory::from_json)
                .collect();
        }
    }

    fn rebuild_index(&mut self) {
        self.index = self.notes.iter().map(SearchEntry::of).collect();
    }

    fn save_notes(&mut self) -> bool {
        self.revision += 1;
        let array = Value::Array(self.notes.iter().map(Note::to_json).collect());
        let ok = write_compact(&self.notes_path(), &array);
        if !ok {
            log::warn!("Impossible de sauvegarder les notes");
        }
        ok
    }

    fn save_categories(&mut self) -> bool {
        self.revision += 1;
        let array = Value::Array(self.categories.iter().map(NoteCategory::to_json).collect());
        write_compact(&self.categories_path(), &array)
    }

    fn position(&self, id: &str) -> Option<usize> {
        self.notes.iter().position(|n| n.id == id)
    }

    // --- CRUD ---------------------------------------------------------------

    /// Creates a note; rejects a wholly empty one. Returns its id.
    pub fn create_note(
        &mut self,
        title: &str,
        content: &str,
        category: &str,
        tags: &[String],
        due_at: Option<NaiveDateTime>,
    ) -> Option<String> {
        if title.trim().is_empty() && content.trim().is_empty() {
            return None;
        }
        let now = now_local();
        let note = Note {
            id: uuid::Uuid::new_v4().to_string(),
            title: title.trim().to_string(),
            content: content.trim().to_string(),
            category: category.to_string(),
            tags: tags.to_vec(),
            created_at: Some(now),
            updated_at: Some(now),
            is_favorite: false,
            is_archived: false,
            due_at,
            reminder_done: false,
        };
        let id = note.id.clone();
        self.index.push(SearchEntry::of(&note));
        self.notes.push(note);
        if self.save_notes() {
            return Some(id);
        }
        self.notes.pop();
        self.index.pop();
        None
    }

    /// Updates the given fields (`None` = unchanged; empty `tags` = unchanged).
    pub fn update_note(
        &mut self,
        id: &str,
        title: Option<&str>,
        content: Option<&str>,
        category: Option<&str>,
        tags: &[String],
    ) -> bool {
        let Some(i) = self.position(id) else {
            return false;
        };
        let note = &mut self.notes[i];
        if let Some(t) = title {
            note.title = t.trim().to_string();
        }
        if let Some(c) = content {
            note.content = c.trim().to_string();
        }
        if let Some(c) = category {
            note.category = c.to_string();
        }
        if !tags.is_empty() {
            note.tags = tags.to_vec();
        }
        note.updated_at = Some(now_local());
        self.index[i] = SearchEntry::of(&self.notes[i]);
        self.save_notes()
    }

    pub fn delete_note(&mut self, id: &str) -> bool {
        let Some(i) = self.position(id) else {
            return false;
        };
        self.notes.remove(i);
        self.index.remove(i);
        self.save_notes()
    }

    // --- Reminders (v3.0) ---------------------------------------------------

    /// Sets/clears the due date. Resets `reminder_done` so a re-scheduled
    /// note notifies again.
    pub fn set_note_due_date(&mut self, id: &str, due_at: Option<NaiveDateTime>) -> bool {
        let Some(i) = self.position(id) else {
            return false;
        };
        let note = &mut self.notes[i];
        note.due_at = due_at;
        note.reminder_done = false;
        note.updated_at = Some(now_local());
        self.save_notes()
    }

    /// Marks reminders as notified (one save for the whole batch).
    pub fn mark_reminders_done(&mut self, ids: &[String]) -> bool {
        let mut changed = false;
        for note in self.notes.iter_mut().filter(|n| ids.contains(&n.id)) {
            note.reminder_done = true;
            changed = true;
        }
        changed && self.save_notes()
    }

    /// Notes whose reminder is due (due ≤ now, not yet notified, not archived).
    pub fn due_reminders(&self, now: NaiveDateTime) -> Vec<Note> {
        self.notes
            .iter()
            .filter(|n| !n.reminder_done && !n.is_archived && n.due_at.is_some_and(|d| d <= now))
            .cloned()
            .collect()
    }

    /// Notes carrying any reminder (calendar decoration).
    pub fn notes_with_reminders(&self) -> Vec<&Note> {
        self.notes
            .iter()
            .filter(|n| n.has_reminder() && !n.is_archived)
            .collect()
    }

    /// Notes "belonging" to a calendar day: created OR due that day.
    pub fn notes_on_day(&self, day: NaiveDate) -> Vec<&Note> {
        self.notes
            .iter()
            .filter(|n| {
                !n.is_archived
                    && (n.created_at.is_some_and(|c| c.date() == day)
                        || n.due_at.is_some_and(|d| d.date() == day))
            })
            .collect()
    }

    // --- Retrieval ----------------------------------------------------------

    pub fn get_note_by_id(&self, id: &str) -> Option<&Note> {
        self.notes.iter().find(|n| n.id == id)
    }

    pub fn get_all_notes(&self, include_archived: bool) -> Vec<&Note> {
        self.notes
            .iter()
            .filter(|n| include_archived || !n.is_archived)
            .collect()
    }

    // --- Import / export ----------------------------------------------------

    /// Writes notes + categories to `path` (default:
    /// `data/notes_export_yyyyMMdd_HHmmss.json`). Returns the path written.
    pub fn export_notes(&self, path: Option<&Path>) -> Option<PathBuf> {
        let path = match path {
            Some(p) => p.to_path_buf(),
            None => self.data_dir.join(format!(
                "notes_export_{}.json",
                Local::now().format("%Y%m%d_%H%M%S")
            )),
        };
        let mut obj = Map::new();
        obj.insert(
            "categories".into(),
            Value::Array(self.categories.iter().map(NoteCategory::to_json).collect()),
        );
        obj.insert(
            "exported_at".into(),
            Value::String(format_qt_iso(Some(now_local()))),
        );
        obj.insert(
            "notes".into(),
            Value::Array(self.notes.iter().map(Note::to_json).collect()),
        );
        obj.insert("version".into(), Value::String("1.0".into()));

        // Indented (4 spaces, like QJsonDocument::Indented): user-facing file.
        let mut out = Vec::new();
        let fmt = serde_json::ser::PrettyFormatter::with_indent(b"    ");
        let mut ser = serde_json::Serializer::with_formatter(&mut out, fmt);
        serde::Serialize::serialize(&Value::Object(obj), &mut ser).ok()?;
        out.push(b'\n');
        paths::write_atomic(&path, &out).ok()?;
        Some(path)
    }

    /// Imports an export file. Rejects empty files, files over
    /// [`MAX_IMPORT_BYTES`] and files lacking the `notes`/`categories` arrays.
    /// `merge` keeps existing data (imported notes get new ids); otherwise
    /// everything is replaced.
    pub fn import_notes(&mut self, path: &Path, merge: bool) -> bool {
        let Ok(meta) = std::fs::metadata(path) else {
            return false;
        };
        if meta.len() == 0 || meta.len() > MAX_IMPORT_BYTES {
            log::warn!("Import rejeté: taille hors limites ({})", meta.len());
            return false;
        }
        let Ok(bytes) = std::fs::read(path) else {
            return false;
        };
        let Ok(Value::Object(data)) = serde_json::from_slice::<Value>(&bytes) else {
            return false;
        };
        let (Some(Value::Array(notes)), Some(Value::Array(cats))) =
            (data.get("notes"), data.get("categories"))
        else {
            log::warn!("Import rejeté: schéma invalide (notes/categories manquants)");
            return false;
        };

        if !merge {
            self.notes.clear();
            self.categories.clear();
        }
        for cat in cats
            .iter()
            .filter_map(Value::as_object)
            .map(NoteCategory::from_json)
        {
            if !cat.id.is_empty() && !self.categories.iter().any(|c| c.id == cat.id) {
                self.categories.push(cat);
            }
        }
        for mut note in notes
            .iter()
            .filter_map(Value::as_object)
            .map(Note::from_json)
        {
            if note.id.is_empty() || note.title.is_empty() {
                continue;
            }
            if merge {
                note.id = uuid::Uuid::new_v4().to_string();
            }
            self.notes.push(note);
        }
        self.rebuild_index();
        self.save_notes() && self.save_categories()
    }
}

/// Rest of the Qt `NotesManager` API, kept for parity (not wired in the UI).
#[allow(dead_code)]
impl NotesStore {
    pub fn get_notes_by_category(&self, category: &str, include_archived: bool) -> Vec<&Note> {
        self.notes
            .iter()
            .filter(|n| n.category == category && (include_archived || !n.is_archived))
            .collect()
    }

    pub fn get_favorite_notes(&self) -> Vec<&Note> {
        self.notes
            .iter()
            .filter(|n| n.is_favorite && !n.is_archived)
            .collect()
    }

    /// Case-insensitive search in title, content and tags.
    pub fn search_notes(&self, query: &str, include_archived: bool) -> Vec<&Note> {
        if query.is_empty() {
            return self.get_all_notes(include_archived);
        }
        let q = query.to_lowercase();
        self.notes
            .iter()
            .zip(&self.index)
            .filter(|(n, _)| include_archived || !n.is_archived)
            .filter(|(_, e)| {
                e.title.contains(&q)
                    || e.content.contains(&q)
                    || e.tags.iter().any(|t| t.contains(&q))
            })
            .map(|(n, _)| n)
            .collect()
    }

    pub fn toggle_favorite(&mut self, id: &str) -> bool {
        let Some(i) = self.position(id) else {
            return false;
        };
        self.notes[i].is_favorite = !self.notes[i].is_favorite;
        self.notes[i].updated_at = Some(now_local());
        self.save_notes()
    }

    pub fn toggle_archive(&mut self, id: &str) -> bool {
        let Some(i) = self.position(id) else {
            return false;
        };
        self.notes[i].is_archived = !self.notes[i].is_archived;
        self.notes[i].updated_at = Some(now_local());
        self.save_notes()
    }

    pub fn categories(&self) -> &[NoteCategory] {
        &self.categories
    }

    pub fn add_category(&mut self, id: &str, name: &str, color: &str) -> bool {
        if self.categories.iter().any(|c| c.id == id) {
            return false;
        }
        self.categories.push(NoteCategory::new(id, name, color));
        self.save_categories()
    }

    pub fn stats(&self) -> NotesStats {
        let mut s = NotesStats {
            total_notes: self.notes.len(),
            ..Default::default()
        };
        for n in &self.notes {
            if n.is_archived {
                s.archived_notes += 1;
            } else {
                s.active_notes += 1;
            }
            if n.is_favorite {
                s.favorite_notes += 1;
            }
            *s.category_stats.entry(n.category.clone()).or_default() += 1;
        }
        s
    }
}

/// Reads a JSON file, refusing missing, unreadable, oversized or invalid ones.
fn read_json_capped(path: &Path) -> Option<Value> {
    let meta = std::fs::metadata(path).ok()?;
    if meta.len() > MAX_IMPORT_BYTES {
        log::warn!(
            "{} ignoré: taille hors limites ({})",
            path.display(),
            meta.len()
        );
        return None;
    }
    let bytes = std::fs::read(path).ok()?;
    match serde_json::from_slice(&bytes) {
        Ok(v) => Some(v),
        Err(e) => {
            log::warn!("Erreur de chargement de {}: {e}", path.display());
            None
        }
    }
}

fn write_compact(path: &Path, value: &Value) -> bool {
    match serde_json::to_vec(value) {
        Ok(bytes) => paths::write_atomic(path, &bytes).is_ok(),
        Err(_) => false,
    }
}

/// Copies notes from a pre-AppData location when `data_dir` has none yet
/// (Qt `migrateOldNotes`: first candidate larger than "[]" wins, existing
/// files are never overwritten).
fn migrate_old_notes(data_dir: &Path, candidates: &[PathBuf]) {
    let current = data_dir.join(NOTES_FILE);
    if std::fs::metadata(&current).is_ok_and(|m| m.len() > 2) {
        return;
    }
    for old in candidates {
        if !std::fs::metadata(old).is_ok_and(|m| m.len() > 2) {
            continue;
        }
        if !current.exists() {
            let _ = std::fs::copy(old, &current);
        }
        let old_cats = old.with_file_name(CATEGORIES_FILE);
        let new_cats = data_dir.join(CATEGORIES_FILE);
        if old_cats.exists() && !new_cats.exists() {
            let _ = std::fs::copy(&old_cats, &new_cats);
        }
        log::info!("Notes migrées depuis: {}", old.display());
        break;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn dt(s: &str) -> NaiveDateTime {
        NaiveDateTime::parse_from_str(s, "%Y-%m-%d %H:%M:%S").unwrap()
    }

    /// What the Qt launcher (QJsonDocument::Compact) wrote: one pre-3.0 note
    /// without reminder keys, one v3.0 note with a reminder.
    const QT_SAMPLE: &str = r##"[{"category":"#FFE066","content":"Acheter des potions","created_at":"2025-03-12T14:05:33","id":"6f1c0a52-3d1e-4b8e-9a51-1c7a0c0f2e11","is_archived":false,"is_favorite":false,"tags":[],"title":"Raid ICC","updated_at":"2025-03-12T14:06:01"},{"category":"#80D4FF","content":"Événement « Foire de Sombrelune »","created_at":"2026-11-20T09:00:00","due_at":"2026-11-25T20:30:00","id":"a2b9e1f0-0000-4000-8000-000000000001","is_archived":false,"is_favorite":true,"reminder_done":false,"tags":["wow","event"],"title":"Sombrelune","updated_at":"2026-11-20T09:00:00"}]"##;

    fn store_with(json: &str) -> (tempfile::TempDir, NotesStore) {
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(dir.path().join(NOTES_FILE), json).unwrap();
        let store = NotesStore::open(dir.path().to_path_buf());
        (dir, store)
    }

    #[test]
    fn qt_iso_parsing() {
        assert_eq!(
            parse_qt_iso("2025-03-12T14:05:33"),
            Some(dt("2025-03-12 14:05:33"))
        );
        assert_eq!(
            parse_qt_iso("2025-03-12T14:05"),
            Some(dt("2025-03-12 14:05:00"))
        );
        assert_eq!(parse_qt_iso("2025-03-12"), Some(dt("2025-03-12 00:00:00")));
        assert_eq!(
            parse_qt_iso("2025-03-12T14:05:33.250").map(|d| d.and_utc().timestamp_subsec_millis()),
            Some(250)
        );
        assert_eq!(
            parse_qt_iso("2025-03-12T24:00:00"),
            Some(dt("2025-03-13 00:00:00"))
        );
        let z = Utc
            .from_utc_datetime(&dt("2025-03-12 14:05:33"))
            .with_timezone(&Local)
            .naive_local();
        assert_eq!(parse_qt_iso("2025-03-12T14:05:33Z"), Some(z));
        assert_eq!(parse_qt_iso("2025-03-12T16:05:33+02:00"), Some(z));
        assert_eq!(parse_qt_iso("2025-03-12T12:05:33-0200"), Some(z));
        for bad in [
            "",
            "garbage",
            "2025-13-01T00:00:00",
            "2025-03-12T25:00",
            "2025-03-12X10:00",
            "2025-03-12T1:00",
        ] {
            assert_eq!(parse_qt_iso(bad), None, "{bad}");
        }
        assert_eq!(
            format_qt_iso(Some(dt("2025-03-02 04:05:06"))),
            "2025-03-02T04:05:06"
        );
        assert_eq!(format_qt_iso(None), "");
    }

    #[test]
    fn qt_json_round_trip_is_byte_identical() {
        let (dir, mut store) = store_with(QT_SAMPLE);
        let notes = store.get_all_notes(true);
        assert_eq!(notes.len(), 2);
        assert_eq!(notes[0].title, "Raid ICC");
        assert!(!notes[0].has_reminder());
        assert_eq!(notes[1].due_at, Some(dt("2026-11-25 20:30:00")));
        assert_eq!(notes[1].tags, vec!["wow", "event"]);
        assert!(notes[1].is_favorite);

        // Force a save without changing anything: output must match Qt's.
        assert!(store.save_notes());
        let written = std::fs::read_to_string(dir.path().join(NOTES_FILE)).unwrap();
        assert_eq!(written, QT_SAMPLE);

        // Default categories were created, in Qt's compact format.
        let cats = std::fs::read_to_string(dir.path().join(CATEGORIES_FILE)).unwrap();
        assert!(
            cats.starts_with(r##"[{"color":"#4CAF50","id":"general","name":"Général"}"##),
            "{cats}"
        );
    }

    #[test]
    fn lenient_parsing_of_malformed_fields() {
        let (_d, store) = store_with(
            r#"[{"id":"x","title":5,"due_at":"not a date","reminder_done":"yes","tags":[1,"a"]}, 42]"#,
        );
        let n = store.get_note_by_id("x").unwrap();
        assert_eq!(n.title, "");
        assert_eq!(n.category, "general");
        assert_eq!(n.due_at, None);
        assert!(!n.reminder_done);
        assert_eq!(n.tags, vec!["", "a"]);
        assert_eq!(store.get_all_notes(true).len(), 1);
    }

    #[test]
    fn invalid_or_missing_file_loads_empty() {
        let (_d, store) = store_with("{not json");
        assert!(store.get_all_notes(true).is_empty());
        let dir = tempfile::tempdir().unwrap();
        let store = NotesStore::open(dir.path().to_path_buf());
        assert!(store.get_all_notes(true).is_empty());
        assert_eq!(store.categories().len(), 5);
    }

    #[test]
    fn crud_and_reminder_fields() {
        let dir = tempfile::tempdir().unwrap();
        let mut s = NotesStore::open(dir.path().to_path_buf());
        assert_eq!(s.create_note("  ", "\n", "#FFE066", &[], None), None);
        let due = dt("2030-01-01 10:00:00");
        let id = s
            .create_note(" Titre ", " corps ", "#FFE066", &[], Some(due))
            .unwrap();
        let n = s.get_note_by_id(&id).unwrap();
        assert_eq!((n.title.as_str(), n.content.as_str()), ("Titre", "corps"));
        assert!(n.created_at.is_some());

        assert!(s.mark_reminders_done(std::slice::from_ref(&id)));
        assert!(s.get_note_by_id(&id).unwrap().reminder_done);
        assert!(s.set_note_due_date(&id, Some(due)));
        assert!(!s.get_note_by_id(&id).unwrap().reminder_done);

        assert!(s.update_note(&id, Some("Nouveau"), None, Some("#80D4FF"), &[]));
        let reloaded = NotesStore::open(dir.path().to_path_buf());
        let n = reloaded.get_note_by_id(&id).unwrap();
        assert_eq!(
            (n.title.as_str(), n.content.as_str(), n.category.as_str()),
            ("Nouveau", "corps", "#80D4FF")
        );
        assert_eq!(n.due_at, Some(due));

        assert!(s.set_note_due_date(&id, None));
        let raw = std::fs::read_to_string(dir.path().join(NOTES_FILE)).unwrap();
        assert!(
            !raw.contains("due_at") && !raw.contains("reminder_done"),
            "{raw}"
        );

        let rev = s.revision();
        assert!(s.delete_note(&id));
        assert!(s.revision() > rev);
        assert!(!s.delete_note(&id));
    }

    #[test]
    fn due_reminder_selection() {
        let (_d, mut s) = store_with(
            r#"[
            {"id":"past","title":"a","due_at":"2026-01-01T10:00:00","reminder_done":false},
            {"id":"exact","title":"b","due_at":"2026-01-02T12:00:00"},
            {"id":"future","title":"c","due_at":"2026-01-02T12:00:01"},
            {"id":"done","title":"d","due_at":"2026-01-01T10:00:00","reminder_done":true},
            {"id":"archived","title":"e","due_at":"2026-01-01T10:00:00","is_archived":true},
            {"id":"none","title":"f"}
        ]"#,
        );
        let now = dt("2026-01-02 12:00:00");
        let ids: Vec<_> = s.due_reminders(now).into_iter().map(|n| n.id).collect();
        assert_eq!(ids, vec!["past", "exact"]);
        let with: Vec<_> = s
            .notes_with_reminders()
            .into_iter()
            .map(|n| n.id.clone())
            .collect();
        assert_eq!(with, vec!["past", "exact", "future", "done"]);
        assert!(s.mark_reminders_done(&ids));
        assert!(s.due_reminders(now).is_empty());
    }

    #[test]
    fn notes_on_day_matches_creation_or_due_date() {
        let (_d, s) = store_with(
            r#"[
            {"id":"created","title":"a","created_at":"2026-11-25T08:00:00"},
            {"id":"due","title":"b","created_at":"2026-10-01T08:00:00","due_at":"2026-11-25T23:59:59"},
            {"id":"other","title":"c","created_at":"2026-11-24T23:59:59","due_at":"2026-11-26T00:00:00"},
            {"id":"archived","title":"d","created_at":"2026-11-25T08:00:00","is_archived":true},
            {"id":"nodate","title":"e","created_at":""}
        ]"#,
        );
        let day = NaiveDate::from_ymd_opt(2026, 11, 25).unwrap();
        let ids: Vec<_> = s
            .notes_on_day(day)
            .into_iter()
            .map(|n| n.id.as_str())
            .collect();
        assert_eq!(ids, vec!["created", "due"]);
    }

    #[test]
    fn search_is_case_insensitive_over_title_content_tags() {
        let (_d, mut s) = store_with(
            r#"[
            {"id":"1","title":"Raid ÉPIQUE","content":"x"},
            {"id":"2","title":"t","content":"le Donjon"},
            {"id":"3","title":"t","content":"y","tags":["PvP"]},
            {"id":"4","title":"épique archivé","is_archived":true}
        ]"#,
        );
        let ids = |v: Vec<&Note>| v.into_iter().map(|n| n.id.clone()).collect::<Vec<_>>();
        assert_eq!(ids(s.search_notes("épique", false)), vec!["1"]);
        assert_eq!(ids(s.search_notes("ÉPIQUE", true)), vec!["1", "4"]);
        assert_eq!(ids(s.search_notes("donjon", false)), vec!["2"]);
        assert_eq!(ids(s.search_notes("pvp", false)), vec!["3"]);
        assert_eq!(s.search_notes("", false).len(), 3);
        // Index follows updates.
        assert!(s.update_note("2", None, Some("autre"), None, &[]));
        assert!(s.search_notes("donjon", false).is_empty());
        assert_eq!(s.stats().archived_notes, 1);
    }

    #[test]
    fn export_import_round_trip_and_caps() {
        let (dir, s) = store_with(QT_SAMPLE);
        let out = s.export_notes(None).unwrap();
        assert!(
            out.file_name()
                .unwrap()
                .to_string_lossy()
                .starts_with("notes_export_")
        );
        let text = std::fs::read_to_string(&out).unwrap();
        assert!(text.starts_with("{\n    \"categories\": ["), "{text}");

        // Merge import into a fresh store: new ids, same content.
        let other = tempfile::tempdir().unwrap();
        let mut t = NotesStore::open(other.path().to_path_buf());
        assert!(t.import_notes(&out, true));
        let titles: Vec<_> = t
            .get_all_notes(true)
            .iter()
            .map(|n| n.title.clone())
            .collect();
        assert_eq!(titles, vec!["Raid ICC", "Sombrelune"]);
        assert!(
            t.get_note_by_id("6f1c0a52-3d1e-4b8e-9a51-1c7a0c0f2e11")
                .is_none()
        );
        assert!(t.import_notes(&out, true));
        assert_eq!(t.get_all_notes(true).len(), 4);
        // Replace import keeps the original ids.
        assert!(t.import_notes(&out, false));
        assert_eq!(t.get_all_notes(true).len(), 2);
        assert!(
            t.get_note_by_id("6f1c0a52-3d1e-4b8e-9a51-1c7a0c0f2e11")
                .is_some()
        );

        // Schema and size checks.
        let bad = dir.path().join("bad.json");
        std::fs::write(&bad, r#"{"notes":[]}"#).unwrap();
        assert!(!t.import_notes(&bad, true));
        std::fs::write(&bad, "").unwrap();
        assert!(!t.import_notes(&bad, true));
        let big = dir.path().join("big.json");
        let f = std::fs::File::create(&big).unwrap();
        f.set_len(MAX_IMPORT_BYTES + 1).unwrap();
        assert!(!t.import_notes(&big, true));
        assert!(!t.import_notes(&dir.path().join("missing.json"), true));
        assert_eq!(t.get_all_notes(true).len(), 2);

        // An oversized notes.json is ignored on load.
        std::fs::copy(&big, other.path().join(NOTES_FILE)).unwrap();
        assert!(
            NotesStore::open(other.path().to_path_buf())
                .get_all_notes(true)
                .is_empty()
        );
    }

    #[test]
    fn migration_copies_only_when_missing() {
        let old = tempfile::tempdir().unwrap();
        std::fs::write(old.path().join(NOTES_FILE), QT_SAMPLE).unwrap();
        std::fs::write(old.path().join(CATEGORIES_FILE), "[]").unwrap();
        let new = tempfile::tempdir().unwrap();
        migrate_old_notes(
            new.path(),
            &[old.path().join("absent.json"), old.path().join(NOTES_FILE)],
        );
        assert_eq!(
            std::fs::read_to_string(new.path().join(NOTES_FILE)).unwrap(),
            QT_SAMPLE
        );
        assert!(new.path().join(CATEGORIES_FILE).exists());

        // Existing notes are never overwritten.
        std::fs::write(new.path().join(NOTES_FILE), r#"[{"id":"keep"}]"#).unwrap();
        migrate_old_notes(new.path(), &[old.path().join(NOTES_FILE)]);
        assert!(
            std::fs::read_to_string(new.path().join(NOTES_FILE))
                .unwrap()
                .contains("keep")
        );
    }

    #[test]
    fn left_chars_counts_characters() {
        assert_eq!(left_chars("Événement", 3), "Évé");
        assert_eq!(left_chars("ab", 5), "ab");
    }
}
