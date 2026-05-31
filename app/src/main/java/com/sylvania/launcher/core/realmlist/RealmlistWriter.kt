package com.sylvania.launcher.core.realmlist

import com.sylvania.launcher.core.Constants
import java.io.File

/**
 * Pure (Android-free) port of the realmlist handling in the C++ `ConfigManager`.
 * Kept dependency-free so it can be unit-tested on the JVM without a device.
 */
object RealmlistWriter {

    /**
     * Legacy hosts inherited from older launcher builds that must be rewritten
     * to the current production address. Match is case-insensitive / substring.
     * Ported from `ConfigManager::migrateLegacyRealmlist`.
     */
    private val LEGACY_HOSTS = listOf(
        "logon.sylvania-wow.com",
        "sylvania-wow.com",
        "logon.sylvania.com",
    )

    /**
     * Known WoW locale directory names (lower-cased for case-insensitive match).
     * Real clients vary the casing (e.g. "enus" lowercase, "frFR" mixed), which
     * matters on case-sensitive Android — so we match by lower-case and also
     * write any Data/<dir> that already contains a realmlist.wtf.
     */
    private val LOCALE_DIRS = setOf(
        "enus", "engb", "frfr", "dede", "eses", "esmx", "ruru",
        "kokr", "zhcn", "zhtw", "ptbr", "itit",
    )

    private fun isLocaleDir(name: String): Boolean = name.lowercase() in LOCALE_DIRS

    /**
     * Returns true if [address] points at a stale legacy host.
     */
    fun isLegacyAddress(address: String): Boolean =
        LEGACY_HOSTS.any { address.contains(it, ignoreCase = true) }

    /**
     * Rewrites any legacy entries to the canonical address. Returns the
     * (possibly unchanged) list and whether anything was migrated — mirrors the
     * idempotent behaviour of `migrateLegacyRealmlist`.
     */
    fun migrateLegacy(
        entries: List<com.sylvania.launcher.core.config.RealmlistEntry>,
    ): Pair<List<com.sylvania.launcher.core.config.RealmlistEntry>, Boolean> {
        var migrated = false
        val result = entries.map { entry ->
            if (isLegacyAddress(entry.address)) {
                migrated = true
                entry.copy(address = Constants.CANONICAL_REALMLIST)
            } else {
                entry
            }
        }
        return result to migrated
    }

    /**
     * Writes [realmlistText] into every realmlist.wtf candidate whose parent
     * directory exists under [wowPath]. Uses atomic write (temp file + rename),
     * the equivalent of the C++ `QSaveFile`. Returns true if at least one file
     * was updated.
     */
    fun updateRealmlist(wowPath: File, realmlistText: String): Boolean {
        if (!wowPath.isDirectory) return false
        val content = realmlistText + "\n"
        var success = false

        // 1. Root realmlist.wtf (the one WoW reads first).
        if (atomicWrite(File(wowPath, "realmlist.wtf"), content)) success = true

        // 2. Per-locale realmlist.wtf under Data/<locale> — only existing dirs,
        //    matched case-insensitively, plus any Data/<dir> that already holds
        //    a realmlist.wtf (preserve whatever layout the client actually has).
        val dataDir = File(wowPath, "Data")
        if (dataDir.isDirectory) {
            dataDir.listFiles { f -> f.isDirectory }?.forEach { dir ->
                val existing = File(dir, "realmlist.wtf")
                if (existing.exists() || isLocaleDir(dir.name)) {
                    if (atomicWrite(existing, content)) success = true
                }
            }
        }
        return success
    }

    /** Atomic write: stage to a sibling temp file, then rename over the target. */
    private fun atomicWrite(target: File, content: String): Boolean = try {
        val tmp = File.createTempFile(target.name, ".tmp", target.parentFile)
        tmp.writeText(content, Charsets.UTF_8)
        // Renaming over an existing file is not atomic on all FS; remove first.
        if (target.exists()) target.delete()
        val ok = tmp.renameTo(target)
        if (!ok) tmp.delete()
        ok
    } catch (e: Exception) {
        false
    }
}
