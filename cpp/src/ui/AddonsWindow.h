#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include <vector>

class ConfigManager;
class QScrollArea;
class QVBoxLayout;

struct AddonInfo {
    QString name;
    QString description;
    QString downloadFileName;
    QString folderName;
};

class AddonsWindow : public QDialog {
    Q_OBJECT

public:
    explicit AddonsWindow(ConfigManager* config, QWidget* parent = nullptr);
    ~AddonsWindow() override;

private slots:
    void onInstallClicked(const QString& fileName, QPushButton* installBtn, QProgressBar* progressBar, QLabel* statusLabel);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);

private:
    void setupUi();
    void populateAddons();
    bool isAddonInstalled(const AddonInfo& addon) const;
    void updateButtonStyle(QPushButton* btn, bool installed);
    void extractZip(const QString& zipPath, const QString& destination, QPushButton* installBtn, QProgressBar* progressBar, QLabel* statusLabel);

    ConfigManager* m_config;
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_currentReply = nullptr;
    
    // UI Elements
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollWidget = nullptr;
    QVBoxLayout* m_addonsLayout = nullptr;
    
    // Current download state
    QString m_currentZipPath;
    QString m_currentDestination;
    QPushButton* m_currentInstallBtn = nullptr;
    QProgressBar* m_currentProgressBar = nullptr;
    QLabel* m_currentStatusLabel = nullptr;
    
    std::vector<AddonInfo> m_addons;
};
