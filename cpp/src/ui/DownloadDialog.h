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
#include <memory>

struct GameEdition;

/**
 * @brief Download dialog for a game client (edition-aware since v3.0)
 *
 * Features:
 * - HTTP download with progress, streamed straight to disk (no in-memory
 *   buffering — Legion clients weigh several GB)
 * - Free-disk-space check before the download starts
 * - Speed and ETA display
 * - Mandatory integrity verification (size + SHA-256) BEFORE extraction;
 *   editions flagged requireHashBeforeExtract refuse to extract while the
 *   expected hash is unknown — never a silent "no verification"
 * - ZIP (Expand-Archive) or tar.gz (tar.exe) extraction, with an anti
 *   path-traversal pre-check on tar archives
 */
class DownloadDialog : public QDialog {
  Q_OBJECT

public:
  explicit DownloadDialog(QWidget *parent = nullptr,
                          const QString &destination = "");
  ~DownloadDialog() override;

  // Pulls URL, archive format, expected size/hash and exe names from the
  // edition descriptor. Must be called before startDownload().
  void configureForEdition(const GameEdition &edition);

  void setDownloadUrl(const QString &url) { m_downloadUrl = url; }
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
  // Address written into the generated launch config (realmList / portal).
  void setRealmlistAddress(const QString &address) { m_realmlistAddress = address; }

signals:
  void downloadFinished(bool success, const QString &message);
  // Mirrors the dialog progress for the main-window primary button:
  // 0-100 while downloading, -1 = indeterminate (verification/extraction).
  void progressChanged(int percent);

protected:
  void setupUi();
  void extractArchive(const QString &archivePath);
  void extractZip(const QString &archivePath);
  void extractTarGz(const QString &archivePath);
  void runExtractionProcess(const QString &archivePath, const QStringList &args);
  void onExtractionSucceeded(const QString &archivePath);
  void generateConfigWtf();
  void failDownload(const QString &message, const QString &fileToRemove = QString());
  bool checkDiskSpace(qint64 requiredBytes);

  // --- Download strategies ------------------------------------------------
  // Single streamed GET (used when the total size is unknown, e.g. the WotLK
  // enUS pack endpoint).
  void startSingleStream();
  // Resumable, segmented HTTP Range download (used for sized clients). Robust
  // against the corruption/latency seen on very large single-stream responses.
  void startSegmentedDownload();
  void requestNextSegment();
  void onSegmentFinished();
  // Appends a chunk to m_file, advancing m_downloadOffset; returns false (and
  // fails the download) on a short/failed disk write.
  bool writeChunk(const QByteArray &data);
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

  // Edition parameters (defaults = historical WotLK behaviour).
  QString m_editionId = "wotlk";
  QString m_archiveFormat = "zip";       // "zip" | "tar.gz"
  qint64 m_expectedSize = 0;             // 0 = unknown
  QString m_expectedSha256;              // empty = unknown
  bool m_requireHash = false;            // true -> refuse extraction w/o hash
  QStringList m_exeCandidates{QStringLiteral("Wow.exe")};

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
  qint64 m_downloadOffset = 0;     // bytes confirmed written = current file size
  int m_segmentRetries = 0;        // consecutive failures with no progress

  // UI Elements
  QLabel *m_statusLabel = nullptr;
  QLabel *m_speedLabel = nullptr;
  QLabel *m_sizeLabel = nullptr;
  QLabel *m_etaLabel = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QPushButton *m_cancelButton = nullptr;
};
