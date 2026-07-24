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

    // Canonical destination root used for the zip-slip containment check.
    const QString destRoot = QDir(destPath).absolutePath() + "/";

    const int bufferSize = 65536; // 64KB buffer
    char buffer[bufferSize];

    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* name = zip_get_name(archive, i, 0);
        if (!name) continue;

        QString entryName = QString::fromUtf8(name);

        // Zip-slip guard: refuse absolute entries and any path that resolves
        // outside the destination ("../", "a/../../b", backslash variants).
        // QDir::cleanPath collapses . and .. before the prefix check.
        const QString normalized = QDir::cleanPath(QString(entryName).replace('\\', '/'));
        if (normalized.startsWith('/') || normalized.startsWith("../")
            || normalized == QLatin1String("..") || normalized.contains(":")) {
            qWarning() << "ZipExtractor: skipping unsafe entry:" << entryName;
            continue;
        }
        QString fullPath = QDir::cleanPath(destRoot + normalized);
        if (!fullPath.startsWith(destRoot)) {
            qWarning() << "ZipExtractor: skipping entry escaping destination:" << entryName;
            continue;
        }

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

    // Paths flow via env vars (never interpolated into the command string)
    // -> no shell-injection surface, same pattern as Constants.h extractors.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SYL_SRC", QDir::toNativeSeparators(zipPath));
    env.insert("SYL_DST", QDir::toNativeSeparators(destPath));
    process->setProcessEnvironment(env);
    const QString command = QStringLiteral(
        "Expand-Archive -LiteralPath $env:SYL_SRC -DestinationPath $env:SYL_DST -Force");

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
    // Synchronous PowerShell extraction for Windows. Paths flow via env vars
    // (never interpolated into the command string) -> no injection surface.
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SYL_SRC", QDir::toNativeSeparators(zipPath));
    env.insert("SYL_DST", QDir::toNativeSeparators(destPath));
    process.setProcessEnvironment(env);
    const QString command = QStringLiteral(
        "Expand-Archive -LiteralPath $env:SYL_SRC -DestinationPath $env:SYL_DST -Force");

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

    QFuture<bool> future = QtConcurrent::run([zipPath, destPath, parent, onProgress]() {
        auto threadSafeOnProgress = [parent, onProgress](int percent, const QString& status) {
            if (onProgress && parent) {
                QMetaObject::invokeMethod(parent, [onProgress, percent, status]() {
                    onProgress(percent, status);
                }, Qt::QueuedConnection);
            }
        };
        return extractWithLibzip(zipPath, destPath, threadSafeOnProgress);
    });

    watcher->setFuture(future);
#else
    extractWithPowerShell(zipPath, destPath, parent, onProgress, onFinished);
#endif
}
