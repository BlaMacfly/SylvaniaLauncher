#pragma once

#include <QMainWindow>
#include <QPixmap>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include <QDateTime>
#include <QTimer>
#include <memory>

class ConfigManager;
class SoundManager;
class NotesManager;
class NotesWindow;
class RealmlistWindow;
class DownloadDialog;
class HdPatchManager;
class QProgressBar;

/**
 * @brief Main window for Sylvania Launcher
 * 
 * Exact replica of Python version with:
 * - Two-panel layout (Server + Stats)
 * - Logo top-left
 * - Styled buttons matching WoW aesthetic
 * - Game statistics display
 * - Play time tracking via process monitoring
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPlayButtonClicked();
    void onDownloadButtonClicked();
    void onHdButtonClicked();
    void onSettingsButtonClicked();
    void onNotesButtonClicked();
    void onServersButtonClicked();
    void onQuitButtonClicked();
    
    void onDownloadComplete(bool success, const QString& message);
    void updateServerInfo();
    void updateStats();
    void checkWowProcess();

private:
    void setupUi();
    void connectSignals();
    void loadBackground();
    void checkWowInstalled();
    void playGame();
    void browseWowDirectory();
    void loadStats();
    void saveStats();
    bool isWowRunning();
    
    QWidget* createServerPanel();
    QWidget* createStatsPanel();
    QPushButton* createStyledButton(const QString& text, const QString& style);

    // Managers
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<SoundManager> m_soundManager;
    std::unique_ptr<NotesManager> m_notesManager;

    // UI Elements - Server Panel
    QLabel* m_serverNameLabel = nullptr;
    QLabel* m_realmlistLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    
    // UI Elements - Stats Panel
    QLabel* m_playTimeLabel = nullptr;
    QLabel* m_launchCountLabel = nullptr;
    QLabel* m_lastSessionLabel = nullptr;
    
    // HD Patch UI
    QLabel* m_hdStatusLabel = nullptr;
    QProgressBar* m_hdProgressBar = nullptr;
    
    // Buttons
    QPushButton* m_playButton = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_hdButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_notesButton = nullptr;
    QPushButton* m_quitButton = nullptr;
    QPushButton* m_changeServerButton = nullptr;

    // Child windows
    NotesWindow* m_notesWindow = nullptr;
    RealmlistWindow* m_realmlistWindow = nullptr;
    DownloadDialog* m_downloadDialog = nullptr;
    std::unique_ptr<HdPatchManager> m_hdPatchManager;

    // Background
    QPixmap m_backgroundImage;
    QPixmap m_logoImage;
    
    // Stats
    int m_launchCount = 0;
    qint64 m_totalPlayTime = 0; // seconds
    QDateTime m_lastSession;
    
    // Play time tracking
    QTimer* m_playTimeTimer = nullptr;
    QDateTime m_sessionStartTime;
    bool m_wowRunning = false;
};

