/**
 * @file main.cpp
 * @brief Sylvania Launcher - World of Warcraft 3.3.5 Launcher
 * 
 * Main entry point for the C++ version of Sylvania Launcher.
 * 
 * Migration from Python/PySide6 to C++/Qt6.
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
#include "utils/PathUtils.h"

int main(int argc, char* argv[]) {
    // High DPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("Sylvania Launcher");
    app.setApplicationVersion("2.2.0");
    app.setOrganizationName("Sylvania");
    app.setOrganizationDomain("sylvania-wow.com");
    
#ifdef Q_OS_WIN
    // Windows: Set AppUserModelID for taskbar grouping
    SetCurrentProcessExplicitAppUserModelID(L"sylvania.launcher.wow.335");
#endif
    
    // Set application icon
    QString iconPath = PathUtils::getAssetsDir() + "/LogoSylvania/LogoSylvania256.ico";
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
