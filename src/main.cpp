#include <QApplication>
#include <QCommandLineParser>
#include <QMessageLogContext>
#include <QIcon>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QDialog>
#include <QEvent>
#include <QLineEdit>
#include <QAbstractItemView>
#include "mainwindow.h"
#include "thememanager.h"
#include "config.h"

// Stylt KEditTagsDialog (Stichwörter) wie unsere eigenen Dialoge
class ScKdeDialogFilter : public QObject {
public:
    explicit ScKdeDialogFilter(QObject *parent = nullptr) : QObject(parent) {}
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (ev->type() == QEvent::Show) {
            if (QByteArray(obj->metaObject()->className()) == "KEditTagsDialog") {
                auto *dlg = static_cast<QDialog*>(obj);
                const auto &c = TM().colors();
                dlg->setStyleSheet(TM().ssDialog() + QString(
                    "QListView { background:%1; border:1px solid %2; color:%3; border-radius:4px; }"
                    "QListView::item { color:%3; padding:2px; }"
                    "QListView::item:selected { background:%4; color:%5; }"
                    "QTreeView { background:%1; border:1px solid %2; color:%3; border-radius:4px; }"
                    "QTreeView::item { color:%3; padding:2px; }"
                    "QTreeView::item:selected { background:%4; color:%5; }")
                    .arg(c.bgDeep, c.accentHover, c.textPrimary, c.bgSelect, c.textLight));
            }
        }
        return false;
    }
};

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
    // Qt-interne Portal-Warnungen unterdrücken (harmlos außerhalb KDE-Session)
    qputenv("QT_LOGGING_RULES", "qt.qpa.services=false");

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
    app.installEventFilter(new ScKdeDialogFilter(&app));

    MainWindow w;
    w.show();
    w.raise();
    w.activateWindow();

    return app.exec();
}
