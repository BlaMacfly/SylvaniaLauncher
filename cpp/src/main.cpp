/**
 * @file main.cpp
 * @brief Sylvania Launcher - World of Warcraft launcher (WotLK 3.3.5a)
 *
 * Main entry point for the C++ version of Sylvania Launcher.
 */

#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

#include "ui/MainWindow.h"
#include "ui/DownloadDialog.h"
#include "utils/PathUtils.h"

#include <QObject>
#include <QStringList>
#include <QTextStream>

// Headless self-test for the segmented downloader:
//   SylvaniaLauncher.exe --selftest-download=<url>;<size>;<sha256>;<segBytes>;<dir>
// Downloads the file via the real DownloadDialog code path (segmented + resume
// + integrity), verifies size/hash, prints OK/FAIL and exits. No window shown.
static int runDownloadSelfTest(QApplication& app, const QString& spec) {
    const QStringList p = spec.split(';');
    if (p.size() < 5) { qWarning() << "selftest: spec invalide"; return 2; }

    // Route qDebug/qWarning to a log file (the GUI subsystem has no console).
    static QString s_logPath = p[4] + "/selftest.log";
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& m) {
        QFile lf(s_logPath);
        if (lf.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream(&lf) << m << "\n";
        }
    });

    const qint64 seg = p[3].toLongLong();
    const QString dir = p[4];

    auto* dlg = new DownloadDialog(nullptr, QString());  // empty dest = no auto-start
    dlg->setDownloadUrl(p[0]);
    dlg->setExpectedSize(p[1].toLongLong());
    dlg->setExpectedSha256(p[2]);
    dlg->setSegmentSize(seg);
    dlg->setVerifyOnly(true);

    int rc = 1;
    QTextStream out(stdout);
    QObject::connect(dlg, &DownloadDialog::downloadFinished, &app,
                     [&](bool ok, const QString& msg) {
        out << (ok ? "SELFTEST OK: " : "SELFTEST FAIL: ") << msg << "\n";
        out.flush();
        rc = ok ? 0 : 1;
        app.quit();
    });
    dlg->startDownload(dir);
    app.exec();
    return rc;
}

int main(int argc, char* argv[]) {
    // High DPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Sylvania Launcher");
    app.setApplicationVersion("3.0");
    app.setOrganizationName("Sylvania");
    app.setOrganizationDomain("sylvania-servergame.com");

#ifdef Q_OS_WIN
    // Windows: explicit AppUserModelID for taskbar grouping.
    SetCurrentProcessExplicitAppUserModelID(L"sylvania.launcher");
#endif

    // Headless self-tests (developer use; no UI).
    for (const QString& arg : app.arguments()) {
        if (arg.startsWith(QStringLiteral("--selftest-download="))) {
            return runDownloadSelfTest(app, arg.section('=', 1));
        }
    }

    // Set application icon.
    const QString iconPath = PathUtils::getAssetsDir() + "/LogoSylvania/LogoSylvania256.ico";
    if (QFile::exists(iconPath)) {
        app.setWindowIcon(QIcon(iconPath));
    } else {
        // Try from resources
        app.setWindowIcon(QIcon(":/icon"));
    }

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
