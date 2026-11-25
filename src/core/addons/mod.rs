//! Addons: remote/embedded manifest of recommended addons, the bundled
//! quick-install catalogue, the local registry of addons installed by the
//! launcher, and the filesystem side of install / uninstall (download,
//! extraction, deployment into `Interface/AddOns`, `.toc` reading).
//!
//! Everything here is UI-free; the window lives in `ui::addons`.

pub mod catalog;
pub mod install;
pub mod manifest;
pub mod registry;

use std::path::{Path, PathBuf};

/// Where the launcher installs to / scans for addons: `<wow>/Interface/AddOns`.
pub fn addons_dir(wow_path: &str) -> PathBuf {
    Path::new(wow_path).join("Interface").join("AddOns")
}

/// Where an install target comes from (stored as `source` in the registry).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Source {
    Recommended,
    Catalog,
}

impl Source {
    pub fn as_str(self) -> &'static str {
        match self {
            Source::Recommended => "recommended",
            Source::Catalog => "catalog",
        }
    }
}

/// Something the launcher can download + install, built from either a
/// manifest entry (recommended) or a catalogue entry.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InstallTarget {
    pub id: String,
    pub name: String,
    /// Empty if unknown (catalogue).
    pub version: String,
    /// Full download URL.
    pub url: String,
    /// Folders expected under Interface/AddOns.
    pub folders: Vec<String>,
    pub source: Source,
}

impl InstallTarget {
    pub fn from_manifest(a: &manifest::ManifestAddon) -> Self {
        InstallTarget {
            id: a.id.clone(),
            name: a.name.clone(),
            version: a.version.clone(),
            url: a.url.clone(),
            folders: a.folders.clone(),
            source: Source::Recommended,
        }
    }

    pub fn from_catalog(e: &catalog::CatalogEntry) -> Self {
        InstallTarget {
            id: e.folder.to_string(),
            name: e.name.to_string(),
            version: String::new(),
            url: catalog::download_url(e.file),
            folders: vec![e.folder.to_string()],
            source: Source::Catalog,
        }
    }
}

/// What an install card shows (Qt `refreshCard`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InstallState {
    /// Recommended, not installed.
    NotInstalled,
    /// Recommended, installed; version shown (may be empty).
    Installed(String),
    /// Recommended, the registry holds another version.
    UpdateAvailable { installed: String, available: String },
    /// Catalogue entry present (registry or disk).
    CatalogInstalled,
    /// Catalogue entry absent.
    CatalogAbsent,
}

/// State of `target` given the registry and the AddOns directory (`None` when
/// no WoW path is configured). Disk presence covers manual installs.
pub fn install_state(
    target: &InstallTarget,
    registry: &registry::AddonRegistry,
    addons_dir: Option<&Path>,
) -> InstallState {
    let in_registry = registry.contains(&target.id);
    let installed_ver = if in_registry { registry.installed_version(&target.id) } else { String::new() };
    let on_disk = addons_dir.is_some_and(|d| target.folders.iter().any(|f| install::folder_exists(d, f)));
    match target.source {
        Source::Recommended => {
            if in_registry && !target.version.is_empty() && !installed_ver.is_empty() && installed_ver != target.version
            {
                InstallState::UpdateAvailable { installed: installed_ver, available: target.version.clone() }
            } else if in_registry || on_disk {
                InstallState::Installed(if installed_ver.is_empty() { target.version.clone() } else { installed_ver })
            } else {
                InstallState::NotInstalled
            }
        }
        Source::Catalog => {
            if in_registry || on_disk {
                InstallState::CatalogInstalled
            } else {
                InstallState::CatalogAbsent
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::registry::{AddonRegistry, InstalledAddon};
    use super::*;

    #[test]
    fn card_states() {
        let dir = tempfile::tempdir().unwrap();
        let addons = dir.path().join("AddOns");
        std::fs::create_dir_all(addons.join("Manual")).unwrap();
        let mut reg = AddonRegistry::open(dir.path().join("reg.json"));
        let rec = |id: &str, version: &str, folder: &str| InstallTarget {
            id: id.into(),
            name: id.into(),
            version: version.into(),
            url: String::new(),
            folders: vec![folder.into()],
            source: Source::Recommended,
        };
        reg.record(&InstalledAddon { id: "old".into(), version: "1.0".into(), ..Default::default() });
        reg.record(&InstalledAddon { id: "nover".into(), ..Default::default() });

        let d = Some(addons.as_path());
        assert_eq!(install_state(&rec("x", "1.0", "X"), &reg, d), InstallState::NotInstalled);
        assert_eq!(
            install_state(&rec("old", "2.0", "Old"), &reg, d),
            InstallState::UpdateAvailable { installed: "1.0".into(), available: "2.0".into() }
        );
        assert_eq!(install_state(&rec("old", "1.0", "Old"), &reg, d), InstallState::Installed("1.0".into()));
        assert_eq!(install_state(&rec("nover", "3.0", "N"), &reg, d), InstallState::Installed("3.0".into()));
        assert_eq!(install_state(&rec("m", "", "Manual"), &reg, d), InstallState::Installed(String::new()));
        assert_eq!(install_state(&rec("m", "", "Manual"), &reg, None), InstallState::NotInstalled);

        let cat = InstallTarget::from_catalog(&catalog::CATALOG[0]);
        assert_eq!(cat.folders, vec!["ArkInventory".to_string()]);
        assert_eq!(install_state(&cat, &reg, d), InstallState::CatalogAbsent);
        std::fs::create_dir_all(addons.join("ArkInventory")).unwrap();
        assert_eq!(install_state(&cat, &reg, d), InstallState::CatalogInstalled);
    }
}
