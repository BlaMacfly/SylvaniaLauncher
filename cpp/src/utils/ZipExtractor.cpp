#include "ZipExtractor.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

#ifdef Q_OS_LINUX
#include <zip.h>
#endif

#ifdef Q_OS_WIN
#include <QProcess>
#include <QTimer>
#endif

ZipExtractor::ZipExtractor(QObject* parent)
    : QObject(parent)
{
}

#ifdef Q_OS_LINUX
bool ZipExtractor::extractWithLibzip(const QString& zipPath, const QString& destPath,
                                     std::function<void(int, const QString&)> onProgress)
{
    int err = 0;
    zip_t* archive = zip_open(zipPath.toUtf8().constData(), ZIP_RDONLY, &err);
    if (!archive) {
        qWarning() << "ZipExtractor: Failed to open ZIP:" << zipPath << "error:" << err;
        return false;
    }

    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    if (numEntries <= 0) {
        zip_close(archive);
        return false;
    }

    QDir destDir(destPath);
    if (!destDir.exists()) {
        destDir.mkpath(".");
    }

    const int bufferSize = 65536; // 64KB buffer
    char buffer[bufferSize];

    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* name = zip_get_name(archive, i, 0);
        if (!name) continue;

        QString entryName = QString::fromUtf8(name);
        QString fullPath = destPath + "/" + entryName;

        // Report progress
        if (onProgress) {
            int percent = static_cast<int>((i * 100) / numEntries);
            onProgress(percent, QObject::tr("Extraction: %1").arg(entryName));
        }

        // Handle directories
        if (entryName.endsWith('/')) {
            QDir().mkpath(fullPath);
            continue;
        }

        // Ensure parent directory exists
        QFileInfo fileInfo(fullPath);
        QDir().mkpath(fileInfo.absolutePath());

        // Extract file
        zip_file_t* zf = zip_fopen_index(archive, i, 0);
        if (!zf) {
            qWarning() << "ZipExtractor: Failed to open entry:" << entryName;
            continue;
        }

        QFile outFile(fullPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            qWarning() << "ZipExtractor: Failed to create file:" << fullPath;
            zip_fclose(zf);
            continue;
        }

        zip_int64_t bytesRead;
        while ((bytesRead = zip_fread(zf, buffer, bufferSize)) > 0) {
            outFile.write(buffer, bytesRead);
        }

        outFile.close();
        zip_fclose(zf);

        // Preserve executable permissions for .exe files (needed for Wine)
        if (entryName.endsWith(".exe", Qt::CaseInsensitive) || 
            entryName.endsWith(".dll", Qt::CaseInsensitive)) {
            outFile.setPermissions(outFile.permissions() | QFile::ExeUser | QFile::ExeGroup);
        }
    }

    zip_close(archive);

    if (onProgress) {
        onProgress(100, QObject::tr("Extraction terminée."));
    }

    return true;
}
#endif

#ifdef Q_OS_WIN
bool ZipExtractor::extractWithPowerShell(const QString& zipPath, const QString& destPath,
                                         QObject* parent,
                                         std::function<void(int, const QString&)> onProgress,
                                         std::function<void(bool, const QString&)> onFinished)
{
    QFileInfo zipInfo(zipPath);
    qint64 estimatedExtractedSize = zipInfo.size() * 2;

    QProcess* process = new QProcess(parent);

    QTimer* progressTimer = new QTimer(parent);
    progressTimer->setInterval(5000);

    QString command = QString(
        "Expand-Archive -Path '%1' -DestinationPath '%2' -Force"
    ).arg(zipPath).arg(destPath);

    progressTimer->start();

    QObject::connect(progressTimer, &QTimer::timeout, parent, [onProgress, destPath, estimatedExtractedSize]() {
        QCoreApplication::processEvents();
        qint64 currentSize = 0;
        QDirIterator it(destPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            currentSize += it.fileInfo().size();
        }

        int progress = static_cast<int>((currentSize * 100) / estimatedExtractedSize);
        progress = qBound(0, progress, 99);

        if (onProgress) {
            onProgress(progress, QObject::tr("Extraction des fichiers... %1%").arg(progress));
        }
    });

    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            parent, [process, progressTimer, onFinished](int exitCode, QProcess::ExitStatus exitStatus) {
        progressTimer->stop();
        progressTimer->deleteLater();
        process->deleteLater();

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            if (onFinished) onFinished(true, "");
        } else {
            if (onFinished) onFinished(false, process->readAllStandardError());
        }
    });

    process->start("powershell", QStringList() << "-Command" << command);
    return true; // Async, result comes via callback
}
#endif

bool ZipExtractor::extract(const QString& zipPath, const QString& destPath,
                           std::function<void(int, const QString&)> onProgress)
{
#ifdef Q_OS_LINUX
    return extractWithLibzip(zipPath, destPath, onProgress);
#else
    // Synchronous PowerShell extraction for Windows
    QProcess process;
    QString command = QString(
        "Expand-Archive -Path '%1' -DestinationPath '%2' -Force"
    ).arg(zipPath).arg(destPath);

    process.start("powershell", QStringList() << "-Command" << command);
    process.waitForFinished(-1);

    return process.exitCode() == 0;
#endif
}

void ZipExtractor::extractAsync(const QString& zipPath, const QString& destPath,
                                QObject* parent,
                                std::function<void(int, const QString&)> onProgress,
                                std::function<void(bool, const QString&)> onFinished)
{
#ifdef Q_OS_LINUX
    // Run libzip extraction in a background thread
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(parent);
    
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, parent, [watcher, onFinished]() {
        bool result = watcher->result();
        watcher->deleteLater();
        if (onFinished) {
            onFinished(result, result ? "" : QObject::tr("Erreur lors de l'extraction"));
        }
    });

    QFuture<bool> future = QtConcurrent::run([zipPath, destPath, onProgress]() {
        return extractWithLibzip(zipPath, destPath, onProgress);
    });

    watcher->setFuture(future);
#else
    extractWithPowerShell(zipPath, destPath, parent, onProgress, onFinished);
#endif
}
