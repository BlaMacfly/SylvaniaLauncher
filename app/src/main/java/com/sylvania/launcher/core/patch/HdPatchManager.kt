package com.sylvania.launcher.core.patch

import com.sylvania.launcher.core.Constants
import com.sylvania.launcher.core.io.ZipExtractor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.security.MessageDigest

/** Progress event emitted during patch installation. [percent] is -1 for an
 *  indeterminate/busy state (matches the C++ `progressChanged(-1, ...)`). */
data class PatchProgress(val percent: Int, val status: String)

sealed class PatchResult {
    data class Success(val message: String) : PatchResult()
    data class Failure(val message: String) : PatchResult()
}

/**
 * Port of the C++ `HdPatchManager`. Downloads the HD patch ZIP, hashes it
 * (SHA-256, logged), extracts it, locates the client root inside the extracted
 * tree, migrates the relevant items into the client directory, and regenerates
 * Config.wtf.
 *
 * @param wowPath the client directory (containing Wow.exe).
 * @param tempDir scratch directory (on Android: cacheDir/SylvaniaLauncher_HD).
 */
class HdPatchManager(
    private val wowPath: File,
    private val tempDir: File,
    private val http: OkHttpClient = OkHttpClient(),
) {

    /**
     * Runs the full install. [onProgress] is called on the calling coroutine's
     * context. Returns a [PatchResult]. Heavy work runs on Dispatchers.IO.
     */
    suspend fun install(
        downloadUrl: String = Constants.HD_PATCH_URL,
        onProgress: (PatchProgress) -> Unit = {},
    ): PatchResult = withContext(Dispatchers.IO) {
        try {
            if (!wowPath.isDirectory) {
                return@withContext PatchResult.Failure("Dossier WoW introuvable: $wowPath")
            }
            tempDir.mkdirs()
            val zip = File(tempDir, "patch-hd.zip")

            onProgress(PatchProgress(0, "Démarrage du téléchargement..."))
            if (!download(downloadUrl, zip, onProgress)) {
                return@withContext PatchResult.Failure("Échec du téléchargement du Patch HD")
            }

            onProgress(PatchProgress(100, "Téléchargement terminé. Vérification..."))
            val sha = sha256(zip)
            // Hash is logged for diagnostics, matching the C++ behaviour (the
            // server does not currently expose a reference hash to compare to).
            android.util.Log.d("HdPatchManager", "HD Patch SHA-256: $sha")

            onProgress(PatchProgress(-1, "Extraction des fichiers HD en cours..."))
            val extractDir = File(tempDir, "extracted").apply { mkdirs() }
            if (!ZipExtractor.extract(zip, extractDir)) {
                return@withContext PatchResult.Failure("Échec de l'extraction du Patch HD")
            }

            val sourceRoot = findExtractedRoot(extractDir)
                ?: return@withContext PatchResult.Failure("Contenu du Patch HD introuvable dans l'archive.")

            migrateFiles(sourceRoot, onProgress)
            WowConfigWriter.writeConfigWtf(wowPath)
            cleanup()
            PatchResult.Success("Patch HD installé avec succès.")
        } catch (e: Exception) {
            cleanup()
            PatchResult.Failure("Erreur: ${e.message}")
        }
    }

    private fun download(url: String, dest: File, onProgress: (PatchProgress) -> Unit): Boolean {
        val request = Request.Builder().url(url).build()
        http.newCall(request).execute().use { resp ->
            if (!resp.isSuccessful) return false
            val body = resp.body ?: return false
            val total = body.contentLength()
            dest.outputStream().use { out ->
                body.byteStream().use { input ->
                    val buf = ByteArray(64 * 1024)
                    var received = 0L
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        received += n
                        if (total > 0) {
                            val pct = ((received * 100) / total).toInt()
                            onProgress(PatchProgress(pct, "Téléchargement du Patch HD... $pct%"))
                        }
                    }
                }
            }
        }
        return true
    }

    private fun sha256(file: File): String {
        val md = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buf = ByteArray(64 * 1024)
            while (true) {
                val n = input.read(buf)
                if (n < 0) break
                md.update(buf, 0, n)
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }

    /**
     * Breadth-first, depth-limited (4) search for the client root inside the
     * extracted tree. Direct port of `findExtractedRoot`. Case-insensitive on
     * the WoW.exe marker so it works on case-sensitive Android filesystems.
     */
    fun findExtractedRoot(extractPath: File): File? {
        fun looksLikeRoot(dir: File): Boolean =
            File(dir, "Data").isDirectory ||
                dir.listFiles { f -> f.name.equals("WoW.exe", ignoreCase = true) }?.isNotEmpty() == true

        val queue = ArrayDeque<Pair<File, Int>>()
        queue.add(extractPath to 0)
        val maxDepth = 4
        while (queue.isNotEmpty()) {
            val (dir, depth) = queue.removeFirst()
            File(dir, "Le Client WoW HD").let { if (it.isDirectory) return it }
            if (looksLikeRoot(dir)) return dir
            if (depth >= maxDepth) continue
            dir.listFiles { f -> f.isDirectory }?.forEach { queue.add(it to depth + 1) }
        }
        return null
    }

    /**
     * Items the HD patch contributes. The actual HD content is Data/ and
     * Interface/ (the .mpq archives + UI). d3d9.dll / dxvk.conf are the
     * Windows-side DXVK shim and are intentionally NOT migrated on Android —
     * the game runtime (Wine-bionic) injects its own DXVK/Turnip wrapper, so a
     * Windows d3d9.dll in the client dir would conflict.
     */
    private val itemsToMigrate = listOf("Data", "Interface", "WoW.exe")

    private fun migrateFiles(sourcePath: File, onProgress: (PatchProgress) -> Unit) {
        val total = itemsToMigrate.size
        itemsToMigrate.forEachIndexed { index, item ->
            val src = File(sourcePath, item)
            if (!src.exists()) return@forEachIndexed
            val dest = File(wowPath, item)
            if (src.isDirectory) {
                src.copyRecursively(dest, overwrite = true)
            } else {
                if (dest.exists()) dest.delete()
                src.copyTo(dest, overwrite = true)
            }
            val pct = ((index + 1) * 100) / total
            onProgress(PatchProgress(pct, "Migration: $item"))
        }
    }

    private fun cleanup() {
        if (tempDir.exists()) tempDir.deleteRecursively()
    }

    companion object {
        /**
         * Port of `HdPatchManager::isInstalled` — detects the signature HD MPQs
         * (patch-w = trees, patch-5 = water/sky/spells), enabled or disabled.
         */
        fun isInstalled(wowPath: File): Boolean {
            if (!wowPath.isDirectory) return false
            val checks = listOf(
                "Data/patch-w.mpq", "Data/patch-w.mpq.disabled",
                "Data/patch-5.mpq", "Data/patch-5.mpq.disabled",
            )
            return checks.any { File(wowPath, it).exists() }
        }
    }
}
