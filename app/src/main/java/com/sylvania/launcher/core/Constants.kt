package com.sylvania.launcher.core

/**
 * Centralised tunable values, ported from the C++ launcher's `Constants.h`.
 * Server endpoints are kept identical so the Android launcher talks to the
 * same backend as the Windows build.
 */
object Constants {
    // --- Network endpoints (verbatim from the C++ launcher) ----------------
    const val SERVER_HOST = "sylvania-servergame.com"
    const val HD_PATCH_URL = "https://sylvania-servergame.com/patch-hd-download.php"
    const val CLIENT_DOWNLOAD_URL = "https://sylvania-servergame.com/launcher-download.php"
    const val ENUS_DOWNLOAD_URL = "https://sylvania-servergame.com/enus-download.php"
    const val ADDON_DOWNLOAD_ENDPOINT = "https://sylvania-servergame.com/download_addon.php"

    // --- Realmlist ---------------------------------------------------------
    /** Canonical realmlist line written into realmlist.wtf. */
    const val CANONICAL_REALMLIST = "set realmlist sylvania-servergame.com"

    /** Limits (ported). */
    const val MAX_IMPORT_BYTES = 10L * 1024 * 1024 // notes import cap (10 MB)
}
