package com.sylvania.launcher.runtime

import android.system.Os
import android.util.Log
import org.apache.commons.compress.archivers.tar.TarArchiveEntry
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream
import org.apache.commons.compress.compressors.xz.XZCompressorInputStream
import java.io.BufferedInputStream
import java.io.File

data class ExtractStats(
    val files: Int = 0,
    val dirs: Int = 0,
    val symlinks: Int = 0,
    val hardlinks: Int = 0,
)

/**
 * Extracts a `.txz` (tar + XZ) rootfs into a destination directory, faithfully
 * recreating directories, regular files, **symbolic links**, hard links and
 * unix permission bits — which `adb push` cannot do and which Wine's rootfs
 * relies on heavily. This is the foundational brick of the runtime (Chantier A,
 * step A1), equivalent to Winlator's TarCompressorUtils.
 *
 * Must be called off the main thread (blocking IO).
 */
object RootfsExtractor {

    private const val TAG = "RootfsExtractor"

    /**
     * @param source a tar.xz archive (e.g. imagefs.txz).
     * @param destDir target root; created if missing.
     * @param onProgress called every [progressEvery] entries with the count.
     */
    fun extractTarXz(
        source: File,
        destDir: File,
        progressEvery: Int = 500,
        onProgress: (Int) -> Unit = {},
    ): ExtractStats {
        if (!destDir.exists()) destDir.mkdirs()
        val canonicalDest = destDir.canonicalFile
        var files = 0; var dirs = 0; var symlinks = 0; var hardlinks = 0; var total = 0

        TarArchiveInputStream(
            XZCompressorInputStream(BufferedInputStream(source.inputStream(), 1 shl 16)),
        ).use { tis ->
            var entry: TarArchiveEntry? = tis.nextTarEntry
            while (entry != null) {
                val e = entry
                val outFile = File(destDir, e.name)

                // Zip-Slip guard on the entry's own location (link *targets* may
                // be relative and point elsewhere — that's legitimate for a rootfs).
                if (!outFile.canonicalFile.path.startsWith(canonicalDest.path)) {
                    throw SecurityException("Entry escapes target dir: ${e.name}")
                }

                when {
                    e.isDirectory -> {
                        outFile.mkdirs(); dirs++
                    }
                    e.isSymbolicLink -> {
                        outFile.parentFile?.mkdirs()
                        if (outFile.exists() || isSymlink(outFile)) outFile.delete()
                        try {
                            Os.symlink(e.linkName, outFile.path)
                            symlinks++
                        } catch (ex: Exception) {
                            Log.w(TAG, "symlink failed ${e.name} -> ${e.linkName}: ${ex.message}")
                        }
                    }
                    e.isLink -> {
                        // Hard link: target path is relative to the archive root.
                        outFile.parentFile?.mkdirs()
                        if (outFile.exists()) outFile.delete()
                        val target = File(destDir, e.linkName)
                        try {
                            Os.link(target.path, outFile.path); hardlinks++
                        } catch (ex: Exception) {
                            // Fallback: copy the target's content.
                            if (target.exists()) target.copyTo(outFile, overwrite = true)
                        }
                    }
                    else -> {
                        outFile.parentFile?.mkdirs()
                        outFile.outputStream().use { os -> tis.copyTo(os, 1 shl 16) }
                        applyMode(outFile, e.mode)
                        files++
                    }
                }

                total++
                if (total % progressEvery == 0) onProgress(total)
                entry = tis.nextTarEntry
            }
        }
        onProgress(total)
        return ExtractStats(files, dirs, symlinks, hardlinks)
    }

    private fun applyMode(file: File, mode: Int) {
        // Preserve unix permission bits (esp. the exec bit for wine/box64 binaries).
        try {
            Os.chmod(file.path, mode and 0xFFF)
        } catch (ex: Exception) {
            // Fallback to the coarse Java API.
            if (mode and 0b001_000_000 != 0) file.setExecutable(true, false)
        }
    }

    private fun isSymlink(file: File): Boolean = try {
        file.canonicalFile != file.absoluteFile
    } catch (e: Exception) {
        false
    }
}
