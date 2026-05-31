package com.sylvania.launcher.core.io

import org.apache.commons.compress.archivers.zip.ZipArchiveInputStream
import java.io.BufferedInputStream
import java.io.File
import java.io.InputStream

/**
 * ZIP extraction replacing the C++ launcher's PowerShell `Expand-Archive`
 * fallback chain. Apache Commons Compress reads the ZIP stream and copes with
 * data descriptors / ZIP64 archives that servers commonly produce.
 *
 * Includes Zip-Slip protection: entries that would escape [destDir] are
 * rejected (the original PowerShell path had no such guard).
 */
object ZipExtractor {

    /**
     * Extracts [zip] into [destDir]. [onEntry] is invoked with each entry name
     * for progress reporting. Returns true on success.
     */
    fun extract(zip: File, destDir: File, onEntry: (String) -> Unit = {}): Boolean =
        zip.inputStream().use { extract(it, destDir, onEntry) }

    fun extract(input: InputStream, destDir: File, onEntry: (String) -> Unit = {}): Boolean {
        if (!destDir.exists()) destDir.mkdirs()
        val canonicalDest = destDir.canonicalFile
        return try {
            ZipArchiveInputStream(BufferedInputStream(input)).use { zis ->
                var entry = zis.nextEntry
                while (entry != null) {
                    val outFile = File(destDir, entry.name)
                    // Zip-Slip guard.
                    if (!outFile.canonicalFile.toPath().startsWith(canonicalDest.toPath())) {
                        throw SecurityException("Zip entry escapes target dir: ${entry.name}")
                    }
                    if (entry.isDirectory) {
                        outFile.mkdirs()
                    } else {
                        outFile.parentFile?.mkdirs()
                        outFile.outputStream().use { os -> zis.copyTo(os) }
                    }
                    onEntry(entry.name)
                    entry = zis.nextEntry
                }
            }
            true
        } catch (e: Exception) {
            false
        }
    }
}
