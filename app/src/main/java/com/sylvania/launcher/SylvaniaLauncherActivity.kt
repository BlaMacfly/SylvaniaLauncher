package com.sylvania.launcher

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import android.os.Environment
import com.sylvania.launcher.core.Constants
import com.sylvania.launcher.core.realmlist.RealmlistWriter
import android.util.Log
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.winlator.cmod.XServerDisplayActivity
import com.winlator.cmod.container.Container
import com.winlator.cmod.container.ContainerManager
import com.winlator.cmod.contents.ContentsManager
import com.winlator.cmod.core.TarCompressorUtils
import com.winlator.cmod.xenvironment.ImageFs
import com.winlator.cmod.xenvironment.ImageFsInstaller
import org.json.JSONObject
import java.io.File

/**
 * Sylvania launcher entry point (replaces Winlator's MainActivity as LAUNCHER).
 *
 * Drives the runtime flow explicitly — Winlator's stock first-run UI does not
 * complete on the AYN Thor (black screen). Steps:
 *   1. extract the bionic rootfs (imagefs.txz) + Wine builds from APK assets,
 *   2. create an arm64ec / WoW64 container (Box64-as-DLL emulation),
 *   3. launch XServerDisplayActivity (no shortcut) → Wine desktop on screen.
 *
 * This is the A4 display-bridge validation milestone; WoW.exe launch (via a
 * shortcut on the client drive) comes next.
 */
class SylvaniaLauncherActivity : AppCompatActivity() {

    private lateinit var status: TextView
    private lateinit var launchButton: Button

    private val WINE_VERSIONS = arrayOf("proton-9.0-x86_64", "proton-9.0-arm64ec")
    private val ARM64EC = "proton-9.0-arm64ec"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#0E1A24"))
            setPadding(48, 64, 48, 48)
        }
        val title = TextView(this).apply {
            text = "Sylvania Launcher"
            setTextColor(Color.parseColor("#7EC8E3"))
            textSize = 26f
        }
        status = TextView(this).apply {
            text = "Prêt."
            setTextColor(Color.WHITE)
            textSize = 15f
            setPadding(0, 32, 0, 32)
        }
        launchButton = Button(this).apply {
            text = "PRÉPARER & AFFICHER WINE"
            setOnClickListener { onLaunchClicked() }
        }
        root.addView(title)
        root.addView(launchButton, lp())
        val scroll = ScrollView(this).apply { addView(status) }
        root.addView(scroll, lp())
        setContentView(root)

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE), 1)
        }

        // Auto-start the prepare+launch flow on a fresh launch (avoids relying on
        // a tap, which the device screensaver intercepts unreliably). Temporary
        // for bring-up; a real home screen comes later.
        if (savedInstanceState == null) launchButton.post { onLaunchClicked() }
    }

    private fun lp() = LinearLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT,
    )

    private fun log(msg: String) {
        Log.i(TAG, msg)
        runOnUiThread { status.append("\n$msg") }
    }
    private fun setStatus(msg: String) {
        Log.i(TAG, msg)
        runOnUiThread { status.text = msg }
    }

    private fun onLaunchClicked() {
        launchButton.isEnabled = false
        setStatus("Préparation du runtime…")
        Thread {
            try {
                val imageFs = ImageFs.find(this)
                if (!isImageFsInstalled(imageFs)) {
                    installImageFs(imageFs)
                } else {
                    log("Runtime déjà installé (v${imageFs.version}).")
                }

                // Container + contents on background, then launch on UI thread
                // (createContainerAsync needs a main-thread Looper for its Handler).
                val contents = ContentsManager(this).apply { syncContents() }
                val manager = ContainerManager(this)
                runOnUiThread { ensureContainerAndLaunch(manager, contents) }
            } catch (e: Exception) {
                Log.e(TAG, "prepare failed", e)
                setStatus("Erreur: ${e.message}")
                runOnUiThread { launchButton.isEnabled = true }
            }
        }.start()
    }

    private fun isImageFsInstalled(imageFs: ImageFs): Boolean =
        imageFs.isValid && imageFs.version >= ImageFsInstaller.LATEST_VERSION &&
            File(imageFs.rootDir, "opt/$ARM64EC/bin/wine").exists()

    private fun installImageFs(imageFs: ImageFs) {
        val rootDir = imageFs.rootDir
        log("Extraction du rootfs (imagefs.txz)… (~quelques minutes)")
        if (!TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "imagefs.txz", rootDir)) {
            throw RuntimeException("échec extraction imagefs.txz")
        }
        for (v in WINE_VERSIONS) {
            val out = File(rootDir, "opt/$v").apply { mkdirs() }
            log("Extraction de $v…")
            TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "$v.txz", out)
        }
        imageFs.createImgVersionFile(ImageFsInstaller.LATEST_VERSION.toInt())
        log("Runtime installé.")
    }

    private fun ensureContainerAndLaunch(manager: ContainerManager, contents: ContentsManager) {
        val existing = manager.containers
        if (existing.isNotEmpty()) {
            launchContainer(existing[0])
            return
        }
        setStatus("Création du conteneur arm64ec / WoW64…")
        val data = JSONObject().apply {
            put("name", "Sylvania")
            put("wineVersion", ARM64EC)
            put("wow64Mode", true)
            put("screenSize", "1280x720")
            put("emulator", "wowbox64")
            put("box64Version", "0.3.7")
            put("fexcoreVersion", "2507")
            // Provide valid wrapper/driver configs — Container.loadData() overwrites
            // these fields with "" when the keys are absent, which later crashes
            // XServerDisplayActivity.compareVersion (NumberFormatException on "").
            put("dxwrapper", Container.DEFAULT_DXWRAPPER)
            put("dxwrapperConfig", Container.DEFAULT_DXWRAPPERCONFIG)
            put("graphicsDriver", Container.DEFAULT_GRAPHICS_DRIVER)
            put("graphicsDriverConfig", Container.DEFAULT_GRAPHICSDRIVERCONFIG)
            put("ddrawrapper", Container.DEFAULT_DDRAWRAPPER)
        }
        manager.createContainerAsync(data, contents) { container ->
            if (container != null) {
                log("Conteneur créé (id=${container.id}).")
                launchContainer(container)
            } else {
                setStatus("Échec de création du conteneur.")
                launchButton.isEnabled = true
            }
        }
    }

    /** WoW client location on the device (pushed/downloaded here). */
    private fun clientDir(): File =
        File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher/wotlk")

    private fun launchContainer(container: Container) {
        // Workaround: Winlator's Container.loadData() switch fall-through
        // (case "ddrawrapper" → "dxwrapperConfig") corrupts dxwrapperConfig to
        // "wined3d"; an empty DXVK version crashes compareVersion. Force valid.
        val cfg = container.dxWrapperConfig
        if (cfg == null || !cfg.contains("version=")) {
            container.dxWrapperConfig = Container.DEFAULT_DXWRAPPERCONFIG
        }

        val client = clientDir()
        val wowExe = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", ignoreCase = true) }?.firstOrNull()
        if (wowExe == null) {
            setStatus("Client WoW introuvable dans ${client.absolutePath}.\nUtilisez le bouton de téléchargement.")
            launchButton.isEnabled = true
            return
        }

        // 1. Write the realmlist into the client (ported launcher logic).
        RealmlistWriter.updateRealmlist(client, Constants.CANONICAL_REALMLIST)

        // 2. Map Wine drive D: → the client directory so Wow.exe is D:\Wow.exe.
        container.drives = "D:" + client.absolutePath
        container.saveData()

        // 3. Create a .desktop shortcut on Wow.exe so the game launches directly
        //    (a bare container would only show the Wine desktop / file manager).
        val desktopDir = container.desktopDir.apply { mkdirs() }
        val shortcut = File(desktopDir, "Sylvania.desktop")
        shortcut.writeText(
            buildString {
                appendLine("[Desktop Entry]")
                appendLine("Type=Application")
                appendLine("Name=Sylvania WoW")
                // D: is the client root; forward slashes survive Winlator's
                // Shortcut path unescape() (backslashes get stripped).
                appendLine("Exec=wine D:/${wowExe.name}")
            },
        )

        setStatus("Lancement de World of Warcraft…")
        val intent = Intent(this, XServerDisplayActivity::class.java).apply {
            putExtra("container_id", container.id)
            putExtra("shortcut_path", shortcut.absolutePath)
            putExtra("shortcut_name", "Sylvania WoW")
        }
        startActivity(intent)
        launchButton.isEnabled = true
    }

    companion object {
        private const val TAG = "SylvaniaLauncher"
    }
}
