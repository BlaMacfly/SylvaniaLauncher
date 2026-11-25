//! Launcher configuration (`config.json` in [`paths::config_dir`]).
//!
//! The file format is the one written by the Qt launcher (all keys top-level),
//! and unknown keys are preserved on save. Configs written by v3.0 (client
//! settings nested under `"editions": {"wotlk": …, "legion": …}`) are
//! flattened on load; the Legion part is dropped.

use crate::constants;
use crate::paths;
use serde_json::{Map, Value, json};
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RealmlistEntry {
    pub name: String,
    pub address: String,
    pub active: bool,
}

pub struct Config {
    values: Map<String, Value>,
    path: PathBuf,
}

impl Config {
    /// Loads (or creates) the config and runs the migrations. Never fails: an
    /// unreadable/corrupt file falls back to defaults.
    pub fn load() -> Self {
        let path = paths::config_dir().join("config.json");
        let mut cfg = Config {
            values: Map::new(),
            path,
        };

        let loaded = std::fs::read(&cfg.path)
            .ok()
            .and_then(|b| serde_json::from_slice::<Value>(&b).ok())
            .and_then(|v| v.as_object().cloned());
        let mut dirty = match loaded {
            Some(map) => {
                cfg.values = map;
                false
            }
            None => {
                cfg.values = default_values();
                true
            }
        };

        dirty |= cfg.migrate_from_edition_config();
        if cfg.migrate_legacy_realmlist() {
            dirty = true;
            if let Some(e) = cfg.active_realmlist_entry() {
                let _ = cfg.update_realmlist(&e.address);
            }
        }
        if dirty {
            cfg.save();
        }
        cfg
    }

    /// Synchronous atomic write. Errors are logged, never fatal.
    pub fn save(&self) -> bool {
        match serde_json::to_vec(&Value::Object(self.values.clone())) {
            Ok(bytes) => match paths::write_atomic(&self.path, &bytes) {
                Ok(()) => true,
                Err(e) => {
                    log::warn!("Impossible de sauvegarder la configuration: {e}");
                    false
                }
            },
            Err(_) => false,
        }
    }

    // --- Generic access -------------------------------------------------------

    /// Sets a key and saves immediately.
    pub fn set(&mut self, key: &str, value: Value) {
        self.values.insert(key.to_string(), value);
        self.save();
    }

    fn get_bool(&self, key: &str, default: bool) -> bool {
        self.values
            .get(key)
            .and_then(Value::as_bool)
            .unwrap_or(default)
    }

    fn get_str(&self, key: &str, default: &str) -> String {
        self.values
            .get(key)
            .and_then(Value::as_str)
            .unwrap_or(default)
            .to_string()
    }

    // --- Typed settings -------------------------------------------------------

    /// Client install directory ("" = not configured).
    pub fn wow_path(&self) -> String {
        self.get_str("wow_path", "")
    }
    pub fn set_wow_path(&mut self, path: &str) {
        self.set("wow_path", json!(path));
    }

    pub fn sound_enabled(&self) -> bool {
        self.get_bool("sound_enabled", true)
    }
    pub fn set_sound_enabled(&mut self, v: bool) {
        self.set("sound_enabled", json!(v));
    }

    /// "fr" or "en".
    pub fn language(&self) -> String {
        let l = self.get_str("language", "fr");
        if l == "en" { l } else { "fr".to_string() }
    }
    pub fn set_language(&mut self, lang: &str) {
        self.set("language", json!(lang));
    }

    pub fn background(&self) -> String {
        self.get_str("background", constants::DEFAULT_BACKGROUND)
    }
    pub fn set_background(&mut self, name: &str) {
        self.set("background", json!(name));
    }

    pub fn random_background(&self) -> bool {
        self.get_bool("random_bg_on_launch", true)
    }
    pub fn set_random_background(&mut self, v: bool) {
        self.set("random_bg_on_launch", json!(v));
    }

    // --- Realmlist ------------------------------------------------------------

    pub fn realmlist_entries(&self) -> Vec<RealmlistEntry> {
        self.values
            .get("realmlist_entries")
            .and_then(Value::as_array)
            .map(|arr| {
                arr.iter()
                    .map(|v| RealmlistEntry {
                        name: v
                            .get("name")
                            .and_then(Value::as_str)
                            .unwrap_or("")
                            .to_string(),
                        address: v
                            .get("address")
                            .and_then(Value::as_str)
                            .unwrap_or("")
                            .to_string(),
                        active: v.get("active").and_then(Value::as_bool).unwrap_or(false),
                    })
                    .collect()
            })
            .unwrap_or_default()
    }

    pub fn set_realmlist_entries(&mut self, entries: &[RealmlistEntry]) {
        let arr: Vec<Value> = entries
            .iter()
            .map(|e| json!({"name": e.name, "address": e.address, "active": e.active}))
            .collect();
        self.set("realmlist_entries", Value::Array(arr));
    }

    pub fn active_realmlist_index(&self) -> usize {
        self.values
            .get("active_realmlist_index")
            .and_then(Value::as_u64)
            .unwrap_or(0) as usize
    }

    pub fn set_active_realmlist_index(&mut self, index: usize) {
        let mut entries = self.realmlist_entries();
        for (i, e) in entries.iter_mut().enumerate() {
            e.active = i == index;
        }
        self.values
            .insert("active_realmlist_index".into(), json!(index));
        self.set_realmlist_entries(&entries); // saves
    }

    /// The active entry, if the index is in range.
    pub fn active_realmlist_entry(&self) -> Option<RealmlistEntry> {
        self.realmlist_entries()
            .get(self.active_realmlist_index())
            .cloned()
    }

    /// Selects an entry and rewrites the client's realmlist.wtf files.
    pub fn switch_realmlist(&mut self, index: usize) -> bool {
        let entries = self.realmlist_entries();
        let Some(entry) = entries.get(index).cloned() else {
            return false;
        };
        self.set_active_realmlist_index(index);
        self.update_realmlist(&entry.address)
    }

    /// Strips "set realmlist" and quotes, leaving just the host.
    pub fn host_from_address(address: &str) -> String {
        let mut s = address.trim().to_string();
        let lower = s.to_lowercase();
        if let Some(rest) = lower.strip_prefix("set") {
            let rest_trim = rest.trim_start();
            if let Some(after) = rest_trim.strip_prefix("realmlist")
                && after.starts_with(char::is_whitespace)
            {
                let cut = s.len() - after.trim_start().len();
                s = s[cut..].to_string();
            }
        }
        s.replace('"', "").trim().to_string()
    }

    /// Writes `realmlist_text` into every existing realmlist.wtf location of
    /// the client (root + frFR/enUS/enGB). Returns true if one was written.
    pub fn update_realmlist(&self, realmlist_text: &str) -> bool {
        let wow = self.wow_path();
        if wow.is_empty() {
            log::warn!("Chemin WoW non défini");
            return false;
        }
        let base = PathBuf::from(&wow);
        let targets = [
            base.join("realmlist.wtf"),
            base.join("Data/frFR/realmlist.wtf"),
            base.join("Data/enUS/realmlist.wtf"),
            base.join("Data/enGB/realmlist.wtf"),
        ];
        let mut ok = false;
        for t in targets {
            // Skip locales that don't exist on disk.
            if !t.parent().is_some_and(|p| p.is_dir()) {
                continue;
            }
            if paths::write_atomic(&t, format!("{realmlist_text}\n").as_bytes()).is_ok() {
                ok = true;
            }
        }
        ok
    }

    // --- Migrations -----------------------------------------------------------

    fn migrate_from_edition_config(&mut self) -> bool {
        let mut migrated = false;
        if let Some(editions) = self.values.remove("editions") {
            if let Some(wotlk) = editions.get("wotlk").and_then(Value::as_object) {
                for key in [
                    "wow_path",
                    "realmlist_entries",
                    "active_realmlist_index",
                    "background",
                ] {
                    if let Some(v) = wotlk.get(key) {
                        self.values
                            .entry(key.to_string())
                            .or_insert_with(|| v.clone());
                    }
                }
            }
            migrated = true;
        }
        if self.values.remove("active_edition").is_some() {
            migrated = true;
        }
        let empty = self
            .values
            .get("realmlist_entries")
            .and_then(Value::as_array)
            .is_none_or(|a| a.is_empty());
        if empty {
            self.values
                .insert("realmlist_entries".into(), default_realmlist());
            self.values
                .insert("active_realmlist_index".into(), json!(0));
            migrated = true;
        }
        if !self.values.contains_key("active_realmlist_index") {
            self.values
                .insert("active_realmlist_index".into(), json!(0));
            migrated = true;
        }
        migrated
    }

    /// Rewrites stale hosts from older launcher builds to the canonical one.
    fn migrate_legacy_realmlist(&mut self) -> bool {
        const LEGACY: [&str; 3] = [
            "logon.sylvania-wow.com",
            "sylvania-wow.com",
            "logon.sylvania.com",
        ];
        let mut entries = self.realmlist_entries();
        let mut changed = false;
        for e in entries.iter_mut() {
            let lower = e.address.to_lowercase();
            if LEGACY.iter().any(|l| lower.contains(l)) {
                log::warn!("Realmlist legacy migré: {}", e.address);
                e.address = constants::DEFAULT_REALMLIST.to_string();
                changed = true;
            }
        }
        if changed {
            let arr: Vec<Value> = entries
                .iter()
                .map(|e| json!({"name": e.name, "address": e.address, "active": e.active}))
                .collect();
            self.values
                .insert("realmlist_entries".into(), Value::Array(arr));
        }
        changed
    }
}

fn default_realmlist() -> Value {
    json!([{
        "name": constants::DEFAULT_REALM_NAME,
        "address": constants::DEFAULT_REALMLIST,
        "active": true
    }])
}

fn default_values() -> Map<String, Value> {
    let v = json!({
        "sound_enabled": true,
        "download_method": "direct",
        "max_upload_rate": 100,
        "max_download_rate": 0,
        "first_run": true,
        "auto_update": true,
        "language": "fr",
        "wow_path": "",
        "realmlist_entries": default_realmlist(),
        "active_realmlist_index": 0
    });
    v.as_object().cloned().unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn host_from_address_strips_prefix_and_quotes() {
        assert_eq!(
            Config::host_from_address("set realmlist sylvania-servergame.com"),
            "sylvania-servergame.com"
        );
        assert_eq!(Config::host_from_address("SET  RealmList \"a.b\""), "a.b");
        assert_eq!(Config::host_from_address("  1.2.3.4 "), "1.2.3.4");
    }

    #[test]
    fn flattens_v3_edition_config() {
        let mut c = Config {
            values: Map::new(),
            path: PathBuf::new(),
        };
        c.values = serde_json::from_str::<Value>(
            r#"{"active_edition":"wotlk","editions":{"legion":{"wow_path":"E:/Legion"},
               "wotlk":{"wow_path":"E:/wotlk","background":"Taverne",
               "realmlist_entries":[{"name":"S","address":"set realmlist x","active":true}],
               "active_realmlist_index":0}}}"#,
        )
        .unwrap()
        .as_object()
        .cloned()
        .unwrap();
        assert!(c.migrate_from_edition_config());
        assert_eq!(c.wow_path(), "E:/wotlk");
        assert_eq!(c.background(), "Taverne");
        assert!(!c.values.contains_key("editions") && !c.values.contains_key("active_edition"));
        assert_eq!(c.realmlist_entries().len(), 1);
    }
}
