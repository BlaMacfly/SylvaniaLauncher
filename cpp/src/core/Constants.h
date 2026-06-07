#pragma once

#include <QStringList>

// Centralised tunable values previously hardcoded across the UI / managers.
// Add new constants here rather than scattering literals through the codebase.

namespace SylvaniaConstants {

// --- Main window --------------------------------------------------------
// Default size on first launch / after "reset window size"; the window is now
// resizable, so these are no longer a hard cap.
constexpr int kMainWindowWidth = 920;
constexpr int kMainWindowHeight = 580;
// Smallest size at which the layout stays usable.
constexpr int kMainWindowMinWidth = 820;
constexpr int kMainWindowMinHeight = 540;
constexpr int kLogoSizePx = 140;

// --- Timers (milliseconds) ---------------------------------------------
constexpr int kPlayTimeCheckMs = 5000;        // MainWindow: poll Wow.exe status
constexpr int kExtractProgressMs = 5000;      // HdPatchManager: extraction progress
constexpr int kConfigSaveDebounceMs = 200;    // ConfigManager: coalesce setter writes
constexpr int kHashVerifyDelayMs = 500;       // HdPatchManager: handle release delay

// --- Limits -------------------------------------------------------------
constexpr long long kMaxImportBytes = 10LL * 1024 * 1024;  // notes import cap (10 MB)

// --- Network ------------------------------------------------------------
inline constexpr const char* kServerHost = "sylvania-servergame.com";
inline constexpr const char* kHdPatchUrl =
    "https://sylvania-servergame.com/patch-hd-download.php";
inline constexpr const char* kAddonDownloadEndpoint =
    "https://sylvania-servergame.com/download_addon.php";
// Remote manifest describing the "recommended addons" (id, version, download
// URL, expected folders). Editing it server-side adds an addon without a client
// rebuild; if it cannot be fetched the launcher falls back to the copy embedded
// in resources (:/addons/manifest).
inline constexpr const char* kAddonManifestUrl =
    "https://sylvania-servergame.com/launcher/addons.json";

// --- Archive extraction -------------------------------------------------
// Arguments passed to "powershell" to extract a ZIP.
//
// Expand-Archive is the PRIMARY method: it reads the ZIP central directory
// (.NET ZipArchive) and so correctly handles archives with data descriptors
// / ZIP64 that servers commonly produce. bsdtar (tar.exe) is much faster but
// aborts with "ZIP bad CRC" on exactly those archives, so it is only used as
// a FALLBACK when Expand-Archive fails (e.g. a path exceeding the 260-char
// MAX_PATH limit, where tar copes better).
//
// We deliberately do NOT set $ErrorActionPreference='Stop' globally: under
// Stop, a native command writing to stderr (tar) is turned into a fatal
// NativeCommandError, which previously aborted the script before the fallback
// could run. Expand-Archive uses a local -ErrorAction Stop inside try/catch
// instead, and tar's success is checked via $LASTEXITCODE.
//
// Paths flow through the SYL_SRC / SYL_DST environment variables (set on the
// QProcess), never interpolated into the command -> no injection surface.
inline QStringList extractArchiveArgs() {
    return QStringList()
        << "-NoProfile"
        << "-ExecutionPolicy" << "Bypass"
        << "-Command"
        << "$src=$env:SYL_SRC; $dst=$env:SYL_DST;"
           "if (-not (Test-Path -LiteralPath $dst)) {"
           " New-Item -ItemType Directory -Force -Path $dst | Out-Null };"
           "$ok=$false;"
           "try { Expand-Archive -LiteralPath $src -DestinationPath $dst -Force -ErrorAction Stop;"
           " $ok=$true } catch { $ok=$false };"
           "if (-not $ok -and (Get-Command tar.exe -ErrorAction SilentlyContinue)) {"
           " & tar.exe -xf $src -C $dst 2>$null;"
           " if ($LASTEXITCODE -eq 0) { $ok=$true } };"
           "if ($ok) { exit 0 } else { exit 1 }";
}

}  // namespace SylvaniaConstants
