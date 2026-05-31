package com.sylvania.launcher.runtime

import android.util.Log
import java.io.File
import java.util.concurrent.TimeUnit

data class WineRunResult(
    val label: String,
    val exitCode: Int,
    val timedOut: Boolean,
    val output: String,
)

/**
 * Minimal headless launcher for the bionic Wine (Chantier A, step A3). Assembles
 * the verified bionic environment and execs the Wine binary directly. Used to
 * prove the Wine-WoW64 (arm64ec) stack runs on the device before wiring the X
 * server (A4). targetSdk 28 permits exec of binaries from internal storage.
 *
 * The arm64ec Wine host is native ARM64, so `wine --version`/`wineboot` run
 * WITHOUT Box64 (Box64/wowbox64 is only needed for the x86 *guest* code).
 */
object WineLauncher {

    private const val TAG = "WineLauncher"
    const val WINE_IDENTIFIER = "proton-9.0-arm64ec"

    fun winePath(imageFs: ImageFs): File = File(imageFs.rootDir, "opt/$WINE_IDENTIFIER")
    fun wineBinary(imageFs: ImageFs): File = File(winePath(imageFs), "bin/wine")

    /** Builds the bionic launch environment (subset sufficient for headless runs). */
    private fun buildEnv(imageFs: ImageFs): Map<String, String> {
        val root = imageFs.rootDir.path
        val wine = winePath(imageFs).path
        return mapOf(
            "HOME" to "$root${ImageFs.HOME_PATH}",
            "USER" to ImageFs.USER,
            "TMPDIR" to "$root/usr/tmp",
            "PREFIX" to "$root/usr",
            "PATH" to "$wine/bin:$root/usr/bin",
            "LD_LIBRARY_PATH" to "$root/usr/lib:/system/lib64",
            "WINEPREFIX" to "$root${ImageFs.HOME_PATH}/.wine",
            "WINEARCH" to "win64",
            "WINEDEBUG" to "fixme-all",
            // For x86 guest emulation later (harmless here):
            "HODLL" to "wowbox64.dll",
            "BOX64_DYNAREC" to "1",
        )
    }

    fun isWineInstalled(imageFs: ImageFs): Boolean = wineBinary(imageFs).exists()

    /** Runs the wine binary with [args], capturing merged stdout/stderr. */
    fun run(imageFs: ImageFs, label: String, vararg args: String, timeoutSec: Long = 90): WineRunResult {
        val wine = wineBinary(imageFs)
        if (!wine.exists()) return WineRunResult(label, -1, false, "wine binary absent: $wine")

        // Ensure exec bits on the wine binaries (defensive).
        winePath(imageFs).resolve("bin").listFiles()?.forEach { it.setExecutable(true, false) }

        val cmd = listOf(wine.path) + args
        Log.i(TAG, "[$label] exec: ${cmd.joinToString(" ")}")
        val pb = ProcessBuilder(cmd).redirectErrorStream(true)
        pb.directory(File(imageFs.rootDir, ImageFs.HOME_RELATIVE).apply { mkdirs() })
        pb.environment().putAll(buildEnv(imageFs))

        return try {
            val proc = pb.start()
            val out = StringBuilder()
            val reader = proc.inputStream.bufferedReader()
            val pumper = Thread {
                try { reader.forEachLine { if (out.length < 16_000) out.appendLine(it) } } catch (_: Exception) {}
            }.apply { isDaemon = true; start() }
            val finished = proc.waitFor(timeoutSec, TimeUnit.SECONDS)
            if (!finished) {
                proc.destroyForcibly()
                pumper.join(1000)
                Log.w(TAG, "[$label] TIMEOUT after ${timeoutSec}s")
                return WineRunResult(label, -1, true, out.toString())
            }
            pumper.join(1500)
            val code = proc.exitValue()
            Log.i(TAG, "[$label] exit=$code\n$out")
            WineRunResult(label, code, false, out.toString())
        } catch (e: Exception) {
            Log.e(TAG, "[$label] exec failed", e)
            WineRunResult(label, -1, false, "exec failed: ${e.message}")
        }
    }

    /** A3 smoke test: wine --version, then wineboot to create the prefix. */
    fun headlessSmokeTest(imageFs: ImageFs): List<WineRunResult> {
        val results = ArrayList<WineRunResult>()
        results += run(imageFs, "wine --version", "--version", timeoutSec = 30)
        results += run(imageFs, "wineboot", "wineboot", "--init", timeoutSec = 120)
        return results
    }
}
