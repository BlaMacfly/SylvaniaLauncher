package com.sylvania.launcher.core.config

import com.sylvania.launcher.core.realmlist.RealmlistWriter
import kotlinx.serialization.json.Json
import java.io.File

/**
 * Port of the C++ `ConfigManager`. Reads/writes config.json and drives
 * realmlist.wtf updates. Pure-ish: takes the directories as constructor args
 * so it is unit-testable on the JVM (no Android Context required).
 *
 * @param configDir directory holding config.json (on Android: filesDir).
 */
class ConfigManager(private val configDir: File) {

    private val json = Json {
        prettyPrint = false
        ignoreUnknownKeys = true
        encodeDefaults = true
    }

    private val configFile: File get() = File(configDir, "config.json")

    var config: LauncherConfig = LauncherConfig()
        private set

    init {
        if (!configDir.exists()) configDir.mkdirs()
        load()

        // Migrate any stale realmlist inherited from an older launcher build,
        // and refresh realmlist.wtf on disk immediately (mirrors the C++ ctor).
        val (migrated, didMigrate) = RealmlistWriter.migrateLegacy(config.realmlistEntries)
        if (didMigrate) {
            config = config.copy(realmlistEntries = migrated)
            save()
            val idx = config.activeRealmlistIndex
            migrated.getOrNull(idx)?.let { writeActiveRealmlist(it.address) }
        }
    }

    private fun load() {
        config = try {
            if (configFile.exists()) {
                json.decodeFromString(LauncherConfig.serializer(), configFile.readText())
            } else {
                LauncherConfig().also { save() }
            }
        } catch (e: Exception) {
            LauncherConfig().also { save() }
        }
    }

    /** Atomic save (temp + rename), the C++ QSaveFile equivalent. */
    fun save(): Boolean = try {
        if (!configDir.exists()) configDir.mkdirs()
        val tmp = File.createTempFile("config", ".tmp", configDir)
        tmp.writeText(json.encodeToString(LauncherConfig.serializer(), config))
        if (configFile.exists()) configFile.delete()
        tmp.renameTo(configFile)
    } catch (e: Exception) {
        false
    }

    fun update(transform: (LauncherConfig) -> LauncherConfig) {
        config = transform(config)
        save()
    }

    // --- WoW path ----------------------------------------------------------
    val wowPath: String get() = config.wowPath
    fun setWowPath(path: String) = update { it.copy(wowPath = path) }

    // --- Realmlist ---------------------------------------------------------
    val realmlistEntries: List<RealmlistEntry> get() = config.realmlistEntries
    val activeRealmlistIndex: Int get() = config.activeRealmlistIndex

    fun setRealmlistEntries(entries: List<RealmlistEntry>) =
        update { it.copy(realmlistEntries = entries) }

    fun setActiveRealmlistIndex(index: Int) = update { cfg ->
        cfg.copy(
            activeRealmlistIndex = index,
            realmlistEntries = cfg.realmlistEntries.mapIndexed { i, e -> e.copy(active = i == index) },
        )
    }

    /** Switch active realm and write realmlist.wtf. Returns true on success. */
    fun switchRealmlist(index: Int): Boolean {
        val entries = config.realmlistEntries
        if (index !in entries.indices) return false
        setActiveRealmlistIndex(index)
        return writeActiveRealmlist(entries[index].address)
    }

    /** Writes the given realmlist text into the client's realmlist.wtf files. */
    fun writeActiveRealmlist(realmlistText: String): Boolean {
        val path = config.wowPath
        if (path.isBlank()) return false
        return RealmlistWriter.updateRealmlist(File(path), realmlistText)
    }
}
