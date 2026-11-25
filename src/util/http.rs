//! Blocking HTTP helpers, meant to run on worker threads (never on the UI
//! thread). Progress and cancellation go through callbacks / an AtomicBool.

use crate::constants::APP_VERSION;
use std::io::{Read, Write};
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

pub fn client(timeout: Option<Duration>) -> reqwest::blocking::Client {
    let mut b = reqwest::blocking::Client::builder()
        .user_agent(format!("SylvaniaLauncher/{APP_VERSION}"))
        .connect_timeout(Duration::from_secs(20));
    // No overall timeout for big downloads; the caller passes one for small requests.
    b = b.timeout(timeout);
    b.build().expect("client HTTP")
}

/// Error text including the whole `source()` chain: reqwest's own message
/// omits the cause (DNS failure, connection refused, timeout…), which callers
/// match on to show a precise message.
pub fn error_chain(e: &dyn std::error::Error) -> String {
    let mut msg = e.to_string();
    let mut cur = e.source();
    while let Some(c) = cur {
        let part = c.to_string();
        if !msg.contains(&part) {
            msg.push_str(": ");
            msg.push_str(&part);
        }
        cur = c.source();
    }
    msg
}

/// GETs a small text resource (e.g. a JSON manifest).
pub fn get_text(url: &str, timeout: Duration) -> Result<String, String> {
    let resp = client(Some(timeout)).get(url).send().map_err(|e| error_chain(&e))?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status()));
    }
    resp.text().map_err(|e| error_chain(&e))
}

/// Outcome of [`download_to_file`].
#[derive(Debug)]
pub enum DownloadError {
    Cancelled,
    Http(String),
    Io(String),
}

impl std::fmt::Display for DownloadError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DownloadError::Cancelled => write!(f, "cancelled"),
            DownloadError::Http(e) | DownloadError::Io(e) => write!(f, "{e}"),
        }
    }
}

/// Streams `url` straight to `dest` (never buffered in memory).
/// `on_progress(received, total)` is called regularly; `total` is the
/// Content-Length when known. Setting `cancel` aborts and removes the file.
pub fn download_to_file(
    url: &str,
    dest: &Path,
    cancel: &AtomicBool,
    mut on_progress: impl FnMut(u64, Option<u64>),
) -> Result<u64, DownloadError> {
    let mut resp = client(None)
        .get(url)
        .send()
        .map_err(|e| DownloadError::Http(error_chain(&e)))?;
    if !resp.status().is_success() {
        return Err(DownloadError::Http(format!("HTTP {}", resp.status())));
    }
    let total = resp.content_length();
    if let Some(parent) = dest.parent() {
        std::fs::create_dir_all(parent).map_err(|e| DownloadError::Io(e.to_string()))?;
    }
    let mut file = std::fs::File::create(dest).map_err(|e| DownloadError::Io(e.to_string()))?;
    let mut buf = vec![0u8; 256 * 1024];
    let mut received = 0u64;
    on_progress(0, total);
    loop {
        if cancel.load(Ordering::Relaxed) {
            drop(file);
            let _ = std::fs::remove_file(dest);
            return Err(DownloadError::Cancelled);
        }
        let n = resp.read(&mut buf).map_err(|e| DownloadError::Http(error_chain(&e)))?;
        if n == 0 {
            break;
        }
        file.write_all(&buf[..n]).map_err(|e| DownloadError::Io(e.to_string()))?;
        received += n as u64;
        on_progress(received, total);
    }
    file.flush().map_err(|e| DownloadError::Io(e.to_string()))?;
    Ok(received)
}

/// SHA-256 of a file, lowercase hex.
pub fn sha256_file(path: &Path) -> std::io::Result<String> {
    use sha2::{Digest, Sha256};
    let mut f = std::fs::File::open(path)?;
    let mut h = Sha256::new();
    let mut buf = vec![0u8; 1024 * 1024];
    loop {
        let n = f.read(&mut buf)?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }
    Ok(hex::encode(h.finalize()))
}
