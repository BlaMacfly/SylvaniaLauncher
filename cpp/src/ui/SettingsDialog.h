#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <vector>


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
  explicit SettingsDialog(ConfigManager *config, SoundManager *soundManager,
                          QWidget *parent = nullptr);

signals:
  void settingsChanged();
  void backgroundChanged(const QString &bgName);

private slots:
  void onBrowseClicked();
  void onBackgroundChanged(const QString &bgName);
  void onClearCacheClicked();
  void onOpenAddonsClicked();
  void onDownloadEnUsClicked();
  void onOkClicked();
  void onCancelClicked();

private:
  void setupUi();
  void setupPatchUI(QVBoxLayout *mainLayout);
  void loadSettings();
  void saveSettings();
  void updateButtonsState();

  bool isHdPatchInstalled() const;
  void togglePatch(const QString &fileName, bool enabled,
                   const QString &subDir = "");
  bool isPatchEnabled(const QString &fileName,
                      const QString &subDir = "") const;

  struct PatchInfo {
    QString label;
    QString fileName;
    QString subDir;
    bool isRoot = false;
  };
  std::vector<PatchInfo> m_patches;
  QMap<QString, QComboBox *> m_patchComboBoxes;

  ConfigManager *m_config;
  SoundManager *m_soundManager;

  QLineEdit *m_pathEdit = nullptr;
  QCheckBox *m_soundCheckbox = nullptr;
  QPushButton *m_browseButton = nullptr;
  QComboBox *m_backgroundCombo = nullptr;
  QPushButton *m_clearCacheButton = nullptr;
  QPushButton *m_openAddonsButton = nullptr;
  QPushButton *m_downloadEnUsButton = nullptr;
  QPushButton *m_okButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
};
