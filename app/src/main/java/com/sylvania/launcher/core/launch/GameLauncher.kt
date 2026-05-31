package com.sylvania.launcher.core.launch

import com.sylvania.launcher.core.config.ConfigManager
import java.io.File

/**
 * The seam between the launcher logic (ported here, Chantier B) and the game
 * runtime (Wine-bionic + wowbox64, Chantier A). The launcher prepares the
 * environment (realmlist) then asks the runtime to execute Wow.exe.
 */
interface GameRuntime {
    /**
     * Launches the client. [wowDir] is the directory containing Wow.exe;
     * [exeName] is the executable (case may differ on disk).
     * Returns true if the process was started.
     */
    fun launch(wowDir: File, exeName: String): Boolean

    /** Whether a runtime is actually wired up and able to launch. */
    val isAvailable: Boolean
}

/**
 * Placeholder runtime used until Chantier A (Wine-bionic) is integrated. It
 * deliberately reports unavailable so the UI can show "runtime not yet
 * installed" instead of pretending to launch.
 */
class NotIntegratedRuntime : GameRuntime {
    override val isAvailable: Boolean = false
    override fun launch(wowDir: File, exeName: String): Boolean = false
}

sealed class LaunchResult {
    object Started : LaunchResult()
    data class Error(val message: String) : LaunchResult()
}

/**
 * Orchestrates the launch flow, ported from the C++ `MainWindow::playGame`:
 *   1. verify Wow.exe exists
 *   2. write the active realmlist
 *   3. hand off to the runtime
 */
class GameLauncher(
    private val config: ConfigManager,
    private val runtime: GameRuntime,
) {
    /** Locates Wow.exe case-insensitively (Linux/Android FS is case-sensitive). */
    private fun findWowExe(wowDir: File): File? =
        wowDir.listFiles { f -> f.isFile && f.name.equals("Wow.exe", ignoreCase = true) }
            ?.firstOrNull()

    fun playGame(): LaunchResult {
        val path = config.wowPath
        if (path.isBlank()) return LaunchResult.Error("Client WoW non configuré.")
        val wowDir = File(path)
        val exe = findWowExe(wowDir)
            ?: return LaunchResult.Error("Wow.exe non trouvé dans le dossier du client.")

        // Update realmlist before launching (mirrors playGame()).
        val entries = config.realmlistEntries
        val idx = config.activeRealmlistIndex
        entries.getOrNull(idx)?.let { config.writeActiveRealmlist(it.address) }

        if (!runtime.isAvailable) {
            return LaunchResult.Error(
                "Le moteur d'exécution (Wine-bionic) n'est pas encore intégré. " +
                    "Realmlist écrit, client prêt — lancement indisponible.",
            )
        }
        return if (runtime.launch(wowDir, exe.name)) LaunchResult.Started
        else LaunchResult.Error("Impossible de lancer World of Warcraft.")
    }
}
