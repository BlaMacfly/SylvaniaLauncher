#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QStringList>
#include <QByteArray>
#include <memory>

/**
 * @brief Download dialog for the WoW 3.3.5a client (and the enUS pack)
 *
 * Features:
 * - HTTP download with progress, streamed straight to disk (no in-memory
 *   buffering)
 * - Free-disk-space check before the download starts
 * - Speed and ETA display
 * - Integrity verification (size + SHA-256) BEFORE extraction, when the
 *   expected values are known
 * - ZIP extraction (Expand-Archive, tar.exe fallback)
 */
class DownloadDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadDialog(QWidget *parent = nullptr,
                          const QString &destination = "");
  ~DownloadDialog() override;

  // Configures the dialog for the WoW 3.3.5a client (URL, expected size/hash,
  // title). Must be called before startDownload().
  void configureForClient();

  void setDownloadUrl(const QString &url) { m_downloadUrl = url; }
  void setExpectedSize(qint64 bytes) { m_expectedSize = bytes; }
  void setExpectedSha256(const QString &hex) { m_expectedSha256 = hex.toLower(); }
  void setWindowTitleText(const QString &title) { setWindowTitle(title); }

  void startDownload(const QString &directory = "");
  QString getDestination() const { return m_destination; }
  // Tunable for the segmented downloader (smaller in tests). 0 = use the
  // SylvaniaConstants default.
  void setSegmentSize(qint64 bytes) { m_segmentSize = bytes; }
  // Stop after the integrity check succeeds (no extraction). Used by the
  // headless download self-test.
  void setVerifyOnly(bool v) { m_verifyOnly = v; }
  void setSkipConfigWtf(bool skip) { m_skipConfigWtf = skip; }
  void setLanguage(const QString &lang) { m_language = lang; }
  // Address written into the generated Config.wtf (realmList).
  void setRealmlistAddress(const QString &address) { m_realmlistAddress = address; }

signals:
  void downloadFinished(bool success, const QString &message);
  // Mirrors the dialog progress for the main-window primary button:
  // 0-100 while downloading, -1 = indeterminate (verification/extraction).
  void progressChanged(int percent);

protected:
  void setupUi();
  void extractArchive(const QString &archivePath);
  void runExtractionProcess(const QString &archivePath, const QStringList &args);
  void onExtractionSucceeded(const QString &archivePath);
  void generateConfigWtf();
  void failDownload(const QString &message, const QString &fileToRemove = QString());
  bool checkDiskSpace(qint64 requiredBytes);

  // --- Download strategies ------------------------------------------------
  // Single streamed GET (used when the total size is unknown, e.g. the enUS
  // pack endpoint).
  void startSingleStream();
  // Resumable, segmented HTTP Range download (used when the expected size is
  // known). Each segment is buffered in memory, committed to disk once fully
  // received, and re-fetched on a network error.
  void startSegmentedDownload();
  void requestNextSegment();
  void onSegmentFinished();
  // Writes the verified segment buffer to disk, advancing m_downloadOffset;
  // returns false (and fails the download) on a short/failed disk write.
  bool commitSegmentBuffer();
  // Called once every byte is downloaded + verified.
  void onDownloadAssembled();
  void updateDownloadUi();

  QString archivePath() const;
  QString formatBytes(qint64 bytes) const;
  QString formatDuration(qint64 seconds) const;
  // Switches the dialog to the "verifying integrity" state (indeterminate bar
  // + explanatory text) before the SHA-256 of a multi-GB client is computed.
  void showVerifyingState();
  void verifyIntegrity(const QString &filePath);

private slots:
  void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void onDownloadFinished();
  void onDownloadError(QNetworkReply::NetworkError error);
  void onCancelClicked();
  void updateSpeed(qint64 bytesReceived);

private:
  std::unique_ptr<QNetworkAccessManager> m_networkManager;
  QNetworkReply *m_reply = nullptr;
  std::unique_ptr<QFile> m_file;

  QString m_destination;
  QString m_downloadUrl;
  QString m_language = "fr";
  QString m_realmlistAddress;

  // Integrity data (configureForClient / self-test). Unknown by default.
  qint64 m_expectedSize = 0;             // 0 = unknown
  QString m_expectedSha256;              // empty = unknown

  qint64 m_totalBytes = 0;
  qint64 m_receivedBytes = 0;
  qint64 m_lastReceivedBytes = 0;
  QElapsedTimer m_speedTimer;
  bool m_cancelled = false;
  bool m_skipConfigWtf = false;
  bool m_diskSpaceChecked = false;
  bool m_verifyOnly = false;       // self-test: stop after integrity check

  // Segmented-download state.
  qint64 m_segmentSize = 0;        // 0 = SylvaniaConstants default
  qint64 m_downloadOffset = 0;     // bytes confirmed (verified) on disk
  int m_segmentRetries = 0;        // consecutive failures for the current segment
  QByteArray m_segBuffer;          // current segment, buffered until complete

  // UI Elements
  QLabel *m_statusLabel = nullptr;
  QLabel *m_speedLabel = nullptr;
  QLabel *m_sizeLabel = nullptr;
  QLabel *m_etaLabel = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QPushButton *m_cancelButton = nullptr;
};
