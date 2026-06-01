#include "MainWindow.h"
#include "NotesWindow.h"
#include "RealmlistWindow.h"
#include "DownloadDialog.h"
#include "SettingsDialog.h"
#include "AddonsWindow.h"
#include "ConfigManager.h"
#include "SoundManager.h"
#include "NotesManager.h"
#include "PathUtils.h"
#include "HdPatchManager.h"
#include "Constants.h"
#include <QProgressBar>

#ifdef Q_OS_LINUX
#include "WineManager.h"
#endif

#include <QApplication>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_config(std::make_unique<ConfigManager>(this))
    , m_soundManager(std::make_unique<SoundManager>(this))
    , m_notesManager(std::make_unique<NotesManager>(this))
{
    setWindowTitle("Sylvania Launcher - World of Warcraft 3.3.5");
    setFixedSize(SylvaniaConstants::kMainWindowWidth,
                 SylvaniaConstants::kMainWindowHeight);
    
    // Load logo
    QString logoPath = PathUtils::getAssetsDir() + "/sylvania_logo.png";
    if (QFile::exists(logoPath)) {
        m_logoImage.load(logoPath);
    }
    
    // Set window icon
    QString iconPath = PathUtils::getAssetsDir() + "/LogoSylvania/LogoSylvania256.ico";
    if (QFile::exists(iconPath)) {
        setWindowIcon(QIcon(iconPath));
    }
    
    setupUi();
    connectSignals();
    checkWowInstalled();
    updateServerInfo();
    
    // Setup play time tracking timer
    m_playTimeTimer = new QTimer(this);
    connect(m_playTimeTimer, &QTimer::timeout, this, &MainWindow::checkWowProcess);
    m_playTimeTimer->start(SylvaniaConstants::kPlayTimeCheckMs);

    loadStats();

    // Initial theme and language setup
    QString currentLang = m_config->getLanguage();
    if (currentLang.isEmpty()) currentLang = "fr";
    changeLanguage(currentLang, true); // true means initial, so we DON'T check for enUS yet
    
    // Background on launch: if the random toggle is ON, pick a fresh one
    // (different from last time); otherwise keep the saved background.
    if (m_config->isRandomBackgroundEnabled()) {
        QString launchBg = pickRandomBackground(m_config->getBackground());
        m_config->setBackground(launchBg);
        applyTheme(launchBg);
    } else {
        applyTheme(m_config->getBackground());
    }
}

MainWindow::~MainWindow() = default;

QString MainWindow::pickRandomBackground(const QString& exclude) const {
    static const QStringList backgrounds = {
        "Alliance", "Arbre de Vie", "Azeroth", "Horde",
        "Ilidan", "Lich King", "Ragnaros", "Taverne"
    };
    if (backgrounds.size() <= 1) return backgrounds.value(0);
    QString chosen = exclude;
    while (chosen == exclude) {
        chosen = backgrounds.at(QRandomGenerator::global()->bounded(backgrounds.size()));
    }
    return chosen;
}

void MainWindow::applyTheme(const QString& bgName) {
    // Load background image (try .jpg first, then .png)
    QString bgPath = PathUtils::getAssetsDir() + "/Background/" + bgName + ".jpg";
    if (!QFile::exists(bgPath)) {
        bgPath = PathUtils::getAssetsDir() + "/Background/" + bgName + ".png";
    }
    if (!QFile::exists(bgPath)) {
        bgPath = PathUtils::getAssetsDir() + "/Background/Taverne.png"; // Fallback
    }
    if (QFile::exists(bgPath)) {
        m_backgroundImage.load(bgPath);
    }
    update(); // Trigger repaint
    
    // Define theme colors based on bgName
    QString panelBgColor, panelBorder, textPrimary, textSecondary;
    QString btnGreen1, btnGreen2, btnGreen3, btnGreenBorder;
    QString btnGold1, btnGold2, btnGold3, btnGoldBorder;
    QString btnBlue1, btnBlue2, btnBlue3, btnBlueBorder;
    QString btnBrown1, btnBrown2, btnBrown3, btnBrownBorder;
    QString btnPink1, btnPink2, btnPink3, btnPinkBorder;
    QString btnTeal1, btnTeal2, btnTeal3, btnTealBorder;

    if (bgName == "Alliance") {
        panelBgColor = "rgba(0, 20, 50, 180)"; panelBorder = "#15479a";
        textPrimary = "#ffffff"; textSecondary = "#7ec8e3";
        btnGreen1 = "#1e5ab8"; btnGreen2 = "#15479a"; btnGreen3 = "#0f326a"; btnGreenBorder = "#3a7de4";
        btnGold1 = "#a67c00"; btnGold2 = "#bf953f"; btnGold3 = "#fcf6ba"; btnGoldBorder = "#d4af37";
        btnBlue1 = "#2a6dd4"; btnBlue2 = "#1e5ab8"; btnBlue3 = "#15479a"; btnBlueBorder = "#3a7de4";
        btnBrown1 = "#3a4a5a"; btnBrown2 = "#2a3a4a"; btnBrown3 = "#1a2a3a"; btnBrownBorder = "#5a6a7a";
        btnPink1 = "#5a3a7a"; btnPink2 = "#4a2a6a"; btnPink3 = "#3a1a5a"; btnPinkBorder = "#7a5a9a";
        btnTeal1 = "#2a5a7a"; btnTeal2 = "#1a4a6a"; btnTeal3 = "#0a3a5a"; btnTealBorder = "#4a7a9a";
    } else if (bgName == "Horde") {
        panelBgColor = "rgba(40, 0, 0, 180)"; panelBorder = "#6a0dad";
        textPrimary = "#e0e0e0"; textSecondary = "#ff6b6b";
        btnGreen1 = "#8b0000"; btnGreen2 = "#660000"; btnGreen3 = "#440000"; btnGreenBorder = "#b22222";
        btnGold1 = "#3a3a3a"; btnGold2 = "#2a2a2a"; btnGold3 = "#1a1a1a"; btnGoldBorder = "#6a6a6a";
        btnBlue1 = "#a52a2a"; btnBlue2 = "#8b0000"; btnBlue3 = "#660000"; btnBlueBorder = "#d2691e";
        btnBrown1 = "#4a2d2d"; btnBrown2 = "#3a1d1d"; btnBrown3 = "#2a0d0d"; btnBrownBorder = "#6a4d4d";
        btnPink1 = "#7a2a2a"; btnPink2 = "#6a1a1a"; btnPink3 = "#5a0a0a"; btnPinkBorder = "#9a4a4a";
        btnTeal1 = "#5a2a2a"; btnTeal2 = "#4a1a1a"; btnTeal3 = "#3a0a0a"; btnTealBorder = "#7a4a4a";
    } else if (bgName == "Arbre de Vie") {
        panelBgColor = "rgba(10, 40, 10, 180)"; panelBorder = "#2e8b57";
        textPrimary = "#f0fff0"; textSecondary = "#98fb98";
        btnGreen1 = "#2e8b57"; btnGreen2 = "#228b22"; btnGreen3 = "#006400"; btnGreenBorder = "#3cb371";
        btnGold1 = "#8b4513"; btnGold2 = "#a0522d"; btnGold3 = "#cd853f"; btnGoldBorder = "#d2b48c";
        btnBlue1 = "#556b2f"; btnBlue2 = "#6b8e23"; btnBlue3 = "#808000"; btnBlueBorder = "#9acd32";
        btnBrown1 = "#5a4a2d"; btnBrown2 = "#4a3a1d"; btnBrown3 = "#3a2a0d"; btnBrownBorder = "#7a6a4d";
        btnPink1 = "#8a5a5a"; btnPink2 = "#7a4a4a"; btnPink3 = "#6a3a3a"; btnPinkBorder = "#aa7a7a";
        btnTeal1 = "#20b2aa"; btnTeal2 = "#008b8b"; btnTeal3 = "#00688b"; btnTealBorder = "#48d1cc";
    } else if (bgName == "Ilidan") {
        panelBgColor = "rgba(20, 0, 30, 180)"; panelBorder = "#32cd32";
        textPrimary = "#e6e6fa"; textSecondary = "#7fff00";
        btnGreen1 = "#32cd32"; btnGreen2 = "#228b22"; btnGreen3 = "#006400"; btnGreenBorder = "#7fff00";
        btnGold1 = "#4b0082"; btnGold2 = "#800080"; btnGold3 = "#9932cc"; btnGoldBorder = "#da70d6";
        btnBlue1 = "#483d8b"; btnBlue2 = "#4b0082"; btnBlue3 = "#2f4f4f"; btnBlueBorder = "#6a5acd";
        btnBrown1 = "#3a2d4a"; btnBrown2 = "#2a1d3a"; btnBrown3 = "#1a0d2a"; btnBrownBorder = "#5a4d6a";
        btnPink1 = "#8b008b"; btnPink2 = "#800080"; btnPink3 = "#4b0082"; btnPinkBorder = "#ba55d3";
        btnTeal1 = "#00ced1"; btnTeal2 = "#008b8b"; btnTeal3 = "#00688b"; btnTealBorder = "#20b2aa";
    } else if (bgName == "Lich King") {
        panelBgColor = "rgba(0, 10, 20, 180)"; panelBorder = "#4682b4";
        textPrimary = "#f0f8ff"; textSecondary = "#87cefa";
        btnGreen1 = "#4682b4"; btnGreen2 = "#5f9ea0"; btnGreen3 = "#1e90ff"; btnGreenBorder = "#87ceeb";
        btnGold1 = "#708090"; btnGold2 = "#778899"; btnGold3 = "#b0c4de"; btnGoldBorder = "#d3d3d3";
        btnBlue1 = "#000080"; btnBlue2 = "#0000cd"; btnBlue3 = "#4169e1"; btnBlueBorder = "#6495ed";
        btnBrown1 = "#4a5a6a"; btnBrown2 = "#3a4a5a"; btnBrown3 = "#2a3a4a"; btnBrownBorder = "#6a7a8a";
        btnPink1 = "#5a5a7a"; btnPink2 = "#4a4a6a"; btnPink3 = "#3a3a5a"; btnPinkBorder = "#7a7a9a";
        btnTeal1 = "#20b2aa"; btnTeal2 = "#008b8b"; btnTeal3 = "#00688b"; btnTealBorder = "#48d1cc";
    } else if (bgName == "Ragnaros") {
        panelBgColor = "rgba(30, 10, 0, 180)"; panelBorder = "#ff4500";
        textPrimary = "#ffebcd"; textSecondary = "#ff8c00";
        btnGreen1 = "#ff4500"; btnGreen2 = "#cd5c5c"; btnGreen3 = "#8b0000"; btnGreenBorder = "#ff6347";
        btnGold1 = "#696969"; btnGold2 = "#808080"; btnGold3 = "#a9a9a9"; btnGoldBorder = "#d3d3d3";
        btnBlue1 = "#b22222"; btnBlue2 = "#8b0000"; btnBlue3 = "#dc143c"; btnBlueBorder = "#ff0000";
        btnBrown1 = "#5a2d1d"; btnBrown2 = "#4a1d0d"; btnBrown3 = "#3a0d00"; btnBrownBorder = "#7a4d3d";
        btnPink1 = "#8a2a2a"; btnPink2 = "#7a1a1a"; btnPink3 = "#6a0a0a"; btnPinkBorder = "#aa4a4a";
        btnTeal1 = "#5a3a2a"; btnTeal2 = "#4a2a1a"; btnTeal3 = "#3a1a0a"; btnTealBorder = "#7a5a4a";
    } else if (bgName == "Taverne") {
        panelBgColor = "rgba(0, 0, 0, 150)"; panelBorder = "#5a4a2d";
        textPrimary = "#ffffff"; textSecondary = "#7ec8e3";
        btnGreen1 = "#4a7c3f"; btnGreen2 = "#3a6a2f"; btnGreen3 = "#2a5a1f"; btnGreenBorder = "#5a8c4f";
        btnGold1 = "#4a3a20"; btnGold2 = "#3a2a15"; btnGold3 = "#2a1a0a"; btnGoldBorder = "#d4af37";
        btnBlue1 = "#2a6dd4"; btnBlue2 = "#1e5ab8"; btnBlue3 = "#15479a"; btnBlueBorder = "#3a7de4";
        btnBrown1 = "#5a4a3d"; btnBrown2 = "#4a3a2d"; btnBrown3 = "#3a2a1d"; btnBrownBorder = "#6a5a4d";
        btnPink1 = "#8a4a5a"; btnPink2 = "#7a3a4a"; btnPink3 = "#6a2a3a"; btnPinkBorder = "#9a5a6a";
        btnTeal1 = "#7a6a3a"; btnTeal2 = "#5a4a2a"; btnTeal3 = "#3a3a1a"; btnTealBorder = "#d4af37";
    } else if (bgName == "Linux") {
        panelBgColor = "rgba(10, 10, 10, 200)"; panelBorder = "#ffcc00"; // Tux yellow/dark mode
        textPrimary = "#e0e0e0"; textSecondary = "#ffcc00";
        btnGreen1 = "#4a7c3f"; btnGreen2 = "#3a6a2f"; btnGreen3 = "#2a5a1f"; btnGreenBorder = "#5a8c4f";
        btnGold1 = "#cc9900"; btnGold2 = "#b38600"; btnGold3 = "#806000"; btnGoldBorder = "#ffcc00";
        btnBlue1 = "#2a6dd4"; btnBlue2 = "#1e5ab8"; btnBlue3 = "#15479a"; btnBlueBorder = "#3a7de4";
        btnBrown1 = "#3a3a3a"; btnBrown2 = "#2a2a2a"; btnBrown3 = "#1a1a1a"; btnBrownBorder = "#6a6a6a";
        btnPink1 = "#8a4a5a"; btnPink2 = "#7a3a4a"; btnPink3 = "#6a2a3a"; btnPinkBorder = "#9a5a6a";
        btnTeal1 = "#00ced1"; btnTeal2 = "#008b8b"; btnTeal3 = "#00688b"; btnTealBorder = "#20b2aa";
    } else { // Azeroth or Default
        panelBgColor = "rgba(230, 210, 170, 200)"; panelBorder = "#5a4a2d";
        textPrimary = "#3a2a1d"; textSecondary = "#5a4a2d";
        btnGreen1 = "#4a7c3f"; btnGreen2 = "#3a6a2f"; btnGreen3 = "#2a5a1f"; btnGreenBorder = "#5a8c4f";
        btnGold1 = "#4a3a20"; btnGold2 = "#3a2a15"; btnGold3 = "#2a1a0a"; btnGoldBorder = "#d4af37";
        btnBlue1 = "#2a6dd4"; btnBlue2 = "#1e5ab8"; btnBlue3 = "#15479a"; btnBlueBorder = "#3a7de4";
        btnBrown1 = "#5a4a3d"; btnBrown2 = "#4a3a2d"; btnBrown3 = "#3a2a1d"; btnBrownBorder = "#6a5a4d";
        btnPink1 = "#8a4a5a"; btnPink2 = "#7a3a4a"; btnPink3 = "#6a2a3a"; btnPinkBorder = "#9a5a6a";
        btnTeal1 = "#7a6a3a"; btnTeal2 = "#5a4a2a"; btnTeal3 = "#3a3a1a"; btnTealBorder = "#d4af37";
    }

    // Apply to Panels
    QString panelStyle = QString("QFrame { background-color: %1; border: 2px solid %2; border-radius: 10px; }").arg(panelBgColor, panelBorder);
    if (m_serverPanel) m_serverPanel->setStyleSheet(panelStyle);
    if (m_statsPanel) m_statsPanel->setStyleSheet(panelStyle);

    // Labels Update Text Colors
    if (bgName == "Azeroth") {
        m_serverNameLabel->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; border: none;").arg(textPrimary));
        m_realmlistLabel->setStyleSheet(QString("color: %1; font-size: 12px; border: none;").arg(textSecondary));
        m_statusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 5px; }").arg(textPrimary));
        m_playTimeLabel->setStyleSheet(QString("QLabel { background-color: rgba(255, 255, 255, 100); color: %1; font-size: 13px; padding: 12px; border: 2px solid %2; border-radius: 8px; }").arg(textPrimary, panelBorder));
        m_launchCountLabel->setStyleSheet(m_playTimeLabel->styleSheet());
        m_lastSessionLabel->setStyleSheet(m_playTimeLabel->styleSheet());
    } else {
        m_serverNameLabel->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; border: none;").arg(textPrimary));
        m_realmlistLabel->setStyleSheet(QString("color: %1; font-size: 12px; border: none;").arg(textSecondary));
        m_statusLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 5px; }").arg(textPrimary));
        m_playTimeLabel->setStyleSheet(QString("QLabel { background-color: rgba(0, 0, 0, 100); color: %1; font-size: 13px; padding: 12px; border: 2px solid %2; border-radius: 8px; }").arg(textPrimary, panelBorder));
        m_launchCountLabel->setStyleSheet(m_playTimeLabel->styleSheet());
        m_lastSessionLabel->setStyleSheet(m_playTimeLabel->styleSheet());
    }

    // Footer - adapt color to theme
    if (m_footerLabel) m_footerLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(textSecondary));

    // H7: per-button styles delegate to a single helper. Used to be 8 inlined
    // lambda calls duplicating the same QSS template.
    const QString downloadText = QStringLiteral("#d4af37");
    const QString secondaryText = (bgName == "Azeroth") ? QStringLiteral("#ffffff") : QStringLiteral("#d4af37");
    const QString langText = (bgName == "Azeroth") ? QStringLiteral("#d4af37") : QStringLiteral("#ffffff");

    styleButton(m_playButton, btnGreen1, btnGreen2, btnGreen3, btnGreenBorder);
    styleButton(m_downloadButton, btnGold1, btnGold2, btnGold3, btnGoldBorder, downloadText, 12);
    styleButton(m_hdButton, btnBlue1, btnBlue2, btnBlue3, btnBlueBorder);

    styleButton(m_settingsButton, btnBrown1, btnBrown2, btnBrown3, btnBrownBorder, secondaryText, 11);
    styleButton(m_notesButton, btnBrown1, btnBrown2, btnBrown3, btnBrownBorder, secondaryText, 11);
    styleButton(m_quitButton, btnPink1, btnPink2, btnPink3, btnPinkBorder, QStringLiteral("#ffffff"), 11);
    styleButton(m_addonsButton, btnTeal1, btnTeal2, btnTeal3, btnTealBorder, QStringLiteral("#ffffff"), 12);
    styleButton(m_changeServerButton, btnTeal1, btnTeal2, btnTeal3, btnTealBorder, QStringLiteral("#ffffff"), 12);
    styleButton(m_langButton, btnGold1, btnGold2, btnGold3, btnGoldBorder, langText, 10);
}

void MainWindow::styleButton(QPushButton* btn,
                             const QString& c1, const QString& c2, const QString& c3,
                             const QString& border,
                             const QString& textColor,
                             int fontSize) {
    if (!btn) return;
    btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:0.5 %2, stop:1 %3);
            color: %5; border: 2px solid %4; border-radius: 8px; padding: 10px 20px; font-size: %6px; font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %4, stop:0.5 %1, stop:1 %2);
        }
        QPushButton:pressed { background-color: %3; }
    )").arg(c1, c2, c3, border, textColor).arg(fontSize));
}

void MainWindow::loadStats() {
    QString statsPath = PathUtils::getDataDir() + "/stats.json";
    QFile file(statsPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        m_launchCount = obj.value("launch_count").toInt(0);
        m_totalPlayTime = obj.value("total_play_time").toInteger(0);
        m_lastSession = QDateTime::fromString(obj.value("last_session").toString(), Qt::ISODate);
        file.close();
    }
}

void MainWindow::saveStats() {
    QString statsPath = PathUtils::getDataDir() + "/stats.json";
    QDir().mkpath(PathUtils::getDataDir());
    
    QJsonObject obj;
    obj["launch_count"] = m_launchCount;
    obj["total_play_time"] = m_totalPlayTime;
    obj["last_session"] = m_lastSession.toString(Qt::ISODate);
    
    QFile file(statsPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();
    }
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QMainWindow::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Draw background
    if (!m_backgroundImage.isNull()) {
        QPixmap scaled = m_backgroundImage.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    }
    
    // Draw logo top-left
    if (!m_logoImage.isNull()) {
        const int sz = SylvaniaConstants::kLogoSizePx;
        QPixmap scaledLogo = m_logoImage.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(15, 15, scaledLogo);
    }
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 170, 20, 15);
    mainLayout->setSpacing(10);
    
    // Two-panel layout
    QHBoxLayout* panelsLayout = new QHBoxLayout();
    panelsLayout->setSpacing(20);
    
    panelsLayout->addWidget(createServerPanel(), 1);
    panelsLayout->addWidget(createStatsPanel(), 1);
    
    mainLayout->addLayout(panelsLayout, 1);
    
    // Status bar at bottom
    m_statusLabel = new QLabel(tr("Chargement..."), this);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet(R"(
        QLabel {
            color: #7ec8e3;
            font-size: 12px;
            padding: 5px;
        }
    )");
    mainLayout->addWidget(m_statusLabel);
    
    // Footer
    m_footerLabel = new QLabel("© 2025 Sylvania Launcher v2.8 - World of Warcraft 3.3.5 - Linux Version", this);
    m_footerLabel->setAlignment(Qt::AlignCenter);
    m_footerLabel->setStyleSheet("color: #d4af37; font-size: 11px;");
    mainLayout->addWidget(m_footerLabel);
}

QWidget* MainWindow::createServerPanel() {
    m_serverPanel = new QFrame(this);
    m_serverPanel->setStyleSheet(R"(
        QFrame {
            background-color: rgba(0, 0, 0, 150);
            border: 2px solid #5a4a2d;
            border-radius: 10px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(m_serverPanel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);
    
    // Title
    m_serverTitleLabel = new QLabel(tr("Serveur"), this);
    m_serverTitleLabel->setAlignment(Qt::AlignCenter);
    m_serverTitleLabel->setStyleSheet("color: #d4af37; font-size: 18px; font-weight: bold; border: none;");
    layout->addWidget(m_serverTitleLabel);
    
    // Server name
    m_serverNameLabel = new QLabel(tr("Serveur: The Kingdom of Sylvania 3.3.5"), this);
    m_serverNameLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; border: none;");
    layout->addWidget(m_serverNameLabel);
    
    // Realmlist
    m_realmlistLabel = new QLabel("Realmlist: sylvania-servergame.com", this);
    m_realmlistLabel->setStyleSheet("color: #7ec8e3; font-size: 12px; border: none;");
    layout->addWidget(m_realmlistLabel);
    
    layout->addSpacing(15);
    
    // Buttons row 1: JOUER + TÉLÉCHARGER + HD
    QHBoxLayout* buttonsRow1 = new QHBoxLayout();
    buttonsRow1->setSpacing(10);
    
    m_playButton = createStyledButton(tr("JOUER"), "green");
    m_downloadButton = createStyledButton(tr("TÉLÉCHARGER"), "gold_outline");
    m_hdButton = createStyledButton(tr("HD"), "blue");
    
    buttonsRow1->addWidget(m_playButton, 2);
    buttonsRow1->addWidget(m_downloadButton, 1);
    buttonsRow1->addWidget(m_hdButton, 1);
    layout->addLayout(buttonsRow1);
    
    // Buttons row 2: RÉGLAGES + NOTES + QUITTER
    QHBoxLayout* buttonsRow2 = new QHBoxLayout();
    buttonsRow2->setSpacing(10);
    
    m_settingsButton = createStyledButton(tr("RÉGLAGES"), "brown");
    m_notesButton = createStyledButton(tr("NOTES"), "brown");
    m_quitButton = createStyledButton(tr("QUITTER"), "pink");
    
    buttonsRow2->addWidget(m_settingsButton);
    buttonsRow2->addWidget(m_notesButton);
    buttonsRow2->addWidget(m_quitButton);
    layout->addLayout(buttonsRow2);
    
    // Buttons row 3: ADDONS
    QHBoxLayout* buttonsRow3 = new QHBoxLayout();
    buttonsRow3->setSpacing(10);
    
    m_addonsButton = createStyledButton(tr("Liste des Addons"), "teal");
    buttonsRow3->addWidget(m_addonsButton);
    layout->addLayout(buttonsRow3);
    
    layout->addStretch();
    
    return m_serverPanel;
}

QWidget* MainWindow::createStatsPanel() {
    m_statsPanel = new QFrame(this);
    m_statsPanel->setStyleSheet(R"(
        QFrame {
            background-color: rgba(0, 0, 0, 150);
            border: 2px solid #5a4a2d;
            border-radius: 10px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(m_statsPanel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(12);
    
    // Title
    m_statsTitleLabel = new QLabel(tr("Statistiques de Jeu"), this);
    m_statsTitleLabel->setAlignment(Qt::AlignCenter);
    m_statsTitleLabel->setStyleSheet("color: #d4af37; font-size: 18px; font-weight: bold; border: none;");
    layout->addWidget(m_statsTitleLabel);
    
    // Play time
    m_playTimeLabel = new QLabel(tr("Temps de jeu: 0h 0min"), this);
    m_playTimeLabel->setAlignment(Qt::AlignCenter);
    m_playTimeLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(80, 60, 30, 200);
            color: #ffffff;
            font-size: 13px;
            padding: 12px;
            border: 2px solid #d4af37;
            border-radius: 8px;
        }
    )");
    layout->addWidget(m_playTimeLabel);
    
    // HD Patch Progress (Hidden by default)
    m_hdStatusLabel = new QLabel("", this);
    m_hdStatusLabel->setStyleSheet("color: #7ec8e3; font-size: 11px; margin-top: 5px;");
    m_hdStatusLabel->setAlignment(Qt::AlignCenter);
    m_hdStatusLabel->hide();
    layout->addWidget(m_hdStatusLabel);
    
    m_hdProgressBar = new QProgressBar(this);
    m_hdProgressBar->setRange(0, 100);
    m_hdProgressBar->setValue(0);
    m_hdProgressBar->setTextVisible(false);
    m_hdProgressBar->setFixedHeight(10);
    m_hdProgressBar->setStyleSheet(R"(
        QProgressBar {
            background-color: #2a2a4e;
            border: 1px solid #3a3a5e;
            border-radius: 5px;
        }
        QProgressBar::chunk {
            background-color: #3498db;
            border-radius: 4px;
        }
    )");
    m_hdProgressBar->hide();
    layout->addWidget(m_hdProgressBar);
    
    layout->addStretch();
    
    // Launch count
    m_launchCountLabel = new QLabel("Lancements: 0", this);
    m_launchCountLabel->setAlignment(Qt::AlignCenter);
    m_launchCountLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(80, 60, 30, 200);
            color: #ffffff;
            font-size: 13px;
            padding: 12px;
            border: 2px solid #d4af37;
            border-radius: 8px;
        }
    )");
    layout->addWidget(m_launchCountLabel);
    
    // Last session
    m_lastSessionLabel = new QLabel("Dernière session: --", this);
    m_lastSessionLabel->setAlignment(Qt::AlignCenter);
    m_lastSessionLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(80, 60, 30, 200);
            color: #ffffff;
            font-size: 13px;
            padding: 12px;
            border: 2px solid #d4af37;
            border-radius: 8px;
        }
    )");
    layout->addWidget(m_lastSessionLabel);
    
    layout->addStretch();
    
    // Change server button
    m_changeServerButton = createStyledButton(tr("Changer de Serveur"), "teal");
    layout->addWidget(m_changeServerButton);
    
    // Lang button
    m_langButton = createStyledButton("FR / EN", "gold_outline");
    m_langButton->setMinimumHeight(30);
    m_langButton->setStyleSheet(m_langButton->styleSheet() + " QPushButton { font-size: 10px; padding: 5px 10px; }");
    layout->addWidget(m_langButton);
    
    return m_statsPanel;
}

QPushButton* MainWindow::createStyledButton(const QString& text, const QString& style) {
    QPushButton* btn = new QPushButton(text, this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(38);
    
    QString styleSheet;
    
    if (style == "green") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #4a7c3f, stop:0.5 #3a6a2f, stop:1 #2a5a1f);
                color: #ffffff;
                border: 2px solid #5a8c4f;
                border-radius: 8px;
                padding: 10px 25px;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #5a8c4f, stop:0.5 #4a7c3f, stop:1 #3a6a2f);
            }
            QPushButton:pressed {
                background-color: #2a5a1f;
            }
        )";
    } else if (style == "gold_outline") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #4a3a20, stop:0.5 #3a2a15, stop:1 #2a1a0a);
                color: #d4af37;
                border: 2px solid #d4af37;
                border-radius: 8px;
                padding: 10px 20px;
                font-size: 12px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #5a4a30, stop:0.5 #4a3a25, stop:1 #3a2a1a);
            }
            QPushButton:pressed {
                background-color: #2a1a0a;
            }
        )";
    } else if (style == "brown") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #5a4a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
                color: #d4af37;
                border: 2px solid #6a5a4d;
                border-radius: 8px;
                padding: 8px 15px;
                font-size: 11px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #6a5a4d, stop:0.5 #5a4a3d, stop:1 #4a3a2d);
            }
        )";
    } else if (style == "pink") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #8a4a5a, stop:0.5 #7a3a4a, stop:1 #6a2a3a);
                color: #ffffff;
                border: 2px solid #9a5a6a;
                border-radius: 8px;
                padding: 8px 15px;
                font-size: 11px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #9a5a6a, stop:0.5 #8a4a5a, stop:1 #7a3a4a);
            }
        )";
    } else if (style == "teal") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #7a6a3a, stop:0.5 #5a4a2a, stop:1 #3a3a1a);
                color: #ffffff;
                border: 2px solid #d4af37;
                border-radius: 8px;
                padding: 10px 20px;
                font-size: 12px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #8a7a4a, stop:0.5 #6a5a3a, stop:1 #4a4a2a);
            }
        )";
    } else if (style == "blue") {
        styleSheet = R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #2a6dd4, stop:0.5 #1e5ab8, stop:1 #15479a);
                color: #ffffff;
                border: 2px solid #3a7de4;
                border-radius: 8px;
                padding: 10px 20px;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #3a7de4, stop:0.5 #2a6dd4, stop:1 #1e5ab8);
            }
            QPushButton:pressed {
                background-color: #15479a;
            }
        )";
    }
    
    btn->setStyleSheet(styleSheet);
    return btn;
}

void MainWindow::connectSignals() {
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadButtonClicked);
    connect(m_hdButton, &QPushButton::clicked, this, &MainWindow::onHdButtonClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
    connect(m_notesButton, &QPushButton::clicked, this, &MainWindow::onNotesButtonClicked);
    connect(m_addonsButton, &QPushButton::clicked, this, &MainWindow::onAddonsButtonClicked);
    connect(m_quitButton, &QPushButton::clicked, this, &MainWindow::onQuitButtonClicked);
    connect(m_changeServerButton, &QPushButton::clicked, this, &MainWindow::onServersButtonClicked);
    connect(m_langButton, &QPushButton::clicked, this, &MainWindow::onLangButtonClicked);
}

void MainWindow::updateServerInfo() {
    auto entries = m_config->getRealmlistEntries();
    int activeIndex = m_config->getActiveRealmlistIndex();
    
    if (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) {
        m_serverNameLabel->setText(tr("Serveur: %1").arg(entries[activeIndex].name));
        m_realmlistLabel->setText(tr("Realmlist: %1").arg(entries[activeIndex].address.replace("set realmlist ", "")));
    }
}

void MainWindow::updateStats() {
    // Format play time
    int hours = m_totalPlayTime / 3600;
    int minutes = (m_totalPlayTime % 3600) / 60;
    m_playTimeLabel->setText(tr("Temps de jeu: %1h %2min").arg(hours).arg(minutes));
    
    // Launch count
    m_launchCountLabel->setText(tr("Lancements: %1").arg(m_launchCount));
    
    // Last session
    if (m_lastSession.isValid()) {
        m_lastSessionLabel->setText(tr("Dernière session: ") + m_lastSession.toString(tr("dd/MM/yyyy hh:mm")));
    } else {
        m_lastSessionLabel->setText(tr("Dernière session: --"));
    }
}

void MainWindow::checkWowInstalled() {
    QString wowPath = m_config->getWowPath();
    
    if (wowPath.isEmpty()) {
        m_playButton->setEnabled(false);
        m_statusLabel->setText(tr("Client WoW non configuré - Cliquez sur RÉGLAGES ou TÉLÉCHARGER"));
        return;
    }
    
    QString exePath = wowPath + "/Wow.exe";
    if (!QFile::exists(exePath)) {
        exePath = wowPath + "/WoW.exe";
        if (!QFile::exists(exePath)) {
            exePath = wowPath + "/wow.exe";
        }
    }

    if (!QFile::exists(exePath)) {
        m_playButton->setEnabled(false);
        m_statusLabel->setText(tr("Client non trouvé - Cliquez sur TÉLÉCHARGER pour installer WoW ici"));
        return;
    }
    
    m_playButton->setEnabled(true);
    
    auto entries = m_config->getRealmlistEntries();
    int activeIndex = m_config->getActiveRealmlistIndex();
    QString serverName = (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) 
        ? entries[activeIndex].name : "Sylvania";
    
    m_statusLabel->setText(tr("World of Warcraft 3.3.5 est installé. Prêt à jouer sur %1!").arg(serverName));
    
    updateStats();
}

void MainWindow::onPlayButtonClicked() {
    m_soundManager->play("play");
    playGame();
}

void MainWindow::playGame() {
    QString wowPath = m_config->getWowPath();
    QString exePath = wowPath + "/Wow.exe";
    if (!QFile::exists(exePath)) {
        exePath = wowPath + "/WoW.exe";
        if (!QFile::exists(exePath)) {
            exePath = wowPath + "/wow.exe";
        }
    }
    
    if (!QFile::exists(exePath)) {
        QMessageBox::critical(this, tr("Erreur"), 
            tr("Wow.exe non trouvé!\nVeuillez vérifier le chemin d'installation."));
        return;
    }
    
    // Update realmlist before launching
    auto entries = m_config->getRealmlistEntries();
    int activeIndex = m_config->getActiveRealmlistIndex();
    if (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) {
        m_config->updateRealmlist(entries[activeIndex].address);
    }
    
    // Update stats
    m_launchCount++;
    m_lastSession = QDateTime::currentDateTime();
    saveStats();
    updateStats();
    
    // Launch WoW
    bool started = false;

#ifdef Q_OS_LINUX
    // On Linux, launch via WineManager
    WineManager wineManager;
    if (!wineManager.isReady()) {
        m_statusLabel->setText(tr("Configuration du moteur de jeu (Wine)..."));
        QCoreApplication::processEvents();
        
        // Connect progress signals
        connect(&wineManager, &WineManager::downloadProgress, this, [this](int percent, const QString& status) {
            m_statusLabel->setText(status);
            QCoreApplication::processEvents();
        });
        connect(&wineManager, &WineManager::error, this, [this](const QString& message) {
            QMessageBox::warning(this, tr("Erreur Wine"), 
                tr("Impossible de configurer Wine: %1").arg(message));
        });
        
        wineManager.downloadAndInstall();
        
        if (!wineManager.isReady()) {
            return; // Installation failed
        }
    }
    started = wineManager.launchExe(exePath, wowPath);
#else
    started = QProcess::startDetached(exePath, {}, wowPath);
#endif
    
    if (started) {
        m_statusLabel->setText(tr("World of Warcraft lancé! Bon jeu!"));
    } else {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible de lancer World of Warcraft."));
    }
}

void MainWindow::onHdButtonClicked() {
    m_soundManager->play("button");
    
    QString wowPath = m_config->getWowPath();
    bool wowExists = false;
    if (!wowPath.isEmpty()) {
        wowExists = QFile::exists(wowPath + "/Wow.exe") || 
                    QFile::exists(wowPath + "/WoW.exe") || 
                    QFile::exists(wowPath + "/wow.exe");
    }

    if (!wowExists) {
        QMessageBox::warning(this, tr("Patch HD"), 
            tr("Vous devez d'abord télécharger ou configurer l'emplacement du client World of Warcraft "
               "avant d'installer le Patch HD."));
        return;
    }

    if (m_hdPatchManager && m_hdPatchManager->isInstalling()) {
        QMessageBox::information(this, tr("Patch HD"), tr("L'installation du Patch HD est déjà en cours."));
        return;
    }

    // Détection si déjà installé
    if (HdPatchManager::isInstalled(wowPath)) {
        auto result = QMessageBox::question(this, tr("Patch HD Détecté"), 
            tr("Le Patch HD semble déjà installé sur votre client.\n\n"
               "Voulez-vous tout de même le réinstaller ou forcer une mise à jour ?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (result == QMessageBox::No) {
            return;
        }
    }

    // Initialize manager if needed
    if (!m_hdPatchManager) {
        m_hdPatchManager = std::make_unique<HdPatchManager>(wowPath, this);
        
        connect(m_hdPatchManager.get(), &HdPatchManager::progressChanged, this, [this](int progress, const QString& status) {
            m_hdProgressBar->show();
            m_hdStatusLabel->show();
            if (progress < 0) {
                m_hdProgressBar->setRange(0, 0); // indeterminate / busy
            } else {
                m_hdProgressBar->setRange(0, 100);
                m_hdProgressBar->setValue(progress);
            }
            m_hdStatusLabel->setText(status);
        });
        
        connect(m_hdPatchManager.get(), &HdPatchManager::finished, this, [this](bool success, const QString& message) {
            m_hdProgressBar->hide();
            m_hdStatusLabel->hide();
            
            if (success) {
                QMessageBox::information(this, tr("Patch HD"), tr("Patch HD installé avec succès."));
            } else {
                QMessageBox::warning(this, tr("Patch HD"), tr("Erreur lors de l'installation : ") + message);
            }
            
            // Re-enable buttons
            m_playButton->setEnabled(true);
            m_downloadButton->setEnabled(true);
            m_hdButton->setEnabled(true);
        });
    }

    // Disable buttons during install
    m_playButton->setEnabled(false);
    m_downloadButton->setEnabled(false);
    m_hdButton->setEnabled(false);
    
    m_hdStatusLabel->setText(tr("Préparation de l'installation..."));
    m_hdStatusLabel->show();
    m_hdProgressBar->setValue(0);
    m_hdProgressBar->show();

    m_hdPatchManager->startInstallation();
}

void MainWindow::onDownloadButtonClicked() {
    m_soundManager->play("button");
    
    QString dir = QFileDialog::getExistingDirectory(this, 
        tr("Choisir l'emplacement d'installation"),
        QDir::homePath());
    
    if (dir.isEmpty()) {
        return;
    }
    
    m_downloadDialog = new DownloadDialog(this, dir);
    m_downloadDialog->setLanguage(m_config->getLanguage());
    connect(m_downloadDialog, &DownloadDialog::downloadFinished,
            this, &MainWindow::onDownloadComplete);
    m_downloadDialog->exec();
    m_downloadDialog->deleteLater();
    m_downloadDialog = nullptr;
}

void MainWindow::onDownloadComplete(bool success, const QString& message) {
    if (success) {
        QMessageBox::information(this, tr("Téléchargement terminé"), message);
        if (m_downloadDialog) {
            QString downloadPath = m_downloadDialog->getDestination();
            
            // Search recursively for Wow.exe (to handle subfolders in ZIP)
            QDirIterator it(downloadPath, QStringList() << "Wow.exe" << "WoW.exe" << "wow.exe", QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) {
                QString foundExe = it.next();
                QString finalPath = QFileInfo(foundExe).absolutePath();
                m_config->setWowPath(finalPath);
                checkWowInstalled();
            }
        }
    } else {
        QMessageBox::warning(this, tr("Erreur de téléchargement"), message);
    }
}

void MainWindow::onSettingsButtonClicked() {
    m_soundManager->play("button");
    
    SettingsDialog* dialog = new SettingsDialog(m_config.get(), m_soundManager.get(), this);
    connect(dialog, &SettingsDialog::settingsChanged, this, &MainWindow::checkWowInstalled);
    connect(dialog, &SettingsDialog::backgroundChanged, this, &MainWindow::applyTheme);
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::browseWowDirectory() {
    // Kept for compatibility, but now handled by SettingsDialog
    onSettingsButtonClicked();
}

void MainWindow::onNotesButtonClicked() {
    m_soundManager->play("button");
    
    if (!m_notesWindow) {
        m_notesWindow = new NotesWindow(m_notesManager.get(), this);
    }
    
    m_notesWindow->show();
    m_notesWindow->raise();
    m_notesWindow->activateWindow();
}

void MainWindow::onServersButtonClicked() {
    m_soundManager->play("button");
    
    if (!m_realmlistWindow) {
        m_realmlistWindow = new RealmlistWindow(m_config.get(), this);
        connect(m_realmlistWindow, &RealmlistWindow::realmlistChanged,
                this, &MainWindow::updateServerInfo);
        connect(m_realmlistWindow, &RealmlistWindow::realmlistChanged,
                this, &MainWindow::checkWowInstalled);
    }
    
    m_realmlistWindow->show();
    m_realmlistWindow->raise();
    m_realmlistWindow->activateWindow();
}

void MainWindow::onQuitButtonClicked() {
    m_soundManager->play("button");
    close();
}

void MainWindow::onAddonsButtonClicked() {
    m_soundManager->play("button");
    
    if (!m_addonsWindow) {
        m_addonsWindow = new AddonsWindow(m_config.get(), this);
    }
    
    m_addonsWindow->show();
    m_addonsWindow->raise();
    m_addonsWindow->activateWindow();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Stop tracking if WoW was running
    if (m_wowRunning && m_sessionStartTime.isValid()) {
        qint64 sessionTime = m_sessionStartTime.secsTo(QDateTime::currentDateTime());
        m_totalPlayTime += sessionTime;
    }
    
    if (m_notesWindow) {
        m_notesWindow->close();
    }
    if (m_addonsWindow) {
        m_addonsWindow->close();
    }
    if (m_realmlistWindow) {
        m_realmlistWindow->close();
    }
    
    m_config->save();
    saveStats();
    
    QMainWindow::closeEvent(event);
}

void MainWindow::checkWowProcess() {
#ifdef Q_OS_WIN
    // Query tasklist ASYNCHRONOUSLY. The previous waitForFinished(3000) ran on
    // the UI thread every 5 s and could freeze the launcher for up to 3 s each
    // tick. Skip this tick if a previous check is still running.
    if (m_wowCheckProcess) return;

    m_wowCheckProcess = new QProcess(this);
    connect(m_wowCheckProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        const bool running = QString::fromLocal8Bit(m_wowCheckProcess->readAllStandardOutput())
                                 .contains("Wow.exe", Qt::CaseInsensitive);
        m_wowCheckProcess->deleteLater();
        m_wowCheckProcess = nullptr;
        handleWowRunningState(running);
    });
    connect(m_wowCheckProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_wowCheckProcess->deleteLater();
        m_wowCheckProcess = nullptr;
    });
    m_wowCheckProcess->start("tasklist",
        QStringList() << "/FI" << "IMAGENAME eq Wow.exe" << "/NH");
#elif defined(Q_OS_LINUX)
    // On Linux, scan /proc for Wine processes running Wow.exe. This is a fast,
    // local, synchronous scan so we can report the result immediately.
    WineManager wineManager;
    handleWowRunningState(wineManager.isProcessRunning("Wow.exe"));
#endif
}

void MainWindow::handleWowRunningState(bool running) {
    if (running && !m_wowRunning) {
        // WoW just started
        m_wowRunning = true;
        m_sessionStartTime = QDateTime::currentDateTime();
        m_statusLabel->setText(tr("World of Warcraft en cours d'exécution..."));
    }
    else if (!running && m_wowRunning) {
        // WoW just stopped
        m_wowRunning = false;
        
        if (m_sessionStartTime.isValid()) {
            qint64 sessionTime = m_sessionStartTime.secsTo(QDateTime::currentDateTime());
            m_totalPlayTime += sessionTime;
            saveStats();
            updateStats();
            
            int mins = sessionTime / 60;
            m_statusLabel->setText(tr("Session terminée! Temps joué: %1 min").arg(mins));
        }
        
        m_sessionStartTime = QDateTime();
    }
    else if (running && m_wowRunning) {
        // Update live play time display
        if (m_sessionStartTime.isValid()) {
            qint64 sessionTime = m_sessionStartTime.secsTo(QDateTime::currentDateTime());
            qint64 totalWithSession = m_totalPlayTime + sessionTime;
            int hours = totalWithSession / 3600;
            int mins = (totalWithSession % 3600) / 60;
            m_playTimeLabel->setText(tr("Temps de jeu: %1h %2min").arg(hours).arg(mins));
        }
    }
}

void MainWindow::onLangButtonClicked() {
    m_soundManager->play("button");
    QString currentLang = m_config->getLanguage();
    QString newLang = (currentLang == "fr") ? "en" : "fr";
    changeLanguage(newLang, false); // false means NOT initial, so we DO check for enUS
}

void MainWindow::changeLanguage(const QString& lang, bool initial) {
    m_config->setLanguage(lang);
    m_config->save();
    
    // Load Qt translator
    qApp->removeTranslator(&m_translator);
    if (lang == "en") {
        if (m_translator.load("SylvaniaLauncher_en", PathUtils::getAssetsDir() + "/translations")) {
            qApp->installTranslator(&m_translator);
        }
    }
    
    if (lang == "en") {
        m_langButton->setText("Lang: EN");
    } else {
        m_langButton->setText("Lang: FR");
    }
    
    retranslateUi();
    
    // Update Config.wtf locale
    QString wtfPath = m_config->getWowPath() + "/WTF/Config.wtf";
    if (QFile::exists(wtfPath)) {
        QFile wtfFile(wtfPath);
        if (wtfFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QString content = wtfFile.readAll();
            if (lang == "en") {
                if (content.contains("SET locale")) {
                    content.replace("SET locale \"frFR\"", "SET locale \"enUS\"");
                } else {
                    content += "\nSET locale \"enUS\"\n";
                }
            } else {
                content.replace("SET locale \"enUS\"", "SET locale \"frFR\"");
            }
            wtfFile.resize(0);
            wtfFile.write(content.toUtf8());
            wtfFile.close();
        }
    }
}

void MainWindow::retranslateUi() {
    m_playButton->setText(tr("JOUER"));
    m_downloadButton->setText(tr("TÉLÉCHARGER"));
    m_hdButton->setText(tr("HD"));
    m_settingsButton->setText(tr("RÉGLAGES"));
    m_notesButton->setText(tr("NOTES"));
    m_quitButton->setText(tr("QUITTER"));
    m_addonsButton->setText(tr("Liste des Addons"));
    m_changeServerButton->setText(tr("Changer de Serveur"));
    
    if (m_serverTitleLabel) m_serverTitleLabel->setText(tr("Serveur"));
    if (m_statsTitleLabel) m_statsTitleLabel->setText(tr("Statistiques de Jeu"));
    // Note: m_serverNameLabel is updated via updateServerInfo below
    
    if (m_footerLabel) m_footerLabel->setText(tr("© 2025 Sylvania Launcher v2.8 - World of Warcraft 3.3.5 - Linux Version"));
    
    checkWowInstalled();
    updateStats();
    updateServerInfo();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}
