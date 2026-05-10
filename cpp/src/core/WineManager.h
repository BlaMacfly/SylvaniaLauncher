#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>

/**
 * @brief Manages Wine-GE installation and game execution on Linux
 * 
 * This class handles:
 * 1. Automatic download of Wine-GE from GitHub releases
 * 2. Creation and management of an isolated WINEPREFIX
 * 3. Transparent launching of Windows .exe games via Wine
 * 4. Process detection for Wine-launched executables
 * 
 * The Wine runtime is stored in:
 *   ~/.local/share/Sylvania Launcher/wine-ge/
 * The WINEPREFIX is isolated at:
 *   ~/.local/share/Sylvania Launcher/wineprefix/
 * 
 * This class is only compiled on Linux (Q_OS_LINUX).
 */

#ifdef Q_OS_LINUX

class WineManager : public QObject {
    Q_OBJECT

public:
    explicit WineManager(QObject* parent = nullptr);
    ~WineManager() override = default;

    /**
     * @brief Check if Wine-GE is installed and ready
     * @return true if wine binary exists and is executable
     */
    bool isReady() const;

    /**
     * @brief Get the path to the wine binary
     * @return Full path to wine executable
     */
    QString getWineBinary() const;

    /**
     * @brief Get the WINEPREFIX path
     * @return Full path to the isolated WINEPREFIX directory
     */
    QString getWinePrefix() const;

    /**
     * @brief Get the Wine-GE installation directory
     * @return Full path to the wine-ge directory
     */
    QString getWineDir() const;

    /**
     * @brief Download and install Wine-GE from GitHub releases
     * This is a blocking operation that shows progress via signals.
     */
    void downloadAndInstall();

    /**
     * @brief Launch a Windows executable through Wine
     * @param exePath Path to the .exe file
     * @param workDir Working directory for the process
     * @return true if the process started successfully
     */
    bool launchExe(const QString& exePath, const QString& workDir);

    /**
     * @brief Check if a specific executable is currently running under Wine
     * @param exeName Name of the executable (e.g., "Wow.exe")
     * @return true if the process is found
     */
    bool isProcessRunning(const QString& exeName) const;

signals:
    /**
     * @brief Emitted during Wine-GE download/installation
     * @param percent Progress percentage (0-100)
     * @param status Human-readable status message
     */
    void downloadProgress(int percent, const QString& status);

    /**
     * @brief Emitted when Wine-GE is ready to use
     */
    void ready();

    /**
     * @brief Emitted on error
     * @param message Error description
     */
    void error(const QString& message);

private:
    /**
     * @brief Create the WINEPREFIX silently (wineboot)
     */
    void createPrefix();

    /**
     * @brief Apply Wine optimizations for WoW 3.3.5
     */
    void configurePrefix();

    /**
     * @brief Get the latest Wine-GE release URL from GitHub API
     * @return Download URL for the tar.xz archive
     */
    QString getLatestReleaseUrl();

    /**
     * @brief Extract the downloaded Wine-GE tar.xz archive
     * @param archivePath Path to the downloaded archive
     * @return true on success
     */
    bool extractArchive(const QString& archivePath);

    QString m_wineDir;    ///< Path to wine-ge installation
    QString m_prefixDir;  ///< Path to WINEPREFIX

    std::unique_ptr<QNetworkAccessManager> m_networkManager;
};

#endif // Q_OS_LINUX
