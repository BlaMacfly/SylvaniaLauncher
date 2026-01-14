#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

class ConfigManager;
class SoundManager;

/**
 * @brief Settings dialog for Sylvania Launcher
 * 
 * Contains:
 * - WoW installation path configuration
 * - Cache clearing option
 * - Audio settings (enable/disable sounds)
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager* config, SoundManager* soundManager, QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onBrowseClicked();
    void onClearCacheClicked();
    void onOpenAddonsClicked();
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    ConfigManager* m_config;
    SoundManager* m_soundManager;

    QLineEdit* m_pathEdit = nullptr;
    QCheckBox* m_soundCheckbox = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_clearCacheButton = nullptr;
    QPushButton* m_openAddonsButton = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};
