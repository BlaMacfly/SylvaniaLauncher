#pragma once

#include <QMainWindow>
#include <QPixmap>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include <QDateTime>
#include <QTimer>
#include <QTranslator>
#include <memory>

#include "ButtonStyles.h"

struct GameEdition;
class ConfigManager;
class SoundManager;
class NotesManager;
class ReminderService;
class NotesWindow;
class RealmlistWindow;
class DownloadDialog;
class HdPatchManager;
class WowButton;
class QProgressBar;
class QProcess;
class QSystemTrayIcon;
class AddonsWindow;

/**
 * @brief Main window for Sylvania Launcher (v3.0)
 *
 * - "2 launchers en 1": a toggle switches the active GameEdition (WotLK ⇄
 *   Legion) in-process — window logo, taskbar icon, background, accent and
 *   all edition-bound data (path, realmlist, stats, addons) follow.
 * - Single primary action button (WowButton): Installer / Téléchargement /
 *   Jouer, driven by per-edition client detection.
 * - Layout: header (logo + edition/lang switches), info panels (server +
 *   stats), anchored primary action zone, secondary nav bar, footer. All
 *   Qt layouts, spacing from SylvaniaConstants, button styles from
 *   ButtonStyles (3 strict levels).
 * - System tray icon hosting reminder notifications (ReminderService).
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onPrimaryButtonClicked();   // Play or Install depending on state
    void onEditionToggleClicked();
    void onHdButtonClicked();
    void onSettingsButtonClicked();
    void onNotesButtonClicked();
    void onServersButtonClicked();
    void onAddonsButtonClicked();
    void onQuitButtonClicked();
    void onLangButtonClicked();

    void onDownloadComplete(bool success, const QString& message);
    void updateServerInfo();
    void updateStats();
    void checkWowProcess();

private:
    void setupUi();
    void connectSignals();
    // Applies everything bound to the active edition: window title, logo,
    // window/taskbar/tray icon, background, accent, HD gating, realmlist,
    // stats and the primary button state. Called at startup and on toggle.
    void applyEdition();
    // Windows: force the running taskbar button to repaint with the edition
    // icon (WM_SETICON). Qt's setWindowIcon doesn't always refresh a taskbar
    // button once the app has been pinned. No-op on other platforms.
    void applyTaskbarIcon(const QString& icoPath);
    void applyTheme(const QString& bgName);
    ButtonStyles::ThemeTokens themeTokensFor(const QString& bgName) const;
    QString pickRandomBackground(const QString& exclude = QString()) const;
    const GameEdition& activeEdition() const;
    // Full path of the active edition's client exe, empty if not found.
    QString clientExePath() const;
    // Refreshes the 3-state primary button from on-disk client detection.
    void refreshPrimaryState();
    void startClientInstall();
    void playGame();
    void loadStats();
    void saveStats();
    void handleWowRunningState(bool running);
    void changeLanguage(const QString& lang, bool initial = false);
    void retranslateUi();

    // Window geometry: restore the saved size/position (clamped to the visible
    // screen area for multi-monitor / unplugged-display safety), and reset it
    // to the default centered size on request.
    void restoreWindowGeometry();
    void clampToAvailableScreen();

    QWidget* createHeader();
    QWidget* createServerPanel();
    QWidget* createStatsPanel();
    QWidget* createActionZone();
    QWidget* createNavBar();

    // Managers
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<SoundManager> m_soundManager;
    std::unique_ptr<NotesManager> m_notesManager;
    std::unique_ptr<ReminderService> m_reminderService;

    // UI Elements - Header
    QLabel* m_logoLabel = nullptr;

    // UI Elements - Server Panel
    QLabel* m_serverNameLabel = nullptr;
    QLabel* m_realmlistLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    // UI Elements - Stats Panel
    QLabel* m_playTimeLabel = nullptr;
    QLabel* m_launchCountLabel = nullptr;
    QLabel* m_lastSessionLabel = nullptr;

    // Shared progress row (client download + HD patch)
    QLabel* m_progressLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Buttons — exactly one primary, a consistent secondary row, tertiary lang.
    WowButton* m_playButton = nullptr;        // primary (Install/Download/Play)
    QPushButton* m_editionButton = nullptr;   // secondary (header, fixed spot)
    QPushButton* m_hdButton = nullptr;        // secondary (WotLK only)
    QPushButton* m_settingsButton = nullptr;  // secondary
    QPushButton* m_notesButton = nullptr;     // secondary
    QPushButton* m_addonsButton = nullptr;    // secondary
    QPushButton* m_serversButton = nullptr;   // secondary
    QPushButton* m_quitButton = nullptr;      // secondary
    QPushButton* m_langButton = nullptr;      // tertiary (header)

    // Panels
    QFrame* m_serverPanel = nullptr;
    QFrame* m_statsPanel = nullptr;
    QLabel* m_serverTitleLabel = nullptr;
    QLabel* m_statsTitleLabel = nullptr;
    QLabel* m_footerLabel = nullptr;

    // Child windows
    NotesWindow* m_notesWindow = nullptr;
    RealmlistWindow* m_realmlistWindow = nullptr;
    DownloadDialog* m_downloadDialog = nullptr;
    AddonsWindow* m_addonsWindow = nullptr;
    std::unique_ptr<HdPatchManager> m_hdPatchManager;

    // Tray (reminder notifications + restore-on-click)
    QSystemTrayIcon* m_trayIcon = nullptr;

    // Background
    QPixmap m_backgroundImage;
    QPixmap m_logoImage;

    // Stats (per active edition)
    int m_launchCount = 0;
    qint64 m_totalPlayTime = 0; // seconds
    QDateTime m_lastSession;

    // Play time tracking
    QTimer* m_playTimeTimer = nullptr;
    QDateTime m_sessionStartTime;
    bool m_wowRunning = false;
    QProcess* m_wowCheckProcess = nullptr; // async Wow.exe presence check
    bool m_downloadInProgress = false;

    QTranslator m_translator;
};
