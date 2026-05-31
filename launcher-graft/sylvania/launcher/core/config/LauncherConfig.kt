package com.sylvania.launcher.core.config

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/**
 * Realmlist entry — direct port of the C++ `RealmlistEntry` struct.
 */
@Serializable
data class RealmlistEntry(
    val name: String = "",
    val address: String = "",
    val active: Boolean = false,
)

/**
 * On-disk launcher configuration. Mirrors the JSON schema produced by the C++
 * `ConfigManager` (config.json), so a config written by either build is
 * interchangeable. Unknown keys are ignored by the parser for forward-compat.
 *
 * On Windows the C++ build stored `wow_path` as a Windows directory containing
 * Wow.exe. On Android `wowPath` is the absolute path of the directory holding
 * the user-provided client inside app storage (or a granted tree).
 */
@Serializable
data class LauncherConfig(
    @SerialName("wow_path") val wowPath: String = "",
    @SerialName("sound_enabled") val soundEnabled: Boolean = true,
    @SerialName("download_method") val downloadMethod: String = "direct",
    @SerialName("max_upload_rate") val maxUploadRate: Int = 100,
    @SerialName("max_download_rate") val maxDownloadRate: Int = 0,
    @SerialName("first_run") val firstRun: Boolean = true,
    @SerialName("auto_update") val autoUpdate: Boolean = true,
    @SerialName("language") val language: String = "fr",
    @SerialName("background") val background: String = "Azeroth",
    @SerialName("random_bg_on_launch") val randomBackgroundEnabled: Boolean = true,
    @SerialName("active_realmlist_index") val activeRealmlistIndex: Int = 0,
    @SerialName("realmlist_entries") val realmlistEntries: List<RealmlistEntry> = listOf(
        RealmlistEntry(
            name = "Sylvania",
            address = "set realmlist sylvania-servergame.com",
            active = true,
        ),
    ),
)
