#pragma once

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QElapsedTimer>
#include <memory>

/**
 * @brief Download dialog for WoW client
 * 
 * Features:
 * - HTTP download with progress
 * - Speed and ETA display
 * - ZIP extraction after download
 * - Resume support on network failure
 */
class DownloadDialog : public QDialog {
    Q_OBJECT

public:
    explicit DownloadDialog(QWidget* parent = nullptr, const QString& destination = "");
    ~DownloadDialog() override;

    void startDownload(const QString& directory = "");
    QString getDestination() const { return m_destination; }

signals:
    void downloadFinished(bool success, const QString& message);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);
    void onCancelClicked();

private:
    void setupUi();
    void updateSpeed(qint64 bytesReceived);
    void extractZip(const QString& zipPath);
    QString formatBytes(qint64 bytes) const;
    QString formatDuration(qint64 seconds) const;

    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_reply = nullptr;
    std::unique_ptr<QFile> m_file;
    
    QString m_destination;
    QString m_downloadUrl;
    qint64 m_totalBytes = 0;
    qint64 m_receivedBytes = 0;
    qint64 m_lastReceivedBytes = 0;
    QElapsedTimer m_speedTimer;
    bool m_cancelled = false;

    // UI Elements
    QLabel* m_statusLabel = nullptr;
    QLabel* m_speedLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_etaLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_cancelButton = nullptr;
};
