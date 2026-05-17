#pragma once

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

}  // namespace SylvaniaConstants
