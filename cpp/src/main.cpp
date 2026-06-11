/**
 * @file main.cpp
 * @brief Sylvania Launcher - World of Warcraft launcher (WotLK 3.3.5a + Legion 7.3.5)
 *
 * Main entry point for the C++ version of Sylvania Launcher.
 */

#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

#include "core/GameEdition.h"
#include "ui/MainWindow.h"
#include "utils/PathUtils.h"

// Reads the persisted active edition without spinning up a full ConfigManager,
// so the correct taskbar icon is set before any window exists. The value is
// validated by GameEdition::byId — garbage in config can never select an
// unknown edition or be injected into an asset path.
static const GameEdition& peekActiveEdition() {
    QFile file(PathUtils::getConfigDir() + "/config.json");
    QString id;
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        id = doc.object().value("active_edition").toString();
    }
    return GameEdition::byId(id);
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
    // Windows: single edition-neutral AppUserModelID for taskbar grouping.
    // The launcher hosts both WotLK and Legion, so the id is no longer tied
    // to 3.3.5; the visible taskbar icon follows the active edition's window
    // icon instead.
    SetCurrentProcessExplicitAppUserModelID(L"sylvania.launcher");
#endif

    // Set application icon from the active edition's theme.
    const GameEdition& edition = peekActiveEdition();
    const QString iconPath = PathUtils::getAssetsDir() + "/" + edition.taskbarIconAsset;
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
