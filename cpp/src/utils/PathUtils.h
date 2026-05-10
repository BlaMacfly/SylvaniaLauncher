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

#ifdef Q_OS_LINUX
/**
 * @brief Get the Wine-GE installation directory (Linux only)
 * @return Path to ~/.local/share/Sylvania Launcher/wine-ge
 */
inline QString getWineDir() {
    QString wineDir = getAppDataDir() + "/wine-ge";
    QDir dir(wineDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return wineDir;
}

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
