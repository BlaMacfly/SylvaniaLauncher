#include "WineManager.h"

#ifdef Q_OS_LINUX

#include "PathUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEventLoop>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>

WineManager::WineManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
{
    m_wineDir = PathUtils::getWineDir();
    m_prefixDir = PathUtils::getWinePrefixDir();
}

bool WineManager::isReady() const
{
    QString wineBin = getWineBinary();
    QFileInfo info(wineBin);
    return info.exists() && info.isExecutable();
}

QString WineManager::getWineBinary() const
{
    // Search for the wine binary inside the wine-ge directory
    // Wine-GE extracts to a subdirectory like "lutris-GE-Proton8-26-x86_64" or "wine-ge-..."
    QDir wineDir(m_wineDir);
    if (!wineDir.exists()) return QString();

    // Look for bin/wine in subdirectories
    QStringList subdirs = wineDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subdir : subdirs) {
        QString candidatePath = m_wineDir + "/" + subdir + "/bin/wine";
        if (QFile::exists(candidatePath)) {
            return candidatePath;
        }
    }

    // Also check directly in wine-ge/bin/wine
    QString directPath = m_wineDir + "/bin/wine";
    if (QFile::exists(directPath)) {
        return directPath;
    }

    return QString();
}

QString WineManager::getWinePrefix() const
{
    return m_prefixDir;
}

QString WineManager::getWineDir() const
{
    return m_wineDir;
}

void WineManager::downloadAndInstall()
{
    emit downloadProgress(0, tr("Recherche de la dernière version de Wine-GE..."));

    // Step 1: Get the latest release URL from GitHub
    QString downloadUrl = getLatestReleaseUrl();
    if (downloadUrl.isEmpty()) {
        emit error(tr("Impossible de récupérer l'URL de téléchargement de Wine-GE."));
        return;
    }

    emit downloadProgress(5, tr("Téléchargement de Wine-GE..."));

    // Step 2: Download the archive
    QDir().mkpath(m_wineDir);
    QString archivePath = m_wineDir + "/wine-ge.tar.xz";

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_networkManager->get(request);
    QFile archiveFile(archivePath);
    if (!archiveFile.open(QIODevice::WriteOnly)) {
        reply->abort();
        reply->deleteLater();
        emit error(tr("Impossible de créer le fichier temporaire."));
        return;
    }

    // Synchronous download with event loop
    QEventLoop loop;
    
    connect(reply, &QNetworkReply::readyRead, this, [&archiveFile, reply]() {
        archiveFile.write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            int percent = 5 + static_cast<int>((received * 65) / total); // 5-70%
            emit downloadProgress(percent, tr("Téléchargement de Wine-GE... %1%").arg(percent));
        }
    });

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    archiveFile.close();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        QFile::remove(archivePath);
        emit error(tr("Erreur de téléchargement: %1").arg(reply->errorString()));
        return;
    }
    reply->deleteLater();

    // Step 3: Extract the archive
    emit downloadProgress(75, tr("Extraction de Wine-GE..."));
    
    if (!extractArchive(archivePath)) {
        QFile::remove(archivePath);
        emit error(tr("Erreur lors de l'extraction de Wine-GE."));
        return;
    }

    // Cleanup archive
    QFile::remove(archivePath);

    // Step 4: Create WINEPREFIX
    emit downloadProgress(90, tr("Création du préfixe Wine..."));
    createPrefix();

    // Step 5: Configure for WoW
    emit downloadProgress(95, tr("Configuration pour World of Warcraft..."));
    configurePrefix();

    emit downloadProgress(100, tr("Wine-GE installé et configuré!"));
    emit ready();
}

QString WineManager::getLatestReleaseUrl()
{
    QNetworkRequest request{QUrl("https://api.github.com/repos/GloriousEggroll/wine-ge-custom/releases/latest")};
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "SylvaniaLauncher/2.7");

    QNetworkReply* reply = m_networkManager->get(request);
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "WineManager: GitHub API error:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();

    QJsonArray assets = doc.object().value("assets").toArray();
    for (const QJsonValue& asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj.value("name").toString();
        // Look for the x86_64 tar.xz archive (not the sha512sum)
        if (name.contains("x86_64") && name.endsWith(".tar.xz") && !name.contains("sha512")) {
            return assetObj.value("browser_download_url").toString();
        }
    }

    qWarning() << "WineManager: No suitable Wine-GE asset found in latest release";
    return QString();
}

bool WineManager::extractArchive(const QString& archivePath)
{
    // Use tar to extract .tar.xz
    QProcess process;
    process.setWorkingDirectory(m_wineDir);
    process.start("tar", QStringList() << "xJf" << archivePath << "--strip-components=0");
    process.waitForFinished(300000); // 5 min timeout for large archives

    if (process.exitCode() != 0) {
        qWarning() << "WineManager: tar extraction failed:" << process.readAllStandardError();
        return false;
    }

    qDebug() << "WineManager: Wine-GE extracted successfully to" << m_wineDir;
    return true;
}

void WineManager::createPrefix()
{
    QString wineBin = getWineBinary();
    if (wineBin.isEmpty()) {
        qWarning() << "WineManager: Wine binary not found, cannot create prefix";
        return;
    }

    QDir().mkpath(m_prefixDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", m_prefixDir);
    env.insert("WINEDEBUG", "-all");  // Suppress all Wine debug output
    env.insert("DISPLAY", env.value("DISPLAY", ":0"));

    // Get wineboot path (same dir as wine binary)
    QString wineboot = QFileInfo(wineBin).absolutePath() + "/wineboot";

    QProcess process;
    process.setProcessEnvironment(env);
    process.start(wineboot, QStringList() << "--init");
    process.waitForFinished(120000); // 2 min timeout

    if (process.exitCode() != 0) {
        qWarning() << "WineManager: wineboot failed:" << process.readAllStandardError();
    } else {
        qDebug() << "WineManager: WINEPREFIX created at" << m_prefixDir;
    }
}

void WineManager::configurePrefix()
{
    QString wineBin = getWineBinary();
    if (wineBin.isEmpty()) return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", m_prefixDir);
    env.insert("WINEDEBUG", "-all");

    // Configure Wine for optimal WoW 3.3.5 experience
    // Set Windows version to Windows 7 (best compatibility for WoTLK)
    QString winecfgReg = m_prefixDir + "/user.reg";
    
    // Use winecfg via reg to set Windows version
    QString winePath = QFileInfo(wineBin).absolutePath();
    QString wineRegPath = winePath + "/wine";
    
    QProcess regProcess;
    regProcess.setProcessEnvironment(env);
    
    // Set Windows 7 mode
    regProcess.start(wineRegPath, QStringList() 
        << "reg" << "add" << "HKEY_CURRENT_USER\\Software\\Wine" 
        << "/v" << "Version" << "/d" << "win7" << "/f");
    regProcess.waitForFinished(30000);

    qDebug() << "WineManager: Prefix configured for WoW 3.3.5";
}

bool WineManager::launchExe(const QString& exePath, const QString& workDir)
{
    QString wineBin = getWineBinary();
    if (wineBin.isEmpty()) {
        qWarning() << "WineManager: Wine binary not found";
        return false;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", m_prefixDir);
    env.insert("WINEDEBUG", "-all");
    env.insert("WINE_LARGE_ADDRESS_AWARE", "1");
    env.insert("STAGING_SHARED_MEMORY", "1");
    
    // DXVK support: if d3d9.dll exists in the game folder, use native override
    if (QFile::exists(workDir + "/d3d9.dll")) {
        env.insert("WINEDLLOVERRIDES", "d3d9=n,b");
    }

    // Use MESA_GL_VERSION_OVERRIDE if needed for older GPU drivers
    if (!env.contains("MESA_GL_VERSION_OVERRIDE")) {
        env.insert("MESA_GL_VERSION_OVERRIDE", "4.5");
    }

    QProcess* process = new QProcess();
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(workDir);

    // Start detached so the launcher can close independently
    qint64 pid;
    bool started = QProcess::startDetached(wineBin, QStringList() << exePath, workDir, &pid);

    // The detached process doesn't use the environment we set above,
    // so we need to use a wrapper script approach instead
    if (!started) {
        // Fallback: create a temporary script
        QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                             + "/sylvania_launch.sh";
        QFile script(scriptPath);
        if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&script);
            out << "#!/bin/bash\n";
            out << "export WINEPREFIX=\"" << m_prefixDir << "\"\n";
            out << "export WINEDEBUG=\"-all\"\n";
            out << "export WINE_LARGE_ADDRESS_AWARE=1\n";
            out << "export STAGING_SHARED_MEMORY=1\n";
            if (QFile::exists(workDir + "/d3d9.dll")) {
                out << "export WINEDLLOVERRIDES=\"d3d9=n,b\"\n";
            }
            out << "cd \"" << workDir << "\"\n";
            out << "\"" << wineBin << "\" \"" << exePath << "\" &\n";
            script.close();
            script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            
            started = QProcess::startDetached("/bin/bash", QStringList() << scriptPath);
        }
    }

    delete process;

    if (started) {
        qDebug() << "WineManager: Launched" << exePath << "via Wine";
    } else {
        qWarning() << "WineManager: Failed to launch" << exePath;
    }

    return started;
}

bool WineManager::isProcessRunning(const QString& exeName) const
{
    // Scan /proc for wine processes running the specified executable
    QDir procDir("/proc");
    QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString& entry : entries) {
        bool ok;
        entry.toInt(&ok);
        if (!ok) continue; // Skip non-PID directories

        QFile cmdlineFile("/proc/" + entry + "/cmdline");
        if (cmdlineFile.open(QIODevice::ReadOnly)) {
            QByteArray cmdline = cmdlineFile.readAll();
            cmdlineFile.close();
            
            // cmdline uses null bytes as separators
            QString cmdStr = QString::fromLocal8Bit(cmdline).replace('\0', ' ');
            
            if (cmdStr.contains(exeName, Qt::CaseInsensitive)) {
                return true;
            }
        }
    }

    return false;
}

#endif // Q_OS_LINUX
