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
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.sylvania.launcher.core.Constants
import com.sylvania.launcher.core.config.ConfigManager
import com.sylvania.launcher.core.config.RealmlistEntry
import com.sylvania.launcher.core.io.ZipExtractor
import com.sylvania.launcher.core.patch.HdPatchManager
import com.sylvania.launcher.core.patch.PatchResult
import com.sylvania.launcher.core.patch.WowConfigWriter
import com.sylvania.launcher.core.realmlist.RealmlistWriter
import kotlinx.coroutines.runBlocking
import java.net.HttpURLConnection
import java.net.URL
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
 * MainWindow. Presents the launcher front-end (server info, PLAY / DOWNLOAD /
 * HD / SETTINGS / ADDONS / QUIT, play stats) instead of jumping into the game.
 * Settings, the active server and the background theme are persisted via the
 * ported ConfigManager (config.json), interchangeable with the Windows build.
 */
class SylvaniaLauncherActivity : AppCompatActivity() {

    private lateinit var configMgr: ConfigManager
    private lateinit var bgView: ImageView
    private lateinit var playButton: Button
    private lateinit var status: TextView
    private lateinit var serverNameLabel: TextView
    private lateinit var realmlistLabel: TextView
    private lateinit var playTimeLabel: TextView
    private lateinit var launchCountLabel: TextView
    private lateinit var lastSessionLabel: TextView

    private val WINE_VERSIONS = arrayOf("proton-9.0-x86_64", "proton-9.0-arm64ec")
    private val ARM64EC = "proton-9.0-arm64ec"

    private val THEMES = listOf(
        "Alliance", "Arbre de Vie", "Azeroth", "Horde",
        "Ilidan", "Lich King", "Ragnaros", "Taverne",
    )

    // --- Play stats (mirrors the C++ stats.json) ---------------------------
    private var launchCount = 0
    private var totalPlayTime = 0L // seconds
    private var lastSession: Long = 0L
    private var sessionStart: Long = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        configMgr = ConfigManager(filesDir)
        // Default the client path on first run so the rest of the UI has a target.
        if (configMgr.wowPath.isBlank()) configMgr.setWowPath(defaultClientDir().absolutePath)

        setContentView(buildHome())

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(arrayOf(Manifest.permission.WRITE_EXTERNAL_STORAGE), 1)
        }

        applyBackgroundOnLaunch()
        loadStats()
        updateStatsUi()
        updateServerUi()
        refreshClientState()
    }

    override fun onResume() {
        super.onResume()
        if (sessionStart > 0L) {
            totalPlayTime += (System.currentTimeMillis() - sessionStart) / 1000
            sessionStart = 0L
            saveStats()
        }
        updateStatsUi()
        updateServerUi()
        refreshClientState()
    }

    // ----------------------------------------------------------------- UI

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
    private fun drawableId(name: String): Int = resources.getIdentifier(name, "drawable", packageName)
    private val GOLD = "#d4af37"

    private fun themeDrawable(theme: String): Int {
        val key = "bg_" + theme.lowercase(Locale.ROOT)
            .replace("é", "e").replace("è", "e").replace(" ", "")
        val id = drawableId(key)
        return if (id != 0) id else drawableId("launcher_bg")
    }

    private fun applyBackgroundOnLaunch() {
        var theme = configMgr.config.background
        if (configMgr.config.randomBackgroundEnabled) {
            val others = THEMES.filter { it != theme }
            if (others.isNotEmpty()) {
                theme = others[(System.nanoTime() % others.size).toInt()]
                configMgr.update { it.copy(background = theme) }
            }
        }
        applyBackground(theme)
    }

    private fun applyBackground(theme: String) {
        val id = themeDrawable(theme)
        if (id != 0) bgView.setImageResource(id) else bgView.setBackgroundColor(Color.parseColor("#0E1B2E"))
    }

    private fun buildHome(): View {
        val root = FrameLayout(this)

        bgView = ImageView(this).apply { scaleType = ImageView.ScaleType.CENTER_CROP }
        root.addView(bgView, FrameLayout.LayoutParams(MATCH, MATCH))
        root.addView(View(this).apply { setBackgroundColor(Color.parseColor("#660A0F1A")) },
            FrameLayout.LayoutParams(MATCH, MATCH))

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(14), dp(18), dp(12))
        }

        val header = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        drawableId("sylvania_logo").let { if (it != 0) header.addView(ImageView(this).apply { setImageResource(it) }, LinearLayout.LayoutParams(dp(72), dp(72))) }
        header.addView(TextView(this).apply {
            text = "  Sylvania Launcher"; setTextColor(Color.parseColor(GOLD)); textSize = 22f
            setShadowLayer(6f, 0f, 2f, Color.BLACK)
        })
        content.addView(header, wrap())

        val panels = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        panels.addView(buildServerPanel(), lin(0, MATCH, 1f).apply { marginEnd = dp(8) })
        panels.addView(buildStatsPanel(), lin(0, MATCH, 1f).apply { marginStart = dp(8) })
        content.addView(panels, LinearLayout.LayoutParams(MATCH, 0, 1f).apply { topMargin = dp(10) })

        status = TextView(this).apply { setTextColor(Color.parseColor("#7ec8e3")); textSize = 12f; setPadding(0, dp(6), 0, dp(2)) }
        content.addView(status, wrap())
        content.addView(TextView(this).apply {
            text = "© 2025 Sylvania Launcher Android Edition — World of Warcraft 3.3.5"
            setTextColor(Color.parseColor(GOLD)); textSize = 10f; gravity = Gravity.CENTER
        }, wrap())

        root.addView(content, FrameLayout.LayoutParams(MATCH, MATCH))
        return root
    }

    private fun buildServerPanel(): View {
        val panel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; background = panelBg(); setPadding(dp(16), dp(12), dp(16), dp(12)) }
        panel.addView(panelTitle("Serveur"))
        serverNameLabel = label("", "#ffffff", 13f, true); panel.addView(serverNameLabel)
        realmlistLabel = label("", "#7ec8e3", 12f, false); panel.addView(realmlistLabel)

        val row1 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        playButton = wowButton("JOUER", "#4a7c3f", "#2a5a1f", "#5a8c4f", "#ffffff", 15f).also { it.setOnClickListener { onPlayClicked() } }
        row1.addView(playButton, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton("TÉLÉCHARGER", "#4a3a20", "#2a1a0a", GOLD, GOLD, 11f).also { it.setOnClickListener { onDownloadClicked() } }, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton("HD", "#2a6dd4", "#15479a", "#3a7de4", "#ffffff", 13f).also { it.setOnClickListener { onHdClicked() } }, lin(0, WRAP, 1f))
        panel.addView(row1, topMargin(wrap(), 14))

        val row2 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        row2.addView(wowButton("RÉGLAGES", "#5a4a3d", "#3a2a1d", "#6a5a4d", GOLD, 11f).also { it.setOnClickListener { onSettingsClicked() } }, lin(0, WRAP, 1f).apply { marginEnd = dp(6) })
        row2.addView(wowButton("QUITTER", "#8a4a5a", "#6a2a3a", "#9a5a6a", "#ffffff", 11f).also { it.setOnClickListener { finishAffinity() } }, lin(0, WRAP, 1f))
        panel.addView(row2, topMargin(wrap(), 8))

        panel.addView(wowButton("Liste des Addons", "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f).also { it.setOnClickListener { onAddonsClicked() } }, topMargin(wrap(), 8))
        panel.addView(View(this), lin(MATCH, 0, 1f))
        return panel
    }

    private fun buildStatsPanel(): View {
        val panel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; background = panelBg(); setPadding(dp(16), dp(12), dp(16), dp(12)) }
        panel.addView(panelTitle("Statistiques de Jeu"))
        playTimeLabel = statBox("Temps de jeu : 0h 0min")
        launchCountLabel = statBox("Lancements : 0")
        lastSessionLabel = statBox("Dernière session : --")
        panel.addView(playTimeLabel, topMargin(wrap(), 6))
        panel.addView(launchCountLabel, topMargin(wrap(), 8))
        panel.addView(lastSessionLabel, topMargin(wrap(), 8))
        panel.addView(View(this), lin(MATCH, 0, 1f))
        panel.addView(wowButton("Changer de Serveur", "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f).also { it.setOnClickListener { onChangeServerClicked() } }, topMargin(wrap(), 8))
        return panel
    }

    // -------------------------------------------------------- UI helpers

    private val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
    private val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT
    private fun wrap() = LinearLayout.LayoutParams(MATCH, WRAP)
    private fun lin(w: Int, h: Int, weight: Float) = LinearLayout.LayoutParams(w, h, weight)
    private fun topMargin(lp: LinearLayout.LayoutParams, m: Int) = lp.apply { topMargin = dp(m) }

    private fun panelBg() = GradientDrawable().apply { setColor(Color.argb(160, 0, 0, 0)); setStroke(dp(2), Color.parseColor("#5a4a2d")); cornerRadius = dp(10).toFloat() }
    private fun panelTitle(t: String) = TextView(this).apply { text = t; setTextColor(Color.parseColor(GOLD)); textSize = 17f; gravity = Gravity.CENTER; setPadding(0, 0, 0, dp(6)) }
    private fun label(t: String, color: String, size: Float, bold: Boolean) = TextView(this).apply {
        text = t; setTextColor(Color.parseColor(color)); textSize = size
        if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD)
    }
    private fun statBox(t: String) = TextView(this).apply {
        text = t; setTextColor(Color.WHITE); textSize = 13f; gravity = Gravity.CENTER; setPadding(dp(10), dp(10), dp(10), dp(10))
        background = GradientDrawable().apply { setColor(Color.argb(150, 50, 38, 20)); setStroke(dp(2), Color.parseColor(GOLD)); cornerRadius = dp(8).toFloat() }
    }
    /** ScrollView whose child is WRAP_CONTENT height so it actually scrolls
     *  (the FrameLayout default of MATCH_PARENT clips long content to one screen). */
    private fun scrollOf(child: View): ScrollView = ScrollView(this).apply {
        isFillViewport = false
        addView(child, FrameLayout.LayoutParams(MATCH, WRAP))
    }

    private fun styleButton(b: Button, text: String, top: String, bottom: String, border: String, txt: String, size: Float) {
        b.text = text; b.isAllCaps = false; b.textSize = size; b.setTextColor(Color.parseColor(txt))
        b.setTypeface(b.typeface, android.graphics.Typeface.BOLD); b.stateListAnimator = null
        b.setPadding(dp(6), dp(10), dp(6), dp(10))
        b.background = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(Color.parseColor(top), Color.parseColor(bottom)))
            .apply { cornerRadius = dp(8).toFloat(); setStroke(dp(2), Color.parseColor(border)) }
    }
    private fun wowButton(text: String, top: String, bottom: String, border: String, txt: String, size: Float): Button =
        Button(this).also { styleButton(it, text, top, bottom, border, txt, size) }

    private fun log(msg: String) { Log.i(TAG, msg); runOnUiThread { status.append("\n$msg") } }
    private fun setStatus(msg: String) { Log.i(TAG, msg); runOnUiThread { status.text = msg } }
    private fun comingSoon(feature: String) = Toast.makeText(this, "$feature — bientôt disponible", Toast.LENGTH_SHORT).show()

    // --------------------------------------------------------- Server UI

    private fun activeEntry(): RealmlistEntry {
        val entries = configMgr.realmlistEntries
        val idx = configMgr.activeRealmlistIndex
        return entries.getOrNull(idx) ?: RealmlistEntry("Sylvania", Constants.CANONICAL_REALMLIST, true)
    }

    private fun updateServerUi() {
        val e = activeEntry()
        serverNameLabel.text = "Serveur : ${e.name}"
        realmlistLabel.text = "Realmlist : ${e.address.removePrefix("set realmlist ").trim()}"
    }

    // ------------------------------------------------------ Settings UI

    private fun onSettingsClicked() {
        val cfg = configMgr.config
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(12), dp(20), dp(4)) }

        box.addView(label("Dossier du client WoW :", GOLD, 13f, true))
        val pathField = EditText(this).apply { setText(if (cfg.wowPath.isBlank()) defaultClientDir().absolutePath else cfg.wowPath); textSize = 12f }
        box.addView(pathField)

        box.addView(label("Langue du client :", GOLD, 13f, true).apply { setPadding(0, dp(12), 0, 0) })
        val langGroup = RadioGroup(this).apply { orientation = RadioGroup.HORIZONTAL }
        val rbFr = RadioButton(this).apply { text = "Français (frFR)" }
        val rbEn = RadioButton(this).apply { text = "English (enUS)" }
        langGroup.addView(rbFr); langGroup.addView(rbEn)
        (if (cfg.language == "en") rbEn else rbFr).isChecked = true
        box.addView(langGroup)

        box.addView(label("Thème de fond :", GOLD, 13f, true).apply { setPadding(0, dp(12), 0, 0) })
        val themeSpinner = Spinner(this).apply {
            adapter = ArrayAdapter(this@SylvaniaLauncherActivity, android.R.layout.simple_spinner_dropdown_item, THEMES)
            setSelection(THEMES.indexOf(cfg.background).coerceAtLeast(0))
        }
        box.addView(themeSpinner)

        val randomCb = CheckBox(this).apply { text = "Fond aléatoire au démarrage"; isChecked = cfg.randomBackgroundEnabled }
        box.addView(randomCb, topMargin(wrap(), 8))

        AlertDialog.Builder(this)
            .setTitle("Réglages")
            .setView(scrollOf(box))
            .setPositiveButton("Enregistrer") { _, _ ->
                val newPath = pathField.text.toString().trim()
                val newLang = if (rbEn.isChecked) "en" else "fr"
                val newTheme = THEMES[themeSpinner.selectedItemPosition]
                val langChanged = newLang != configMgr.config.language
                configMgr.update {
                    it.copy(wowPath = newPath, language = newLang, background = newTheme, randomBackgroundEnabled = randomCb.isChecked)
                }
                if (langChanged) applyClientLocale(newLang)
                applyBackground(newTheme)
                refreshClientState()
                Toast.makeText(this, "Réglages enregistrés", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Annuler", null)
            .show()
    }

    /** FR/EN → Config.wtf locale (frFR/enUS), mirrors the C++ changeLanguage. */
    private fun applyClientLocale(lang: String) {
        val wtf = File(clientDir(), "WTF/Config.wtf")
        if (!wtf.exists()) return
        try {
            var c = wtf.readText()
            c = if (lang == "en") {
                if (c.contains("SET locale")) c.replace("SET locale \"frFR\"", "SET locale \"enUS\"")
                else c + "\nSET locale \"enUS\"\n"
            } else {
                c.replace("SET locale \"enUS\"", "SET locale \"frFR\"")
            }
            wtf.writeText(c)
        } catch (e: Exception) { Log.w(TAG, "applyClientLocale", e) }
    }

    // -------------------------------------------------- Change server UI

    private fun onChangeServerClicked() {
        val entries = configMgr.realmlistEntries.toMutableList()
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(4)) }
        val group = RadioGroup(this)
        entries.forEachIndexed { i, e ->
            group.addView(RadioButton(this).apply {
                text = "${e.name}  (${e.address.removePrefix("set realmlist ").trim()})"
                id = i; isChecked = i == configMgr.activeRealmlistIndex
            })
        }
        box.addView(group)

        AlertDialog.Builder(this)
            .setTitle("Changer de serveur")
            .setView(scrollOf(box))
            .setPositiveButton("Sélectionner") { _, _ ->
                val idx = group.checkedRadioButtonId
                if (idx in entries.indices) {
                    configMgr.setActiveRealmlistIndex(idx)
                    updateServerUi()
                    Toast.makeText(this, "Serveur : ${entries[idx].name}", Toast.LENGTH_SHORT).show()
                }
            }
            .setNeutralButton("Ajouter…") { _, _ -> promptAddServer() }
            .setNegativeButton("Fermer", null)
            .show()
    }

    private fun promptAddServer() {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(4)) }
        val nameField = EditText(this).apply { hint = "Nom du serveur" }
        val addrField = EditText(this).apply { hint = "adresse (ex. logon.monserveur.com)" }
        box.addView(label("Nom :", GOLD, 12f, true)); box.addView(nameField)
        box.addView(label("Adresse realmlist :", GOLD, 12f, true).apply { setPadding(0, dp(8), 0, 0) }); box.addView(addrField)
        AlertDialog.Builder(this)
            .setTitle("Ajouter un serveur")
            .setView(box)
            .setPositiveButton("Ajouter") { _, _ ->
                val name = nameField.text.toString().trim()
                val addr = addrField.text.toString().trim()
                if (name.isNotEmpty() && addr.isNotEmpty()) {
                    val line = if (addr.startsWith("set realmlist")) addr else "set realmlist $addr"
                    val newEntries = configMgr.realmlistEntries + RealmlistEntry(name, line, false)
                    configMgr.setRealmlistEntries(newEntries)
                    Toast.makeText(this, "Serveur ajouté", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("Annuler", null)
            .show()
    }

    @Volatile private var downloadCancel = false

    private fun onDownloadClicked() {
        AlertDialog.Builder(this)
            .setTitle("Télécharger le client")
            .setMessage("Télécharger le client WoW 3.3.5 (~19 Go) depuis ${Constants.SERVER_HOST} et l'extraire dans ${clientDir().absolutePath} ?\n\nPrévois ~40 Go libres et une connexion Wi-Fi stable. Le téléchargement peut être long.")
            .setPositiveButton("Télécharger") { _, _ -> startClientDownload() }
            .setNegativeButton("Annuler", null)
            .show()
    }

    private fun fmt(bytes: Long): String {
        var s = bytes.toDouble(); val u = arrayOf("o", "Ko", "Mo", "Go"); var i = 0
        while (s >= 1024 && i < 3) { s /= 1024; i++ }
        return String.format(Locale.FRANCE, "%.1f %s", s, u[i])
    }

    /** Find the directory actually containing Wow.exe under [root] (the zip may nest it). */
    private fun findClientRoot(root: File): File {
        if (File(root, "Wow.exe").exists()) return root
        root.walkTopDown().maxDepth(3).forEach { if (it.isFile && it.name.equals("Wow.exe", true)) return it.parentFile!! }
        return root
    }

    private fun startClientDownload() {
        val dest = clientDir().apply { mkdirs() }
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(16), dp(20), dp(8)) }
        val st = TextView(this).apply { text = "Connexion…"; setTextColor(Color.parseColor(GOLD)); textSize = 14f }
        val bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply { max = 100 }
        val info = TextView(this).apply { textSize = 11f; setTextColor(Color.LTGRAY); setPadding(0, dp(6), 0, 0) }
        box.addView(st); box.addView(bar, topMargin(wrap(), 8)); box.addView(info)

        downloadCancel = false
        val dlg = AlertDialog.Builder(this).setTitle("Téléchargement du client").setView(box).setCancelable(false)
            .setNegativeButton("Annuler") { _, _ -> downloadCancel = true }.create()
        dlg.show()

        Thread {
            val zip = File(dest, "WoWClient_temp.zip")
            var ok = false; var err = ""
            try {
                val conn = (URL(Constants.CLIENT_DOWNLOAD_URL).openConnection() as HttpURLConnection).apply {
                    instanceFollowRedirects = true; connectTimeout = 30000; readTimeout = 60000
                }
                if (conn.responseCode !in 200..299) throw RuntimeException("HTTP ${conn.responseCode}")
                val total = conn.contentLengthLong
                conn.inputStream.use { input ->
                    zip.outputStream().buffered(1 shl 16).use { out ->
                        val buf = ByteArray(1 shl 16); var done = 0L; var lastT = System.currentTimeMillis(); var lastB = 0L
                        while (true) {
                            if (downloadCancel) throw InterruptedException("Annulé")
                            val n = input.read(buf); if (n < 0) break
                            out.write(buf, 0, n); done += n
                            val now = System.currentTimeMillis()
                            if (now - lastT >= 500) {
                                val pct = if (total > 0) (done * 100 / total).toInt() else 0
                                val spd = (done - lastB) * 1000 / (now - lastT).coerceAtLeast(1)
                                lastT = now; lastB = done
                                runOnUiThread {
                                    st.text = if (total > 0) "Téléchargement… $pct%" else "Téléchargement…"
                                    bar.isIndeterminate = total <= 0; if (total > 0) bar.progress = pct
                                    info.text = "${fmt(done)}${if (total > 0) " / " + fmt(total) else ""}  •  ${fmt(spd)}/s"
                                }
                            }
                        }
                    }
                }
                runOnUiThread { st.text = "Extraction…"; bar.isIndeterminate = true; info.text = "Veuillez patienter (peut prendre plusieurs minutes)" }
                ok = ZipExtractor.extract(zip, dest) { }
                zip.delete()
                if (ok) {
                    val clientRoot = findClientRoot(dest)
                    WowConfigWriter.writeConfigWtf(clientRoot)
                    RealmlistWriter.updateRealmlist(clientRoot, activeEntry().address)
                    if (clientRoot.absolutePath != configMgr.wowPath) configMgr.setWowPath(clientRoot.absolutePath)
                }
            } catch (e: Exception) {
                err = e.message ?: "erreur"; if (zip.exists()) zip.delete()
            }
            runOnUiThread {
                dlg.dismiss()
                if (ok) {
                    Toast.makeText(this, "Client installé ✓", Toast.LENGTH_LONG).show()
                    refreshClientState()
                } else {
                    AlertDialog.Builder(this).setTitle("Téléchargement")
                        .setMessage(if (downloadCancel) "Téléchargement annulé." else "Échec : $err")
                        .setPositiveButton("OK", null).show()
                }
            }
        }.start()
    }

    // ----------------------------------------------------------- HD patch

    private fun onHdClicked() {
        val client = clientDir()
        if (!File(client, "Wow.exe").exists()) {
            AlertDialog.Builder(this).setTitle("Patch HD")
                .setMessage("Téléchargez d'abord le client (TÉLÉCHARGER) avant d'installer le Patch HD.")
                .setPositiveButton("OK", null).show()
            return
        }
        val msg = if (HdPatchManager.isInstalled(client))
            "Le Patch HD semble déjà installé. Le réinstaller / mettre à jour ?"
        else "Télécharger et installer le Patch HD (textures/UI haute définition) ?"
        AlertDialog.Builder(this).setTitle("Patch HD").setMessage(msg)
            .setPositiveButton("Installer") { _, _ -> startHdPatch(client) }
            .setNegativeButton("Annuler", null).show()
    }

    private fun startHdPatch(client: File) {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(16), dp(20), dp(8)) }
        val st = TextView(this).apply { text = "Démarrage…"; setTextColor(Color.parseColor(GOLD)); textSize = 14f }
        val bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply { max = 100 }
        box.addView(st); box.addView(bar, topMargin(wrap(), 8))
        val dlg = AlertDialog.Builder(this).setTitle("Patch HD").setView(box).setCancelable(false).create()
        dlg.show()
        Thread {
            val result = runBlocking {
                HdPatchManager(client, File(cacheDir, "SylvaniaLauncher_HD")).install { p ->
                    runOnUiThread {
                        st.text = p.status
                        bar.isIndeterminate = p.percent < 0
                        if (p.percent >= 0) bar.progress = p.percent
                    }
                }
            }
            runOnUiThread {
                dlg.dismiss()
                val ok = result is PatchResult.Success
                val text = when (result) { is PatchResult.Success -> result.message; is PatchResult.Failure -> result.message }
                AlertDialog.Builder(this).setTitle("Patch HD").setMessage(text).setPositiveButton("OK", null).show()
                if (ok) refreshClientState()
            }
        }.start()
    }

    // ------------------------------------------------------------- Addons

    private data class AddonInfo(val name: String, val file: String, val folder: String)

    /** Curated Sylvania addon set (ported from the Windows AddonsWindow). */
    private val ADDONS = listOf(
        AddonInfo("ArkInventory", "ArkInventory.zip", "ArkInventory"),
        AddonInfo("AtlasLoot", "AtlasLoot.zip", "AtlasLoot"),
        AddonInfo("Auctionator", "Auctionator.zip", "Auctionator"),
        AddonInfo("Bagnon", "Bagnon.zip", "Bagnon"),
        AddonInfo("Bartender4", "Bartender4.zip", "Bartender4"),
        AddonInfo("Deadly Boss Mods (DBM)", "DBM.zip", "DBM-Core"),
        AddonInfo("GatherMate2", "GatherMate2.zip", "GatherMate2"),
        AddonInfo("GearScore", "GearScore.zip", "GearScore"),
        AddonInfo("HandyNotes", "HandyNotes.zip", "HandyNotes"),
        AddonInfo("Immersion", "Immersion.zip", "Immersion"),
        AddonInfo("Mapster", "Mapster.zip", "Mapster"),
        AddonInfo("Postal", "Postal.zip", "Postal"),
        AddonInfo("Prat 3.0", "Prat-3.0.zip", "Prat-3.0"),
        AddonInfo("Quartz", "Quartz.zip", "Quartz"),
        AddonInfo("QuestHelper", "QuestHelper.zip", "QuestHelper"),
        AddonInfo("TomTom", "TomTom.zip", "TomTom"),
        AddonInfo("WeakAuras", "WeakAuras.zip", "WeakAuras2"),
        AddonInfo("XPerl", "XPerl.zip", "XPerl"),
        AddonInfo("Details!", "details.zip", "Details"),
        AddonInfo("ElvUI", "elvui.zip", "ElvUI"),
        AddonInfo("TotalRP3", "totalRP3.zip", "TotalRP3"),
        AddonInfo("VuhDo", "vuhdo.zip", "VuhDo"),
    )

    private fun onAddonsClicked() {
        val addonsDir = File(clientDir(), "Interface/AddOns")
        if (!File(clientDir(), "Wow.exe").exists()) {
            AlertDialog.Builder(this).setTitle("Addons")
                .setMessage("Téléchargez d'abord le client (TÉLÉCHARGER) pour gérer les addons.")
                .setPositiveButton("OK", null).show()
            return
        }
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(16), dp(8), dp(16), dp(8)) }
        list.addView(TextView(this).apply {
            text = if (File(addonsDir, "ConsolePort").isDirectory) "✓ ConsolePort (manette) installé\n" else ""
            setTextColor(Color.parseColor("#7ec8e3")); textSize = 12f
        })
        for (a in ADDONS) {
            val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(6), 0, dp(6)) }
            row.addView(TextView(this).apply { text = a.name; setTextColor(Color.WHITE); textSize = 14f }, lin(0, WRAP, 1f))
            val btn = Button(this)
            val statusTv = TextView(this).apply { textSize = 10f; setTextColor(Color.parseColor("#7ec8e3")) }
            configAddonButton(btn, a, addonsDir, statusTv)
            val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.END }
            col.addView(btn); col.addView(statusTv)
            row.addView(col, LinearLayout.LayoutParams(WRAP, WRAP))
            list.addView(row, LinearLayout.LayoutParams(MATCH, WRAP))
        }
        AlertDialog.Builder(this).setTitle("Addons recommandés")
            .setView(scrollOf(list))
            .setPositiveButton("Fermer", null).show()
    }

    /** Sets the row button to Installer (green) or Supprimer (red) per install state. */
    private fun configAddonButton(btn: Button, a: AddonInfo, addonsDir: File, statusTv: TextView) {
        if (File(addonsDir, a.folder).isDirectory) {
            styleButton(btn, "Supprimer", "#8a4a5a", "#6a2a3a", "#9a5a6a", "#ffffff", 11f)
            btn.setOnClickListener { confirmDeleteAddon(a, addonsDir, btn, statusTv) }
        } else {
            styleButton(btn, "Installer", "#4a7c3f", "#2a5a1f", "#5a8c4f", "#ffffff", 11f)
            btn.setOnClickListener { installAddon(a, addonsDir, btn, statusTv) }
        }
    }

    private fun confirmDeleteAddon(a: AddonInfo, addonsDir: File, btn: Button, statusTv: TextView) {
        AlertDialog.Builder(this)
            .setTitle("Supprimer ${a.name} ?")
            .setMessage("Le dossier de l'addon sera supprimé du client.")
            .setPositiveButton("Supprimer") { _, _ ->
                val ok = File(addonsDir, a.folder).deleteRecursively()
                statusTv.text = if (ok) "Supprimé" else "Échec"
                configAddonButton(btn, a, addonsDir, statusTv)
            }
            .setNegativeButton("Annuler", null)
            .show()
    }

    private fun installAddon(a: AddonInfo, addonsDir: File, btn: Button, statusTv: TextView) {
        btn.isEnabled = false; btn.text = "Patientez…"; statusTv.text = "Téléchargement…"
        addonsDir.mkdirs()
        Thread {
            val zip = File(addonsDir, a.file)
            var ok = false
            try {
                val url = Constants.ADDON_DOWNLOAD_ENDPOINT + "?file=" + a.file
                val conn = (URL(url).openConnection() as HttpURLConnection).apply { instanceFollowRedirects = true; connectTimeout = 30000; readTimeout = 60000 }
                if (conn.responseCode in 200..299) {
                    conn.inputStream.use { i -> zip.outputStream().buffered().use { o -> i.copyTo(o) } }
                    runOnUiThread { statusTv.text = "Extraction…" }
                    ok = ZipExtractor.extract(zip, addonsDir) { }
                }
                zip.delete()
            } catch (e: Exception) { Log.w(TAG, "addon ${a.name}", e); if (zip.exists()) zip.delete() }
            runOnUiThread {
                btn.isEnabled = true
                configAddonButton(btn, a, addonsDir, statusTv) // → Supprimer if now installed, else Installer
                statusTv.text = if (ok) "Installé ✓" else "Échec"
            }
        }.start()
    }

    // ------------------------------------------------------------- Stats

    private fun statsFile() = File(filesDir, "stats.json")
    private fun loadStats() {
        try {
            val f = statsFile(); if (!f.exists()) return
            val o = JSONObject(f.readText())
            launchCount = o.optInt("launch_count", 0); totalPlayTime = o.optLong("total_play_time", 0L); lastSession = o.optLong("last_session", 0L)
        } catch (e: Exception) { Log.w(TAG, "loadStats", e) }
    }
    private fun saveStats() {
        try { statsFile().writeText(JSONObject().apply { put("launch_count", launchCount); put("total_play_time", totalPlayTime); put("last_session", lastSession) }.toString()) }
        catch (e: Exception) { Log.w(TAG, "saveStats", e) }
    }
    private fun updateStatsUi() {
        val h = totalPlayTime / 3600; val m = (totalPlayTime % 3600) / 60
        playTimeLabel.text = "Temps de jeu : ${h}h ${m}min"
        launchCountLabel.text = "Lancements : $launchCount"
        lastSessionLabel.text = if (lastSession > 0) "Dernière session : " + SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.FRANCE).format(Date(lastSession)) else "Dernière session : --"
    }

    private fun refreshClientState() {
        val client = clientDir()
        val hasClient = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", true) }?.isNotEmpty() == true
        playButton.isEnabled = hasClient; playButton.alpha = if (hasClient) 1f else 0.5f
        if (!hasClient) setStatus("Client WoW introuvable dans ${client.absolutePath}.\nUtilisez TÉLÉCHARGER ou RÉGLAGES.")
        else setStatus("Prêt à jouer sur ${activeEntry().name} !")
    }

    // ----------------------------------------------------------- Launch

    private fun onPlayClicked() {
        playButton.isEnabled = false
        setStatus("Préparation du runtime…")
        Thread {
            try {
                val imageFs = ImageFs.find(this)
                if (!isImageFsInstalled(imageFs)) installImageFs(imageFs) else log("Runtime déjà installé (v${imageFs.version}).")
                ensureControlsProfile()
                val contents = ContentsManager(this).apply { syncContents() }
                val manager = ContainerManager(this)
                runOnUiThread { ensureContainerAndLaunch(manager, contents) }
            } catch (e: Exception) {
                Log.e(TAG, "prepare failed", e); setStatus("Erreur: ${e.message}")
                runOnUiThread { playButton.isEnabled = true }
            }
        }.start()
    }

    private fun ensureControlsProfile() {
        val dest = File(filesDir, "profiles/controls-$CONTROLS_PROFILE_ID.icp")
        if (dest.exists()) return
        try {
            dest.parentFile?.mkdirs()
            assets.open("profiles/controls-$CONTROLS_PROFILE_ID.icp").use { input -> dest.outputStream().use { input.copyTo(it) } }
            log("Profil manette installé (WoW ConsolePort).")
        } catch (e: Exception) { Log.w(TAG, "controls profile install failed", e) }
    }

    private fun isImageFsInstalled(imageFs: ImageFs): Boolean =
        imageFs.isValid && imageFs.version >= ImageFsInstaller.LATEST_VERSION && File(imageFs.rootDir, "opt/$ARM64EC/bin/wine").exists()

    private fun installImageFs(imageFs: ImageFs) {
        val rootDir = imageFs.rootDir
        log("Extraction du rootfs (imagefs.txz)… (~quelques minutes)")
        if (!TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "imagefs.txz", rootDir)) throw RuntimeException("échec extraction imagefs.txz")
        for (v in WINE_VERSIONS) {
            val out = File(rootDir, "opt/$v").apply { mkdirs() }
            log("Extraction de $v…")
            TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "$v.txz", out)
        }
        imageFs.createImgVersionFile(ImageFsInstaller.LATEST_VERSION.toInt())
        log("Runtime installé.")
    }

    private fun defaultClientDir(): File = File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher/wotlk")
    private fun clientDir(): File = configMgr.wowPath.ifBlank { null }?.let { File(it) } ?: defaultClientDir()

    private fun ensureContainerAndLaunch(manager: ContainerManager, contents: ContentsManager) {
        val existing = manager.containers
        if (existing.isNotEmpty()) { launchContainer(existing[0]); return }
        setStatus("Création du conteneur arm64ec / WoW64…")
        val data = JSONObject().apply {
            put("name", "Sylvania"); put("wineVersion", ARM64EC); put("wow64Mode", true)
            put("screenSize", "1280x720"); put("emulator", "wowbox64"); put("box64Version", "0.3.7"); put("fexcoreVersion", "2507")
            put("dxwrapper", Container.DEFAULT_DXWRAPPER); put("dxwrapperConfig", Container.DEFAULT_DXWRAPPERCONFIG)
            put("graphicsDriver", Container.DEFAULT_GRAPHICS_DRIVER); put("graphicsDriverConfig", Container.DEFAULT_GRAPHICSDRIVERCONFIG)
            put("ddrawrapper", Container.DEFAULT_DDRAWRAPPER)
        }
        manager.createContainerAsync(data, contents) { container ->
            if (container != null) { log("Conteneur créé (id=${container.id})."); launchContainer(container) }
            else { setStatus("Échec de création du conteneur."); playButton.isEnabled = true }
        }
    }

    private fun launchContainer(container: Container) {
        val cfg = container.dxWrapperConfig
        if (cfg == null || !cfg.contains("version=")) container.dxWrapperConfig = Container.DEFAULT_DXWRAPPERCONFIG
        container.screenSize = "1920x1080"
        // Turnip (Mesa) via AdrenoTools — the proprietary Adreno driver fails a DXVK
        // memory allocation and hangs WoW at the login screen; Turnip works.
        container.graphicsDriver = "wrapper"
        container.graphicsDriverConfig = "version=turnip25.1.0;blacklistedExtensions=;maxDeviceMemory=0;adrenotoolsTurnip=1;frameSync=Normal"

        val client = clientDir()
        val wowExe = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", ignoreCase = true) }?.firstOrNull()
        if (wowExe == null) { setStatus("Client WoW introuvable dans ${client.absolutePath}."); playButton.isEnabled = true; return }

        // Write the active realmlist (from config) into the client.
        RealmlistWriter.updateRealmlist(client, activeEntry().address)

        container.drives = "D:" + client.absolutePath
        container.saveData()

        val desktopDir = container.desktopDir.apply { mkdirs() }
        val shortcut = File(desktopDir, "Sylvania.desktop")
        shortcut.writeText(buildString {
            appendLine("[Desktop Entry]"); appendLine("Type=Application"); appendLine("Name=Sylvania WoW")
            appendLine("Exec=wine D:/${wowExe.name}")
            appendLine(""); appendLine("[Extra Data]"); appendLine("controlsProfile=$CONTROLS_PROFILE_ID")
        })

        launchCount++; lastSession = System.currentTimeMillis(); sessionStart = lastSession; saveStats(); updateStatsUi()

        setStatus("Lancement de World of Warcraft…")
        startActivity(Intent(this, XServerDisplayActivity::class.java).apply {
            putExtra("container_id", container.id); putExtra("shortcut_path", shortcut.absolutePath); putExtra("shortcut_name", "Sylvania WoW")
        })
        playButton.isEnabled = true
    }

    companion object {
        private const val TAG = "SylvaniaLauncher"
        private const val CONTROLS_PROFILE_ID = 10
    }
}
