#pragma once

#include <QObject>
#include <QString>
#include <functional>

/**
 * @brief Cross-platform ZIP extraction utility
 * 
 * On Linux: Uses libzip for native extraction (no external dependency)
 * On Windows: Falls back to PowerShell Expand-Archive
 * 
 * Provides both synchronous and asynchronous extraction with progress reporting.
 */
class ZipExtractor : public QObject {
    Q_OBJECT

public:
    explicit ZipExtractor(QObject* parent = nullptr);

    /**
     * @brief Extract a ZIP file synchronously
     * @param zipPath Path to the ZIP file
     * @param destPath Destination directory
     * @param onProgress Optional progress callback (percent, statusMessage)
     * @return true on success
     */
    static bool extract(const QString& zipPath, const QString& destPath,
                        std::function<void(int, const QString&)> onProgress = nullptr);

    /**
     * @brief Extract a ZIP file asynchronously (non-blocking)
     * @param zipPath Path to the ZIP file
     * @param destPath Destination directory
     * @param parent QObject parent for signal connections
     * @param onProgress Progress callback (percent, statusMessage)
     * @param onFinished Completion callback (success, errorMessage)
     */
    static void extractAsync(const QString& zipPath, const QString& destPath,
                             QObject* parent,
                             std::function<void(int, const QString&)> onProgress,
                             std::function<void(bool, const QString&)> onFinished);

private:
#ifdef Q_OS_LINUX
    static bool extractWithLibzip(const QString& zipPath, const QString& destPath,
                                  std::function<void(int, const QString&)> onProgress);
#endif

#ifdef Q_OS_WIN
    static bool extractWithPowerShell(const QString& zipPath, const QString& destPath,
                                      QObject* parent,
                                      std::function<void(int, const QString&)> onProgress,
                                      std::function<void(bool, const QString&)> onFinished);
#endif
};
