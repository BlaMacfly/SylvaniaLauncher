#include "SettingsDialog.h"
#include "ConfigManager.h"
#include "SoundManager.h"
#include "PathUtils.h"
#include "HdPatchManager.h"
#include "DownloadDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QEvent>

class WheelEventFilter : public QObject {
public:
    explicit WheelEventFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            return true; // Ignore wheel event
        }
        return QObject::eventFilter(obj, event);
    }
};

SettingsDialog::SettingsDialog(ConfigManager* config, SoundManager* soundManager, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_soundManager(soundManager)
{
    setWindowTitle(tr("Réglages"));
    setModal(true);
    setFixedSize(600, 750); // Increased height for patch list

    // Initialize patch mapping
    m_patches = {
        {tr("Nouveaux arbres"), "patch-w.mpq", ""},
        {tr("Nouvelles Icônes"), "patch-8.mpq", ""},
        {tr("Nouvelle eau"), "patch-5.mpq", ""},
        {tr("Textures du ciel"), "patch-5.mpq", ""},
        {tr("Nouveaux sorts"), "patch-5.mpq", ""},
        {tr("Cartes des donjons"), "patch-k.mpq", ""},
        {tr("Nouvelle musique"), "patch-c.mpq", ""},
        {tr("Écrans de chargement"), "patch-frFR-q.mpq", "frFR"},
        {tr("Polices"), "patch-frFR-f.mpq", "frFR"},
        {tr("DXVK 'API Vulkan'"), "d3d9.dll", "", true}, // isRoot = true
        {tr("Personnages HD"), "patch-f.mpq", ""},
        {tr("Mort-vivant sans os"), "patch-frFR-9.mpq", "frFR"},
        {tr("Modèles HD et PNJ"), "patch-x.mpq", ""},
        {tr("Textures du monde HD"), "patch-6.mpq", ""},
        {tr("Armures HD"), "patch-a.mpq", ""},
        {tr("Armes HD"), "patch-a.mpq", ""},
        {tr("Fenêtres d'Interface"), "patch-frFR-q.mpq", "frFR"},
        {tr("Curseur et Interface"), "patch-frFR-q.mpq", "frFR"},
        {tr("Sang"), "patch-s.mpq", ""}
    };
    
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Installation de WoW section
    QLabel* installTitle = new QLabel(tr("Installation de WoW"), this);
    installTitle->setAlignment(Qt::AlignCenter);
    installTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(installTitle);
    
    // Path row
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(10);
    
    QLabel* pathLabel = new QLabel(tr("Dossier d'installation:"), this);
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
    
    m_browseButton = new QPushButton(tr("Parcourir"), this);
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
    m_clearCacheButton = new QPushButton(tr("Vider le cache"), this);
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
    QLabel* addonsTitle = new QLabel(tr("AddOns"), this);
    addonsTitle->setAlignment(Qt::AlignCenter);
    addonsTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(addonsTitle);
    
    m_openAddonsButton = new QPushButton(tr("Ouvrir le dossier AddOns"), this);
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
    
    // Download enUS section
    m_downloadEnUsButton = new QPushButton(tr("Télécharger le Pack Anglais (enUS)"), this);
    m_downloadEnUsButton->setMinimumHeight(40);
    m_downloadEnUsButton->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a3a4a, stop:0.5 #1e2a3a, stop:1 #151a2a);
            color: #7ec8e3;
            border: 1px solid #3a4a5a;
            border-radius: 5px;
            padding: 10px 20px;
            font-weight: bold;
            font-size: 12px;
            margin-top: 5px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3a4a5a, stop:0.5 #2a3a4a, stop:1 #1e2a3a);
        }
    )");
    mainLayout->addWidget(m_downloadEnUsButton);
    
    // Appearance section
    QLabel* appearanceTitle = new QLabel(tr("Apparence"), this);
    appearanceTitle->setAlignment(Qt::AlignCenter);
    appearanceTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold; margin-top: 10px;");
    mainLayout->addWidget(appearanceTitle);
    
    QHBoxLayout* appearanceLayout = new QHBoxLayout();
    QLabel* bgLabel = new QLabel(tr("Arrière-plan:"), this);
    bgLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
    
    m_backgroundCombo = new QComboBox(this);
    m_backgroundCombo->addItem("Alliance", "Alliance");
    m_backgroundCombo->addItem(tr("Arbre de Vie"), "Arbre de Vie");
    m_backgroundCombo->addItem("Azeroth", "Azeroth");
    m_backgroundCombo->addItem("Horde", "Horde");
    m_backgroundCombo->addItem("Ilidan", "Ilidan");
    m_backgroundCombo->addItem("Lich King", "Lich King");
    m_backgroundCombo->addItem("Ragnaros", "Ragnaros");
    m_backgroundCombo->addItem(tr("Taverne"), "Taverne");
    m_backgroundCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #2a2a2a;
            color: #ffffff;
            border: 1px solid #5a4a2d;
            border-radius: 4px;
            padding: 5px;
            min-width: 150px;
        }
        QComboBox::drop-down { border: none; }
    )");
    m_backgroundCombo->installEventFilter(new WheelEventFilter(m_backgroundCombo));
    
    appearanceLayout->addWidget(bgLabel);
    appearanceLayout->addWidget(m_backgroundCombo);
    appearanceLayout->addStretch();
    mainLayout->addLayout(appearanceLayout);
    
    // Audio section
    QLabel* audioTitle = new QLabel(tr("Audio"), this);
    audioTitle->setAlignment(Qt::AlignCenter);
    audioTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(audioTitle);
    
    m_soundCheckbox = new QCheckBox(tr("Activer les sons du launcher"), this);
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
    
    // Patch Management Section (Visible if WoW path is set)
    if (!m_config->getWowPath().isEmpty()) {
        setupPatchUI(mainLayout);
    }
    
    mainLayout->addStretch();
    
    // Buttons row
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton(tr("OK"), this);
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
    
    m_cancelButton = new QPushButton(tr("Annuler"), this);
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
    connect(m_downloadEnUsButton, &QPushButton::clicked, this, &SettingsDialog::onDownloadEnUsClicked);
    connect(m_okButton, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(m_soundCheckbox, &QCheckBox::clicked, this, [this]() {
        m_soundManager->play("button");
    });
    connect(m_backgroundCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        onBackgroundChanged(m_backgroundCombo->itemData(index).toString());
    });
    
    // Dialog style
    setStyleSheet("QDialog { background-color: #1a1a1a; }");
}

void SettingsDialog::setupPatchUI(QVBoxLayout* mainLayout) {
    QLabel* patchesTitle = new QLabel(tr("Gestion des Patchs HD"), this);
    patchesTitle->setAlignment(Qt::AlignCenter);
    patchesTitle->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold; margin-top: 10px;");
    mainLayout->addWidget(patchesTitle);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: 1px solid #5a4a2d; border-radius: 5px; background: transparent; }");
    scrollArea->setMaximumHeight(350);

    QWidget* scrollContent = new QWidget();
    QGridLayout* gridLayout = new QGridLayout(scrollContent);
    gridLayout->setSpacing(15);

    int row = 0;
    int col = 0;
    for (const auto& patch : m_patches) {
        QVBoxLayout* itemLayout = new QVBoxLayout();
        
        QLabel* label = new QLabel(patch.label, this);
        label->setStyleSheet("color: #d4af37; font-size: 11px; font-weight: bold;");
        
        QComboBox* combo = new QComboBox(this);
        combo->addItem(tr("Inclus"), true);
        combo->addItem(tr("Eteindre"), false);
        combo->setStyleSheet(R"(
            QComboBox {
                background-color: #2a2a2a;
                color: #ffffff;
                border: 1px solid #5a4a2d;
                border-radius: 4px;
                padding: 5px;
                min-width: 100px;
            }
            QComboBox::drop-down { border: none; }
        )");
        
        combo->installEventFilter(new WheelEventFilter(combo));
        
        itemLayout->addWidget(label);
        itemLayout->addWidget(combo);
        
        gridLayout->addLayout(itemLayout, row, col);
        
        m_patchComboBoxes[patch.label] = combo;
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            m_soundManager->play("button");
        });
        
        col++;
        if (col > 1) {
            col = 0;
            row++;
        }
    }

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
}

void SettingsDialog::loadSettings() {
    m_pathEdit->setText(m_config->getWowPath());
    m_soundCheckbox->setChecked(!m_soundManager->isMuted());
    
    int bgIndex = m_backgroundCombo->findData(m_config->getBackground());
    if (bgIndex != -1) {
        m_backgroundCombo->setCurrentIndex(bgIndex);
    }

    // Load patch states
    for (const auto& patch : m_patches) {
        if (m_patchComboBoxes.contains(patch.label)) {
            bool enabled = isPatchEnabled(patch.fileName, patch.subDir);
            m_patchComboBoxes[patch.label]->setCurrentIndex(enabled ? 0 : 1);
        }
    }
}

void SettingsDialog::saveSettings() {
    m_config->setWowPath(m_pathEdit->text());
    m_soundManager->setMuted(!m_soundCheckbox->isChecked());
    m_config->setBackground(m_backgroundCombo->currentData().toString());
    
    // Save patch states: Group by file name and subDir to handle shared files
    // Logic: If ANY UI option for a file is enabled, the file is enabled.
    QMap<QString, bool> finalStates;
    for (const auto& patch : m_patches) {
        if (m_patchComboBoxes.contains(patch.label)) {
            bool shouldBeEnabled = m_patchComboBoxes[patch.label]->currentData().toBool();
            QString key = patch.fileName + "|" + patch.subDir;
            
            if (shouldBeEnabled) {
                finalStates[key] = true;
            } else if (!finalStates.contains(key)) {
                finalStates[key] = false;
            }
        }
    }

    for (auto it = finalStates.begin(); it != finalStates.end(); ++it) {
        QStringList parts = it.key().split("|");
        togglePatch(parts[0], it.value(), parts[1]);
    }

    m_config->save();
}

bool SettingsDialog::isHdPatchInstalled() const {
    return HdPatchManager::isInstalled(m_config->getWowPath());
}

bool SettingsDialog::isPatchEnabled(const QString& fileName, const QString& subDir) const {
    QString wowPath = m_config->getWowPath();
    QString fullPath = wowPath;
    if (fileName == "d3d9.dll") {
        fullPath += "/" + fileName;
    } else {
        fullPath += "/Data/";
        if (!subDir.isEmpty()) fullPath += subDir + "/";
        fullPath += fileName;
    }
    return QFile::exists(fullPath);
}

void SettingsDialog::togglePatch(const QString& fileName, bool enabled, const QString& subDir) {
    QString wowPath = m_config->getWowPath();
    QString basePath = wowPath;
    bool isRootFile = (fileName == "d3d9.dll");
    
    if (isRootFile) {
        // Handle DXVK specially (dll and conf)
        QStringList files = {"d3d9.dll", "dxvk.conf"};
        for (const QString& f : files) {
            QString activePath = basePath + "/" + f;
            QString disabledPath = activePath + ".disabled";
            
            if (enabled && !QFile::exists(activePath) && QFile::exists(disabledPath)) {
                QFile::rename(disabledPath, activePath);
            } else if (!enabled && QFile::exists(activePath)) {
                if (QFile::exists(disabledPath)) QFile::remove(disabledPath);
                QFile::rename(activePath, disabledPath);
            }
        }
        return;
    }

    // Normal MPQ patch
    basePath += "/Data/";
    if (!subDir.isEmpty()) basePath += subDir + "/";
    
    QString activePath = basePath + fileName;
    QString disabledPath = activePath + ".disabled";

    if (enabled) {
        if (!QFile::exists(activePath) && QFile::exists(disabledPath)) {
            QFile::rename(disabledPath, activePath);
        }
    } else {
        if (QFile::exists(activePath)) {
            // Check if another option shares this file and is still enabled
            // (Simpler: if user set it to disabled, we disable it)
            if (QFile::exists(disabledPath)) QFile::remove(disabledPath);
            QFile::rename(activePath, disabledPath);
        }
    }
}

void SettingsDialog::onBrowseClicked() {
    m_soundManager->play("button");
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Sélectionner le dossier World of Warcraft"),
        m_pathEdit->text().isEmpty() ? QDir::homePath() : m_pathEdit->text());
    
    if (dir.isEmpty()) {
        return;
    }
    
    QString wowExe = dir + "/Wow.exe";
    if (!QFile::exists(wowExe)) {
        auto result = QMessageBox::question(this, tr("Confirmation"),
            tr("Wow.exe n'a pas été trouvé dans ce dossier.\n\n"
               "Voulez-vous utiliser cet emplacement (par exemple pour une nouvelle installation) ?"),
            QMessageBox::Yes | QMessageBox::No);
            
        if (result == QMessageBox::No) {
            return;
        }
    }
    
    m_pathEdit->setText(dir);
}

void SettingsDialog::onClearCacheClicked() {
    m_soundManager->play("button");
    QString wowPath = m_pathEdit->text();
    if (wowPath.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun dossier WoW configuré."));
        return;
    }
    
    QString cachePath = wowPath + "/Cache";
    QDir cacheDir(cachePath);
    
    if (!cacheDir.exists()) {
        QMessageBox::information(this, tr("Info"), tr("Aucun cache à vider."));
        return;
    }
    
    auto result = QMessageBox::question(this, tr("Confirmer"),
        tr("Êtes-vous sûr de vouloir vider le cache WoW?\n"
           "Cela supprimera le dossier Cache."),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result != QMessageBox::Yes) return;
    
    if (cacheDir.removeRecursively()) {
        QMessageBox::information(this, tr("Succès"), tr("Cache vidé avec succès!"));
    } else {
        QMessageBox::warning(this, tr("Erreur"), tr("Impossible de vider le cache."));
    }
}

void SettingsDialog::onOpenAddonsClicked() {
    m_soundManager->play("button");
    QString wowPath = m_pathEdit->text();
    if (wowPath.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun dossier WoW configuré."));
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

void SettingsDialog::onDownloadEnUsClicked() {
    m_soundManager->play("button");
    QString wowPath = m_pathEdit->text();
    if (wowPath.isEmpty()) {
        QMessageBox::warning(this, tr("Erreur"), tr("Aucun dossier WoW configuré."));
        return;
    }

    QString enUsUrl = "https://sylvania-servergame.com/enus-download.php";
    DownloadDialog* dlDialog = new DownloadDialog(this, wowPath + "/Data");
    dlDialog->setDownloadUrl(enUsUrl);
    dlDialog->setWindowTitleText(tr("Téléchargement du Pack Anglais"));
    
    connect(dlDialog, &DownloadDialog::downloadFinished, this, [this](bool success, const QString& message) {
        if (success) {
            QMessageBox::information(this, tr("Succès"), tr("Pack Anglais installé avec succès !"));
        }
    });
    
    dlDialog->show();
}

void SettingsDialog::onOkClicked() {
    m_soundManager->play("button");
    saveSettings();
    emit settingsChanged();
    accept();
}

void SettingsDialog::onCancelClicked() {
    m_soundManager->play("button");
    
    // If user cancels, we might need to revert background if they changed it
    // But since it emits live, we restore the original if they cancel.
    emit backgroundChanged(m_config->getBackground());
    
    reject();
}

void SettingsDialog::onBackgroundChanged(const QString& bgName) {
    emit backgroundChanged(bgName);
}
