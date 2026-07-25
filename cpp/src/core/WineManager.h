#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

/**
 * @brief Drives the Wine runtime EMBEDDED in the AppImage (Linux only).
 *
 * Design (v3.0 Linux edition):
 *  - Wine is no longer downloaded at runtime. A pinned build
 *    (Kron4ek wine-10.8-staging-amd64-wow64) is shipped inside the AppImage
 *    under $APPDIR/usr/wine and located relative to the launcher binary.
 *    The wow64 build runs the 32-bit WoW client without any 32-bit host
 *    libraries — mandatory for the "clean machine" guarantee.
 *  - The WINEPREFIX lives in the user's persistent data directory
 *    (~/.local/share/Sylvania Launcher/wineprefix), NEVER inside the
 *    read-only AppImage mount.
 *  - Every Wine invocation runs with an explicit environment:
 *    WINELOADER / WINESERVER / WINEDLLPATH / LD_LIBRARY_PATH all point at
 *    the embedded runtime, and the AppImage-injected variables (Qt plugin
 *    paths, LD_PRELOAD...) are scrubbed first.
 *  - Prefix initialisation is asynchronous and robust: missing OR corrupt
 *    prefix -> `wineboot --init` -> real completion wait (`wineserver -w`)
 *    -> explicit timeout -> detailed log -> user-facing error signal.
 *  - WINEDEBUG=-all in production; launching the app with --debug keeps
 *    Wine logs and writes them to the logs directory.
 */

#ifdef Q_OS_LINUX

class QProcess;

class WineManager : public QObject {
    Q_OBJECT

public:
    explicit WineManager(QObject* parent = nullptr);
    ~WineManager() override;

    /// Global debug switch (set from --debug in main). Affects WINEDEBUG and
    /// redirects wine output to the log file.
    static void setDebugMode(bool enabled);
    static bool debugMode();

    /// True if the EMBEDDED Wine runtime was found (AppImage intact). False for
    /// a distro package, which runs on the system's Wine instead — check
    /// isWineAvailable() to know whether the game can be launched at all.
    bool isWineBundled() const;

    /// True when a usable Wine was found, embedded or system-wide.
    bool isWineAvailable() const;

    /// User-facing explanation when no Wine was found, worded for the kind of
    /// build in use (damaged AppImage vs. distro package missing a dependency).
    QString missingWineMessage() const;

    /// True if the WINEPREFIX exists and is genuinely usable: registry hives,
    /// drive_c, AND every system DLL the client needs. The DLL check matters —
    /// a wineboot that failed to install one still leaves complete-looking
    /// hives behind, and such a prefix would otherwise never be repaired.
    bool isPrefixValid() const;

    /// Convenience: a Wine is usable AND the prefix is initialised.
    bool isReady() const { return isWineAvailable() && isPrefixValid(); }

    QString wineBinary() const;      ///< .../bin/wine (empty if not bundled)
    QString wineServerBinary() const;///< .../bin/wineserver
    QString wineRoot() const;        ///< embedded runtime root directory
    QString winePrefix() const;      ///< persistent WINEPREFIX path
    QString wineLogPath() const;     ///< log file used in --debug mode

    /// True when DXVK is bundled AND the host exposes a Vulkan driver, i.e.
    /// the game can be rendered through D3D9 -> Vulkan instead of WineD3D's
    /// D3D9 -> OpenGL translation (a very large frame-rate difference).
    bool isDxvkUsable() const;

    /**
     * @brief Initialise (or repair) the WINEPREFIX asynchronously.
     *
     * Emits initProgress() while running and initFinished() exactly once.
     * If a previous initialisation is still running the call is ignored.
     * The sequence is: wipe half-initialised prefix -> wineboot --init
     * (timeout initTimeoutMs) -> wineserver -w (timeout serverWaitMs)
     * -> validation -> initFinished(true/false).
     */
    void initializePrefix();

    /**
     * @brief Launch a Windows executable through the embedded Wine.
     * @param exePath  Absolute path to the .exe
     * @param workDir  Working directory for the process
     * @return true if the detached process started.
     *
     * The environment is carried by the QProcess instance (Qt >= 5.10
     * supports setProcessEnvironment with startDetached), so no wrapper
     * script in /tmp is needed.
     */
    bool launchExe(const QString& exePath, const QString& workDir);

    /// Scan /proc for a Wine process whose command line contains exeName.
    bool isProcessRunning(const QString& exeName) const;

signals:
    /// Prefix initialisation progress: percent is indicative (0-100), status
    /// is a user-facing message.
    void initProgress(int percent, const QString& status);

    /// Prefix initialisation outcome. On failure `message` is user-facing;
    /// details are in the log file.
    void initFinished(bool success, const QString& message);

private:
    QProcessEnvironment wineEnvironment() const;
    void logLine(const QString& line) const;
    void finishInit(bool success, const QString& message);
    void startWineserverWait();

    /// System DLLs missing from the prefix, as "system32/foo.dll" paths.
    /// Empty on a healthy prefix.
    QStringList missingSystemDlls() const;

    /// User-facing explanation for a prefix that did not come out complete.
    /// Distinguishes a damaged AppImage (the missing DLL is unreadable in the
    /// bundle too) from a plain initialisation failure.
    QString diagnoseIncompletePrefix() const;

    /// Copy the bundled DXVK d3d9 into the prefix (no-op when already current).
    /// Returns true when the prefix ends up with a usable DXVK d3d9.dll.
    bool ensureDxvkInstalled();
    /// Does the HOST provide a Vulkan loader plus at least one driver (ICD)?
    static bool hasVulkanDriver();

    QString m_wineRoot;    ///< Wine root in use (empty when none was found)
    bool m_wineIsBundled = false;  ///< true when m_wineRoot is the embedded one
    QString m_dxvkRoot;    ///< embedded DXVK root (empty if absent)
    QString m_prefixDir;   ///< persistent WINEPREFIX
    QProcess* m_initProcess = nullptr;  ///< wineboot/wineserver during init

    static bool s_debugMode;

    // Timeouts (ms). wineboot on a cold prefix can take > 60 s on slow disks.
    static constexpr int kInitTimeoutMs = 180000;
    static constexpr int kServerWaitTimeoutMs = 60000;
};

#endif // Q_OS_LINUX
