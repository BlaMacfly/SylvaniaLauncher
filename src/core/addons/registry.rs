//! Registry of addons installed *by the launcher*.
//!
//! Persisted as `installed_addons.json` in `paths::config_dir()`, with the
//! same schema as the Qt version: an object keyed by addon id whose values
//! are `{"id", "name", "version", "source", "folders": [...]}`. `folders`
//! holds the real folder names dropped into Interface/AddOns, so a
//! multi-folder addon is removed cleanly in one go and updates are detected.

use serde_json::{Map, Value, json};
use std::collections::BTreeMap;
use std::path::PathBuf;

pub const FILE_NAME: &str = "installed_addons.json";

/// One record of the registry.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct InstalledAddon {
    pub id: String,
    pub name: String,
    pub version: String,
    pub folders: Vec<String>,
    /// "recommended" | "catalog" (informational).
    pub source: String,
}

#[derive(Debug, Clone)]
pub struct AddonRegistry {
    path: PathBuf,
    /// id -> raw record (sorted by id, like QJsonObject). Raw values are kept
    /// so unknown fields survive a rewrite.
    addons: BTreeMap<String, Value>,
}

impl AddonRegistry {
    /// The registry in the launcher config directory, loaded.
    pub fn new() -> Self {
        Self::open(Self::default_path())
    }

    /// A registry stored at `path`, loaded (missing/corrupt file = empty).
    pub fn open(path: PathBuf) -> Self {
        let mut r = AddonRegistry {
            path,
            addons: BTreeMap::new(),
        };
        r.reload();
        r
    }

    pub fn default_path() -> PathBuf {
        crate::paths::config_dir().join(FILE_NAME)
    }

    pub fn reload(&mut self) {
        self.addons = std::fs::read(&self.path)
            .ok()
            .and_then(|bytes| serde_json::from_slice::<Value>(&bytes).ok())
            .and_then(|v| match v {
                Value::Object(map) => Some(map.into_iter().collect()),
                _ => None,
            })
            .unwrap_or_default();
    }

    pub fn contains(&self, id: &str) -> bool {
        self.addons.contains_key(id)
    }

    pub fn get(&self, id: &str) -> InstalledAddon {
        from_json(id, self.addons.get(id))
    }

    pub fn installed_version(&self, id: &str) -> String {
        self.addons
            .get(id)
            .and_then(|v| v.get("version"))
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_string()
    }

    pub fn folders(&self, id: &str) -> Vec<String> {
        self.get(id).folders
    }

    pub fn all(&self) -> Vec<InstalledAddon> {
        self.addons
            .iter()
            .map(|(id, v)| from_json(id, Some(v)))
            .collect()
    }

    /// Inserts or replaces a record, then persists atomically. Returns false
    /// if the id is empty or the file could not be written.
    pub fn record(&mut self, addon: &InstalledAddon) -> bool {
        if addon.id.is_empty() {
            return false;
        }
        self.addons.insert(addon.id.clone(), to_json(addon));
        self.save()
    }

    /// Removes a record, then persists atomically. No-op (true) if absent.
    pub fn remove(&mut self, id: &str) -> bool {
        if self.addons.remove(id).is_none() {
            return true;
        }
        self.save()
    }

    fn save(&self) -> bool {
        let map: Map<String, Value> = self
            .addons
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();
        // 4-space indentation, like QJsonDocument::Indented.
        let mut out = Vec::new();
        let fmt = serde_json::ser::PrettyFormatter::with_indent(b"    ");
        let mut ser = serde_json::Serializer::with_formatter(&mut out, fmt);
        if serde::Serialize::serialize(&Value::Object(map), &mut ser).is_err() {
            return false;
        }
        out.push(b'\n');
        match crate::paths::write_atomic(&self.path, &out) {
            Ok(()) => true,
            Err(e) => {
                log::error!("Écriture de {} impossible : {e}", self.path.display());
                false
            }
        }
    }
}

impl Default for AddonRegistry {
    fn default() -> Self {
        Self::new()
    }
}

fn from_json(id: &str, value: Option<&Value>) -> InstalledAddon {
    let s = |key: &str| {
        value
            .and_then(|v| v.get(key))
            .and_then(Value::as_str)
            .map(String::from)
    };
    InstalledAddon {
        id: s("id").unwrap_or_else(|| id.to_string()),
        name: s("name").unwrap_or_default(),
        version: s("version").unwrap_or_default(),
        source: s("source").unwrap_or_default(),
        folders: value
            .and_then(|v| v.get("folders"))
            .and_then(Value::as_array)
            .map(|a| {
                a.iter()
                    .filter_map(Value::as_str)
                    .filter(|f| !f.is_empty())
                    .map(String::from)
                    .collect()
            })
            .unwrap_or_default(),
    }
}

fn to_json(a: &InstalledAddon) -> Value {
    json!({
        "folders": a.folders,
        "id": a.id,
        "name": a.name,
        "source": a.source,
        "version": a.version,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample(id: &str) -> InstalledAddon {
        InstalledAddon {
            id: id.into(),
            name: format!("Name {id}"),
            version: "1.2".into(),
            folders: vec![format!("{id}_A"), format!("{id}_B")],
            source: "recommended".into(),
        }
    }

    #[test]
    fn round_trip_on_disk() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("sub").join(FILE_NAME);
        let mut reg = AddonRegistry::open(path.clone());
        assert!(reg.all().is_empty());
        assert!(!reg.contains("zeta"));

        assert!(reg.record(&sample("zeta")));
        assert!(reg.record(&sample("alpha")));
        assert!(!reg.record(&InstalledAddon::default()));

        let reloaded = AddonRegistry::open(path.clone());
        let all = reloaded.all();
        assert_eq!(
            all.iter().map(|a| a.id.as_str()).collect::<Vec<_>>(),
            ["alpha", "zeta"]
        );
        assert_eq!(reloaded.get("zeta"), sample("zeta"));
        assert_eq!(reloaded.installed_version("alpha"), "1.2");
        assert_eq!(
            reloaded.folders("alpha"),
            vec!["alpha_A".to_string(), "alpha_B".to_string()]
        );

        // Qt-compatible schema.
        let raw: Value = serde_json::from_slice(&std::fs::read(&path).unwrap()).unwrap();
        assert_eq!(raw["zeta"]["source"], "recommended");
        assert_eq!(raw["zeta"]["folders"][1], "zeta_B");

        let mut reg = reloaded;
        assert!(reg.remove("zeta"));
        assert!(reg.remove("absent"));
        assert!(!AddonRegistry::open(path).contains("zeta"));
    }

    #[test]
    fn reads_qt_file_and_tolerates_garbage() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILE_NAME);
        std::fs::write(
            &path,
            r#"{"dbm": {"folders": ["DBM-Core", "", "DBM-GUI"], "name": "DBM", "source": "catalog", "version": ""},
                "odd": 42}"#,
        )
        .unwrap();
        let reg = AddonRegistry::open(path.clone());
        let dbm = reg.get("dbm");
        assert_eq!(dbm.id, "dbm"); // falls back to the key
        assert_eq!(
            dbm.folders,
            vec!["DBM-Core".to_string(), "DBM-GUI".to_string()]
        );
        assert!(reg.contains("odd"));
        assert!(reg.get("odd").folders.is_empty());

        std::fs::write(&path, "{corrupt").unwrap();
        assert!(AddonRegistry::open(path).all().is_empty());
    }
}
