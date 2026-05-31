package com.sylvania.launcher.runtime

import android.content.Context
import android.os.Environment
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

sealed class RuntimeInstallResult {
    object AlreadyInstalled : RuntimeInstallResult()
    data class SourceMissing(val expectedPath: String) : RuntimeInstallResult()
    data class Installed(val message: String, val ms: Long) : RuntimeInstallResult()
    data class Failed(val message: String) : RuntimeInstallResult()
}

/**
 * Installs the bionic runtime on first launch (Chantier A) by extracting:
 *   1. the base rootfs (imagefs.txz) into <filesDir>/imagefs
 *   2. the Wine arm64ec build (proton-9.0-arm64ec.txz) into <root>/opt/<id>
 *
 * For the prototype the archives are pushed to shared storage via `adb push`;
 * the final build will download them. Extraction goes through our code (tar+xz
 * with symlinks) because `adb push` cannot recreate the rootfs symlinks.
 */
object RuntimeInstaller {

    private const val TAG = "RuntimeInstaller"
    const val IMAGEFS_VERSION = 1

    private fun srcDir(): File = File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher")
    fun sourceImagefs(): File = File(srcDir(), "imagefs.txz")
    fun sourceProton(): File = File(srcDir(), "${WineLauncher.WINE_IDENTIFIER}.txz")

    fun isRuntimeInstalled(context: Context): Boolean {
        val imageFs = ImageFs.of(context)
        return imageFs.isInstalled(IMAGEFS_VERSION) && WineLauncher.isWineInstalled(imageFs)
    }

    private fun sourceWowbox64(): File = File(srcDir(), "wowbox64/wowbox64.dll")

    /**
     * Installs the i386 emulator DLL (wowbox64.dll = Box64-as-DLL) into the
     * WINEPREFIX system32 so Wine WoW64 can execute x86 (i386) guest code. The
     * prefix must already exist (run wineboot first). Returns a status string.
     */
    fun installWowbox64(context: Context): String {
        val imageFs = ImageFs.of(context)
        val system32 = File(imageFs.rootDir, "${ImageFs.HOME_RELATIVE}/.wine/drive_c/windows/system32")
        if (!system32.isDirectory) return "préfixe absent (system32)"
        val dest = File(system32, "wowbox64.dll")
        if (dest.exists()) return "déjà présent"
        val src = sourceWowbox64()
        if (!src.exists()) return "source absente (${src.absolutePath})"
        return try {
            src.copyTo(dest, overwrite = true)
            "installé (${dest.length() / 1024} Ko)"
        } catch (e: Exception) {
            "échec copie: ${e.message}"
        }
    }

    suspend fun installIfNeeded(
        context: Context,
        onProgress: (String) -> Unit = {},
    ): RuntimeInstallResult = withContext(Dispatchers.IO) {
        val imageFs = ImageFs.of(context)
        val start = System.currentTimeMillis()
        val summary = StringBuilder()

        try {
            // --- 1. Base rootfs (version-gated) ---
            if (!imageFs.isInstalled(IMAGEFS_VERSION)) {
                val src = sourceImagefs()
                if (!src.exists()) return@withContext RuntimeInstallResult.SourceMissing(src.absolutePath)
                if (imageFs.rootDir.exists()) imageFs.rootDir.deleteRecursively()
                imageFs.rootDir.mkdirs()
                onProgress("extraction rootfs…")
                Log.i(TAG, "Extracting ${src.name} -> ${imageFs.rootDir}")
                val st = RootfsExtractor.extractTarXz(src, imageFs.rootDir) { onProgress("rootfs… $it entrées") }
                imageFs.markInstalled(IMAGEFS_VERSION)
                summary.append("rootfs(${st.files}f/${st.symlinks}sl) ")
            } else {
                summary.append("rootfs(ok) ")
            }

            // --- 2. Wine arm64ec (existence-gated) ---
            if (!WineLauncher.isWineInstalled(imageFs)) {
                val src = sourceProton()
                if (!src.exists()) {
                    summary.append("[proton manquant: ${src.absolutePath}]")
                } else {
                    val dest = WineLauncher.winePath(imageFs).apply { mkdirs() }
                    onProgress("extraction Wine arm64ec…")
                    Log.i(TAG, "Extracting ${src.name} -> $dest")
                    val st = RootfsExtractor.extractTarXz(src, dest) { onProgress("wine… $it entrées") }
                    summary.append("wine(${st.files}f/${st.symlinks}sl)")
                }
            } else {
                summary.append("wine(ok)")
            }

            RuntimeInstallResult.Installed(summary.toString().trim(), System.currentTimeMillis() - start)
        } catch (e: Exception) {
            Log.e(TAG, "Runtime install failed", e)
            RuntimeInstallResult.Failed(e.message ?: e.toString())
        }
    }
}
