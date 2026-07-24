#include "WineManager.h"

#ifdef Q_OS_LINUX

#include "PathUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
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

// Locate the embedded Wine runtime. Priority:
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

} // namespace

void WineManager::setDebugMode(bool enabled) { s_debugMode = enabled; }
bool WineManager::debugMode() { return s_debugMode; }

WineManager::WineManager(QObject* parent)
    : QObject(parent)
{
    m_wineRoot = locateWineRoot();
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
    return !m_wineRoot.isEmpty();
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

QString WineManager::wineLogPath() const
{
    return PathUtils::getLogsDir() + "/wine.log";
}

bool WineManager::isPrefixValid() const
{
    // A healthy prefix has its registry hives and a Windows tree. A crashed
    // wineboot typically leaves drive_c missing or system.reg absent/empty.
    const QFileInfo systemReg(m_prefixDir + "/system.reg");
    const QFileInfo userReg(m_prefixDir + "/user.reg");
    const QFileInfo windowsDir(m_prefixDir + "/drive_c/windows");
    return systemReg.exists() && systemReg.size() > 0
        && userReg.exists()
        && windowsDir.isDir();
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
    env.insert("WINEDLLPATH", m_wineRoot + "/lib/wine");
    env.insert("LD_LIBRARY_PATH", m_wineRoot + "/lib");
    env.insert("PATH", m_wineRoot + "/bin:" + env.value("PATH"));

    // WoW needs neither .NET (mono) nor an embedded browser (gecko): skip
    // their installers entirely -> first init is much faster and fully
    // offline, and the AppImage stays smaller.
    env.insert("WINEDLLOVERRIDES", "mscoree=;mshtml=");

    if (s_debugMode) {
        // Default Wine verbosity, captured to the log file by the caller.
        env.remove("WINEDEBUG");
    } else {
        env.insert("WINEDEBUG", "-all");
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

    if (!isWineBundled()) {
        emit initFinished(false,
            tr("Le moteur Wine embarqué est introuvable. L'AppImage est "
               "peut-être corrompue — retéléchargez le launcher."));
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
            emit initProgress(100, tr("Moteur de jeu prêt."));
            finishInit(true, tr("Préfixe Wine initialisé."));
        } else {
            finishInit(false,
                tr("Le préfixe Wine est resté incomplet après "
                   "l'initialisation. Consultez le journal : %1")
                    .arg(wineLogPath()));
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
    if (!isWineBundled()) {
        qWarning() << "WineManager: embedded Wine not found";
        return false;
    }

    QProcessEnvironment env = wineEnvironment();
    env.insert("WINE_LARGE_ADDRESS_AWARE", "1");

    // DXVK (HD patch) ships d3d9.dll next to the client: tell Wine to prefer
    // the native DLL for d3d9 only in that case.
    if (QFile::exists(workDir + "/d3d9.dll")) {
        const QString overrides = env.value("WINEDLLOVERRIDES");
        env.insert("WINEDLLOVERRIDES",
                   overrides.isEmpty() ? "d3d9=n,b" : overrides + ";d3d9=n,b");
    }
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
