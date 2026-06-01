package com.sylvania.launcher

import android.Manifest
import android.app.AlertDialog
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
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
import com.winlator.cmod.R
import com.winlator.cmod.XServerDisplayActivity
import com.winlator.cmod.container.Container
import com.winlator.cmod.container.ContainerManager
import com.winlator.cmod.contents.ContentsManager
import com.winlator.cmod.core.TarCompressorUtils
import com.winlator.cmod.xenvironment.ImageFs
import com.winlator.cmod.xenvironment.ImageFsInstaller
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Sylvania launcher home screen — Android port of the Windows/Qt launcher's
 * MainWindow. Strings are localized FR (default) / EN via string resources, and
 * the language button toggles the in-app locale (recreate) + the client
 * Config.wtf locale, mirroring the C++ changeLanguage()/retranslateUi().
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
    private val THEMES = listOf("Alliance", "Arbre de Vie", "Azeroth", "Horde", "Ilidan", "Lich King", "Ragnaros", "Taverne")

    private var launchCount = 0
    private var totalPlayTime = 0L
    private var lastSession: Long = 0L
    private var sessionStart: Long = 0L

    /** Apply the saved UI language (FR default / EN) to this activity's resources. */
    override fun attachBaseContext(newBase: Context) {
        val lang = readSavedLanguage(newBase)
        val locale = Locale(lang)
        Locale.setDefault(locale)
        val cfg = Configuration(newBase.resources.configuration).apply { setLocale(locale) }
        super.attachBaseContext(newBase.createConfigurationContext(cfg))
    }

    private fun readSavedLanguage(ctx: Context): String = try {
        val f = File(ctx.filesDir, "config.json")
        if (f.exists()) (if (JSONObject(f.readText()).optString("language", "fr") == "en") "en" else "fr") else "fr"
    } catch (e: Exception) { "fr" }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        configMgr = ConfigManager(filesDir)
        if (configMgr.wowPath.isBlank()) configMgr.setWowPath(defaultClientDir().absolutePath)

        setContentView(buildHome())

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
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
        finalizePendingSession()
        updateStatsUi()
        updateServerUi()
        refreshClientState()
    }

    // ----------------------------------------------------------------- UI

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
    private fun drawableId(name: String): Int = resources.getIdentifier(name, "drawable", packageName)
    private val GOLD = "#d4af37"

    private fun themeDrawable(theme: String): Int {
        val key = "bg_" + theme.lowercase(Locale.ROOT).replace("é", "e").replace("è", "e").replace(" ", "")
        val id = drawableId(key); return if (id != 0) id else drawableId("launcher_bg")
    }

    private fun applyBackgroundOnLaunch() {
        var theme = configMgr.config.background
        if (configMgr.config.randomBackgroundEnabled) {
            val others = THEMES.filter { it != theme }
            if (others.isNotEmpty()) { theme = others[(System.nanoTime() % others.size).toInt()]; configMgr.update { it.copy(background = theme) } }
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
        root.addView(View(this).apply { setBackgroundColor(Color.parseColor("#660A0F1A")) }, FrameLayout.LayoutParams(MATCH, MATCH))

        val content = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(18), dp(14), dp(18), dp(12)) }
        val header = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        drawableId("sylvania_logo").let { if (it != 0) header.addView(ImageView(this).apply { setImageResource(it) }, LinearLayout.LayoutParams(dp(72), dp(72))) }
        header.addView(TextView(this).apply { text = "  Sylvania Launcher"; setTextColor(Color.parseColor(GOLD)); textSize = 22f; setShadowLayer(6f, 0f, 2f, Color.BLACK) })
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
        panel.addView(panelTitle(getString(R.string.syl_panel_server)))
        serverNameLabel = label("", "#ffffff", 13f, true); panel.addView(serverNameLabel)
        realmlistLabel = label("", "#7ec8e3", 12f, false); panel.addView(realmlistLabel)

        val row1 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        playButton = wowButton(getString(R.string.syl_play), "#4a7c3f", "#2a5a1f", "#5a8c4f", "#ffffff", 15f).also { it.setOnClickListener { onPlayClicked() } }
        row1.addView(playButton, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton(getString(R.string.syl_download), "#4a3a20", "#2a1a0a", GOLD, GOLD, 11f).also { it.setOnClickListener { onDownloadClicked() } }, lin(0, WRAP, 2f).apply { marginEnd = dp(6) })
        row1.addView(wowButton(getString(R.string.syl_hd), "#2a6dd4", "#15479a", "#3a7de4", "#ffffff", 13f).also { it.setOnClickListener { onHdClicked() } }, lin(0, WRAP, 1f))
        panel.addView(row1, topMargin(wrap(), 14))

        val row2 = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        row2.addView(wowButton(getString(R.string.syl_settings), "#5a4a3d", "#3a2a1d", "#6a5a4d", GOLD, 11f).also { it.setOnClickListener { onSettingsClicked() } }, lin(0, WRAP, 1f).apply { marginEnd = dp(6) })
        row2.addView(wowButton(getString(R.string.syl_quit), "#8a4a5a", "#6a2a3a", "#9a5a6a", "#ffffff", 11f).also { it.setOnClickListener { finishAffinity() } }, lin(0, WRAP, 1f))
        panel.addView(row2, topMargin(wrap(), 8))

        panel.addView(wowButton(getString(R.string.syl_addons), "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f).also { it.setOnClickListener { onAddonsClicked() } }, topMargin(wrap(), 8))
        panel.addView(View(this), lin(MATCH, 0, 1f))
        return panel
    }

    private fun buildStatsPanel(): View {
        val panel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; background = panelBg(); setPadding(dp(16), dp(12), dp(16), dp(12)) }
        panel.addView(panelTitle(getString(R.string.syl_panel_stats)))
        playTimeLabel = statBox(""); launchCountLabel = statBox(""); lastSessionLabel = statBox("")
        panel.addView(playTimeLabel, topMargin(wrap(), 6))
        panel.addView(launchCountLabel, topMargin(wrap(), 8))
        panel.addView(lastSessionLabel, topMargin(wrap(), 8))
        panel.addView(View(this), lin(MATCH, 0, 1f))
        // Language is changed from RÉGLAGES (FR/EN), so no language button here.
        panel.addView(wowButton(getString(R.string.syl_change_server), "#7a6a3a", "#3a3a1a", GOLD, "#ffffff", 12f).also { it.setOnClickListener { onChangeServerClicked() } }, topMargin(wrap(), 10))
        return panel
    }

    private val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
    private val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT
    private fun wrap() = LinearLayout.LayoutParams(MATCH, WRAP)
    private fun lin(w: Int, h: Int, weight: Float) = LinearLayout.LayoutParams(w, h, weight)
    private fun topMargin(lp: LinearLayout.LayoutParams, m: Int) = lp.apply { topMargin = dp(m) }
    private fun panelBg() = GradientDrawable().apply { setColor(Color.argb(160, 0, 0, 0)); setStroke(dp(2), Color.parseColor("#5a4a2d")); cornerRadius = dp(10).toFloat() }
    private fun panelTitle(t: String) = TextView(this).apply { text = t; setTextColor(Color.parseColor(GOLD)); textSize = 17f; gravity = Gravity.CENTER; setPadding(0, 0, 0, dp(6)) }
    private fun label(t: String, color: String, size: Float, bold: Boolean) = TextView(this).apply { text = t; setTextColor(Color.parseColor(color)); textSize = size; if (bold) setTypeface(typeface, android.graphics.Typeface.BOLD) }
    private fun statBox(t: String) = TextView(this).apply {
        text = t; setTextColor(Color.WHITE); textSize = 13f; gravity = Gravity.CENTER; setPadding(dp(10), dp(8), dp(10), dp(8))
        background = GradientDrawable().apply { setColor(Color.argb(150, 50, 38, 20)); setStroke(dp(2), Color.parseColor(GOLD)); cornerRadius = dp(8).toFloat() }
    }
    private fun scrollOf(child: View): ScrollView = ScrollView(this).apply { isFillViewport = false; addView(child, FrameLayout.LayoutParams(MATCH, WRAP)) }
    private fun styleButton(b: Button, text: String, top: String, bottom: String, border: String, txt: String, size: Float) {
        b.text = text; b.isAllCaps = false; b.textSize = size; b.setTextColor(Color.parseColor(txt))
        b.setTypeface(b.typeface, android.graphics.Typeface.BOLD); b.stateListAnimator = null; b.setPadding(dp(6), dp(10), dp(6), dp(10))
        b.background = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(Color.parseColor(top), Color.parseColor(bottom))).apply { cornerRadius = dp(8).toFloat(); setStroke(dp(2), Color.parseColor(border)) }
    }
    private fun wowButton(text: String, top: String, bottom: String, border: String, txt: String, size: Float): Button = Button(this).also { styleButton(it, text, top, bottom, border, txt, size) }

    private fun log(msg: String) { Log.i(TAG, msg); runOnUiThread { status.append("\n$msg") } }
    private fun setStatus(msg: String) { Log.i(TAG, msg); runOnUiThread { status.text = msg } }

    // --------------------------------------------------------- Server UI

    private fun activeEntry(): RealmlistEntry {
        val entries = configMgr.realmlistEntries
        return entries.getOrNull(configMgr.activeRealmlistIndex) ?: RealmlistEntry("Sylvania", Constants.CANONICAL_REALMLIST, true)
    }
    private fun updateServerUi() {
        val e = activeEntry()
        serverNameLabel.text = getString(R.string.syl_server_name, e.name)
        realmlistLabel.text = getString(R.string.syl_realmlist, e.address.removePrefix("set realmlist ").trim())
    }

    // ------------------------------------------------------ Language

    /**
     * Apply a UI language change (mirrors the C++ changeLanguage): persist it,
     * switch the client Config.wtf locale (frFR/enUS), and — when switching to
     * English — scan the client for the enUS language pack and offer to download
     * it (enus-download.php) if missing. Then recreate() to retranslate the UI.
     */
    private fun applyLanguageChange(newLang: String) {
        configMgr.update { it.copy(language = newLang) }
        applyClientLocale(newLang)
        if (newLang == "en") {
            val client = clientDir()
            if (File(client, "Wow.exe").exists() && !isEnusInstalled(client)) {
                AlertDialog.Builder(this)
                    .setTitle(getString(R.string.syl_enus_missing_title))
                    .setMessage(getString(R.string.syl_enus_missing_msg))
                    .setPositiveButton(getString(R.string.syl_download)) { _, _ -> startEnusDownload { recreate() } }
                    .setNegativeButton(getString(R.string.syl_cancel)) { _, _ -> recreate() }
                    .setCancelable(false)
                    .show()
                return
            }
        }
        recreate() // attachBaseContext re-reads the saved language → UI rebuilt translated
    }

    /** FR/EN → Config.wtf locale (frFR/enUS), mirrors the C++ changeLanguage. */
    private fun applyClientLocale(lang: String) {
        val wtf = File(clientDir(), "WTF/Config.wtf")
        if (!wtf.exists()) return
        try {
            var c = wtf.readText()
            c = if (lang == "en") {
                if (c.contains("SET locale")) c.replace("SET locale \"frFR\"", "SET locale \"enUS\"") else c + "\nSET locale \"enUS\"\n"
            } else c.replace("SET locale \"enUS\"", "SET locale \"frFR\"")
            wtf.writeText(c)
        } catch (e: Exception) { Log.w(TAG, "applyClientLocale", e) }
    }

    // ------------------------------------------------------ Settings UI

    private fun onSettingsClicked() {
        val cfg = configMgr.config
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(12), dp(20), dp(4)) }
        box.addView(label(getString(R.string.syl_set_client_path), GOLD, 13f, true))
        val pathField = EditText(this).apply { setText(if (cfg.wowPath.isBlank()) defaultClientDir().absolutePath else cfg.wowPath); textSize = 12f }
        box.addView(pathField)
        box.addView(label(getString(R.string.syl_set_language), GOLD, 13f, true).apply { setPadding(0, dp(12), 0, 0) })
        val langGroup = RadioGroup(this).apply { orientation = RadioGroup.HORIZONTAL }
        val rbFr = RadioButton(this).apply { text = "Français (frFR)" }
        val rbEn = RadioButton(this).apply { text = "English (enUS)" }
        langGroup.addView(rbFr); langGroup.addView(rbEn); (if (cfg.language == "en") rbEn else rbFr).isChecked = true
        box.addView(langGroup)
        box.addView(label(getString(R.string.syl_set_theme), GOLD, 13f, true).apply { setPadding(0, dp(12), 0, 0) })
        val themeSpinner = Spinner(this).apply {
            adapter = ArrayAdapter(this@SylvaniaLauncherActivity, android.R.layout.simple_spinner_dropdown_item, THEMES)
            setSelection(THEMES.indexOf(cfg.background).coerceAtLeast(0))
        }
        box.addView(themeSpinner)
        val randomCb = CheckBox(this).apply { text = getString(R.string.syl_set_random_bg); isChecked = cfg.randomBackgroundEnabled }
        box.addView(randomCb, topMargin(wrap(), 8))

        AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_settings)).setView(scrollOf(box))
            .setPositiveButton(getString(R.string.syl_save)) { _, _ ->
                val newLang = if (rbEn.isChecked) "en" else "fr"
                val langChanged = newLang != configMgr.config.language
                configMgr.update { it.copy(wowPath = pathField.text.toString().trim(), background = THEMES[themeSpinner.selectedItemPosition], randomBackgroundEnabled = randomCb.isChecked) }
                applyBackground(configMgr.config.background)
                Toast.makeText(this, getString(R.string.syl_settings_saved), Toast.LENGTH_SHORT).show()
                // Switching to English also checks/offers the enUS client pack + sets Config.wtf.
                if (langChanged) applyLanguageChange(newLang) else refreshClientState()
            }
            .setNegativeButton(getString(R.string.syl_cancel), null).show()
    }

    // -------------------------------------------------- Change server UI

    private fun onChangeServerClicked() {
        val entries = configMgr.realmlistEntries
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(4)) }
        val group = RadioGroup(this)
        entries.forEachIndexed { i, e -> group.addView(RadioButton(this).apply { text = "${e.name}  (${e.address.removePrefix("set realmlist ").trim()})"; id = i; isChecked = i == configMgr.activeRealmlistIndex }) }
        box.addView(group)
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_change_server)).setView(scrollOf(box))
            .setPositiveButton(getString(R.string.syl_select)) { _, _ ->
                val idx = group.checkedRadioButtonId
                if (idx in entries.indices) { configMgr.setActiveRealmlistIndex(idx); updateServerUi(); Toast.makeText(this, getString(R.string.syl_server_switched, entries[idx].name), Toast.LENGTH_SHORT).show() }
            }
            .setNeutralButton(getString(R.string.syl_add)) { _, _ -> promptAddServer() }
            .setNegativeButton(getString(R.string.syl_close), null).show()
    }

    private fun promptAddServer() {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(4)) }
        val nameField = EditText(this); val addrField = EditText(this)
        box.addView(label(getString(R.string.syl_server_name_label), GOLD, 12f, true)); box.addView(nameField)
        box.addView(label(getString(R.string.syl_server_addr_label), GOLD, 12f, true).apply { setPadding(0, dp(8), 0, 0) }); box.addView(addrField)
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_add_server)).setView(box)
            .setPositiveButton(getString(R.string.syl_add).removeSuffix("…")) { _, _ ->
                val name = nameField.text.toString().trim(); val addr = addrField.text.toString().trim()
                if (name.isNotEmpty() && addr.isNotEmpty()) {
                    val line = if (addr.startsWith("set realmlist")) addr else "set realmlist $addr"
                    configMgr.setRealmlistEntries(configMgr.realmlistEntries + RealmlistEntry(name, line, false))
                    Toast.makeText(this, getString(R.string.syl_server_added), Toast.LENGTH_SHORT).show()
                }
            }.setNegativeButton(getString(R.string.syl_cancel), null).show()
    }

    // ----------------------------------------------------------- Download

    @Volatile private var downloadCancel = false

    private fun onDownloadClicked() {
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_download))
            .setMessage(getString(R.string.syl_download_confirm, Constants.SERVER_HOST, clientDir().absolutePath))
            .setPositiveButton(getString(R.string.syl_download)) { _, _ -> startClientDownload() }
            .setNegativeButton(getString(R.string.syl_cancel), null).show()
    }

    private fun fmt(bytes: Long): String {
        var s = bytes.toDouble(); val u = arrayOf("o", "Ko", "Mo", "Go"); var i = 0
        while (s >= 1024 && i < 3) { s /= 1024; i++ }
        return String.format(Locale.getDefault(), "%.1f %s", s, u[i])
    }

    private fun findClientRoot(root: File): File {
        if (File(root, "Wow.exe").exists()) return root
        root.walkTopDown().maxDepth(3).forEach { if (it.isFile && it.name.equals("Wow.exe", true)) return it.parentFile!! }
        return root
    }

    /** Download [url] → temp zip in [dest], extract into [dest], then run [onSuccess]
     *  on the UI thread. Shows a progress dialog (percent/size/speed) with cancel. */
    private fun downloadAndExtract(url: String, dest: File, title: String, onSuccess: () -> Unit) {
        dest.mkdirs()
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(16), dp(20), dp(8)) }
        val st = TextView(this).apply { text = getString(R.string.syl_download_connecting); setTextColor(Color.parseColor(GOLD)); textSize = 14f }
        val bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply { max = 100 }
        val info = TextView(this).apply { textSize = 11f; setTextColor(Color.LTGRAY); setPadding(0, dp(6), 0, 0) }
        box.addView(st); box.addView(bar, topMargin(wrap(), 8)); box.addView(info)
        downloadCancel = false
        val dlg = AlertDialog.Builder(this).setTitle(title).setView(box).setCancelable(false)
            .setNegativeButton(getString(R.string.syl_cancel)) { _, _ -> downloadCancel = true }.create()
        dlg.show()
        Thread {
            val zip = File(dest, "_sylvania_dl.zip"); var ok = false; var err = ""
            try {
                val conn = (URL(url).openConnection() as HttpURLConnection).apply { instanceFollowRedirects = true; connectTimeout = 30000; readTimeout = 60000 }
                if (conn.responseCode !in 200..299) throw RuntimeException("HTTP ${conn.responseCode}")
                val total = conn.contentLengthLong
                conn.inputStream.use { input -> zip.outputStream().buffered(1 shl 16).use { out ->
                    val buf = ByteArray(1 shl 16); var done = 0L; var lastT = System.currentTimeMillis(); var lastB = 0L
                    while (true) {
                        if (downloadCancel) throw InterruptedException("cancelled")
                        val n = input.read(buf); if (n < 0) break
                        out.write(buf, 0, n); done += n
                        val now = System.currentTimeMillis()
                        if (now - lastT >= 500) {
                            val pct = if (total > 0) (done * 100 / total).toInt() else 0
                            val spd = (done - lastB) * 1000 / (now - lastT).coerceAtLeast(1); lastT = now; lastB = done
                            runOnUiThread {
                                st.text = if (total > 0) getString(R.string.syl_download_progress, pct) else getString(R.string.syl_download_downloading)
                                bar.isIndeterminate = total <= 0; if (total > 0) bar.progress = pct
                                info.text = "${fmt(done)}${if (total > 0) " / " + fmt(total) else ""}  •  ${fmt(spd)}/s"
                            }
                        }
                    }
                } }
                runOnUiThread { st.text = getString(R.string.syl_download_extracting); bar.isIndeterminate = true; info.text = "" }
                ok = ZipExtractor.extract(zip, dest) { }
                zip.delete()
            } catch (e: Exception) { err = e.message ?: "erreur"; if (zip.exists()) zip.delete() }
            runOnUiThread {
                dlg.dismiss()
                if (ok) onSuccess()
                else AlertDialog.Builder(this).setTitle(title)
                    .setMessage(if (downloadCancel) getString(R.string.syl_download_cancelled) else getString(R.string.syl_download_failed, err))
                    .setPositiveButton(getString(R.string.syl_ok), null).show()
            }
        }.start()
    }

    private fun startClientDownload() {
        val dest = clientDir()
        downloadAndExtract(Constants.CLIENT_DOWNLOAD_URL, dest, getString(R.string.syl_dlg_download)) {
            val clientRoot = findClientRoot(dest)
            WowConfigWriter.writeConfigWtf(clientRoot)
            RealmlistWriter.updateRealmlist(clientRoot, activeEntry().address)
            if (clientRoot.absolutePath != configMgr.wowPath) configMgr.setWowPath(clientRoot.absolutePath)
            Thread { ensureConsolePort(clientRoot) }.start() // pre-install the controller addon
            Toast.makeText(this, getString(R.string.syl_download_installed), Toast.LENGTH_LONG).show()
            refreshClientState()
        }
    }

    private fun startEnusDownload(onDone: () -> Unit) {
        downloadAndExtract(Constants.ENUS_DOWNLOAD_URL, clientDir(), getString(R.string.syl_dlg_enus)) {
            Toast.makeText(this, getString(R.string.syl_enus_installed), Toast.LENGTH_LONG).show()
            onDone()
        }
    }

    /** True if the English (enUS) client locale pack is present. */
    private fun isEnusInstalled(client: File): Boolean = listOf(
        "Data/enUS/locale-enUS.MPQ", "Data/enus/locale-enUS.MPQ",
        "Data/enUS/base-enUS.MPQ", "Data/enus/base-enUS.MPQ",
    ).any { File(client, it).exists() }

    // ----------------------------------------------------------- HD patch

    private fun onHdClicked() {
        val client = clientDir()
        if (!File(client, "Wow.exe").exists()) {
            AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_hd)).setMessage(getString(R.string.syl_hd_need_client)).setPositiveButton(getString(R.string.syl_ok), null).show(); return
        }
        val msg = if (HdPatchManager.isInstalled(client)) getString(R.string.syl_hd_reinstall) else getString(R.string.syl_hd_install_q)
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_hd)).setMessage(msg)
            .setPositiveButton(getString(R.string.syl_install)) { _, _ -> startHdPatch(client) }
            .setNegativeButton(getString(R.string.syl_cancel), null).show()
    }

    private fun startHdPatch(client: File) {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(16), dp(20), dp(8)) }
        val st = TextView(this).apply { setTextColor(Color.parseColor(GOLD)); textSize = 14f }
        val bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply { max = 100 }
        box.addView(st); box.addView(bar, topMargin(wrap(), 8))
        val dlg = AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_hd)).setView(box).setCancelable(false).create(); dlg.show()
        Thread {
            val result = runBlocking {
                HdPatchManager(client, File(cacheDir, "SylvaniaLauncher_HD")).install { p -> runOnUiThread { st.text = p.status; bar.isIndeterminate = p.percent < 0; if (p.percent >= 0) bar.progress = p.percent } }
            }
            runOnUiThread {
                dlg.dismiss()
                val text = when (result) { is PatchResult.Success -> result.message; is PatchResult.Failure -> result.message }
                AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_hd)).setMessage(text).setPositiveButton(getString(R.string.syl_ok), null).show()
                if (result is PatchResult.Success) refreshClientState()
            }
        }.start()
    }

    // ------------------------------------------------------------- Addons

    private data class AddonInfo(val name: String, val file: String, val folder: String)
    private val ADDONS = listOf(
        AddonInfo("ArkInventory", "ArkInventory.zip", "ArkInventory"), AddonInfo("AtlasLoot", "AtlasLoot.zip", "AtlasLoot"),
        AddonInfo("Auctionator", "Auctionator.zip", "Auctionator"), AddonInfo("Bagnon", "Bagnon.zip", "Bagnon"),
        AddonInfo("Bartender4", "Bartender4.zip", "Bartender4"), AddonInfo("Deadly Boss Mods (DBM)", "DBM.zip", "DBM-Core"),
        AddonInfo("GatherMate2", "GatherMate2.zip", "GatherMate2"), AddonInfo("GearScore", "GearScore.zip", "GearScore"),
        AddonInfo("HandyNotes", "HandyNotes.zip", "HandyNotes"), AddonInfo("Immersion", "Immersion.zip", "Immersion"),
        AddonInfo("Mapster", "Mapster.zip", "Mapster"), AddonInfo("Postal", "Postal.zip", "Postal"),
        AddonInfo("Prat 3.0", "Prat-3.0.zip", "Prat-3.0"), AddonInfo("Quartz", "Quartz.zip", "Quartz"),
        AddonInfo("QuestHelper", "QuestHelper.zip", "QuestHelper"), AddonInfo("TomTom", "TomTom.zip", "TomTom"),
        AddonInfo("WeakAuras", "WeakAuras.zip", "WeakAuras2"), AddonInfo("XPerl", "XPerl.zip", "XPerl"),
        AddonInfo("Details!", "details.zip", "Details"), AddonInfo("ElvUI", "elvui.zip", "ElvUI"),
        AddonInfo("TotalRP3", "totalRP3.zip", "TotalRP3"), AddonInfo("VuhDo", "vuhdo.zip", "VuhDo"),
    )

    private fun onAddonsClicked() {
        val addonsDir = File(clientDir(), "Interface/AddOns")
        if (!File(clientDir(), "Wow.exe").exists()) {
            AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_addons)).setMessage(getString(R.string.syl_addons_need_client)).setPositiveButton(getString(R.string.syl_ok), null).show(); return
        }
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(16), dp(8), dp(16), dp(8)) }
        list.addView(TextView(this).apply { text = if (File(addonsDir, "ConsolePort").isDirectory) getString(R.string.syl_consoleport_installed) else ""; setTextColor(Color.parseColor("#7ec8e3")); textSize = 12f })
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
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_dlg_addons)).setView(scrollOf(list)).setPositiveButton(getString(R.string.syl_close), null).show()
    }

    private fun configAddonButton(btn: Button, a: AddonInfo, addonsDir: File, statusTv: TextView) {
        if (File(addonsDir, a.folder).isDirectory) {
            styleButton(btn, getString(R.string.syl_delete), "#8a4a5a", "#6a2a3a", "#9a5a6a", "#ffffff", 11f)
            btn.setOnClickListener { confirmDeleteAddon(a, addonsDir, btn, statusTv) }
        } else {
            styleButton(btn, getString(R.string.syl_install), "#4a7c3f", "#2a5a1f", "#5a8c4f", "#ffffff", 11f)
            btn.setOnClickListener { installAddon(a, addonsDir, btn, statusTv) }
        }
    }

    private fun confirmDeleteAddon(a: AddonInfo, addonsDir: File, btn: Button, statusTv: TextView) {
        AlertDialog.Builder(this).setTitle(getString(R.string.syl_addon_delete_title, a.name)).setMessage(getString(R.string.syl_addon_delete_msg))
            .setPositiveButton(getString(R.string.syl_delete)) { _, _ ->
                val ok = File(addonsDir, a.folder).deleteRecursively()
                statusTv.text = if (ok) getString(R.string.syl_addon_deleted) else getString(R.string.syl_addon_failed)
                configAddonButton(btn, a, addonsDir, statusTv)
            }.setNegativeButton(getString(R.string.syl_cancel), null).show()
    }

    private fun installAddon(a: AddonInfo, addonsDir: File, btn: Button, statusTv: TextView) {
        btn.isEnabled = false; btn.text = getString(R.string.syl_addon_wait); statusTv.text = getString(R.string.syl_addon_installing); addonsDir.mkdirs()
        Thread {
            val zip = File(addonsDir, a.file); var ok = false
            try {
                val conn = (URL(Constants.ADDON_DOWNLOAD_ENDPOINT + "?file=" + a.file).openConnection() as HttpURLConnection).apply { instanceFollowRedirects = true; connectTimeout = 30000; readTimeout = 60000 }
                if (conn.responseCode in 200..299) {
                    conn.inputStream.use { i -> zip.outputStream().buffered().use { o -> i.copyTo(o) } }
                    runOnUiThread { statusTv.text = getString(R.string.syl_addon_extracting) }
                    ok = ZipExtractor.extract(zip, addonsDir) { }
                }
                zip.delete()
            } catch (e: Exception) { Log.w(TAG, "addon ${a.name}", e); if (zip.exists()) zip.delete() }
            runOnUiThread { btn.isEnabled = true; configAddonButton(btn, a, addonsDir, statusTv); statusTv.text = if (ok) getString(R.string.syl_addon_installed) else getString(R.string.syl_addon_failed) }
        }.start()
    }

    // ------------------------------------------------------------- Stats

    private fun statsFile() = File(filesDir, "stats.json")
    private fun loadStats() {
        try {
            val f = statsFile(); if (!f.exists()) return
            val o = JSONObject(f.readText())
            launchCount = o.optInt("launch_count", 0); totalPlayTime = o.optLong("total_play_time", 0L)
            lastSession = o.optLong("last_session", 0L); sessionStart = o.optLong("session_start", 0L)
        } catch (e: Exception) { Log.w(TAG, "loadStats", e) }
    }
    private fun saveStats() {
        try { statsFile().writeText(JSONObject().apply { put("launch_count", launchCount); put("total_play_time", totalPlayTime); put("last_session", lastSession); put("session_start", sessionStart) }.toString()) }
        catch (e: Exception) { Log.w(TAG, "saveStats", e) }
    }
    private fun finalizePendingSession() {
        if (sessionStart <= 0L) return
        totalPlayTime += ((System.currentTimeMillis() - sessionStart) / 1000).coerceIn(0L, 24 * 3600L); sessionStart = 0L; saveStats()
    }
    private fun updateStatsUi() {
        val h = totalPlayTime / 3600; val m = (totalPlayTime % 3600) / 60
        playTimeLabel.text = getString(R.string.syl_play_time, h.toInt(), m.toInt())
        launchCountLabel.text = getString(R.string.syl_launches, launchCount)
        lastSessionLabel.text = if (lastSession > 0) getString(R.string.syl_last_session, SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.getDefault()).format(Date(lastSession))) else getString(R.string.syl_last_session_none)
    }

    private fun refreshClientState() {
        val client = clientDir()
        val hasClient = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", true) }?.isNotEmpty() == true
        playButton.isEnabled = hasClient; playButton.alpha = if (hasClient) 1f else 0.5f
        if (!hasClient) setStatus(getString(R.string.syl_status_no_client, client.absolutePath)) else setStatus(getString(R.string.syl_status_ready, activeEntry().name))
    }

    // ----------------------------------------------------------- Launch

    private fun onPlayClicked() {
        playButton.isEnabled = false; setStatus(getString(R.string.syl_status_preparing))
        Thread {
            try {
                val imageFs = ImageFs.find(this)
                if (!isImageFsInstalled(imageFs)) installImageFs(imageFs) else log(getString(R.string.syl_status_runtime_installed))
                ensureControlsProfile()
                ensureConsolePort(clientDir())
                val contents = ContentsManager(this).apply { syncContents() }
                val manager = ContainerManager(this)
                runOnUiThread { ensureContainerAndLaunch(manager, contents) }
            } catch (e: Exception) { Log.e(TAG, "prepare failed", e); setStatus(getString(R.string.syl_status_error, e.message ?: "")); runOnUiThread { playButton.isEnabled = true } }
        }.start()
    }

    private fun ensureControlsProfile() {
        val dest = File(filesDir, "profiles/controls-$CONTROLS_PROFILE_ID.icp"); if (dest.exists()) return
        try { dest.parentFile?.mkdirs(); assets.open("profiles/controls-$CONTROLS_PROFILE_ID.icp").use { input -> dest.outputStream().use { input.copyTo(it) } } }
        catch (e: Exception) { Log.w(TAG, "controls profile install failed", e) }
    }

    /**
     * Install the bundled ConsolePortLK gamepad addon into the client's
     * Interface/AddOns if absent (a freshly downloaded client doesn't include it).
     * Idempotent: skips if the ConsolePort folder is already present.
     */
    private fun ensureConsolePort(client: File) {
        if (!File(client, "Wow.exe").exists()) return
        val addonsDir = File(client, "Interface/AddOns")
        if (File(addonsDir, "ConsolePort").isDirectory) return
        try {
            addonsDir.mkdirs()
            assets.open("consoleport.zip").use { ZipExtractor.extract(it, addonsDir) { } }
            log("ConsolePort installé.")
        } catch (e: Exception) { Log.w(TAG, "ConsolePort install failed", e) }
    }

    private fun isImageFsInstalled(imageFs: ImageFs): Boolean = imageFs.isValid && imageFs.version >= ImageFsInstaller.LATEST_VERSION && File(imageFs.rootDir, "opt/$ARM64EC/bin/wine").exists()

    private fun installImageFs(imageFs: ImageFs) {
        val rootDir = imageFs.rootDir
        log("Extraction du rootfs (imagefs.txz)…")
        if (!TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "imagefs.txz", rootDir)) throw RuntimeException("échec extraction imagefs.txz")
        for (v in WINE_VERSIONS) { val out = File(rootDir, "opt/$v").apply { mkdirs() }; log("Extraction de $v…"); TarCompressorUtils.extract(TarCompressorUtils.Type.XZ, this, "$v.txz", out) }
        imageFs.createImgVersionFile(ImageFsInstaller.LATEST_VERSION.toInt())
    }

    private fun defaultClientDir(): File = File(Environment.getExternalStorageDirectory(), "SylvaniaLauncher/wotlk")
    private fun clientDir(): File = configMgr.wowPath.ifBlank { null }?.let { File(it) } ?: defaultClientDir()

    private fun ensureContainerAndLaunch(manager: ContainerManager, contents: ContentsManager) {
        val existing = manager.containers
        if (existing.isNotEmpty()) { launchContainer(existing[0]); return }
        setStatus(getString(R.string.syl_status_container_creating))
        val data = JSONObject().apply {
            put("name", "Sylvania"); put("wineVersion", ARM64EC); put("wow64Mode", true); put("screenSize", "1280x720")
            put("emulator", "wowbox64"); put("box64Version", "0.3.7"); put("fexcoreVersion", "2507")
            put("dxwrapper", Container.DEFAULT_DXWRAPPER); put("dxwrapperConfig", Container.DEFAULT_DXWRAPPERCONFIG)
            put("graphicsDriver", Container.DEFAULT_GRAPHICS_DRIVER); put("graphicsDriverConfig", Container.DEFAULT_GRAPHICSDRIVERCONFIG); put("ddrawrapper", Container.DEFAULT_DDRAWRAPPER)
        }
        manager.createContainerAsync(data, contents) { container ->
            if (container != null) launchContainer(container) else { setStatus(getString(R.string.syl_status_container_failed)); playButton.isEnabled = true }
        }
    }

    private fun launchContainer(container: Container) {
        val cfg = container.dxWrapperConfig
        if (cfg == null || !cfg.contains("version=")) container.dxWrapperConfig = Container.DEFAULT_DXWRAPPERCONFIG
        container.screenSize = "1920x1080"
        container.graphicsDriver = "wrapper"
        container.graphicsDriverConfig = "version=turnip25.1.0;blacklistedExtensions=;maxDeviceMemory=0;adrenotoolsTurnip=1;frameSync=Normal"

        val client = clientDir()
        val wowExe = client.listFiles { f -> f.isFile && f.name.equals("Wow.exe", ignoreCase = true) }?.firstOrNull()
        if (wowExe == null) { setStatus(getString(R.string.syl_status_no_client, client.absolutePath)); playButton.isEnabled = true; return }

        RealmlistWriter.updateRealmlist(client, activeEntry().address)
        container.drives = "D:" + client.absolutePath
        container.saveData()

        val desktopDir = container.desktopDir.apply { mkdirs() }
        val shortcut = File(desktopDir, "Sylvania.desktop")
        shortcut.writeText(buildString {
            appendLine("[Desktop Entry]"); appendLine("Type=Application"); appendLine("Name=Sylvania WoW"); appendLine("Exec=wine D:/${wowExe.name}")
            appendLine(""); appendLine("[Extra Data]"); appendLine("controlsProfile=$CONTROLS_PROFILE_ID")
        })

        launchCount++; lastSession = System.currentTimeMillis(); sessionStart = lastSession; saveStats(); updateStatsUi()
        setStatus(getString(R.string.syl_status_launching))
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
