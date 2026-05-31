package com.sylvania.launcher.ui

import android.app.Application
import android.os.Environment
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.sylvania.launcher.core.config.ConfigManager
import com.sylvania.launcher.core.launch.GameLauncher
import com.sylvania.launcher.core.launch.LaunchResult
import com.sylvania.launcher.core.launch.NotIntegratedRuntime
import com.sylvania.launcher.core.patch.HdPatchManager
import com.sylvania.launcher.core.patch.PatchProgress
import com.sylvania.launcher.core.patch.PatchResult
import com.sylvania.launcher.runtime.ImageFs
import com.sylvania.launcher.runtime.RuntimeInstallResult
import com.sylvania.launcher.runtime.RuntimeInstaller
import com.sylvania.launcher.runtime.WineLauncher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.withContext
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File

data class LauncherUiState(
    val wowPath: String = "",
    val clientReady: Boolean = false,
    val activeRealm: String = "",
    val status: String = "",
    val hdInstalled: Boolean = false,
    val patchProgress: PatchProgress? = null,
    val runtimeStatus: String = "",
)

class LauncherViewModel(app: Application) : AndroidViewModel(app) {

    private val config = ConfigManager(app.filesDir)
    private val launcher = GameLauncher(config, NotIntegratedRuntime())

    private val _ui = MutableStateFlow(LauncherUiState())
    val ui: StateFlow<LauncherUiState> = _ui.asStateFlow()

    /**
     * Suggested location for the WoW client under shared storage. The user can
     * place/copy the client here with a file manager; with targetSdk 28 the app
     * reads it via a real path (usable later by the Wine runtime).
     */
    val suggestedClientPath: String =
        File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher/Client").absolutePath

    init {
        refresh()
        checkRuntime()
    }

    /**
     * A1: extract the bionic rootfs on first launch if the archive has been
     * provided (dev: pushed to <externalStorage>/SylvaniaLauncher/imagefs.txz).
     */
    private fun checkRuntime() {
        val ctx = getApplication<Application>()
        viewModelScope.launch {
            if (!RuntimeInstaller.isRuntimeInstalled(ctx)) {
                _ui.value = _ui.value.copy(runtimeStatus = "Runtime : installation…")
                val res = RuntimeInstaller.installIfNeeded(ctx) { s ->
                    _ui.value = _ui.value.copy(runtimeStatus = "Runtime : $s")
                }
                val msg = when (res) {
                    is RuntimeInstallResult.Installed -> "Runtime installé : ${res.message} (${res.ms} ms)"
                    is RuntimeInstallResult.AlreadyInstalled -> "Runtime déjà installé"
                    is RuntimeInstallResult.SourceMissing -> "Runtime : archive absente (${res.expectedPath})"
                    is RuntimeInstallResult.Failed -> "Runtime : échec — ${res.message}"
                }
                _ui.value = _ui.value.copy(runtimeStatus = msg)
                if (res is RuntimeInstallResult.SourceMissing || res is RuntimeInstallResult.Failed) return@launch
            }

            // A3/x86 smoke test: prove arm64ec Wine runs AND i386 (x86) emulation
            // via wowbox64 works (the i386 load error must disappear after install).
            val imageFs = ImageFs.of(ctx)
            if (WineLauncher.isWineInstalled(imageFs)) {
                _ui.value = _ui.value.copy(runtimeStatus = "Wine : wineboot (création préfixe)…")
                val r1 = withContext(Dispatchers.IO) {
                    WineLauncher.run(imageFs, "wineboot", "wineboot", "--init", timeoutSec = 120)
                }
                _ui.value = _ui.value.copy(runtimeStatus = "Wine : installation wowbox64…")
                val wb = withContext(Dispatchers.IO) { RuntimeInstaller.installWowbox64(ctx) }
                _ui.value = _ui.value.copy(runtimeStatus = "Wine : test émulation x86…")
                val r2 = withContext(Dispatchers.IO) {
                    WineLauncher.run(imageFs, "x86-wineboot", "wineboot", "--init", timeoutSec = 120)
                }
                val x86ok = r2.exitCode == 0 && !r2.output.contains("load_64bit_module failed")
                _ui.value = _ui.value.copy(
                    runtimeStatus = "Wine ${if (r1.exitCode == 0) "OK (wine-9.0)" else "exit=${r1.exitCode}"} | " +
                        "wowbox64: $wb | émulation x86 : ${if (x86ok) "OK ✓" else "KO"}",
                )
            }
        }
    }

    fun refresh() {
        val path = config.wowPath
        val wowDir = File(path)
        val clientReady = path.isNotBlank() &&
            (wowDir.listFiles { f -> f.name.equals("Wow.exe", ignoreCase = true) }?.isNotEmpty() == true)
        val entries = config.realmlistEntries
        val active = entries.getOrNull(config.activeRealmlistIndex)
        _ui.value = _ui.value.copy(
            wowPath = path,
            clientReady = clientReady,
            activeRealm = active?.address?.removePrefix("set realmlist ") ?: "",
            hdInstalled = if (path.isNotBlank()) HdPatchManager.isInstalled(wowDir) else false,
            status = when {
                path.isBlank() -> "Client WoW non configuré — choisissez le dossier du client."
                !clientReady -> "Wow.exe introuvable dans le dossier configuré."
                else -> "World of Warcraft 3.3.5 prêt. Realmlist: ${active?.address?.removePrefix("set realmlist ")}"
            },
        )
    }

    fun setWowPath(path: String) {
        config.setWowPath(path)
        refresh()
    }

    fun play() {
        when (val r = launcher.playGame()) {
            is LaunchResult.Started -> _ui.value = _ui.value.copy(status = "World of Warcraft lancé !")
            is LaunchResult.Error -> _ui.value = _ui.value.copy(status = r.message)
        }
    }

    fun installHdPatch() {
        val path = config.wowPath
        if (path.isBlank()) {
            _ui.value = _ui.value.copy(status = "Configurez d'abord le dossier du client.")
            return
        }
        val app = getApplication<Application>()
        val manager = HdPatchManager(
            wowPath = File(path),
            tempDir = File(app.cacheDir, "SylvaniaLauncher_HD"),
        )
        viewModelScope.launch {
            val result = manager.install(onProgress = { p ->
                _ui.value = _ui.value.copy(patchProgress = p, status = p.status)
            })
            _ui.value = _ui.value.copy(patchProgress = null)
            when (result) {
                is PatchResult.Success -> _ui.value = _ui.value.copy(status = result.message)
                is PatchResult.Failure -> _ui.value = _ui.value.copy(status = result.message)
            }
            refresh()
        }
    }
}
