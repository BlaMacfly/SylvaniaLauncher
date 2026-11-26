//! Centralised tunable values. Add new constants here rather than scattering
//! literals through the codebase.

// --- Main window ------------------------------------------------------------
pub const MAIN_WINDOW_WIDTH: f32 = 920.0;
pub const MAIN_WINDOW_HEIGHT: f32 = 580.0;
pub const MAIN_WINDOW_MIN_WIDTH: f32 = 820.0;
pub const MAIN_WINDOW_MIN_HEIGHT: f32 = 540.0;
pub const LOGO_HEIGHT: f32 = 92.0;

// --- Spacing scale (px) -----------------------------------------------------
pub const SPACE_XS: f32 = 4.0;
pub const SPACE_SM: f32 = 8.0;
pub const SPACE_MD: f32 = 16.0;
pub const SPACE_LG: f32 = 24.0;

// --- Button system (see ui::widgets) ----------------------------------------
pub const BUTTON_HEIGHT_PRIMARY: f32 = 48.0;
pub const BUTTON_HEIGHT_SECONDARY: f32 = 36.0;
pub const BUTTON_HEIGHT_TERTIARY: f32 = 28.0;
pub const BUTTON_RADIUS: f32 = 8.0;

// --- Timers (milliseconds) --------------------------------------------------
pub const PLAY_TIME_CHECK_MS: u64 = 5_000; // poll Wow.exe status
pub const REMINDER_CHECK_MS: u64 = 30_000; // due-reminder poll

// --- Limits -----------------------------------------------------------------
pub const MAX_IMPORT_BYTES: u64 = 10 * 1024 * 1024; // notes import cap (10 MB)
/// Free disk space required before downloading a client, as a multiple of the
/// archive size (archive + extracted tree coexist during installation).
pub const CLIENT_DISK_SPACE_FACTOR: f64 = 2.5;

// --- Segmented download -----------------------------------------------------
pub const DOWNLOAD_SEGMENT_BYTES: u64 = 32 * 1024 * 1024; // 32 MiB
pub const DOWNLOAD_SEGMENT_RETRIES: u32 = 5;

// --- Network ----------------------------------------------------------------
pub const SERVER_HOST: &str = "sylvania-servergame.com";
pub const HD_PATCH_URL: &str = "https://sylvania-servergame.com/patch-hd-download.php";
pub const ADDON_DOWNLOAD_ENDPOINT: &str = "https://sylvania-servergame.com/download_addon.php";
/// Remote "recommended addons" manifest; falls back to the embedded copy.
pub const ADDON_MANIFEST_URL: &str = "https://sylvania-servergame.com/launcher/addons.json";

// --- WotLK 3.3.5a client ----------------------------------------------------
pub const CLIENT_URL: &str = "https://sylvania-servergame.com/launcher-download.php";
pub const ENUS_PACK_URL: &str = "https://sylvania-servergame.com/enus-download.php";
/// Integrity data not published by the endpoint yet: 0/empty = "unknown".
/// Filling these in makes the download size+hash enforced automatically.
pub const CLIENT_EXPECTED_SIZE: u64 = 0;
pub const CLIENT_SHA256: &str = "";

pub const CLIENT_DISPLAY_NAME: &str = "WoW 3.3.5a";
pub const WINDOW_TITLE: &str = "Sylvania Launcher - World of Warcraft 3.3.5";
pub const CLIENT_EXE_NAME: &str = "Wow.exe";
pub const DEFAULT_REALM_NAME: &str = "Sylvania";
pub const DEFAULT_REALMLIST: &str = "set realmlist sylvania-servergame.com";
pub const APP_VERSION: &str = "3.1";

// --- Theme ------------------------------------------------------------------
pub const ACCENT_COLOR: &str = "#d4af37"; // WoW gold
pub const BACKGROUNDS: [&str; 8] = [
    "Alliance",
    "Arbre de Vie",
    "Azeroth",
    "Horde",
    "Ilidan",
    "Lich King",
    "Ragnaros",
    "Taverne",
];
pub const DEFAULT_BACKGROUND: &str = "Azeroth";
