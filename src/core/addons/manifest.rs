//! "Recommended addons" manifest.
//!
//! Fetches `constants::ADDON_MANIFEST_URL` on a worker thread and, if that
//! fails or returns nothing usable, falls back to the copy embedded in the
//! executable (`assets::ADDONS_MANIFEST_JSON`) so the list always renders —
//! even offline. Adding an addon later is a server-side manifest edit.

use serde_json::Value;
use std::sync::mpsc;
use std::time::Duration;

/// Timeout of the manifest request (small JSON document).
pub const FETCH_TIMEOUT: Duration = Duration::from_secs(10);

/// One entry of the manifest.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct ManifestAddon {
    pub id: String,
    pub name: String,
    pub version: String,
    pub description: String,
    /// Download URL (may be a `?file=…` PHP endpoint).
    pub url: String,
    /// Folders expected under Interface/AddOns.
    pub folders: Vec<String>,
}

/// Result of [`fetch_blocking`] / [`spawn_fetch`].
#[derive(Debug, Clone)]
pub struct ManifestLoad {
    pub addons: Vec<ManifestAddon>,
    /// false = the embedded fallback was used.
    pub from_remote: bool,
}

fn str_field(obj: &serde_json::Map<String, Value>, key: &str) -> String {
    obj.get(key).and_then(Value::as_str).unwrap_or_default().to_string()
}

/// Parses a manifest document `{"addons": [...]}`. Malformed documents yield
/// an empty list; entries without an id, a URL or at least one folder are
/// skipped (they could be neither installed nor uninstalled).
pub fn parse(json: &str) -> Vec<ManifestAddon> {
    let Ok(Value::Object(doc)) = serde_json::from_str::<Value>(json) else {
        return Vec::new();
    };
    let Some(Value::Array(items)) = doc.get("addons") else {
        return Vec::new();
    };
    items
        .iter()
        .filter_map(Value::as_object)
        .map(|obj| ManifestAddon {
            id: str_field(obj, "id"),
            name: str_field(obj, "name"),
            version: str_field(obj, "version"),
            description: str_field(obj, "description"),
            url: str_field(obj, "url"),
            folders: obj
                .get("folders")
                .and_then(Value::as_array)
                .map(|a| a.iter().filter_map(Value::as_str).filter(|f| !f.is_empty()).map(String::from).collect())
                .unwrap_or_default(),
        })
        .filter(|a| !a.id.is_empty() && !a.url.is_empty() && !a.folders.is_empty())
        .collect()
}

/// The manifest bundled in the executable (offline fallback).
pub fn embedded() -> Vec<ManifestAddon> {
    parse(crate::assets::ADDONS_MANIFEST_JSON)
}

/// Picks the remote document when it is usable, else the embedded copy.
pub fn resolve(remote: Result<String, String>) -> ManifestLoad {
    match remote {
        Ok(text) => {
            let addons = parse(&text);
            if !addons.is_empty() {
                return ManifestLoad { addons, from_remote: true };
            }
            log::warn!("Manifeste d'addons distant inutilisable, copie embarquée utilisée");
        }
        Err(e) => log::warn!("Manifeste d'addons injoignable ({e}), copie embarquée utilisée"),
    }
    ManifestLoad { addons: embedded(), from_remote: false }
}

/// Blocking fetch (worker threads only).
pub fn fetch_blocking(url: &str) -> ManifestLoad {
    resolve(crate::util::http::get_text(url, FETCH_TIMEOUT))
}

/// Fetches `constants::ADDON_MANIFEST_URL` on a worker thread; the result
/// arrives exactly once on the returned channel.
pub fn spawn_fetch() -> mpsc::Receiver<ManifestLoad> {
    let (tx, rx) = mpsc::channel();
    std::thread::Builder::new()
        .name("addons-manifest".into())
        .spawn(move || {
            let _ = tx.send(fetch_blocking(crate::constants::ADDON_MANIFEST_URL));
        })
        .ok();
    rx
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn embedded_manifest_is_usable() {
        let addons = embedded();
        assert!(!addons.is_empty());
        for a in &addons {
            assert!(!a.id.is_empty() && !a.url.is_empty() && !a.folders.is_empty());
            assert!(crate::core::addons::install::is_trusted_url(&a.url), "{}", a.url);
        }
        let elvui = addons.iter().find(|a| a.id == "elvui").expect("elvui");
        assert_eq!(elvui.name, "ElvUI");
        assert_eq!(elvui.version, "6.08");
        assert!(elvui.folders.len() > 1);
    }

    #[test]
    fn parse_skips_unusable_entries() {
        let json = r#"{"addons": [
            {"id": "a", "name": "A", "version": "1.0", "description": "d",
             "url": "https://x/a.zip", "folders": ["A", "", 3, "A_Extra"]},
            {"id": "", "url": "https://x/b.zip", "folders": ["B"]},
            {"id": "c", "url": "", "folders": ["C"]},
            {"id": "d", "url": "https://x/d.zip", "folders": []},
            {"id": "e", "url": "https://x/e.zip"},
            "garbage",
            {"id": "f", "name": 12, "url": "https://x/f.zip", "folders": ["F"]}
        ]}"#;
        let addons = parse(json);
        assert_eq!(addons.len(), 2);
        assert_eq!(addons[0].folders, vec!["A".to_string(), "A_Extra".to_string()]);
        assert_eq!(addons[0].version, "1.0");
        assert_eq!(addons[1].id, "f");
        assert_eq!(addons[1].name, "");
    }

    #[test]
    fn parse_rejects_malformed_documents() {
        assert!(parse("").is_empty());
        assert!(parse("not json").is_empty());
        assert!(parse("[1, 2]").is_empty());
        assert!(parse(r#"{"addons": {}}"#).is_empty());
        assert!(parse(r#"{"other": []}"#).is_empty());
    }

    #[test]
    fn resolve_falls_back_to_embedded() {
        let offline = resolve(Err("timeout".into()));
        assert!(!offline.from_remote);
        assert_eq!(offline.addons, embedded());

        let empty = resolve(Ok(r#"{"addons": []}"#.into()));
        assert!(!empty.from_remote);
        assert!(!empty.addons.is_empty());

        let remote = resolve(Ok(r#"{"addons": [{"id": "x", "url": "https://h/x.zip", "folders": ["X"]}]}"#.into()));
        assert!(remote.from_remote);
        assert_eq!(remote.addons.len(), 1);
    }
}
