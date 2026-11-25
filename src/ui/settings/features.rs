//! HD patch features shown in the settings window. A feature is a set of
//! files toggled together: `Data/patch-<letter>.mpq` (root) and
//! `Data/<loc>/patch-<loc>-<letter>.mpq` (every locale), or DXVK
//! (`d3d9.dll` + `dxvk.conf`). Disabling RENAMES a file to `<name>.disabled`,
//! enabling renames it back: nothing is ever deleted.
//!
//! Every path is built from the client folder plus the constant letters and
//! locales below, so no operation can reach outside the client.

use std::path::{Path, PathBuf};

/// Locales shipped by the HD pack.
pub const LOCALES: [&str; 5] = ["frFR", "enUS", "deDE", "esES", "ruRU"];

const MOP: &str = "Création de perso (Mists of Pandaria)";
const SL: &str = "Création de perso (Shadowlands)";

#[derive(Debug)]
pub struct FeatureDef {
    /// French source label (translated at display time).
    pub label: &'static str,
    /// `Data/patch-<letter>.mpq`
    pub root: &'static [&'static str],
    /// `Data/<loc>/patch-<loc>-<letter>.mpq` for every locale.
    pub locale: &'static [&'static str],
    /// Special: `d3d9.dll` (+ `dxvk.conf`).
    pub dxvk: bool,
    /// Embeds a .dbc → risk of detection / crash.
    pub dangerous: bool,
    /// Recommended state (decides which side of an exclusive pair wins).
    pub default_on: bool,
    /// Label of a mutually exclusive feature.
    pub exclusive_with: Option<&'static str>,
}

const fn f(
    label: &'static str,
    root: &'static [&'static str],
    locale: &'static [&'static str],
    dangerous: bool,
    default_on: bool,
) -> FeatureDef {
    FeatureDef { label, root, locale, dxvk: false, dangerous, default_on, exclusive_with: None }
}

/// Catalogue from the pack's technical README (§4/§5/§6): a letter does NOT
/// mean the same thing at the root and in a locale, so each feature lists
/// its exact files.
pub const FEATURES: &[FeatureDef] = &[
    FeatureDef {
        label: "DXVK 'API Vulkan'",
        root: &[],
        locale: &[],
        dxvk: true,
        dangerous: false,
        default_on: false,
        exclusive_with: None,
    },
    f("Personnages HD", &["d"], &[], false, true),
    f("Modèles HD et PNJ", &["f"], &["f"], true, true),
    f("Textures du monde HD", &["6"], &[], false, true),
    f("Nouvelle eau", &["w"], &["w"], true, true),
    f("Textures du ciel", &["x"], &["x"], true, true),
    f("Nouveaux sorts", &["s"], &[], true, true),
    f("Écrans, serveurs, comptes", &["c"], &["c"], false, true),
    f("Écrans de chargement", &[], &["q"], true, true),
    f("Curseur et Fenêtres d'interface", &[], &["d"], false, true),
    f("Cartes des donjons", &[], &["m"], true, true),
    f("Addons (ACP)", &["5"], &["9"], false, true),
    FeatureDef { exclusive_with: Some(MOP), ..f(SL, &["k"], &["k"], true, true) },
    // --- OFF by default (heavier / "dangerous" / optional) ---
    f("Armures HD", &["7"], &[], true, false),
    f("Armes HD", &["8"], &[], false, false),
    f("Nouvelles icônes", &["i"], &[], false, false),
    f("Nouveaux arbres", &["t"], &[], false, false),
    f("Mort-vivant sans os", &["u"], &["u", "a"], true, false),
    FeatureDef { exclusive_with: Some(SL), ..f(MOP, &["j"], &["j"], true, false) },
    f("Cartes des mini-donjons", &[], &["n"], true, false),
    f("Sang", &["b"], &[], true, false),
    // ARAC = All Races All Classes (root patch-a.mpq). Independent of the HD
    // pack; modifies race/class .dbc tables. Distinct from the locale "-a"
    // used by "Mort-vivant sans os".
    f("ARAC (toutes races / classes)", &["a"], &[], true, false),
];

pub fn root_patch_path(wow: &Path, letter: &str) -> PathBuf {
    wow.join("Data").join(format!("patch-{letter}.mpq"))
}

pub fn locale_patch_path(wow: &Path, locale: &str, letter: &str) -> PathBuf {
    wow.join("Data").join(locale).join(format!("patch-{locale}-{letter}.mpq"))
}

/// `<path>.disabled`
pub fn disabled_path(active: &Path) -> PathBuf {
    let mut s = active.as_os_str().to_owned();
    s.push(".disabled");
    PathBuf::from(s)
}

const DXVK_FILES: [&str; 2] = ["d3d9.dll", "dxvk.conf"];

/// The file whose state represents the feature, per locale for locale-only
/// features (first letter only, like the Qt version).
fn marker_paths(wow: &Path, f: &FeatureDef) -> Vec<PathBuf> {
    if f.dxvk {
        vec![wow.join(DXVK_FILES[0])]
    } else if let Some(letter) = f.root.first() {
        vec![root_patch_path(wow, letter)]
    } else if let Some(letter) = f.locale.first() {
        LOCALES.iter().map(|loc| locale_patch_path(wow, loc, letter)).collect()
    } else {
        Vec::new()
    }
}

/// Installed, enabled or not.
pub fn is_present(wow: &Path, f: &FeatureDef) -> bool {
    marker_paths(wow, f).iter().any(|p| p.exists() || disabled_path(p).exists())
}

/// Installed AND enabled.
pub fn is_enabled(wow: &Path, f: &FeatureDef) -> bool {
    marker_paths(wow, f).iter().any(|p| p.exists())
}

/// Enables (`x.disabled` → `x`) or disables (`x` → `x.disabled`) one file.
/// A no-op when the file is already in the requested state or absent.
pub fn toggle_file(active: &Path, enabled: bool) -> std::io::Result<()> {
    let disabled = disabled_path(active);
    if enabled {
        if !active.exists() && disabled.exists() {
            std::fs::rename(&disabled, active)?;
        }
    } else if active.exists() {
        // Replaces a stale `.disabled` copy left by an older pack, like Qt.
        std::fs::rename(active, &disabled)?;
    }
    Ok(())
}

/// Applies a feature state to its whole file set. Returns the failures
/// (typically files locked by a running game).
pub fn apply(wow: &Path, f: &FeatureDef, enabled: bool) -> Vec<(PathBuf, std::io::Error)> {
    let mut targets: Vec<PathBuf> = Vec::new();
    if f.dxvk {
        targets.extend(DXVK_FILES.iter().map(|n| wow.join(n)));
    } else {
        targets.extend(f.root.iter().map(|l| root_patch_path(wow, l)));
        for letter in f.locale {
            for loc in LOCALES {
                if wow.join("Data").join(loc).is_dir() {
                    targets.push(locale_patch_path(wow, loc, letter));
                }
            }
        }
    }
    targets
        .into_iter()
        .filter_map(|p| toggle_file(&p, enabled).err().map(|e| (p, e)))
        .collect()
}

/// State a feature takes when the master toggle is switched: everything
/// follows the master, except that only the preferred side of an exclusive
/// pair (MoP vs SL character creation) is turned on.
pub fn master_target(f: &FeatureDef, master_on: bool) -> bool {
    master_on && !(f.exclusive_with.is_some() && !f.default_on)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn feature(label: &str) -> &'static FeatureDef {
        FEATURES.iter().find(|f| f.label == label).unwrap()
    }

    fn touch(p: &Path) {
        fs::create_dir_all(p.parent().unwrap()).unwrap();
        fs::write(p, b"mpq").unwrap();
    }

    #[test]
    fn catalogue_is_consistent() {
        for f in FEATURES {
            if let Some(other) = f.exclusive_with {
                let o = feature(other);
                assert_eq!(o.exclusive_with, Some(f.label), "{}", f.label);
                assert_ne!(o.default_on, f.default_on, "{}", f.label);
            }
        }
        let mut labels: Vec<_> = FEATURES.iter().map(|f| f.label).collect();
        labels.sort();
        labels.dedup();
        assert_eq!(labels.len(), FEATURES.len());
    }

    #[test]
    fn detects_root_feature_state() {
        let tmp = tempfile::tempdir().unwrap();
        let wow = tmp.path();
        let f = feature("Personnages HD");
        assert!(!is_present(wow, f));

        touch(&disabled_path(&root_patch_path(wow, "d")));
        assert!(is_present(wow, f));
        assert!(!is_enabled(wow, f));

        assert!(apply(wow, f, true).is_empty());
        assert!(root_patch_path(wow, "d").is_file());
        assert!(!disabled_path(&root_patch_path(wow, "d")).exists());
        assert!(is_enabled(wow, f));
    }

    #[test]
    fn locale_feature_renames_in_existing_locales_only() {
        let tmp = tempfile::tempdir().unwrap();
        let wow = tmp.path();
        let f = feature("Mort-vivant sans os"); // root u + locale u, a
        touch(&root_patch_path(wow, "u"));
        touch(&locale_patch_path(wow, "frFR", "u"));
        touch(&locale_patch_path(wow, "frFR", "a"));
        assert!(is_present(wow, f) && is_enabled(wow, f));

        assert!(apply(wow, f, false).is_empty());
        for p in [root_patch_path(wow, "u"), locale_patch_path(wow, "frFR", "u"), locale_patch_path(wow, "frFR", "a")] {
            assert!(!p.exists());
            assert!(disabled_path(&p).is_file(), "{}", p.display());
        }
        assert!(!wow.join("Data").join("enUS").exists());
        assert!(is_present(wow, f) && !is_enabled(wow, f));

        assert!(apply(wow, f, true).is_empty());
        assert!(locale_patch_path(wow, "frFR", "a").is_file());
    }

    #[test]
    fn locale_only_feature_detected_in_any_locale() {
        let tmp = tempfile::tempdir().unwrap();
        let wow = tmp.path();
        let f = feature("Écrans de chargement");
        touch(&disabled_path(&locale_patch_path(wow, "ruRU", "q")));
        assert!(is_present(wow, f));
        assert!(!is_enabled(wow, f));
    }

    #[test]
    fn dxvk_toggles_dll_and_conf() {
        let tmp = tempfile::tempdir().unwrap();
        let wow = tmp.path();
        let f = feature("DXVK 'API Vulkan'");
        touch(&wow.join("d3d9.dll"));
        touch(&wow.join("dxvk.conf"));
        assert!(apply(wow, f, false).is_empty());
        assert!(wow.join("d3d9.dll.disabled").is_file() && wow.join("dxvk.conf.disabled").is_file());
        assert!(is_present(wow, f) && !is_enabled(wow, f));
        assert!(apply(wow, f, true).is_empty());
        assert!(wow.join("d3d9.dll").is_file() && wow.join("dxvk.conf").is_file());
    }

    #[test]
    fn disabling_replaces_stale_disabled_copy_and_never_deletes() {
        let tmp = tempfile::tempdir().unwrap();
        let active = tmp.path().join("patch-x.mpq");
        fs::write(&active, b"new").unwrap();
        fs::write(disabled_path(&active), b"old").unwrap();
        toggle_file(&active, false).unwrap();
        assert!(!active.exists());
        assert_eq!(fs::read(disabled_path(&active)).unwrap(), b"new");
        // Absent file: no-op either way.
        toggle_file(&tmp.path().join("none.mpq"), true).unwrap();
        toggle_file(&tmp.path().join("none.mpq"), false).unwrap();
    }

    #[test]
    fn master_keeps_only_preferred_exclusive_side() {
        assert!(master_target(feature(SL), true));
        assert!(!master_target(feature(MOP), true));
        assert!(master_target(feature("Sang"), true));
        assert!(FEATURES.iter().all(|f| !master_target(f, false)));
    }
}
