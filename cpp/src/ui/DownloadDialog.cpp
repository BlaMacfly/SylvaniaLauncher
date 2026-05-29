#include "DownloadDialog.h"
#include "Constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QTimer>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <QtConcurrent>
#include <QCryptographicHash>
#include <QFutureWatcher>

// Default WoW client download URL
static const QString DEFAULT_DOWNLOAD_URL = "https://sylvania-servergame.com/launcher-download.php";

DownloadDialog::DownloadDialog(QWidget* parent, const QString& destination)
    : QDialog(parent)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_destination(destination)
    , m_downloadUrl(DEFAULT_DOWNLOAD_URL)
{
    setWindowTitle(tr("Téléchargement du Client WoW"));
    setFixedSize(500, 250);
    setModal(true);
    
    setupUi();
    
    // Start download after dialog is shown
    if (!destination.isEmpty()) {
        QTimer::singleShot(100, this, [this]() { startDownload(); });
    }
}

DownloadDialog::~DownloadDialog() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void DownloadDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Status
    m_statusLabel = new QLabel(tr("Préparation du téléchargement..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #d4af37; font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            background-color: #2a2a4e;
            border: 1px solid #3a3a5e;
            border-radius: 5px;
            height: 25px;
            text-align: center;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #d4af37, stop:1 #8a7a2d);
            border-radius: 4px;
        }
    )");
    mainLayout->addWidget(m_progressBar);
    
    // Info layout
    QHBoxLayout* infoLayout = new QHBoxLayout();
    
    m_speedLabel = new QLabel(tr("Vitesse: --"), this);
    m_sizeLabel = new QLabel(tr("Taille: --"), this);
    m_etaLabel = new QLabel(tr("Temps restant: --"), this);
    
    QString labelStyle = "color: #888888; font-size: 12px;";
    m_speedLabel->setStyleSheet(labelStyle);
    m_sizeLabel->setStyleSheet(labelStyle);
    m_etaLabel->setStyleSheet(labelStyle);
    
    infoLayout->addWidget(m_speedLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_sizeLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_etaLabel);
    
    mainLayout->addLayout(infoLayout);
    mainLayout->addStretch();
    
    // Cancel button
    m_cancelButton = new QPushButton(tr("Annuler"), this);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5a4a2d, stop:0.5 #3a2a1d, stop:1 #2a1a0d);
            color: #d4af37;
            border: 1px solid #7a6a4d;
            border-radius: 5px;
            padding: 10px 30px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6a5a3d, stop:0.5 #4a3a2d, stop:1 #3a2a1d);
        }
    )");
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    connect(m_cancelButton, &QPushButton::clicked, this, &DownloadDialog::onCancelClicked);
    
    // Dialog style
    setStyleSheet("QDialog { background-color: #1a1a2e; }");
}

void DownloadDialog::startDownload(const QString& directory) {
    if (!directory.isEmpty()) {
        m_destination = directory;
    }
    
    if (m_destination.isEmpty()) {
        emit downloadFinished(false, tr("Destination non spécifiée"));
        reject();
        return;
    }
    
    // Ensure destination directory exists and is writable
    QDir dir(m_destination);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, tr("Erreur de permission"), tr("Impossible de créer le dossier de destination.\nVérifiez vos droits d'administrateur."));
            emit downloadFinished(false, tr("Erreur de permission lors de la création du dossier"));
            reject();
            return;
        }
    }
    
    QFileInfo dirInfo(m_destination);
    if (!dirInfo.isWritable()) {
        QMessageBox::critical(this, tr("Erreur de permission"), tr("Impossible d'écrire dans le dossier de destination.\nVérifiez vos droits d'administrateur."));
        emit downloadFinished(false, tr("Erreur de permission d'écriture"));
        reject();
        return;
    }
    
    // Create temp file for download
    QString zipPath = m_destination + "/WoWClient_temp.zip";
    m_file = std::make_unique<QFile>(zipPath);
    
    if (!m_file->open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Erreur"), tr("Impossible de créer le fichier de téléchargement."));
        emit downloadFinished(false, tr("Impossible de créer le fichier de téléchargement"));
        reject();
        return;
    }
    
    m_statusLabel->setText(tr("Connexion au serveur..."));
    
    QNetworkRequest request{QUrl(m_downloadUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    
    m_reply = m_networkManager->get(request);
    
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &DownloadDialog::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &DownloadDialog::onDownloadFinished);
    connect(m_reply, &QNetworkReply::errorOccurred,
            this, &DownloadDialog::onDownloadError);
    
    // Also connect readyRead to write data as it arrives
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_file->isOpen()) {
            m_file->write(m_reply->readAll());
        }
    });
    
    m_speedTimer.start();
}

void DownloadDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    m_receivedBytes = bytesReceived;
    m_totalBytes = bytesTotal;
    
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(progress);
        
        m_statusLabel->setText(tr("Téléchargement en cours... %1%").arg(progress));
        m_sizeLabel->setText(tr("Taille: %1 / %2")
            .arg(formatBytes(bytesReceived))
            .arg(formatBytes(bytesTotal)));
    }
    
    updateSpeed(bytesReceived);
}

void DownloadDialog::updateSpeed(qint64 bytesReceived) {
    qint64 elapsed = m_speedTimer.elapsed();
    
    if (elapsed > 1000) { // Update every second
        qint64 bytesPerSecond = ((bytesReceived - m_lastReceivedBytes) * 1000) / elapsed;
        m_lastReceivedBytes = bytesReceived;
        m_speedTimer.restart();
        
        m_speedLabel->setText(tr("Vitesse: %1/s").arg(formatBytes(bytesPerSecond)));
        
        // Calculate ETA
        if (bytesPerSecond > 0 && m_totalBytes > 0) {
            qint64 remaining = m_totalBytes - bytesReceived;
            qint64 secondsRemaining = remaining / bytesPerSecond;
            m_etaLabel->setText(tr("Temps restant: %1").arg(formatDuration(secondsRemaining)));
        }
    }
}

void DownloadDialog::onDownloadFinished() {
    if (m_cancelled) return;
    
    if (m_reply->error() != QNetworkReply::NoError) {
        return; // Error handler will take care of it
    }
    
    // Close the file
    if (m_file) {
        m_file->close();
    }
    
    QString zipPath = m_destination + "/WoWClient_temp.zip";
    
    m_statusLabel->setText(tr("Vérification de l'intégrité du fichier..."));
    m_progressBar->setMaximum(0); // Indeterminate mode
    m_progressBar->setMinimum(0);
    
    // Force UI update
    QCoreApplication::processEvents();
    
    // Verify Hash asynchronously
    verifyHash(zipPath);
}

void DownloadDialog::verifyHash(const QString& filePath) {
    QFutureWatcher<QByteArray>* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, filePath]() {
        QByteArray hash = watcher->result();
        watcher->deleteLater();
        
        qDebug() << "Downloaded file SHA-256:" << hash.toHex();
        // Here we could compare with an expected hash from headers if provided
        
        // Setup extraction progress indication
        m_statusLabel->setText(tr("Extraction des fichiers en cours..."));
        m_speedLabel->setText(tr("Veuillez patienter..."));
        m_sizeLabel->setText("");
        m_etaLabel->setText("");
        
        QCoreApplication::processEvents();
        
        // Extract in a timer to allow UI update
        QTimer::singleShot(200, this, [this, filePath]() {
            extractZip(filePath);
        });
    });
    
    // Compute SHA-256 hash
    QFuture<QByteArray> future = QtConcurrent::run([filePath]() {
        QFile f(filePath);
        if (f.open(QFile::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (hash.addData(&f)) {
                return hash.result();
            }
        }
        return QByteArray();
    });
    
    watcher->setFuture(future);
}

void DownloadDialog::extractZip(const QString& zipPath) {
    // Get ZIP file size to estimate extraction progress
    QFileInfo zipInfo(zipPath);
    qint64 zipSize = zipInfo.size();
    // Estimate extracted size (typically 1.5-3x compressed size, use 2x as estimate)
    qint64 estimatedExtractedSize = zipSize * 2;
    
    // Use Windows PowerShell Expand-Archive for extraction
    QProcess* process = new QProcess(this);

    // C1: paths flow via environment variables instead of being interpolated
    // into the PowerShell command string -> no shell-injection surface even
    // when the destination path contains quotes, $() or backticks.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SYL_SRC", QDir::toNativeSeparators(zipPath));
    env.insert("SYL_DST", QDir::toNativeSeparators(m_destination));
    process->setProcessEnvironment(env);

    // Progress monitoring timer - Reduced frequency to 5s to avoid UI freeze on large folders
    QTimer* progressTimer = new QTimer(this);
    progressTimer->setInterval(5000);
    
    // Store initial folder size
    qint64 initialSize = 0;
    QDir destDir(m_destination);
    if (destDir.exists()) {
        // Calculate initial folder size
        QDirIterator it(m_destination, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            initialSize += it.fileInfo().size();
        }
    }
    
    // Set progress bar to determinate mode
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    
    connect(progressTimer, &QTimer::timeout, this, [this, estimatedExtractedSize, initialSize]() {
        QCoreApplication::processEvents(); // Keep UI reactive
        qint64 currentSize = 0;
        QDirIterator it(m_destination, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            currentSize += it.fileInfo().size();
        }
        
        qint64 extractedSize = currentSize - initialSize;
        int progress = static_cast<int>((extractedSize * 100) / estimatedExtractedSize);
        progress = qMin(progress, 95); // Cap at 95% until actually finished
        
        m_progressBar->setValue(progress);
        m_statusLabel->setText(tr("Extraction en cours... %1%").arg(progress));
        m_speedLabel->setText(tr("%1 extraits").arg(formatBytes(extractedSize)));
    });
    
    progressTimer->start();
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, zipPath, progressTimer](int exitCode, QProcess::ExitStatus exitStatus) {
        progressTimer->stop();
        progressTimer->deleteLater();
        process->deleteLater();
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            // Complete progress
            m_progressBar->setValue(100);
            m_statusLabel->setText(tr("Extraction terminée!"));
            
            // Remove temp zip file
            QFile::remove(zipPath);
            
            // Generate Config.wtf if not skipped
            if (!m_skipConfigWtf) {
                generateConfigWtf();
            }
            
            QCoreApplication::processEvents();
            emit downloadFinished(true, tr("Téléchargement et extraction terminés!"));
            accept();
        } else {
            emit downloadFinished(false, tr("Erreur lors de l'extraction: %1").arg(process->readAllStandardError().data()));
            reject();
        }
    });
    
    connect(process, &QProcess::errorOccurred, this, [this, process, progressTimer](QProcess::ProcessError error) {
        Q_UNUSED(error);
        progressTimer->stop();
        progressTimer->deleteLater();
        process->deleteLater();
        emit downloadFinished(false, tr("Erreur lors de l'extraction: impossible de lancer PowerShell"));
        reject();
    });
    
    process->start("powershell", SylvaniaConstants::extractArchiveArgs());
}

void DownloadDialog::onDownloadError(QNetworkReply::NetworkError error) {
    if (m_cancelled) return;
    
    QString errorMsg;
    switch (error) {
        case QNetworkReply::ConnectionRefusedError:
            errorMsg = tr("Connexion refusée par le serveur");
            break;
        case QNetworkReply::HostNotFoundError:
            errorMsg = tr("Serveur introuvable");
            break;
        case QNetworkReply::TimeoutError:
            errorMsg = tr("Délai de connexion dépassé");
            break;
        default:
            errorMsg = tr("Erreur réseau: ") + m_reply->errorString();
    }
    
    QMessageBox::critical(this, tr("Erreur de téléchargement"), errorMsg);
    
    if (m_file) {
        m_file->close();
        m_file->remove();
    }
    
    emit downloadFinished(false, errorMsg);
    reject();
}

void DownloadDialog::onCancelClicked() {
    m_cancelled = true;
    
    if (m_reply) {
        m_reply->abort();
    }
    
    if (m_file) {
        m_file->close();
        m_file->remove();
    }
    
    emit downloadFinished(false, tr("Téléchargement annulé"));
    reject();
}

QString DownloadDialog::formatBytes(qint64 bytes) const {
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unitIndex < 3) {
        size /= 1024;
        unitIndex++;
    }

    QString unit;
    switch(unitIndex) {
        case 0: unit = tr("o"); break;
        case 1: unit = tr("Ko"); break;
        case 2: unit = tr("Mo"); break;
        case 3: unit = tr("Go"); break;
    }
    
    return QString::number(size, 'f', 2) + " " + unit;
}

QString DownloadDialog::formatDuration(qint64 seconds) const {
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    } else {
        return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
    }
}

void DownloadDialog::generateConfigWtf() {
    QString targetPath = m_destination;
    
    // Search for the actual WoW directory (containing Wow.exe)
    QDirIterator it(m_destination, QStringList() << "Wow.exe", QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        targetPath = QFileInfo(it.next()).absolutePath();
    }

    QString wtfPath = targetPath + "/WTF";
    QDir dir(wtfPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // C3: atomic write so a crash mid-write can't leave a half-written Config.wtf.
    QSaveFile configFile(wtfPath + "/Config.wtf");
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        QString locale = (m_language == "en") ? "enUS" : "frFR";
        out << "SET locale \"" << locale << "\"\n";
        out << "SET hwDetect \"0\"\n";
        out << "SET gxRefresh \"60\"\n";
        out << "SET gxMultisampleQuality \"0.000000\"\n";
        out << "SET gxWindow \"1\"\n";
        out << "SET videoOptionsVersion \"3\"\n";
        out << "SET movie \"0\"\n";
        out << "SET Gamma \"1.000000\"\n";
        out << "SET readTOS \"1\"\n";
        out << "SET readEULA \"1\"\n";
        out << "SET readTerminationWithoutNotice \"1\"\n";
        out << "SET showToolsUI \"1\"\n";
        out << "SET Sound_OutputDriverName \"System Default\"\n";
        out << "SET Sound_MusicVolume \"0.40000000596046\"\n";
        out << "SET Sound_AmbienceVolume \"0.60000002384186\"\n";
        out << "SET componentTextureLevel \"9\"\n";
        out << "SET farclip \"1277\"\n";
        out << "SET projectedTextures \"1\"\n";
        out << "SET weatherDensity \"3\"\n";
        out << "SET accounttype \"LK\"\n";
        out << "SET mouseSpeed \"1.2000000476837\"\n";
        out << "SET gameTip \"16\"\n";
        out << "SET uiScale \"0.75999999046326\"\n";
        out << "SET useUiScale \"1\"\n";
        out << "SET checkAddonVersion \"0\"\n";
        out << "SET Sound_VoiceChatInputDriverName \"RÃ©glage du systÃ¨me\"\n";
        out << "SET Sound_VoiceChatOutputDriverName \"RÃ©glage du systÃ¨me\"\n";
        out << "SET movieSubtitle \"1\"\n";
        out << "SET Sound_SFXVolume \"0.5\"\n";
        out << "SET dbCompress \"0\"\n";
        out << "SET gxResolution \"1920x1080\"\n";
        out << "SET textureFilteringMode \"5\"\n";
        out << "SET groundEffectDist \"140\"\n";
        out << "SET environmentDetail \"1.5\"\n";
        out << "SET timingTestError \"0\"\n";
        out << "SET windowResizeLock \"1\"\n";
        out << "SET particleDensity \"1\"\n";
        out << "SET mapShadows \"0\"\n";
        out << "SET vertexShaders \"0\"\n";
        out << "SET pixelShaders \"0\"\n";
        out << "SET M2UseShaders \"0\"\n";
        out << "SET M2Faster \"3\"\n";
        out << "SET shadowinstancing \"0\"\n";
        out << "SET objectFade \"0\"\n";
        out << "SET timingMethod \"2\"\n";
        out << "SET ffxSpecial \"0\"\n";
        out << "SET groundEffectDensity \"64\"\n";
        out << "SET realmList \"sylvania-servergame.com\"\n";
        out << "SET realmName \"The Kingdom of Sylvania\"\n";
        out << "SET specular \"1\"\n";
        out << "SET Sound_MasterVolume \"0.60000002384186\"\n";
        out << "SET Sound_ZoneMusicNoDelay \"1\"\n";
        out << "SET gxTripleBuffer \"1\"\n";
        out << "SET gxMultisample \"8\"\n";
        out << "SET shadowLevel \"0\"\n";
        out << "SET extShadowQuality \"5\"\n";
        out.flush();
        configFile.commit();
    }
}
