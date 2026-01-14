#include "SettingsDialog.h"
#include "ConfigManager.h"
#include "SoundManager.h"
#include "PathUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>

SettingsDialog::SettingsDialog(ConfigManager* config, SoundManager* soundManager, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_soundManager(soundManager)
{
    setWindowTitle("Réglages");
    setModal(true);
    setFixedSize(550, 320);
    
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Installation de WoW section
    QLabel* installTitle = new QLabel("Installation de WoW", this);
    installTitle->setAlignment(Qt::AlignCenter);
    installTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(installTitle);
    
    // Path row
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(10);
    
    QLabel* pathLabel = new QLabel("Dossier d'installation:", this);
    pathLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
    pathLabel->setMinimumWidth(130);
    
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a2a;
            color: #d4af37;
            border: 1px solid #5a4a2d;
            border-radius: 5px;
            padding: 8px;
            font-size: 11px;
        }
    )");
    
    m_browseButton = new QPushButton("Parcourir", this);
    m_browseButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #5a5a5a;
            border-radius: 5px;
            padding: 8px 15px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
    )");
    
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(m_browseButton);
    mainLayout->addLayout(pathLayout);
    
    // Clear cache button
    m_clearCacheButton = new QPushButton("Vider le cache", this);
    m_clearCacheButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #5a5a5a;
            border-radius: 5px;
            padding: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
    )");
    mainLayout->addWidget(m_clearCacheButton);
    
    // AddOns section
    QLabel* addonsTitle = new QLabel("AddOns", this);
    addonsTitle->setAlignment(Qt::AlignCenter);
    addonsTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(addonsTitle);
    
    m_openAddonsButton = new QPushButton("Ouvrir le dossier AddOns", this);
    m_openAddonsButton->setMinimumHeight(40);
    m_openAddonsButton->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a6a4a, stop:0.5 #3a5a3a, stop:1 #2a4a2a);
            color: #ffffff;
            border: 1px solid #5a7a5a;
            border-radius: 5px;
            padding: 10px 20px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a7a5a, stop:0.5 #4a6a4a, stop:1 #3a5a3a);
        }
    )");
    mainLayout->addWidget(m_openAddonsButton);
    
    // Audio section
    QLabel* audioTitle = new QLabel("Audio", this);
    audioTitle->setAlignment(Qt::AlignCenter);
    audioTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(audioTitle);
    
    m_soundCheckbox = new QCheckBox("Activer les sons du launcher", this);
    m_soundCheckbox->setStyleSheet(R"(
        QCheckBox {
            color: #ffffff;
            font-size: 12px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
        }
        QCheckBox::indicator:checked {
            background-color: #d4af37;
            border: 2px solid #d4af37;
            border-radius: 4px;
        }
        QCheckBox::indicator:unchecked {
            background-color: #2a2a2a;
            border: 2px solid #5a4a2d;
            border-radius: 4px;
        }
    )");
    mainLayout->addWidget(m_soundCheckbox);
    
    mainLayout->addStretch();
    
    // Buttons row
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("OK", this);
    m_okButton->setMinimumSize(100, 40);
    m_okButton->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
            color: #d4af37;
            border: 1px solid #7a6a4d;
            border-radius: 5px;
            padding: 10px 30px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6a5a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
        }
    )");
    
    m_cancelButton = new QPushButton("Annuler", this);
    m_cancelButton->setMinimumSize(100, 40);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #ffffff;
            border: 1px solid #5a5a5a;
            border-radius: 5px;
            padding: 10px 25px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
    )");
    
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);
    connect(m_clearCacheButton, &QPushButton::clicked, this, &SettingsDialog::onClearCacheClicked);
    connect(m_openAddonsButton, &QPushButton::clicked, this, &SettingsDialog::onOpenAddonsClicked);
    connect(m_okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    
    // Dialog style
    setStyleSheet("QDialog { background-color: #1a1a1a; }");
}

void SettingsDialog::loadSettings() {
    m_pathEdit->setText(m_config->getWowPath());
    m_soundCheckbox->setChecked(!m_soundManager->isMuted());
}

void SettingsDialog::saveSettings() {
    m_config->setWowPath(m_pathEdit->text());
    m_soundManager->setMuted(!m_soundCheckbox->isChecked());
    m_config->save();
}

void SettingsDialog::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this,
        "Sélectionner le dossier World of Warcraft",
        m_pathEdit->text().isEmpty() ? QDir::homePath() : m_pathEdit->text());
    
    if (dir.isEmpty()) {
        return;
    }
    
    QString wowExe = dir + "/Wow.exe";
    if (!QFile::exists(wowExe)) {
        QMessageBox::warning(this, "Erreur",
            "Wow.exe non trouvé dans ce dossier.\n"
            "Veuillez sélectionner le dossier contenant World of Warcraft.");
        return;
    }
    
    m_pathEdit->setText(dir);
}

void SettingsDialog::onClearCacheClicked() {
    QString wowPath = m_pathEdit->text();
    if (wowPath.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "Aucun dossier WoW configuré.");
        return;
    }
    
    QString cachePath = wowPath + "/Cache";
    QDir cacheDir(cachePath);
    
    if (!cacheDir.exists()) {
        QMessageBox::information(this, "Info", "Aucun cache à vider.");
        return;
    }
    
    auto result = QMessageBox::question(this, "Confirmer",
        "Êtes-vous sûr de vouloir vider le cache WoW?\n"
        "Cela supprimera le dossier Cache.",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result != QMessageBox::Yes) return;
    
    if (cacheDir.removeRecursively()) {
        QMessageBox::information(this, "Succès", "Cache vidé avec succès!");
    } else {
        QMessageBox::warning(this, "Erreur", "Impossible de vider le cache.");
    }
}

void SettingsDialog::onOpenAddonsClicked() {
    QString wowPath = m_pathEdit->text();
    if (wowPath.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "Aucun dossier WoW configuré.");
        return;
    }
    
    QString addonsPath = wowPath + "/Interface/AddOns";
    QDir addonsDir(addonsPath);
    
    // Create the folder if it doesn't exist
    if (!addonsDir.exists()) {
        addonsDir.mkpath(".");
    }
    
    // Open in file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(addonsPath));
}

void SettingsDialog::onOkClicked() {
    saveSettings();
    emit settingsChanged();
    accept();
}

void SettingsDialog::onCancelClicked() {
    reject();
}
