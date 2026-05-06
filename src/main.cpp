#include <QApplication>
#include <QCommandLineParser>
#include <QMessageLogContext>
#include <QIcon>
#include <QDir>
#include "mainwindow.h"
#include "thememanager.h"
#include "settingsdialog.h"

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
    app.setOrganizationName("SplitCommander");
    app.setApplicationVersion(QStringLiteral("0.2.5"));
    app.setDesktopFileName("splitcommander");

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);

    // Theme vor MainWindow laden — Sidebar liest Farben beim Aufbau
    TM().apply();

    MainWindow w;
    w.show();
    w.raise();
    w.activateWindow();

    return app.exec();
}
