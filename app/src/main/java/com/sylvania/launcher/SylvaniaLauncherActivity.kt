package com.sylvania.launcher

import android.Manifest
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.sylvania.launcher.core.Constants
import com.sylvania.launcher.core.realmlist.RealmlistWriter
import com.winlator.cmod.XServerDisplayActivity
import com.winlator.cmod.container.Container
import com.winlator.cmod.container.ContainerManager
import com.winlator.cmod.contents.ContentsManager
import com.winlator.cmod.core.TarCompressorUtils
import com.winlator.cmod.xenvironment.ImageFs
import com.winlator.cmod.xenvironment.ImageFsInstaller
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Sylvania launcher home screen — Android port of the Windows/Qt launcher's
 * MainWindow (see "Sylvania Launcher - C++"). Presents the launcher front-end
 * (server info, PLAY / DOWNLOAD / HD / SETTINGS / ADDONS / QUIT, play stats)
 * instead of jumping straight into the game.
 *
 * PLAY drives the verified runtime flow: extract the bionic rootfs + Wine from
 * APK assets, create an arm64ec / WoW64 (wowbox64) container with Turnip, then
 * launch XServerDisplayActivity on the Wow.exe shortcut.
 */
class SylvaniaLauncherActivity : AppCompatActivity() {

    private lateinit var playButton: Button
    private lateinit var status: TextView
    private lateinit var playTimeLabel: TextView
    private lateinit var launchCountLabel: TextView
    private lateinit var lastSessionLabel: TextView

    private val WINE_VERSIONS = arrayOf("proton-9.0-x86_64", "proton-9.0-arm64ec")
    private val ARM64EC = "proton-9.0-arm64ec"

    // --- Play stats (mirrors the C++ stats.json) ---------------------------
    private var launchCount = 0
    private var totalPlayTime = 0L // seconds
    private var lastSession: Long = 0L
    private var sessionStart: Long = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(buildHome())

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE), 1)
        }

        loadStats()
        updateStatsUi()
        refreshClientState()
        // No auto-launch: the launcher home is shown first (the point of a launcher).
    }

    override fun onResume() {
        super.onResume()
        // Returning from the game: accumulate a rough session time.
        if (sessionStart > 0L) {
            totalPlayTime += (System.currentTimeMillis() - sessionStart) / 1000
            sessionStart = 0L
            saveStats()
        }
        updateStatsUi()
        refreshClientState()
    }

    // ----------------------------------------------------------------- UI

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
    private fun drawableId(name: String): Int = resources.getIdentifier(name, "drawable", packageName)

    private val GOLD = "#d4af37"

    private fun buildHome(): View {
        val root = FrameLayout(this)

        // Full-screen background
        val bg = ImageView(this).apply {
            scaleType = ImageView.ScaleType.CENTER_CROP
            val id = drawableId("launcher_bg")
            if (id != 0) setImageResource(id) else setBackgroundColor(Color.parseColor("#0E1B2E"))
        }
        root.addView(bg, FrameLayout.LayoutParams(MATCH, MATCH))
        // Dark scrim for readability
        root.addView(View(this).apply { setBackgroundColor(Color.parseColor("#660A0F1A")) },
            FrameLayout.LayoutParams(MATCH, MATCH))

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(14), dp(18), dp(12))
        }

        // Header: logo + title
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        drawableId("sylvania_logo").let { if (it != 0) header.addView(ImageView(this).apply { setImageResource(it) },
            LinearLayout.LayoutParams(dp(72), dp(72))) }
        header.addView(TextView(this).apply {
            text = "  Sylvania Launcher"
            setTextColor(Color.parseColor(GOLD))
            textSize = 22f
            setShadowLayer(6f, 0f, 2f, Color.BLACK)
        })
        content.addView(header, wrap())

        // Two panels
        val panels = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        panels.addView(buildServerPanel(), lin(0, MATCH, 1f).apply { marginEnd = dp(8) })
        panels.addView(buildStatsPanel(), lin(0, MATCH, 1f).apply { marginStart = dp(8) })
        content.addView(panels, LinearLayout.LayoutParams(MATCH, 0, 1f).apply { topMargin = dp(10) })

        status = TextView(this).apply {
            setTextColor(Color.parseColor("#7ec8e3"))
            textSize = 12f
            setPadding(0, dp(6), 0, dp(2))
        }
        content.addView(status, wrap())

        content.addView(TextView(this).apply {
            text = "© 2025 Sylvania Launcher — World of Warcraft 3.3.5"
            setTextColor(Color.parseColor(GOLD))
            textSize = 10f
            gravity = Gravity.CENTER
        }, wrap())

        root.addView(content, FrameLayout.LayoutParams(MATCH, MATCH))
        return root
    }

    private fun buildServerPanel(): View {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = panelBg()
            setPadding(dp(16), dp(12), dp(16), dp(12))
        }
        panel.addView(panelTitle("Serveur"))
        panel.addView(label("The Kingdom of Sylvania — 3.3.5", "#ffffff", 13f, true))
        panel.addView(label("Realmlist : ${Constants.SERVER_HOST}", "#7ec8e3", 12f, false))

        // Row 1: JOUER (big) + TÉLÉCHARGER + HD
        val row1 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; }
        playButton = wowButton("JOUER", "#4a7c3f", "#2a5a1f", "#5a8c4f", "#ffffff", 15f)
        playButton.setOnClickListener { onPlayClicked() }
        row1.addView(playButton, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton("TÉLÉCHARGER", "#4a3a20", "#2a1a0a", GOLD, GOLD, 11f)
            .also { it.setOnClickListener { onDownloadClicked() } }, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton("HD", "#2a6dd4", "#15479a", "#3a7de4", "#ffffff", 13f)
            .also { it.setOnClickListener { comingSoon("Patch HD") } }, lin(0, WRAP, 1f))
        panel.addView(row1, topMargin(wrap(), 14))

        // Row 2: RÉGLAGES + QUITTER
        val row2 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        row2.addView(wowButton("RÉGLAGES", "#5a4a3d", "#3a2a1d", "#6a5a4d", GOLD, 11f)
            .also { it.setOnClickListener { comingSoon("Réglages") } }, lin(0, WRAP, 1f).apply { marginEnd = dp(6) })
        row2.addView(wowButton("QUITTER", "#8a4a5a", "#6a2a3a", "#9a5a6a", "#ffffff", 11f)
            .also { it.setOnClickListener { finishAffinity() } }, lin(0, WRAP, 1f))
        panel.addView(row2, topMargin(wrap(), 8))

        // Row 3: Addons
        panel.addView(wowButton("Liste des Addons", "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f)
            .also { it.setOnClickListener { onAddonsClicked() } }, topMargin(wrap(), 8))

        panel.addView(View(this), lin(MATCH, 0, 1f)) // spacer
        return panel
    }

    private fun buildStatsPanel(): View {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = panelBg()
            setPadding(dp(16), dp(12), dp(16), dp(12))
        }
        panel.addView(panelTitle("Statistiques de Jeu"))
        playTimeLabel = statBox("Temps de jeu : 0h 0min")
        launchCountLabel = statBox("Lancements : 0")
        lastSessionLabel = statBox("Dernière session : --")
        panel.addView(playTimeLabel, topMargin(wrap(), 6))
        panel.addView(launchCountLabel, topMargin(wrap(), 8))
        panel.addView(lastSessionLabel, topMargin(wrap(), 8))
        panel.addView(View(this), lin(MATCH, 0, 1f))
        panel.addView(wowButton("Changer de Serveur", "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f)
            .also { it.setOnClickListener { comingSoon("Changer de serveur") } }, topMargin(wrap(), 8))
        return panel
    }

    // -------------------------------------------------------- UI helpers

    private val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
    private val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT
    private fun wrap() = LinearLayout.LayoutParams(MATCH, WRAP)
    private fun lin(w: Int, h: Int, weight: Float) = LinearLayout.LayoutParams(w, h, weight)
    private fun topMargin(lp: LinearLayout.LayoutParams, m: Int) = lp.apply { topMargin = dp(m) }

    private fun panelBg() = GradientDrawable().apply {
        setColor(Color.argb(160, 0, 0, 0)); setStroke(dp(2), Color.parseColor("#5a4a2d")); cornerRadius = dp(10).toFloat()
    }

    private fun panelTitle(t: String) = TextView(this).apply {
        text = t; setTextColor(Color.parseColor(GOLD)); textSize = 17f; gravity = Gravity.CENTER
        setPadding(0, 0, 0, dp(6))
    }

    private fun label(t: String, color: String, size: Float, bold: Boolean) = TextView(this).apply {
        text = t; setTextColor(Color.parseColor(color)); textSize = size
        if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD)
    }

    private fun statBox(t: String) = TextView(this).apply {
        text = t; setTextColor(Color.WHITE); textSize = 13f; gravity = Gravity.CENTER
        setPadding(dp(10), dp(10), dp(10), dp(10))
        background = GradientDrawable().apply {
            setColor(Color.argb(150, 50, 38, 20)); setStroke(dp(2), Color.parseColor(GOLD)); cornerRadius = dp(8).toFloat()
        }
    }

    private fun wowButton(text: String, top: String, bottom: String, border: String, txt: String, size: Float): Button {
        val b = Button(this)
        b.text = text
        b.isAllCaps = false
        b.textSize = size
        b.setTextColor(Color.parseColor(txt))
        b.setTypeface(b.typeface, android.graphics.Typeface.BOLD)
        b.stateListAnimator = null
        b.setPadding(dp(6), dp(10), dp(6), dp(10))
        b.background = GradientDrawable(
            GradientDrawable.Orientation.TOP_BOTTOM,
            intArrayOf(Color.parseColor(top), Color.parseColor(bottom)),
        ).apply { cornerRadius = dp(8).toFloat(); setStroke(dp(2), Color.parseColor(border)) }
        return b
    }

    private fun log(msg: String) { Log.i(TAG, msg); runOnUiThread { status.append("\n$msg") } }
    private fun setStatus(msg: String) { Log.i(TAG, msg); runOnUiThread { status.text = msg } }
    private fun comingSoon(feature: String) =
        Toast.makeText(this, "$feature — bientôt disponible", Toast.LENGTH_SHORT).show()

    // ------------------------------------------------------------- Stats

    private fun statsFile() = File(filesDir, "stats.json")

    private fun loadStats() {
        try {
            val f = statsFile()
            if (!f.exists()) return
            val o = JSONObject(f.readText())
            launchCount = o.optInt("launch_count", 0)
            totalPlayTime = o.optLong("total_play_time", 0L)
            lastSession = o.optLong("last_session", 0L)
        } catch (e: Exception) { Log.w(TAG, "loadStats", e) }
    }

    private fun saveStats() {
        try {
            statsFile().writeText(JSONObject().apply {
                put("launch_count", launchCount)
                put("total_play_time", totalPlayTime)
                put("last_session", lastSession)
            }.toString())
        } catch (e: Exception) { Log.w(TAG, "saveStats", e) }
    }

    private fun updateStatsUi() {
        val h = totalPlayTime / 3600; val m = (totalPlayTime % 3600) / 60
        playTimeLabel.text = "Temps de jeu : ${h}h ${m}min"
        launchCountLabel.text = "Lancements : $launchCount"
        lastSessionLabel.text = if (lastSession > 0)
            "Dernière session : " + SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.FRANCE).format(Date(lastSession))
        else "Dernière session : --"
    }

    private fun refreshClientState() {
        val client = clientDir()
        val hasClient = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", true) }?.isNotEmpty() == true
        playButton.isEnabled = hasClient
        playButton.alpha = if (hasClient) 1f else 0.5f
        if (!hasClient) setStatus("Client WoW introuvable dans ${client.absolutePath}.\nUtilisez TÉLÉCHARGER.")
        else if (status.text.isNullOrBlank()) setStatus("Prêt à jouer sur The Kingdom of Sylvania !")
    }

    // ----------------------------------------------------------- Handlers

    private fun onDownloadClicked() {
        AlertDialog.Builder(this)
            .setTitle("Télécharger le client")
            .setMessage("Le client WoW 3.3.5 (~19 Go) sera téléchargé depuis ${Constants.SERVER_HOST} puis extrait dans ${clientDir().absolutePath}.\n\nTéléchargement intégré en cours d'implémentation.")
            .setPositiveButton("OK", null)
            .show()
    }

    private fun onAddonsClicked() {
        val addonsDir = File(clientDir(), "Interface/AddOns")
        val count = addonsDir.listFiles { f -> f.isDirectory }?.size ?: 0
        val hasConsolePort = File(addonsDir, "ConsolePort").isDirectory
        AlertDialog.Builder(this)
            .setTitle("Addons")
            .setMessage(buildString {
                append("$count addon(s) installé(s) dans le client.\n")
                append(if (hasConsolePort) "✓ ConsolePort (manette) installé." else "ConsolePort non détecté.")
                append("\n\nGestion complète des addons en cours d'implémentation.")
            })
            .setPositiveButton("OK", null)
            .show()
    }

    private fun onPlayClicked() {
        playButton.isEnabled = false
        setStatus("Préparation du runtime…")
        Thread {
            try {
                val imageFs = ImageFs.find(this)
                if (!isImageFsInstalled(imageFs)) installImageFs(imageFs)
                else log("Runtime déjà installé (v${imageFs.version}).")

                ensureControlsProfile()

                val contents = ContentsManager(this).apply { syncContents() }
                val manager = ContainerManager(this)
                runOnUiThread { ensureContainerAndLaunch(manager, contents) }
            } catch (e: Exception) {
                Log.e(TAG, "prepare failed", e)
                setStatus("Erreur: ${e.message}")
                runOnUiThread { playButton.isEnabled = true }
            }
        }.start()
    }

    /**
     * Install the pre-built gamepad mapping profile ("WoW ConsolePort") from APK
     * assets into the Winlator profiles dir if absent, so the controller is mapped
     * for ConsolePortLK out of the box (the shortcut's controlsProfile extra then
     * auto-activates it). Maps the gamepad to the WoWmapper key scheme.
     */
    private fun ensureControlsProfile() {
        val dest = File(filesDir, "profiles/controls-$CONTROLS_PROFILE_ID.icp")
        if (dest.exists()) return
        try {
            dest.parentFile?.mkdirs()
            assets.open("profiles/controls-$CONTROLS_PROFILE_ID.icp").use { input ->
                dest.outputStream().use { input.copyTo(it) }
            }
            log("Profil manette installé (WoW ConsolePort).")
        } catch (e: Exception) {
            Log.w(TAG, "controls profile install failed", e)
        }
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

    /** WoW client location on the device (pushed/downloaded here). */
    private fun clientDir(): File =
        File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher/wotlk")

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
                playButton.isEnabled = true
            }
        }
    }

    private fun launchContainer(container: Container) {
        // Workaround: Winlator's Container.loadData() switch fall-through
        // (case "ddrawrapper" → "dxwrapperConfig") corrupts dxwrapperConfig to
        // "wined3d"; an empty DXVK version crashes compareVersion. Force valid.
        val cfg = container.dxWrapperConfig
        if (cfg == null || !cfg.contains("version=")) {
            container.dxWrapperConfig = Container.DEFAULT_DXWRAPPERCONFIG
        }
        // Match the Wine desktop to the device + the client's Config.wtf (1920x1080).
        container.screenSize = "1920x1080"

        // Use Turnip (Mesa) via AdrenoTools instead of the proprietary Adreno
        // driver. The proprietary Qualcomm driver fails a DXVK Vulkan memory
        // allocation (4 MiB, ~11 GB free) right after swapchain creation, which
        // stalls WoW's render thread → WoW hangs at the login screen → its
        // watchdog aborts. Turnip allocates correctly and exposes VK_KHR_surface
        // via the wrapper ICD → the login screen renders. The turnip25.1.0 driver
        // ships as an APK asset (adrenotools-turnip25.1.0.tzst).
        container.graphicsDriver = "wrapper"
        container.graphicsDriverConfig =
            "version=turnip25.1.0;blacklistedExtensions=;maxDeviceMemory=0;adrenotoolsTurnip=1;frameSync=Normal"

        val client = clientDir()
        val wowExe = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", ignoreCase = true) }?.firstOrNull()
        if (wowExe == null) {
            setStatus("Client WoW introuvable dans ${client.absolutePath}.\nUtilisez TÉLÉCHARGER.")
            playButton.isEnabled = true
            return
        }

        // 1. Write the realmlist into the client (ported launcher logic).
        RealmlistWriter.updateRealmlist(client, Constants.CANONICAL_REALMLIST)

        // 2. Map Wine drive D: → the client directory so Wow.exe is D:\Wow.exe.
        container.drives = "D:" + client.absolutePath
        container.saveData()

        // 3. Create a .desktop shortcut on Wow.exe so the game launches directly.
        val desktopDir = container.desktopDir.apply { mkdirs() }
        val shortcut = File(desktopDir, "Sylvania.desktop")
        shortcut.writeText(
            buildString {
                appendLine("[Desktop Entry]")
                appendLine("Type=Application")
                appendLine("Name=Sylvania WoW")
                appendLine("Exec=wine D:/${wowExe.name}")
                // Auto-activate the gamepad mapping for ConsolePort (profile id =
                // CONTROLS_PROFILE_ID): XServerDisplayActivity reads this extra and
                // shows the controls profile, so the controller works without the
                // player picking it in-game.
                appendLine("")
                appendLine("[Extra Data]")
                appendLine("controlsProfile=$CONTROLS_PROFILE_ID")
            },
        )

        // Update play stats.
        launchCount++
        lastSession = System.currentTimeMillis()
        sessionStart = lastSession
        saveStats()
        updateStatsUi()

        setStatus("Lancement de World of Warcraft…")
        val intent = Intent(this, XServerDisplayActivity::class.java).apply {
            putExtra("container_id", container.id)
            putExtra("shortcut_path", shortcut.absolutePath)
            putExtra("shortcut_name", "Sylvania WoW")
        }
        startActivity(intent)
        playButton.isEnabled = true
    }

    companion object {
        private const val TAG = "SylvaniaLauncher"
        /** Id of the pre-provisioned Winlator controls profile "WoW ConsolePort"
         *  (files/profiles/controls-10.icp): maps the gamepad to the key scheme
         *  ConsolePortLK expects. Activated via the shortcut's controlsProfile extra. */
        private const val CONTROLS_PROFILE_ID = 10
    }
}
