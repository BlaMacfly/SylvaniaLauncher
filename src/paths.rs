//! Filesystem locations. They match the Qt build exactly
//! (`QStandardPaths::AppLocalDataLocation` with organization "Sylvania" and
//! application "Sylvania Launcher"), so existing user data is picked up.

use std::path::PathBuf;

/// `%LOCALAPPDATA%/Sylvania/Sylvania Launcher` (created on demand).
pub fn app_data_dir() -> PathBuf {
    let base = dirs::data_local_dir().unwrap_or_else(std::env::temp_dir);
    let dir = base.join("Sylvania").join("Sylvania Launcher");
    let _ = std::fs::create_dir_all(&dir);
    dir
}

/// Directory holding config.json and installed_addons.json.
pub fn config_dir() -> PathBuf {
    app_data_dir()
}

/// Directory holding notes, categories and stats (created on demand).
pub fn data_dir() -> PathBuf {
    let dir = app_data_dir().join("data");
    let _ = std::fs::create_dir_all(&dir);
    dir
}

/// Directory for log files (created on demand).
pub fn logs_dir() -> PathBuf {
    let dir = app_data_dir().join("logs");
    let _ = std::fs::create_dir_all(&dir);
    dir
}

/// Writes `bytes` to `path` atomically (temp file in the same directory, then
/// rename), so a crash mid-write never leaves a half-written file.
pub fn write_atomic(path: &std::path::Path, bytes: &[u8]) -> std::io::Result<()> {
    use std::io::Write;
    let dir = path.parent().unwrap_or_else(|| std::path::Path::new("."));
    std::fs::create_dir_all(dir)?;
    let mut tmp = tempfile::NamedTempFile::new_in(dir)?;
    tmp.write_all(bytes)?;
    tmp.as_file().sync_all()?;
    tmp.persist(path).map_err(|e| e.error)?;
    Ok(())
}
