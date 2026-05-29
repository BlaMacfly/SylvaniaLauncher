#pragma once

#include <QStringList>

// Centralised tunable values previously hardcoded across the UI / managers.
// Add new constants here rather than scattering literals through the codebase.

namespace SylvaniaConstants {

// --- Main window --------------------------------------------------------
constexpr int kMainWindowWidth = 920;
constexpr int kMainWindowHeight = 580;
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

// --- Archive extraction -------------------------------------------------
// Arguments passed to "powershell" to extract a ZIP. Prefers bsdtar
// (tar.exe, bundled on Windows 10 build 17063+): it is much faster than
// Expand-Archive and is free of the 260-char MAX_PATH limit that makes
// Expand-Archive fail on deep WoW paths (World\Maps\...). Falls back to
// Expand-Archive on older Windows. The source/destination paths are passed
// via the SYL_SRC / SYL_DST environment variables (set on the QProcess),
// never interpolated into the command -> no command-injection surface.
inline QStringList extractArchiveArgs() {
    return QStringList()
        << "-NoProfile"
        << "-ExecutionPolicy" << "Bypass"
        << "-Command"
        << "$ErrorActionPreference='Stop';"
           "$src=$env:SYL_SRC; $dst=$env:SYL_DST;"
           "if (-not (Test-Path -LiteralPath $dst)) {"
           " New-Item -ItemType Directory -Force -Path $dst | Out-Null };"
           "$ok=$false;"
           "if (Get-Command tar.exe -ErrorAction SilentlyContinue) {"
           " & tar.exe -xf $src -C $dst 2>$null;"
           " if ($LASTEXITCODE -eq 0) { $ok=$true } };"
           "if (-not $ok) { Expand-Archive -LiteralPath $src -DestinationPath $dst -Force }";
}

}  // namespace SylvaniaConstants
