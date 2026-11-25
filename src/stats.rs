//! Play statistics (`data/stats.json`). Reads both the flat format and the
//! v3.0 per-edition format (only the WotLK counters are kept); always writes
//! the flat format.

use crate::paths;
use chrono::NaiveDateTime;
use serde_json::{Value, json};

#[derive(Debug, Default, Clone)]
pub struct Stats {
    pub launch_count: u64,
    pub total_play_time: i64, // seconds
    pub last_session: Option<NaiveDateTime>,
}

const ISO: &str = "%Y-%m-%dT%H:%M:%S";

impl Stats {
    fn path() -> std::path::PathBuf {
        paths::data_dir().join("stats.json")
    }

    pub fn load() -> Self {
        let Some(root) = std::fs::read(Self::path())
            .ok()
            .and_then(|b| serde_json::from_slice::<Value>(&b).ok())
        else {
            return Stats::default();
        };
        let s = match root.get("editions") {
            Some(ed) => ed.get("wotlk").cloned().unwrap_or(Value::Null),
            None => root,
        };
        Stats {
            launch_count: s.get("launch_count").and_then(Value::as_u64).unwrap_or(0),
            total_play_time: s
                .get("total_play_time")
                .and_then(Value::as_i64)
                .unwrap_or(0),
            last_session: s
                .get("last_session")
                .and_then(Value::as_str)
                .and_then(|t| NaiveDateTime::parse_from_str(t, ISO).ok()),
        }
    }

    pub fn save(&self) {
        let v = json!({
            "launch_count": self.launch_count,
            "total_play_time": self.total_play_time,
            "last_session": self.last_session.map(|d| d.format(ISO).to_string()).unwrap_or_default(),
        });
        if let Ok(bytes) = serde_json::to_vec_pretty(&v) {
            let _ = paths::write_atomic(&Self::path(), &bytes);
        }
    }
}
