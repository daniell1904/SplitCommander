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
#include <KLocalizedString>

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

    // Umgebungsvariablen für gettext/KDE-Bibliotheken frühzeitig setzen
    const QString earlySavedLang = Config::appLanguage();
    if (!earlySavedLang.isEmpty()) {
        QString localeStr = earlySavedLang;
        if (earlySavedLang == "de") localeStr = "de_DE.UTF-8";
        else if (earlySavedLang == "en") localeStr = "en_US.UTF-8";
        else if (earlySavedLang == "fr") localeStr = "fr_FR.UTF-8";
        else if (earlySavedLang == "es") localeStr = "es_ES.UTF-8";
        else if (earlySavedLang == "it") localeStr = "it_IT.UTF-8";
        else if (earlySavedLang == "nl") localeStr = "nl_NL.UTF-8";
        else if (earlySavedLang == "pl") localeStr = "pl_PL.UTF-8";
        else if (earlySavedLang == "pt") localeStr = "pt_PT.UTF-8";
        else if (earlySavedLang == "ru") localeStr = "ru_RU.UTF-8";
        else if (earlySavedLang == "da") localeStr = "da_DK.UTF-8";
        else if (earlySavedLang == "fi") localeStr = "fi_FI.UTF-8";
        else if (earlySavedLang == "nb") localeStr = "nb_NO.UTF-8";
        else if (earlySavedLang == "sv") localeStr = "sv_SE.UTF-8";
        else if (earlySavedLang == "tr") localeStr = "tr_TR.UTF-8";
        else if (earlySavedLang == "cs") localeStr = "cs_CZ.UTF-8";
        else if (earlySavedLang == "hu") localeStr = "hu_HU.UTF-8";
        else if (earlySavedLang == "ja") localeStr = "ja_JP.UTF-8";
        else if (earlySavedLang == "ko") localeStr = "ko_KR.UTF-8";
        else if (earlySavedLang == "ro") localeStr = "ro_RO.UTF-8";
        else if (earlySavedLang == "sk") localeStr = "sk_SK.UTF-8";
        else if (earlySavedLang == "ar") localeStr = "ar_SA.UTF-8";
        else if (!earlySavedLang.contains('.')) localeStr += ".UTF-8";

        qputenv("LANGUAGE", earlySavedLang.toUtf8());
        qputenv("LC_ALL", localeStr.toUtf8());
        qputenv("LANG", localeStr.toUtf8());
    } else {
        // Wenn keine Sprache gewählt ist und die Systemumgebung "C" (nicht UTF-8) ist,
        // auf "C.UTF-8" wechseln, um Qt-Warnungen zu vermeiden.
        QByteArray parentLang = qgetenv("LANG");
        if (parentLang.isEmpty() || parentLang == "C" || parentLang == "POSIX") {
            qputenv("LC_ALL", "C.UTF-8");
            qputenv("LANG", "C.UTF-8");
        }
    }

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
    // Gespeicherte Sprache hat Vorrang vor Systemsprache
    const QString savedLang = Config::appLanguage();
    if (!savedLang.isEmpty()) {
        KLocalizedString::setLanguages(QStringList{savedLang});
    }
    const QLocale locale = savedLang.isEmpty() ? QLocale::system() : QLocale(savedLang);

    // Qt-eigene Übersetzungen (Buttons, Dialoge etc.)
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // App-Übersetzungen (sucht in AppDataLocation/translations/ und neben der Binary)
    QTranslator appTranslator;
    QStringList dataDirs =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // Binary-Verzeichnis ebenfalls durchsuchen (deckt build/ und build-release/ ab)
    dataDirs.prepend(QCoreApplication::applicationDirPath());
    for (const QString &dir : dataDirs) {
        if (appTranslator.load(locale, "splitcommander", "_",
                               dir + "/translations")) {
            app.installTranslator(&appTranslator);
            break;
        }
        // Fallback: .qm direkt im Binary-Verzeichnis (wie build/ sie ablegt)
        if (appTranslator.load(locale, "splitcommander", "_", dir)) {
            app.installTranslator(&appTranslator);
            break;
        }
    }

    // Theme vor MainWindow laden — Sidebar liest Farben beim Aufbau
    // TM().apply() setzt auch die gespeicherte Schriftart
    TM().apply();
    app.installEventFilter(new ScKdeDialogFilter(&app));

    MainWindow w;
    w.show();
    w.raise();
    w.activateWindow();

    return app.exec();
}
