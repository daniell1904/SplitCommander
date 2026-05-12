#include <QApplication>
#include <QCommandLineParser>
#include <QMessageLogContext>
#include <QIcon>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QStandardPaths>
#include "mainwindow.h"
#include "thememanager.h"
#include "config.h"

static void scMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (msg.contains("grabbing the mouse only for popup")) return;
    if (msg.contains("Could not register app ID")) return;
    if (msg.contains("Failed to register with host portal")) return;
    switch (type) {
    case QtDebugMsg:    fprintf(stderr, "D: %s\n", qPrintable(msg)); break;
    case QtWarningMsg:  fprintf(stderr, "W: %s\n", qPrintable(msg)); break;
    case QtCriticalMsg: fprintf(stderr, "C: %s\n", qPrintable(msg)); break;
    case QtFatalMsg:    fprintf(stderr, "F: %s\n", qPrintable(msg)); abort();
    default: break;
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(scMessageHandler);
    QApplication app(argc, argv);
    app.setApplicationName("SplitCommander");
    // OrganizationName entfernt für flachere Ordnerstruktur
    app.setApplicationVersion(QStringLiteral(SC_VERSION));
    app.setDesktopFileName("splitcommander");

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);

    // Systemsprache ermitteln (respektiert LANG/LANGUAGE Umgebungsvariablen)
    const QLocale locale = QLocale::system();

    // Qt-eigene Übersetzungen (Buttons, Dialoge etc.)
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // App-Übersetzungen (sucht in AppDataLocation/translations/)
    QTranslator appTranslator;
    const QStringList dataDirs =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString &dir : dataDirs) {
        if (appTranslator.load(locale, "splitcommander", "_",
                               dir + "/translations")) {
            app.installTranslator(&appTranslator);
            break;
        }
    }

    // Theme vor MainWindow laden — Sidebar liest Farben beim Aufbau
    TM().apply();

    MainWindow w;
    w.show();
    w.raise();
    w.activateWindow();

    return app.exec();
}
