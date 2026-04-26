#include "AddonsWindow.h"
#include "ConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
#include <QNetworkRequest>
#include <QFrame>

AddonsWindow::AddonsWindow(ConfigManager* config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
{
    setWindowTitle("Gestionnaire d'Addons - Sylvania");
    setFixedSize(600, 700);
    
    // Style the dialog
    setStyleSheet("QDialog { background-color: #1a1a2e; }");
    
    populateAddons();
    setupUi();
}

AddonsWindow::~AddonsWindow() {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void AddonsWindow::populateAddons() {
    m_addons = {
        {"ArkInventory", "Gestion d'inventaire avancée avec catégories automatiques.", "ArkInventory.zip"},
        {"AtlasLoot", "Base de données complète des butins de donjons et raids.", "AtlasLoot.zip"},
        {"Auctionator", "Améliore l'interface de l'Hôtel des Ventes.", "Auctionator.zip"},
        {"Bagnon", "Regroupe tous vos sacs en une seule fenêtre épurée.", "Bagnon.zip"},
        {"Bartender4", "Personnalisation totale de vos barres d'actions.", "Bartender4.zip"},
        {"Deadly Boss Mods (DBM)", "Alertes et chronomètres indispensables pour les raids.", "DBM.zip"},
        {"GatherMate2", "Mémorise l'emplacement des plantes et minerais sur la carte.", "GatherMate2.zip"},
        {"GearScore", "Évalue le niveau d'équipement global des joueurs.", "GearScore.zip"},
        {"HandyNotes", "Ajoute des notes personnalisées et des points d'intérêt.", "HandyNotes.zip"},
        {"Healium", "Interface de soin avec boutons d'accès rapide aux sorts.", "Healium.zip"},
        {"Immersion", "Modernise l'affichage des textes de quêtes et dialogues.", "Immersion.zip"},
        {"Mapster", "Améliore la visibilité et les fonctions de la carte du monde.", "Mapster.zip"},
        {"OneButtonHelper", "Assistant de rotation de combat pour optimiser votre DPS.", "OneButtonHelper.zip"},
        {"Postal", "Améliore radicalement la gestion de la boîte aux lettres.", "Postal.zip"},
        {"Prat", "Personnalisation poussée de la fenêtre de discussion.", "Prat-3.0.zip"},
        {"Quartz", "Barres d'incantation modulaires et ultra-précises.", "Quartz.zip"},
        {"QuestHelper", "Le guide classique pour optimiser vos itinéraires de quêtes.", "QuestHelper.zip"},
        {"Scrap", "Vente et réparation automatique chez les marchands.", "Scrap.zip"},
        {"SexyMap", "Donne un look moderne et personnalisable à votre mini-carte.", "SexyMap.zip"},
        {"TomTom", "Navigation par coordonnées GPS et flèche directionnelle.", "TomTom.zip"},
        {"WIM", "Gère vos chuchotements dans des fenêtres séparées.", "WIM.zip"},
        {"WeakAuras", "L'outil ultime pour créer vos propres alertes visuelles.", "WeakAuras.zip"},
        {"XPerl", "Refonte visuelle des portraits de joueurs (Style Classique).", "XPerl.zip"},
        {"Details!", "Le compteur de combat le plus précis et détaillé.", "details.zip"},
        {"ElvUI", "Une interface utilisateur complète, moderne et minimaliste.", "elvui.zip"},
        {"Shadowed Unit Frames", "Portraits de joueurs épurés et hautement configurables.", "shadowedunitframes.zip"},
        {"TotalRP3", "L'addon de référence pour l'immersion et le jeu de rôle.", "totalRP3.zip"},
        {"VuhDo", "Interface de raid et de soin extrêmement personnalisable.", "vuhdo.zip"}
    };
}

void AddonsWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header
    QLabel* titleLabel = new QLabel("Addons Recommandés", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #d4af37; font-size: 20px; font-weight: bold; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);
    
    QLabel* infoLabel = new QLabel("Sélectionnez les addons que vous souhaitez installer. Ils seront automatiquement placés dans le dossier de votre jeu.", this);
    infoLabel->setWordWrap(true);
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setStyleSheet("color: #aaaaaa; font-size: 13px; margin-bottom: 15px;");
    mainLayout->addWidget(infoLabel);
    
    // Scroll Area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: 1px solid #3a3a5e; border-radius: 5px; background-color: #151525; }");
    
    m_scrollWidget = new QWidget(m_scrollArea);
    m_scrollWidget->setStyleSheet("background-color: transparent;");
    m_addonsLayout = new QVBoxLayout(m_scrollWidget);
    m_addonsLayout->setSpacing(10);
    
    for (const auto& addon : m_addons) {
        QFrame* itemFrame = new QFrame(m_scrollWidget);
        itemFrame->setStyleSheet("QFrame { background-color: #2a2a4e; border-radius: 5px; } QFrame:hover { background-color: #32325c; }");
        
        QHBoxLayout* itemLayout = new QHBoxLayout(itemFrame);
        itemLayout->setContentsMargins(15, 15, 15, 15);
        
        // Info Layout
        QVBoxLayout* textLayout = new QVBoxLayout();
        QLabel* nameLabel = new QLabel(addon.name, itemFrame);
        nameLabel->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: bold;");
        
        QLabel* descLabel = new QLabel(addon.description, itemFrame);
        descLabel->setStyleSheet("color: #aaaaaa; font-size: 12px;");
        descLabel->setWordWrap(true);
        
        textLayout->addWidget(nameLabel);
        textLayout->addWidget(descLabel);
        
        // Progress Bar (Hidden by default)
        QProgressBar* progressBar = new QProgressBar(itemFrame);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setTextVisible(false);
        progressBar->setFixedHeight(5);
        progressBar->setStyleSheet(R"(
            QProgressBar { background-color: #1a1a2e; border: none; border-radius: 2px; }
            QProgressBar::chunk { background-color: #d4af37; border-radius: 2px; }
        )");
        progressBar->hide();
        textLayout->addWidget(progressBar);
        
        // Status Label (Hidden by default)
        QLabel* statusLabel = new QLabel("", itemFrame);
        statusLabel->setStyleSheet("color: #7ec8e3; font-size: 11px;");
        statusLabel->hide();
        textLayout->addWidget(statusLabel);
        
        itemLayout->addLayout(textLayout, 1);
        
        // Install Button
        QPushButton* installBtn = new QPushButton("Installer", itemFrame);
        installBtn->setCursor(Qt::PointingHandCursor);
        installBtn->setFixedSize(100, 35);
        installBtn->setStyleSheet(R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a7c3f, stop:0.5 #3a6a2f, stop:1 #2a5a1f);
                color: #ffffff; border: 1px solid #5a8c4f; border-radius: 5px; font-weight: bold;
            }
            QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5a8c4f, stop:0.5 #4a7c3f, stop:1 #3a6a2f); }
            QPushButton:disabled { background-color: #555555; color: #888888; border: 1px solid #444444; }
        )");
        
        connect(installBtn, &QPushButton::clicked, this, [this, addon, installBtn, progressBar, statusLabel]() {
            onInstallClicked(addon.downloadFileName, installBtn, progressBar, statusLabel);
        });
        
        itemLayout->addWidget(installBtn);
        m_addonsLayout->addWidget(itemFrame);
    }
    
    m_addonsLayout->addStretch();
    m_scrollArea->setWidget(m_scrollWidget);
    mainLayout->addWidget(m_scrollArea, 1);
    
    // Close Button
    QPushButton* closeBtn = new QPushButton("Fermer", this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(120, 40);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5a4a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
            color: #d4af37; border: 1px solid #6a5a4d; border-radius: 5px; font-weight: bold;
        }
        QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #6a5a4d, stop:0.5 #5a4a3d, stop:1 #4a3a2d); }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    bottomLayout->addWidget(closeBtn);
    bottomLayout->addStretch();
    
    mainLayout->addLayout(bottomLayout);
}

void AddonsWindow::onInstallClicked(const QString& fileName, QPushButton* installBtn, QProgressBar* progressBar, QLabel* statusLabel) {
    QString wowPath = m_config->getWowPath();
    if (wowPath.isEmpty() || !QFile::exists(wowPath + "/Wow.exe")) {
        QMessageBox::warning(this, "Erreur", "Veuillez d'abord configurer le chemin d'installation de World of Warcraft dans les réglages.");
        return;
    }
    
    if (m_currentReply) {
        QMessageBox::information(this, "Information", "Un téléchargement est déjà en cours. Veuillez patienter.");
        return;
    }
    
    QString addonsDir = wowPath + "/Interface/AddOns";
    QDir dir(addonsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_currentZipPath = addonsDir + "/" + fileName;
    m_currentDestination = addonsDir;
    m_currentInstallBtn = installBtn;
    m_currentProgressBar = progressBar;
    m_currentStatusLabel = statusLabel;
    
    // Create file
    QFile* file = new QFile(m_currentZipPath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Erreur", "Impossible de créer le fichier temporaire.");
        file->deleteLater();
        return;
    }
    
    // Setup UI
    installBtn->setEnabled(false);
    installBtn->setText("Patientez...");
    progressBar->setValue(0);
    progressBar->show();
    statusLabel->setText("Téléchargement...");
    statusLabel->show();
    
    QString urlStr = "https://sylvania-servergame.com/download_addon.php?file=" + fileName;
    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &AddonsWindow::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &AddonsWindow::onDownloadFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &AddonsWindow::onDownloadError);
    
    connect(m_currentReply, &QNetworkReply::readyRead, this, [this, file]() {
        if (file && file->isOpen()) {
            file->write(m_currentReply->readAll());
        }
    });
    
    // Store file pointer in the reply object for easy access
    m_currentReply->setProperty("file", QVariant::fromValue(file));
}

void AddonsWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0 && m_currentProgressBar) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_currentProgressBar->setValue(progress);
        if (m_currentStatusLabel) {
            m_currentStatusLabel->setText(QString("Téléchargement en cours... %1%").arg(progress));
        }
    }
}

void AddonsWindow::onDownloadError(QNetworkReply::NetworkError error) {
    Q_UNUSED(error);
    
    if (m_currentReply) {
        QFile* file = m_currentReply->property("file").value<QFile*>();
        if (file) {
            file->close();
            file->remove();
            file->deleteLater();
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    if (m_currentInstallBtn) {
        m_currentInstallBtn->setEnabled(true);
        m_currentInstallBtn->setText("Réessayer");
    }
    if (m_currentProgressBar) {
        m_currentProgressBar->hide();
    }
    if (m_currentStatusLabel) {
        m_currentStatusLabel->setText("Erreur réseau.");
        m_currentStatusLabel->setStyleSheet("color: #ff5555; font-size: 11px;");
    }
    
    QMessageBox::warning(this, "Erreur", "Erreur lors du téléchargement de l'addon.");
}

void AddonsWindow::onDownloadFinished() {
    if (!m_currentReply) return;
    
    if (m_currentReply->error() != QNetworkReply::NoError) {
        return; // Handled by onDownloadError
    }
    
    QFile* file = m_currentReply->property("file").value<QFile*>();
    if (file) {
        file->close();
        file->deleteLater();
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    
    // Start extraction
    extractZip(m_currentZipPath, m_currentDestination, m_currentInstallBtn, m_currentProgressBar, m_currentStatusLabel);
}

void AddonsWindow::extractZip(const QString& zipPath, const QString& destination, QPushButton* installBtn, QProgressBar* progressBar, QLabel* statusLabel) {
    statusLabel->setText("Extraction en cours...");
    progressBar->setMaximum(0); // Indeterminate mode
    
    QProcess* process = new QProcess(this);
    QString command = QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipPath).arg(destination);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [zipPath, installBtn, progressBar, statusLabel, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            QFile::remove(zipPath); // Cleanup
            
            progressBar->setMaximum(100);
            progressBar->setValue(100);
            statusLabel->setText("Installé avec succès !");
            statusLabel->setStyleSheet("color: #55ff55; font-size: 11px;");
            
            installBtn->setText("Installé");
            installBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: #2a5a1f; color: #aaaaaa; border: 1px solid #5a8c4f; border-radius: 5px; font-weight: bold;
                }
            )");
            // Keep button disabled to avoid reinstalling by mistake
        } else {
            progressBar->setMaximum(100);
            progressBar->setValue(0);
            statusLabel->setText("Erreur d'extraction.");
            statusLabel->setStyleSheet("color: #ff5555; font-size: 11px;");
            
            installBtn->setEnabled(true);
            installBtn->setText("Réessayer");
        }
    });
    
    process->start("powershell", QStringList() << "-Command" << command);
}
