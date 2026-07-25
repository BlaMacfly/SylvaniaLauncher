#include "WineManager.h"

#ifdef Q_OS_LINUX

#include "PathUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

bool WineManager::s_debugMode = false;

namespace {

// Variables an AppImage's AppRun injects to point at the launcher's OWN
// bundled Qt/runtime. They MUST be stripped before running Wine: otherwise
// Wine inherits them and loads the launcher's libraries instead of its own,
// which breaks the prefix bootstrap ("could not load kernel32.dll").
const char* const kAppImageInjectedVars[] = {
    "LD_LIBRARY_PATH", "LD_PRELOAD",
    "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QTWEBENGINEPROCESS_PATH",
    "GST_PLUGIN_SYSTEM_PATH", "GST_PLUGIN_SYSTEM_PATH_1_0",
    "GTK_PATH", "GTK_EXE_PREFIX", "GTK_DATA_PREFIX",
    "PERLLIB", "GCONV_PATH", "GIO_MODULE_DIR", "PYTHONPATH",
    "FONTCONFIG_PATH", "FONTCONFIG_FILE",
};

// Locate the EMBEDDED Wine runtime. Priority:
//  1. SYLVANIA_WINE_DIR (developer override)
//  2. $APPDIR/usr/wine   (inside the mounted AppImage)
//  3. <exe dir>/../wine  (usr/bin -> usr/wine, works for extracted AppDir)
QString locateWineRoot() {
    const auto looksLikeWine = [](const QString& root) {
        return QFileInfo(root + "/bin/wine").isExecutable()
            && QFileInfo(root + "/bin/wineserver").isExecutable();
    };

    const QString override_ = qEnvironmentVariable("SYLVANIA_WINE_DIR");
    if (!override_.isEmpty() && looksLikeWine(override_))
        return QDir(override_).absolutePath();

    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty() && looksLikeWine(appDir + "/usr/wine"))
        return appDir + "/usr/wine";

    const QString relative = QCoreApplication::applicationDirPath() + "/../wine";
    if (looksLikeWine(relative))
        return QDir(relative).absolutePath();

    return QString();
}

// Locate the Wine the DISTRIBUTION provides, for builds that ship no runtime of
// their own (the Arch/pacman package depends on the distro's wine instead of
// embedding 500 MB). Resolved through PATH so it follows the user's choice
// between wine and wine-staging.
QString locateSystemWineRoot() {
    const QString loader = QStandardPaths::findExecutable(QStringLiteral("wine"));
    if (loader.isEmpty()) return QString();

    // .../bin/wine -> the root Wine was installed under (usually /usr).
    const QString root = QFileInfo(loader).dir().absoluteFilePath(QStringLiteral(".."));
    if (!QFileInfo(root + "/bin/wineserver").isExecutable()) return QString();

    return QDir(root).canonicalPath();
}

// Locate the embedded DXVK tree, next to the Wine runtime — or the one the
// distribution installed (Arch's dxvk packages use the same x32/x64 layout).
QString locateDxvkRoot() {
    const auto looksLikeDxvk = [](const QString& root) {
        return QFile::exists(root + "/x32/d3d9.dll");
    };

    const QString override_ = qEnvironmentVariable("SYLVANIA_DXVK_DIR");
    if (!override_.isEmpty() && looksLikeDxvk(override_))
        return QDir(override_).absolutePath();

    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty() && looksLikeDxvk(appDir + "/usr/dxvk"))
        return appDir + "/usr/dxvk";

    const QString relative = QCoreApplication::applicationDirPath() + "/../dxvk";
    if (looksLikeDxvk(relative))
        return QDir(relative).absolutePath();

    for (const QString& system : {QStringLiteral("/usr/share/dxvk"),
                                 QStringLiteral("/usr/lib/dxvk")}) {
        if (looksLikeDxvk(system)) return system;
    }

    return QString();
}

// Overwrite-safe copy (QFile::copy refuses an existing destination).
bool replaceFile(const QString& from, const QString& to) {
    QFile::remove(to);
    return QFile::copy(from, to);
}

QString readFirstLine(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(f.readLine()).trimmed();
}

// DLLs without which neither Wine's own bootstrap nor the WoW client can
// start. Their presence is what separates a usable prefix from one that
// merely *looks* initialised: a file that cannot be read out of the AppImage
// is skipped SILENTLY by wineboot's fake-dll installer, so the registry and
// drive_c come out perfectly normal while system32 is missing a DLL that
// every later import fails on.
const char* const kRequiredSystemDlls[] = {
    "advapi32.dll", "gdi32.dll", "kernel32.dll", "ntdll.dll",
    "ole32.dll", "oleaut32.dll", "opengl32.dll", "shell32.dll",
    "shlwapi.dll", "user32.dll", "ws2_32.dll",
};

// Read a file end to end. Returns false on a short read or an I/O error —
// which is exactly how a corrupted AppImage (bad copy, failing drive) shows
// up: stat() reports the real size, read() dies partway through.
bool isFullyReadable(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const qint64 expected = f.size();
    qint64 total = 0;
    char buffer[64 * 1024];
    forever {
        const qint64 got = f.read(buffer, sizeof(buffer));
        if (got < 0) return false;   // I/O error
        if (got == 0) break;         // EOF
        total += got;
    }
    return total == expected;
}

} // namespace

void WineManager::setDebugMode(bool enabled) { s_debugMode = enabled; }
bool WineManager::debugMode() { return s_debugMode; }

WineManager::WineManager(QObject* parent)
    : QObject(parent)
{
    m_wineRoot = locateWineRoot();
    m_wineIsBundled = !m_wineRoot.isEmpty();
    if (!m_wineIsBundled) {
        // Distro package (no embedded runtime): use the system's Wine.
        m_wineRoot = locateSystemWineRoot();
    }
    m_dxvkRoot = locateDxvkRoot();
    m_prefixDir = PathUtils::getWinePrefixDir();
}

WineManager::~WineManager()
{
    // Never leave a zombie init process behind the UI.
    if (m_initProcess) {
        m_initProcess->kill();
        m_initProcess->waitForFinished(2000);
    }
}

bool WineManager::isWineBundled() const
{
    return m_wineIsBundled;
}

bool WineManager::isWineAvailable() const
{
    return !m_wineRoot.isEmpty();
}

QString WineManager::missingWineMessage() const
{
    // The AppImage ships its own Wine, so its absence means a damaged download.
    // A distro package deliberately ships none: there the fix is to install it.
    if (QFile::exists(QCoreApplication::applicationDirPath() + "/../wine")
        || qEnvironmentVariableIsSet("APPDIR")) {
        return tr("Le moteur Wine embarqué est introuvable. L'AppImage est "
                  "peut-être corrompue — retéléchargez le launcher.");
    }
    return tr("Wine est introuvable. Installez-le avec le gestionnaire de "
              "paquets de votre distribution (paquet « wine » ou "
              "« wine-staging »), puis relancez le launcher.");
}

QString WineManager::wineBinary() const
{
    return m_wineRoot.isEmpty() ? QString() : m_wineRoot + "/bin/wine";
}

QString WineManager::wineServerBinary() const
{
    return m_wineRoot.isEmpty() ? QString() : m_wineRoot + "/bin/wineserver";
}

QString WineManager::wineRoot() const { return m_wineRoot; }
QString WineManager::winePrefix() const { return m_prefixDir; }

bool WineManager::hasVulkanDriver()
{
    // The loader alone is not enough: a machine can ship libvulkan without any
    // usable driver, and DXVK would then fail at startup. Require both.
    QLibrary loader(QStringLiteral("vulkan"), 1);
    if (!loader.load()) return false;
    loader.unload();

    const QStringList icdDirs = {
        QStringLiteral("/usr/share/vulkan/icd.d"),
        QStringLiteral("/usr/local/share/vulkan/icd.d"),
        QStringLiteral("/etc/vulkan/icd.d"),
        QDir::homePath() + QStringLiteral("/.local/share/vulkan/icd.d"),
    };
    for (const QString& dir : icdDirs) {
        if (!QDir(dir).entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty())
            return true;
    }
    return false;
}

bool WineManager::isDxvkUsable() const
{
    if (m_dxvkRoot.isEmpty()) return false;
    // Escape hatch: lets a user fall back to WineD3D without a rebuild if a
    // driver turns out to be broken.
    if (qEnvironmentVariableIsSet("SYLVANIA_DISABLE_DXVK")) return false;
    return hasVulkanDriver();
}

bool WineManager::ensureDxvkInstalled()
{
    if (!isDxvkUsable()) return false;

    // 32-bit PE DLLs live in syswow64 on a WoW64 prefix (what the bundled
    // wow64 build creates), in system32 on a pure 32-bit one.
    const QString sys32 = m_prefixDir + "/drive_c/windows/system32";
    const QString syswow64 = m_prefixDir + "/drive_c/windows/syswow64";
    const QString target32 = QDir(syswow64).exists() ? syswow64 : sys32;
    if (!QDir(target32).exists()) return false;

    const QString bundledVersion = readFirstLine(m_dxvkRoot + "/VERSION");
    const QString markerPath = m_prefixDir + "/.sylvania-dxvk";

    // Already installed at the bundled version: nothing to do (cheap path,
    // runs on every launch).
    if (QFile::exists(target32 + "/d3d9.dll")
        && readFirstLine(markerPath) == bundledVersion
        && !bundledVersion.isEmpty()) {
        return true;
    }

    if (!replaceFile(m_dxvkRoot + "/x32/d3d9.dll", target32 + "/d3d9.dll")) {
        logLine("DXVK: failed to install x32/d3d9.dll into " + target32);
        return false;
    }
    // 64-bit counterpart, for completeness (the 3.3.5a client is 32-bit).
    if (target32 != sys32 && QDir(sys32).exists())
        replaceFile(m_dxvkRoot + "/x64/d3d9.dll", sys32 + "/d3d9.dll");

    QFile marker(markerPath);
    if (marker.open(QIODevice::WriteOnly | QIODevice::Text))
        marker.write(bundledVersion.toUtf8() + "\n");

    logLine("DXVK " + bundledVersion + " installed into the prefix");
    return true;
}

QString WineManager::wineLogPath() const
{
    return PathUtils::getLogsDir() + "/wine.log";
}

QStringList WineManager::missingSystemDlls() const
{
    const QString windows = m_prefixDir + "/drive_c/windows";
    const QString syswow64 = windows + "/syswow64";

    QStringList dirs{QStringLiteral("system32")};
    // On the WoW64 prefix the bundled build creates, the 32-bit client resolves
    // its imports from syswow64 — an incomplete one breaks the game even when
    // system32 is pristine. Only checked when the directory exists (a pure
    // 32-bit prefix has none).
    if (QDir(syswow64).exists())
        dirs << QStringLiteral("syswow64");

    QStringList missing;
    for (const QString& dir : std::as_const(dirs)) {
        for (const char* dll : kRequiredSystemDlls) {
            const QString relative = dir + '/' + QLatin1String(dll);
            if (!QFile::exists(windows + '/' + relative))
                missing << relative;
        }
    }
    return missing;
}

bool WineManager::isPrefixValid() const
{
    // A healthy prefix has its registry hives and a Windows tree. A crashed
    // wineboot typically leaves drive_c missing or system.reg absent/empty.
    const QFileInfo systemReg(m_prefixDir + "/system.reg");
    const QFileInfo userReg(m_prefixDir + "/user.reg");
    const QFileInfo windowsDir(m_prefixDir + "/drive_c/windows");
    if (!systemReg.exists() || systemReg.size() <= 0
        || !userReg.exists()
        || !windowsDir.isDir()) {
        return false;
    }

    // The registry alone is NOT proof of a working prefix: a wineboot that
    // failed to install some system DLL still writes complete-looking hives.
    // Checking the DLLs is what makes such a prefix repairable instead of
    // permanently broken (initializePrefix() wipes an invalid prefix, and
    // MainWindow refuses to launch the game on one).
    return missingSystemDlls().isEmpty();
}

QProcessEnvironment WineManager::wineEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char* var : kAppImageInjectedVars)
        env.remove(QString::fromLatin1(var));

    // Everything points EXPLICITLY at the embedded runtime: never rely on
    // PATH lookup or on the wineboot/wineserver wrapper scripts (they fall
    // back to `wine` from PATH, which does not exist on a clean machine —
    // that was the root cause of the old "prefix never initialises" bug).
    env.insert("WINEPREFIX", m_prefixDir);
    env.insert("WINELOADER", wineBinary());
    env.insert("WINESERVER", wineServerBinary());
    env.insert("PATH", m_wineRoot + "/bin:" + env.value("PATH"));

    // ONLY for the embedded runtime. A distro Wine knows where its own dll and
    // library directories are (compiled in, and they are not laid out the same
    // on every distro — /usr/lib/wine here, /usr/lib64/wine there): pointing it
    // at guessed paths would break what already works.
    if (m_wineIsBundled) {
        env.insert("WINEDLLPATH", m_wineRoot + "/lib/wine");
        env.insert("LD_LIBRARY_PATH", m_wineRoot + "/lib");
    }

    // WoW needs neither .NET (mono) nor an embedded browser (gecko): skip
    // their installers entirely -> first init is much faster and fully
    // offline, and the AppImage stays smaller.
    env.insert("WINEDLLOVERRIDES", "mscoree=;mshtml=");

    if (s_debugMode) {
        // Default Wine verbosity, captured to the log file by the caller.
        env.remove("WINEDEBUG");
    } else {
        // NOT "-all": that also silences the err: channel, so wine.log came out
        // EMPTY exactly when an initialisation failure had to be explained —
        // while the error naming the culprit ("Library shlwapi.dll not found")
        // is an err: line. "fixme-all" drops only the chatty fixme noise.
        env.insert("WINEDEBUG", "fixme-all");
    }
    return env;
}

void WineManager::logLine(const QString& line) const
{
    QFile f(PathUtils::getLogsDir() + "/wine-setup.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&f) << QDateTime::currentDateTime().toString(Qt::ISODate)
                        << "  " << line << "\n";
    }
    qInfo().noquote() << "[WineManager]" << line;
}

void WineManager::finishInit(bool success, const QString& message)
{
    if (m_initProcess) {
        m_initProcess->deleteLater();
        m_initProcess = nullptr;
    }
    logLine(QString("init %1: %2").arg(success ? "OK" : "FAILED", message));
    emit initFinished(success, message);
}

void WineManager::initializePrefix()
{
    if (m_initProcess) return;  // already running

    if (!isWineAvailable()) {
        emit initFinished(false, missingWineMessage());
        return;
    }

    // A half-initialised prefix (previous crash/timeout) makes wineboot fail
    // in odd ways: wipe it and start clean. A VALID prefix is never wiped.
    if (QDir(m_prefixDir).exists() && !isPrefixValid()) {
        logLine("removing corrupt/incomplete prefix at " + m_prefixDir);
        QDir(m_prefixDir).removeRecursively();
    }
    QDir().mkpath(m_prefixDir);

    logLine("wineboot --init  (wine root: " + m_wineRoot + ")");
    emit initProgress(5, tr("Initialisation du moteur de jeu (Wine)…\n"
                            "Cette étape ne se fait qu'au premier lancement "
                            "et peut prendre une minute."));

    m_initProcess = new QProcess(this);
    m_initProcess->setProcessEnvironment(wineEnvironment());
    m_initProcess->setStandardOutputFile(wineLogPath(), QIODevice::Append);
    m_initProcess->setStandardErrorFile(wineLogPath(), QIODevice::Append);

    // Explicit timeout: wineboot that hangs must not hang the launcher.
    auto* timeout = new QTimer(m_initProcess);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this]() {
        if (!m_initProcess) return;
        logLine("wineboot TIMEOUT — killing");
        m_initProcess->kill();
        // finished() handler below reports the failure.
    });
    timeout->start(kInitTimeoutMs);

    connect(m_initProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!m_initProcess) return;  // already reported (FailedToStart)
        if (status != QProcess::NormalExit || exitCode != 0) {
            logLine(QString("wineboot exited with code %1").arg(exitCode));
            // wineboot's exit code says nothing usable on its own (a missing
            // system DLL surfaces as an opaque 53). Name the actual cause when
            // the prefix tells us what it is.
            if (!missingSystemDlls().isEmpty()) {
                finishInit(false, diagnoseIncompletePrefix());
                return;
            }
            finishInit(false,
                tr("L'initialisation de Wine a échoué (wineboot, code %1). "
                   "Consultez le journal : %2")
                    .arg(exitCode).arg(wineLogPath()));
            return;
        }
        // wineboot returned, but background services may still be writing
        // the registry: wait for the wineserver to fully terminate.
        startWineserverWait();
    });

    // Only FailedToStart is handled here: any other error also fires
    // finished(), which is the single reporting path (no double emit).
    connect(m_initProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (!m_initProcess || error != QProcess::FailedToStart) return;
        const QString err = m_initProcess->errorString();
        finishInit(false,
            tr("Impossible de lancer wineboot : %1").arg(err));
    });

    // Call the LOADER directly with wineboot as argument — never the
    // bin/wineboot wrapper script.
    m_initProcess->start(wineBinary(), {"wineboot", "--init"});
}

QString WineManager::diagnoseIncompletePrefix() const
{
    const QStringList missing = missingSystemDlls();
    if (missing.isEmpty()) {
        // Registry hives or drive_c are what went wrong, not the DLLs.
        return tr("Le préfixe Wine est resté incomplet après l'initialisation. "
                  "Consultez le journal : %1").arg(wineLogPath());
    }

    logLine("prefix incomplete, missing: " + missing.join(", "));

    // A DLL missing from the prefix that is ALSO unreadable in the bundle means
    // the AppImage itself is damaged (bad copy, failing drive): wineboot skips
    // such a file without a word, which is impossible to guess from the outside.
    QStringList damaged;
    for (const QString& relative : missing) {
        const QString dll = relative.section('/', -1);
        const QString arch = relative.startsWith(QLatin1String("syswow64"))
            ? QStringLiteral("i386-windows") : QStringLiteral("x86_64-windows");
        const QString source = m_wineRoot + "/lib/wine/" + arch + '/' + dll;
        if (!isFullyReadable(source))
            damaged << source;
    }

    if (!damaged.isEmpty()) {
        logLine("UNREADABLE in the Wine bundle: " + damaged.join(", "));
        return tr("Le moteur Wine embarqué est corrompu : le fichier %1 est "
                  "illisible.\n\nL'AppImage a été endommagée (copie incomplète "
                  "ou disque défaillant) — recopiez-la ou retéléchargez le "
                  "launcher, et évitez de la lancer depuis un disque externe.")
            .arg(damaged.first());
    }

    return tr("Le préfixe Wine est resté incomplet après l'initialisation : "
              "%1 manque.\nConsultez le journal : %2")
        .arg(missing.first(), wineLogPath());
}

void WineManager::startWineserverWait()
{
    if (m_initProcess) {
        m_initProcess->deleteLater();
        m_initProcess = nullptr;
    }

    logLine("wineserver -w (waiting for full shutdown)");
    emit initProgress(70, tr("Finalisation de l'initialisation de Wine…"));

    m_initProcess = new QProcess(this);
    m_initProcess->setProcessEnvironment(wineEnvironment());
    m_initProcess->setStandardOutputFile(wineLogPath(), QIODevice::Append);
    m_initProcess->setStandardErrorFile(wineLogPath(), QIODevice::Append);

    auto* timeout = new QTimer(m_initProcess);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, this, [this]() {
        if (!m_initProcess) return;
        logLine("wineserver -w TIMEOUT — killing");
        m_initProcess->kill();
    });
    timeout->start(kServerWaitTimeoutMs);

    connect(m_initProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (!m_initProcess) return;  // already reported (FailedToStart)
        // Whatever wineserver's exit status, the truth is on disk: validate.
        if (isPrefixValid()) {
            // Install the Direct3D 9 -> Vulkan layer while we are still in the
            // "setting things up" phase (no-op without a Vulkan driver).
            emit initProgress(90, tr("Installation de la couche graphique (DXVK)…"));
            ensureDxvkInstalled();
            emit initProgress(100, tr("Moteur de jeu prêt."));
            finishInit(true, tr("Préfixe Wine initialisé."));
        } else {
            finishInit(false, diagnoseIncompletePrefix());
        }
    });

    connect(m_initProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (!m_initProcess || error != QProcess::FailedToStart) return;
        finishInit(false,
            tr("Impossible d'attendre la fin de wineserver : %1")
                .arg(m_initProcess->errorString()));
    });

    m_initProcess->start(wineServerBinary(), {"-w"});
}

bool WineManager::launchExe(const QString& exePath, const QString& workDir)
{
    if (!isWineAvailable()) {
        qWarning() << "WineManager: no Wine found (neither embedded nor system)";
        return false;
    }

    QProcessEnvironment env = wineEnvironment();
    env.insert("WINE_LARGE_ADDRESS_AWARE", "1");

    // --- Synchronisation primitives (wine-staging) -----------------------
    // Without these, Wine falls back to server-side synchronisation: every
    // wait round-trips through wineserver, which costs a large amount of CPU
    // in a game doing thousands of them per frame. Both degrade gracefully
    // (Wine probes kernel support and reverts to the server path if absent).
    if (!env.contains("WINEESYNC")) env.insert("WINEESYNC", "1");
    if (!env.contains("WINEFSYNC")) env.insert("WINEFSYNC", "1");
    env.insert("STAGING_SHARED_MEMORY", "1");

    // --- Direct3D 9 back end ---------------------------------------------
    // DXVK translates D3D9 to Vulkan instead of WineD3D's D3D9 -> OpenGL.
    // For this client it is the single largest frame-rate factor, especially
    // under the wow64 build where each API call crosses a 32->64 bit thunk.
    // The DLL installed in the prefix is used unless the game directory ships
    // its own (the HD patch does), which takes precedence via the Windows
    // search order — either way the native override below is what activates it.
    const bool dxvkInPrefix = ensureDxvkInstalled();
    const bool dxvkInGameDir = QFile::exists(workDir + "/d3d9.dll");
    if (dxvkInPrefix || dxvkInGameDir) {
        const QString overrides = env.value("WINEDLLOVERRIDES");
        env.insert("WINEDLLOVERRIDES",
                   overrides.isEmpty() ? "d3d9=n,b" : overrides + ";d3d9=n,b");
        logLine(QString("D3D9 back end: DXVK (prefix=%1, gameDir=%2)")
                    .arg(dxvkInPrefix).arg(dxvkInGameDir));
    } else {
        logLine("D3D9 back end: WineD3D (OpenGL) — "
                + QString(m_dxvkRoot.isEmpty() ? "DXVK not bundled"
                                               : "no Vulkan driver detected"));
    }

    // --- Driver-side threading -------------------------------------------
    // Old single-threaded D3D9 titles benefit hugely from the driver's own
    // command-submission thread. Both variables are ignored by drivers that
    // do not implement them.
    if (!env.contains("mesa_glthread")) env.insert("mesa_glthread", "true");
    if (!env.contains("__GL_THREADED_OPTIMIZATIONS"))
        env.insert("__GL_THREADED_OPTIMIZATIONS", "1");

    // Help older Mesa drivers unless the user already overrode it.
    if (!env.contains("MESA_GL_VERSION_OVERRIDE"))
        env.insert("MESA_GL_VERSION_OVERRIDE", "4.5");

    // Qt >= 5.10: instance startDetached() carries setProcessEnvironment(),
    // working directory and I/O redirections — no /tmp wrapper script (the
    // old fixed-path script was a symlink/TOCTOU hazard on shared machines).
    auto* proc = new QProcess;  // no parent: detached
    proc->setProgram(wineBinary());
    proc->setArguments({exePath});
    proc->setWorkingDirectory(workDir);
    proc->setProcessEnvironment(env);
    if (s_debugMode) {
        proc->setStandardOutputFile(wineLogPath(), QIODevice::Append);
        proc->setStandardErrorFile(wineLogPath(), QIODevice::Append);
    }

    qint64 pid = 0;
    const bool started = proc->startDetached(&pid);
    delete proc;

    if (started) {
        logLine(QString("launched %1 (pid %2)").arg(exePath).arg(pid));
    } else {
        logLine("FAILED to launch " + exePath);
    }
    return started;
}

bool WineManager::isProcessRunning(const QString& exeName) const
{
    // Scan /proc for wine processes running the specified executable
    QDir procDir("/proc");
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        bool ok = false;
        entry.toInt(&ok);
        if (!ok) continue; // Skip non-PID directories

        QFile cmdlineFile("/proc/" + entry + "/cmdline");
        if (cmdlineFile.open(QIODevice::ReadOnly)) {
            const QByteArray cmdline = cmdlineFile.readAll();
            const QString cmdStr = QString::fromLocal8Bit(cmdline).replace(QChar('\0'), QChar(' '));
            if (cmdStr.contains(exeName, Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
    return false;
}

#endif // Q_OS_LINUX
