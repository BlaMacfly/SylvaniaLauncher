#pragma once

#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

namespace PathUtils {

/**
 * @brief Get the application data directory
 * @return Path to %LOCALAPPDATA%/SylvaniaLauncher
 */
inline QString getAppDataDir() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(appData);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return appData;
}

/**
 * @brief Get the configuration directory
 * @return Path to the config directory
 */
inline QString getConfigDir() {
    return getAppDataDir();
}

/**
 * @brief Get the data directory for notes, etc.
 * @return Path to the data directory
 */
inline QString getDataDir() {
    QString dataDir = getAppDataDir() + "/data";
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dataDir;
}

/**
 * @brief Get the logs directory
 * @return Path to the logs directory
 */
inline QString getLogsDir() {
    QString logsDir = getAppDataDir() + "/logs";
    QDir dir(logsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return logsDir;
}

/**
 * @brief Get the assets directory (development or compiled)
 * @return Path to the Asset directory
 */
inline QString getAssetsDir() {
    // Check relative to executable
    QString exeDir = QCoreApplication::applicationDirPath();
    
    QStringList possiblePaths = {
        exeDir + "/Asset",
        exeDir + "/../Asset",
        QDir::currentPath() + "/Asset"
    };
    
    for (const QString& path : possiblePaths) {
        if (QDir(path).exists()) {
            return QDir(path).absolutePath();
        }
    }
    
    // Fallback
    return exeDir + "/Asset";
}

/**
 * @brief Get the sounds directory
 * @return Path to the Sounds directory
 */
inline QString getSoundsDir() {
    return getAssetsDir() + "/Sounds";
}

/**
 * @brief Resolve a file inside a directory, case-insensitively on Linux.
 *
 * WoW client archives ship the executable as Wow.exe / WoW.exe / wow.exe
 * indistinctly. NTFS resolves these to the same file, but Linux filesystems
 * are case-sensitive, so every "does the client exe exist" probe must go
 * through this helper instead of a bare QFile::exists().
 *
 * @return Absolute path of the match, or an empty string if absent.
 */
inline QString resolveFileCaseInsensitive(const QString& dir, const QString& fileName) {
    const QString direct = dir + "/" + fileName;
    if (QFile::exists(direct)) return direct;
#ifdef Q_OS_LINUX
    const QStringList entries = QDir(dir).entryList(QDir::Files);
    for (const QString& entry : entries) {
        if (entry.compare(fileName, Qt::CaseInsensitive) == 0) {
            return dir + "/" + entry;
        }
    }
#endif
    return QString();
}

#ifdef Q_OS_LINUX
/**
 * @brief Get the WINEPREFIX directory (Linux only)
 * @return Path to ~/.local/share/Sylvania Launcher/wineprefix
 */
inline QString getWinePrefixDir() {
    QString prefixDir = getAppDataDir() + "/wineprefix";
    QDir dir(prefixDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return prefixDir;
}
#endif // Q_OS_LINUX

} // namespace PathUtils
