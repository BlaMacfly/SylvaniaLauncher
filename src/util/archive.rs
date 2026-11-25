//! Pure-Rust ZIP extraction (replaces the PowerShell Expand-Archive / tar.exe
//! pipeline). Entries escaping the destination (`..`, absolute paths) are
//! rejected via `enclosed_name`, so a hostile archive can't write elsewhere.

use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};

/// Extracts `zip_path` into `dest`. `on_progress(done, total)` counts entries.
pub fn extract_zip(
    zip_path: &Path,
    dest: &Path,
    cancel: &AtomicBool,
    mut on_progress: impl FnMut(usize, usize),
) -> Result<(), String> {
    let file = std::fs::File::open(zip_path).map_err(|e| e.to_string())?;
    let mut archive =
        zip::ZipArchive::new(std::io::BufReader::new(file)).map_err(|e| e.to_string())?;
    std::fs::create_dir_all(dest).map_err(|e| e.to_string())?;
    let total = archive.len();
    for i in 0..total {
        if cancel.load(Ordering::Relaxed) {
            return Err("cancelled".into());
        }
        let mut entry = archive.by_index(i).map_err(|e| e.to_string())?;
        let Some(rel) = entry.enclosed_name() else {
            log::warn!(
                "Entrée d'archive ignorée (chemin hors destination): {:?}",
                entry.name()
            );
            continue;
        };
        let out = dest.join(rel);
        if entry.is_dir() {
            std::fs::create_dir_all(&out).map_err(|e| e.to_string())?;
        } else {
            if let Some(p) = out.parent() {
                std::fs::create_dir_all(p).map_err(|e| e.to_string())?;
            }
            let mut f =
                std::fs::File::create(&out).map_err(|e| format!("{}: {e}", out.display()))?;
            std::io::copy(&mut entry, &mut f).map_err(|e| format!("{}: {e}", out.display()))?;
        }
        on_progress(i + 1, total);
    }
    Ok(())
}

/// True if the file starts with the ZIP local-header magic ("PK\x03\x04").
pub fn is_zip_file(path: &Path) -> bool {
    use std::io::Read;
    let mut magic = [0u8; 4];
    std::fs::File::open(path)
        .and_then(|mut f| f.read_exact(&mut magic))
        .map(|_| magic == *b"PK\x03\x04")
        .unwrap_or(false)
}
