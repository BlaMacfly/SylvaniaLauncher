#include "MainWindow.h"
#include "NotesWindow.h"
#include "RealmlistWindow.h"
#include "DownloadDialog.h"
#include "SettingsDialog.h"
#include "ConfigManager.h"
#include "SoundManager.h"
#include "NotesManager.h"
#include "PathUtils.h"

#include <QApplication>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QCloseEvent>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_config(std::make_unique<ConfigManager>(this))
    , m_soundManager(std::make_unique<SoundManager>(this))
    , m_notesManager(std::make_unique<NotesManager>(this))
{
    setWindowTitle("Sylvania Launcher - World of Warcraft 3.3.5");
    setFixedSize(920, 580);
    
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
    
    loadBackground();
    loadStats();
    setupUi();
    connectSignals();
    checkWowInstalled();
    updateServerInfo();
    
    // Setup play time tracking timer (checks every 5 seconds)
    m_playTimeTimer = new QTimer(this);
    connect(m_playTimeTimer, &QTimer::timeout, this, &MainWindow::checkWowProcess);
    m_playTimeTimer->start(5000);
}

MainWindow::~MainWindow() = default;

void MainWindow::loadBackground() {
    QString bgPath = PathUtils::getAssetsDir() + "/Background.jpg";
    if (QFile::exists(bgPath)) {
        m_backgroundImage.load(bgPath);
    }
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
        QPixmap scaledLogo = m_logoImage.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    m_statusLabel = new QLabel("Chargement...", this);
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
    QLabel* footerLabel = new QLabel("© 2025 Sylvania Launcher v2.3 - World of Warcraft 3.3.5", this);
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setStyleSheet("color: #d4af37; font-size: 11px;");
    mainLayout->addWidget(footerLabel);
}

QWidget* MainWindow::createServerPanel() {
    QFrame* panel = new QFrame(this);
    panel->setStyleSheet(R"(
        QFrame {
            background-color: rgba(0, 0, 0, 150);
            border: 2px solid #5a4a2d;
            border-radius: 10px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);
    
    // Title
    QLabel* titleLabel = new QLabel("Serveur", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #d4af37; font-size: 18px; font-weight: bold; border: none;");
    layout->addWidget(titleLabel);
    
    // Server name
    m_serverNameLabel = new QLabel("Serveur: The Kingdom of Sylvania 3.3.5", this);
    m_serverNameLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; border: none;");
    layout->addWidget(m_serverNameLabel);
    
    // Realmlist
    m_realmlistLabel = new QLabel("Realmlist: sylvania-servergame.com", this);
    m_realmlistLabel->setStyleSheet("color: #7ec8e3; font-size: 12px; border: none;");
    layout->addWidget(m_realmlistLabel);
    
    layout->addSpacing(15);
    
    // Buttons row 1: JOUER + TÉLÉCHARGER
    QHBoxLayout* buttonsRow1 = new QHBoxLayout();
    buttonsRow1->setSpacing(15);
    
    m_playButton = createStyledButton("JOUER", "green");
    m_downloadButton = createStyledButton("TÉLÉCHARGER", "gold_outline");
    
    buttonsRow1->addWidget(m_playButton);
    buttonsRow1->addWidget(m_downloadButton);
    layout->addLayout(buttonsRow1);
    
    // Buttons row 2: RÉGLAGES + NOTES + QUITTER
    QHBoxLayout* buttonsRow2 = new QHBoxLayout();
    buttonsRow2->setSpacing(10);
    
    m_settingsButton = createStyledButton("RÉGLAGES", "brown");
    m_notesButton = createStyledButton("NOTES", "brown");
    m_quitButton = createStyledButton("QUITTER", "pink");
    
    buttonsRow2->addWidget(m_settingsButton);
    buttonsRow2->addWidget(m_notesButton);
    buttonsRow2->addWidget(m_quitButton);
    layout->addLayout(buttonsRow2);
    
    layout->addStretch();
    
    return panel;
}

QWidget* MainWindow::createStatsPanel() {
    QFrame* panel = new QFrame(this);
    panel->setStyleSheet(R"(
        QFrame {
            background-color: rgba(0, 0, 0, 150);
            border: 2px solid #5a4a2d;
            border-radius: 10px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(12);
    
    // Title
    QLabel* titleLabel = new QLabel("Statistiques de Jeu", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #d4af37; font-size: 18px; font-weight: bold; border: none;");
    layout->addWidget(titleLabel);
    
    // Play time
    m_playTimeLabel = new QLabel("Temps de jeu: 0h 0min", this);
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
    m_changeServerButton = createStyledButton("Changer de Serveur", "teal");
    layout->addWidget(m_changeServerButton);
    
    return panel;
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
    }
    
    btn->setStyleSheet(styleSheet);
    return btn;
}

void MainWindow::connectSignals() {
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadButtonClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
    connect(m_notesButton, &QPushButton::clicked, this, &MainWindow::onNotesButtonClicked);
    connect(m_quitButton, &QPushButton::clicked, this, &MainWindow::onQuitButtonClicked);
    connect(m_changeServerButton, &QPushButton::clicked, this, &MainWindow::onServersButtonClicked);
}

void MainWindow::updateServerInfo() {
    auto entries = m_config->getRealmlistEntries();
    int activeIndex = m_config->getActiveRealmlistIndex();
    
    if (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) {
        m_serverNameLabel->setText("Serveur: " + entries[activeIndex].name);
        m_realmlistLabel->setText("Realmlist: " + entries[activeIndex].address.replace("set realmlist ", ""));
    }
}

void MainWindow::updateStats() {
    // Format play time
    int hours = m_totalPlayTime / 3600;
    int minutes = (m_totalPlayTime % 3600) / 60;
    m_playTimeLabel->setText(QString("Temps de jeu: %1h %2min").arg(hours).arg(minutes));
    
    // Launch count
    m_launchCountLabel->setText(QString("Lancements: %1").arg(m_launchCount));
    
    // Last session
    if (m_lastSession.isValid()) {
        m_lastSessionLabel->setText("Dernière session: " + m_lastSession.toString("dd/MM/yyyy à hh:mm"));
    } else {
        m_lastSessionLabel->setText("Dernière session: --");
    }
}

void MainWindow::checkWowInstalled() {
    QString wowPath = m_config->getWowPath();
    
    if (wowPath.isEmpty()) {
        m_playButton->setEnabled(false);
        m_statusLabel->setText("Client WoW non configuré - Cliquez sur RÉGLAGES ou TÉLÉCHARGER");
        return;
    }
    
    QString exePath = wowPath + "/Wow.exe";
    if (!QFile::exists(exePath)) {
        m_playButton->setEnabled(false);
        m_statusLabel->setText("Wow.exe non trouvé - Vérifiez le chemin du client");
        return;
    }
    
    m_playButton->setEnabled(true);
    
    auto entries = m_config->getRealmlistEntries();
    int activeIndex = m_config->getActiveRealmlistIndex();
    QString serverName = (activeIndex >= 0 && activeIndex < static_cast<int>(entries.size())) 
        ? entries[activeIndex].name : "Sylvania";
    
    m_statusLabel->setText("World of Warcraft 3.3.5 est installé. Prêt à jouer sur " + serverName + "!");
    
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
        QMessageBox::warning(this, "Erreur", 
            "Wow.exe non trouvé!\nVeuillez vérifier le chemin d'installation.");
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
    bool started = QProcess::startDetached(exePath, {}, wowPath);
    
    if (started) {
        m_statusLabel->setText("World of Warcraft lancé! Bon jeu!");
    } else {
        QMessageBox::warning(this, "Erreur", "Impossible de lancer World of Warcraft.");
    }
}

void MainWindow::onDownloadButtonClicked() {
    m_soundManager->play("button");
    
    QString dir = QFileDialog::getExistingDirectory(this, 
        "Choisir l'emplacement d'installation",
        QDir::homePath());
    
    if (dir.isEmpty()) {
        return;
    }
    
    m_downloadDialog = new DownloadDialog(this, dir);
    connect(m_downloadDialog, &DownloadDialog::downloadFinished,
            this, &MainWindow::onDownloadComplete);
    m_downloadDialog->exec();
    m_downloadDialog->deleteLater();
    m_downloadDialog = nullptr;
}

void MainWindow::onDownloadComplete(bool success, const QString& message) {
    if (success) {
        QMessageBox::information(this, "Téléchargement terminé", message);
        if (m_downloadDialog) {
            QString downloadPath = m_downloadDialog->getDestination();
            QString wowExe = downloadPath + "/Wow.exe";
            if (QFile::exists(wowExe)) {
                m_config->setWowPath(downloadPath);
                checkWowInstalled();
            }
        }
    } else {
        QMessageBox::warning(this, "Erreur de téléchargement", message);
    }
}

void MainWindow::onSettingsButtonClicked() {
    m_soundManager->play("button");
    
    SettingsDialog* dialog = new SettingsDialog(m_config.get(), m_soundManager.get(), this);
    connect(dialog, &SettingsDialog::settingsChanged, this, &MainWindow::checkWowInstalled);
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

void MainWindow::closeEvent(QCloseEvent* event) {
    // Stop tracking if WoW was running
    if (m_wowRunning && m_sessionStartTime.isValid()) {
        qint64 sessionTime = m_sessionStartTime.secsTo(QDateTime::currentDateTime());
        m_totalPlayTime += sessionTime;
    }
    
    if (m_notesWindow) {
        m_notesWindow->close();
    }
    if (m_realmlistWindow) {
        m_realmlistWindow->close();
    }
    
    m_config->save();
    saveStats();
    
    QMainWindow::closeEvent(event);
}

bool MainWindow::isWowRunning() {
#ifdef Q_OS_WIN
    // Use Windows API to check if Wow.exe is running
    QProcess process;
    process.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq Wow.exe" << "/NH");
    process.waitForFinished(3000);
    QString output = process.readAllStandardOutput();
    return output.contains("Wow.exe", Qt::CaseInsensitive);
#else
    return false;
#endif
}

void MainWindow::checkWowProcess() {
    bool running = isWowRunning();
    
    if (running && !m_wowRunning) {
        // WoW just started
        m_wowRunning = true;
        m_sessionStartTime = QDateTime::currentDateTime();
        m_statusLabel->setText("World of Warcraft en cours d'exécution...");
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
            m_statusLabel->setText(QString("Session terminée! Temps joué: %1 min").arg(mins));
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
            m_playTimeLabel->setText(QString("Temps de jeu: %1h %2min").arg(hours).arg(mins));
        }
    }
}

