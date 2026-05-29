#include "AddonsWindow.h"
#include "ConfigManager.h"
#include "Constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QFrame>

AddonsWindow::AddonsWindow(ConfigManager* config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
{
    setWindowTitle(tr("Gestionnaire d'Addons - Sylvania"));
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
        {"ArkInventory", tr("Gestion d'inventaire avancée avec catégories automatiques."), "ArkInventory.zip", "ArkInventory"},
        {"AtlasLoot", tr("Base de données complète des butins de donjons et raids."), "AtlasLoot.zip", "AtlasLoot"},
        {"Auctionator", tr("Améliore l'interface de l'Hôtel des Ventes."), "Auctionator.zip", "Auctionator"},
        {"Bagnon", tr("Regroupe tous vos sacs en une seule fenêtre épurée."), "Bagnon.zip", "Bagnon"},
        {"Bartender4", tr("Personnalisation totale de vos barres d'actions."), "Bartender4.zip", "Bartender4"},
        {"Deadly Boss Mods (DBM)", tr("Alertes et chronomètres indispensables pour les raids."), "DBM.zip", "DBM-Core"},
        {"GatherMate2", tr("Mémorise l'emplacement des plantes et minerais sur la carte."), "GatherMate2.zip", "GatherMate2"},
        {"GearScore", tr("Évalue le niveau d'équipement global des joueurs."), "GearScore.zip", "GearScore"},
        {"HandyNotes", tr("Ajoute des notes personnalisées et des points d'intérêt."), "HandyNotes.zip", "HandyNotes"},
        {"Healium", tr("Interface de soin avec boutons d'accès rapide aux sorts."), "Healium.zip", "Healium"},
        {"Immersion", tr("Modernise l'affichage des textes de quêtes et dialogues."), "Immersion.zip", "Immersion"},
        {"Mapster", tr("Améliore la visibilité et les fonctions de la carte du monde."), "Mapster.zip", "Mapster"},
        {"NPCScan", tr("Détecte les créatures rares à proximité et vous alerte avec un signal sonore."), "NPCScan.zip", "_NPCScan"},
        {"NPCScan-Overlay", tr("Affiche les trajets et zones d'apparition des créatures rares sur la carte."), "NPCScan-Overlay.zip", "_NPCScan-Overlay"},
        {"Postal", tr("Améliore radicalement la gestion de la boîte aux lettres."), "Postal.zip", "Postal"},
        {"Prat", tr("Personnalisation poussée de la fenêtre de discussion."), "Prat-3.0.zip", "Prat-3.0"},
        {"Quartz", tr("Barres d'incantation modulaires et ultra-précises."), "Quartz.zip", "Quartz"},
        {"QuestHelper", tr("Le guide classique pour optimiser vos itinéraires de quêtes."), "QuestHelper.zip", "QuestHelper"},
        {"Scrap", tr("Vente et réparation automatique chez les marchands."), "Scrap.zip", "Scrap"},
        {"SexyMap", tr("Donne un look moderne et personnalisable à votre mini-carte."), "SexyMap.zip", "SexyMap"},
        {"TomTom", tr("Navigation par coordonnées GPS et flèche directionnelle."), "TomTom.zip", "TomTom"},
        {"WIM", tr("Gère vos chuchotements dans des fenêtres séparées."), "WIM.zip", "WIM"},
        {"WeakAuras", tr("L'outil ultime pour créer vos propres alertes visuelles."), "WeakAuras.zip", "WeakAuras2"},
        {"XPerl", tr("Refonte visuelle des portraits de joueurs (Style Classique)."), "XPerl.zip", "XPerl"},
        {"Details!", tr("Le compteur de combat le plus précis et détaillé."), "details.zip", "Details"},
        {"ElvUI", tr("Une interface utilisateur complète, moderne et minimaliste."), "elvui.zip", "ElvUI"},
        {"Shadowed Unit Frames", tr("Portraits de joueurs épurés et hautement configurables."), "shadowedunitframes.zip", "ShadowedUnitFrames"},
        {"TotalRP3", tr("L'addon de référence pour l'immersion et le jeu de rôle."), "totalRP3.zip", "TotalRP3"},
        {"VuhDo", tr("Interface de raid et de soin extrêmement personnalisable."), "vuhdo.zip", "VuhDo"}
    };
}

void AddonsWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header
    QLabel* titleLabel = new QLabel(tr("Addons Recommandés"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #d4af37; font-size: 20px; font-weight: bold; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel);
    
    QLabel* infoLabel = new QLabel(tr("Sélectionnez les addons que vous souhaitez installer. Ils seront automatiquement placés dans le dossier de votre jeu."), this);
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
    
    QString wowPath = m_config->getWowPath();
    QString addonsDir = wowPath + "/Interface/AddOns";

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
        
        // Check if installed
        bool installed = isAddonInstalled(addon);

        // Install Button
        QPushButton* installBtn = new QPushButton(installed ? tr("Réinstaller") : tr("Installer"), itemFrame);
        installBtn->setCursor(Qt::PointingHandCursor);
        installBtn->setFixedSize(100, 35);
        
        updateButtonStyle(installBtn, installed);
        
        if (installed) {
            statusLabel->setText(tr("Déjà présent"));
            statusLabel->setStyleSheet("color: #7ec8e3; font-size: 11px;");
            statusLabel->show();
        }
        
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
    QPushButton* closeBtn = new QPushButton(tr("Fermer"), this);
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

bool AddonsWindow::isAddonInstalled(const AddonInfo& addon) const {
    QString wowPath = m_config->getWowPath();
    if (wowPath.isEmpty() || addon.folderName.isEmpty()) return false;
    
    QString addonsDir = wowPath + "/Interface/AddOns";
    QDir dir(addonsDir + "/" + addon.folderName);
    return dir.exists();
}

void AddonsWindow::updateButtonStyle(QPushButton* btn, bool installed) {
    if (installed) {
        btn->setText("Réinstaller");
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2a4a1f, stop:0.5 #1a3a1f, stop:1 #0a2a1f);
                color: #aaaaaa; border: 1px solid #3a5c2f; border-radius: 5px; font-weight: bold;
            }
            QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a6a2f, stop:0.5 #2a5a1f, stop:1 #1a4a1f); color: #ffffff; }
        )");
    } else {
        btn->setText("Installer");
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a7c3f, stop:0.5 #3a6a2f, stop:1 #2a5a1f);
                color: #ffffff; border: 1px solid #5a8c4f; border-radius: 5px; font-weight: bold;
            }
            QPushButton:hover { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5a8c4f, stop:0.5 #4a7c3f, stop:1 #3a6a2f); }
            QPushButton:disabled { background-color: #555555; color: #888888; border: 1px solid #444444; }
        )");
    }
}

void AddonsWindow::onInstallClicked(const QString& fileName, QPushButton* installBtn, QProgressBar* progressBar, QLabel* statusLabel) {
    // C2: whitelist addon filenames to prevent path traversal / parameter injection.
    static const QRegularExpression kValidAddonName(QStringLiteral("^[A-Za-z0-9_\\-.]+\\.zip$"));
    if (!kValidAddonName.match(fileName).hasMatch()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Nom de fichier d'addon invalide."));
        return;
    }

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

    QUrl url(QString::fromUtf8(SylvaniaConstants::kAddonDownloadEndpoint));
    QUrlQuery query;
    query.addQueryItem("file", fileName);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    
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

    // C1: paths flow via env vars instead of being interpolated into the
    // PowerShell command string -> no shell-injection surface.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SYL_SRC", QDir::toNativeSeparators(zipPath));
    env.insert("SYL_DST", QDir::toNativeSeparators(destination));
    process->setProcessEnvironment(env);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, zipPath, installBtn, progressBar, statusLabel, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            QFile::remove(zipPath); // Cleanup

            progressBar->setMaximum(100);
            progressBar->setValue(100);
            statusLabel->setText("Installé avec succès !");
            statusLabel->setStyleSheet("color: #55ff55; font-size: 11px;");

            installBtn->setEnabled(true);
            updateButtonStyle(installBtn, true);
        } else {
            progressBar->setMaximum(100);
            progressBar->setValue(0);
            statusLabel->setText("Erreur d'extraction.");
            statusLabel->setStyleSheet("color: #ff5555; font-size: 11px;");

            installBtn->setEnabled(true);
            installBtn->setText("Réessayer");
        }
    });

    process->start("powershell", SylvaniaConstants::extractArchiveArgs());
}
