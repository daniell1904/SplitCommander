// --- panewidget_hamburger.cpp -----------------------------------------------
// Aufbau des Hamburger-Menüs in der PaneWidget-Toolbar.
// ---------------------------------------------------------------------------

#include "panewidget.h"
#include "mainwindow.h"
#include <QShortcut>
#include <QKeySequence>
#include "config.h"
#include "dialogutils.h"
#include "thememanager.h"
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KDialogJobUiDelegate>
#include <KFileItem>
#include <KFileWidget>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/EmptyTrashJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/StoredTransferJob>
#include <KPropertiesDialog>
#include <KShortcutsDialog>
#include <KShortcutsEditor>
#include <KTerminalLauncherJob>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

// --- PaneWidget ---

void PaneWidget::initHamburgerMenu(QToolButton *hamburgerBtn,
                                   QToolButton *layoutBtn) {
  // --- Hamburger-Menü ---
  auto *hamburgerMenu = new QMenu(hamburgerBtn);
  hamburgerMenu->setStyleSheet(TM().ssMenu());

#ifdef SC_PLUGIN_GIT
  // GitHub Manager
  hamburgerMenu->addAction(QIcon::fromTheme("vcs-commit"), tr("GitHub"), this, []() {
      if(auto *mw = MW()) mw->openGitManager();
  });
  hamburgerMenu->addSeparator();
#endif
#ifdef SC_PLUGIN_PAPERLESS
  hamburgerMenu->addAction(QIcon::fromTheme("document-send"), tr("Paperless Manager"), this, []() {
      if(auto *mw = MW()) mw->openPaperlessManager();
  });
  hamburgerMenu->addSeparator();
#endif

  // Neu erstellen
  auto *menuNew = hamburgerMenu->addMenu(QIcon::fromTheme("folder-new"),
                                         tr("Neu erstellen"));
  menuNew->setStyleSheet(TM().ssMenu());
  auto *actNewFolder =
      menuNew->addAction(QIcon::fromTheme("folder-new"), tr("Ordner …"));
  auto *actNewText =
      menuNew->addAction(QIcon::fromTheme("text-plain"), tr("Textdatei …"));
  auto *actNewHtml =
      menuNew->addAction(QIcon::fromTheme("text-html"), tr("HTML-Datei …"));
  auto *actNewEmpty =
      menuNew->addAction(QIcon::fromTheme("document-new"), tr("Leere Datei …"));
  menuNew->addSeparator();
  auto *actNewLinkFile =
      menuNew->addAction(QIcon::fromTheme("inode-symlink"),
                         tr("Verknüpfung zu Datei oder Ordner …"));

  hamburgerMenu->addSeparator();

  auto *menuTerminal = hamburgerMenu->addMenu(
      QIcon::fromTheme("utilities-terminal"), tr("Terminal"));
  menuTerminal->setStyleSheet(TM().ssMenu());

  menuTerminal->addAction(QIcon::fromTheme("utilities-terminal"),
                          tr("Im Terminal öffnen"), this, [this]() {
                            const QString path = this->currentPath();
                            auto *job = new KTerminalLauncherJob(QString());
                            job->setWorkingDirectory(
                                path.isEmpty() ? QDir::homePath() : path);
                            job->setUiDelegate(new KDialogJobUiDelegate(
                                KJobUiDelegate::AutoHandlingEnabled, this));
                            job->start();
                          });

  hamburgerMenu->addAction(QIcon::fromTheme("configure"), tr("Einrichten ..."), this, []() {
      if(auto *mw = MW()) mw->openSettings();
  });

  hamburgerMenu->addSeparator();

  // Über SplitCommander
  auto *actAbout = hamburgerMenu->addAction(QIcon::fromTheme("help-about"),
                                            tr("Über SplitCommander"));
  connect(actAbout, &QAction::triggered, this, [this]() {
    KAboutData aboutData(
        QStringLiteral("splitcommander"), tr("SplitCommander"),
        QStringLiteral(""),
        tr("Nativer Dual-Pane-Dateimanager für Linux mit KDE Plasma.\n"
           "Miller-Column-Interface mit KIO-Integration für lokale und "
           "Remote-Dateisysteme."),
        KAboutLicense::GPL_V3, tr("© 2025–2026 D. Lange"),
        tr("Unterstützt Google Drive, SFTP, SMB, MTP und weitere "
           "KIO-Protokolle.\n"
           "Inspiriert von OneCommander."),
        QStringLiteral("https://github.com/daniell1904/SplitCommander"),
        QStringLiteral("https://github.com/daniell1904/SplitCommander/issues"));
    aboutData.addAuthor(QStringLiteral("D. Lange"),
                        tr("Entwickler und Maintainer"), QStringLiteral(""),
                        QStringLiteral("https://github.com/daniell1904"));
    aboutData.setOrganizationDomain("github.com/daniell1904");
    aboutData.setDesktopFileName(QStringLiteral("splitcommander"));

    // Komponenten
    aboutData.addComponent(QStringLiteral("Qt"),
                           tr("Cross-Platform Application Framework"),
                           QStringLiteral(QT_VERSION_STR),
                           QStringLiteral("https://www.qt.io"));
    aboutData.addComponent(QStringLiteral("KDE Frameworks"),
                           tr("KDE-Bibliotheken (KF6)"),
                           QStringLiteral("6.26.0"),
                           QStringLiteral("https://api.kde.org/frameworks/"));
    aboutData.addComponent(QStringLiteral("KIO"),
                           tr("KDE Ein-/Ausgabe-Framework für lokale und Remote-Dateisysteme"),
                           QStringLiteral("6.26.0"),
                           QStringLiteral("https://api.kde.org/frameworks/kio/"));
    aboutData.addComponent(QStringLiteral("Solid"),
                           tr("Hardware-Erkennung und Geräte-Integration"),
                           QStringLiteral("6.26.0"),
                           QStringLiteral("https://api.kde.org/frameworks/solid/"));

    // Optionale Plugins
#ifdef SC_PLUGIN_GIT
    aboutData.addComponent(QStringLiteral("Plugin: Git Manager"),
                           tr("Repository-Verwaltung und Git-Sidebar-Integration"),
                           QStringLiteral("1.0"),
                           QStringLiteral("https://github.com/daniell1904/SplitCommander"));
#endif
#ifdef SC_PLUGIN_PAPERLESS
    aboutData.addComponent(QStringLiteral("Plugin: Paperless-ngx"),
                           tr("Dokumente hochladen und durchsuchen"),
                           QStringLiteral("1.0"),
                           QStringLiteral("https://github.com/daniell1904/SplitCommander"));
#endif

    KAboutApplicationDialog *dlg = new KAboutApplicationDialog(aboutData, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumHeight(480);
    dlg->exec();
  });

  // --- Layout-Button Connect ---
  connect(layoutBtn, &QToolButton::clicked, this, [this, layoutBtn]() {
    auto *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    const auto &c = TM().colors();
    popup->setStyleSheet(
        TM().ssDialog() +
        QString("QPushButton { background:%1; border:1px solid %2; "
                "color:%3;"
                " border-radius:4px; padding:8px; font-size:10px; }"
                "QPushButton:hover { background:%4; border-color:%5; }"
                "QPushButton:checked { background:%4; border:2px solid %5; "
                "color:%6; }")
            .arg(c.bgInput, c.borderAlt, c.textPrimary, c.bgHover, c.accent,
                 c.textAccent));

    auto *lay2 = new QHBoxLayout(popup);
    lay2->setContentsMargins(8, 8, 8, 8);
    lay2->setSpacing(6);

    struct ModeEntry {
      QString label, sub, icon;
      int mode;
    };
    const QList<ModeEntry> modes = {
        {tr("Klassisch"), tr("Einzeln"), "view-list-details", 0},
        {tr("Standard"), tr("Dual"), "view-split-left-right", 1},
        {tr("Spalten"), tr("Dual"), "view-split-top-bottom", 2},
    };

    auto *grp = new QButtonGroup(popup);
    auto s = Config::group("UI");

    int current = s.readEntry("layoutMode", 1);

    for (const auto &entry : modes) {
      auto *btn = new QPushButton();
      btn->setCheckable(true);
      btn->setChecked(entry.mode == current);
      btn->setFixedSize(72, 68);

      auto *vl = new QVBoxLayout(btn);
      vl->setContentsMargins(4, 6, 4, 4);
      vl->setSpacing(3);
      auto *ic = new QLabel();
      ic->setPixmap(QIcon::fromTheme(entry.icon).pixmap(24, 24));
      ic->setAlignment(Qt::AlignCenter);
      ic->setStyleSheet("background:transparent;border:none;");
      auto *lb1 = new QLabel(entry.label);
      lb1->setAlignment(Qt::AlignCenter);
      lb1->setStyleSheet("background:transparent;border:none;font-weight:bold;"
                         "font-size:10px;");
      auto *lb2 = new QLabel(entry.sub);
      lb2->setAlignment(Qt::AlignCenter);
      lb2->setStyleSheet(
          QString("background:transparent;border:none;color:%1;font-size:9px;")
              .arg(TM().colors().textMuted));
      vl->addWidget(ic);
      vl->addWidget(lb1);
      vl->addWidget(lb2);

      grp->addButton(btn, entry.mode);
      lay2->addWidget(btn);

      connect(btn, &QPushButton::clicked, this, [this, popup, entry]() {
        auto ss = Config::group("UI");

        ss.writeEntry("layoutMode", entry.mode);
        ss.config()->sync();
        emit layoutChangeRequested(entry.mode);
        popup->close();
      });
    }

    popup->move(layoutBtn->mapToGlobal(QPoint(0, layoutBtn->height() + 2)));
    popup->exec();
  });

  // --- Hamburger-Connects ---
  connect(actNewFolder, &QAction::triggered, this,
          [this]() { emit newFolderRequested(); });

  connect(actNewText, &QAction::triggered, this, [this]() {
    const QString dir = currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Neue Textdatei"), tr("Name:"),
                                        tr("Neue Datei.txt"), &ok);
    if (!ok || name.isEmpty())
      return;
    const QUrl dest = QUrl::fromUserInput(dir + "/" + name);
    auto *job = KIO::storedPut(QByteArray(), dest, -1, KIO::Overwrite);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(
        KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
  });

  connect(actNewHtml, &QAction::triggered, this, [this]() {
    const QString dir = currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Neue HTML-Datei"),
                                        tr("Name:"), tr("index.html"), &ok);
    if (!ok || name.isEmpty())
      return;
    const QUrl dest = QUrl::fromUserInput(dir + "/" + name);
    const QByteArray html =
        "<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\">"
        "<title></title></head>\n<body>\n\n</body>\n</html>\n";
    auto *job = KIO::storedPut(html, dest, -1, KIO::Overwrite);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(
        KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
  });

  connect(actNewEmpty, &QAction::triggered, this, [this]() {
    const QString dir = currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Leere Datei"), tr("Name:"),
                                        tr("Neue Datei"), &ok);
    if (!ok || name.isEmpty())
      return;
    const QUrl dest = QUrl::fromUserInput(dir + "/" + name);
    auto *job = KIO::storedPut(QByteArray(), dest, -1, KIO::Overwrite);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(
        KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
  });

  connect(actNewLinkFile, &QAction::triggered, this, [this]() {
    const QString dir = currentPath();

    // KIO-fähiger Dateiauswahl-Dialog (unterstützt SFTP, SMB, etc.)
    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Ziel wählen"));
    dlg->resize(720, 480);
    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *fw = new KFileWidget(QUrl::fromLocalFile(dir), dlg);
    fw->setOperationMode(KFileWidget::Opening);
    fw->setMode(KFile::File | KFile::Directory | KFile::ExistingOnly |
                KFile::LocalOnly);
    lay->addWidget(fw, 1);

    auto *btnRow = new QWidget(dlg);
    auto *btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(8, 4, 8, 8);
    btnLay->addStretch();
    auto *btnCancel = new QPushButton(tr("Abbrechen"), btnRow);
    auto *btnOk = new QPushButton(tr("Auswählen"), btnRow);
    btnLay->addWidget(btnCancel);
    btnLay->addWidget(btnOk);
    lay->addWidget(btnRow);

    connect(btnCancel, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, fw, &KFileWidget::slotOk);
    connect(fw, &KFileWidget::accepted, dlg, &QDialog::accept);

    connect(dlg, &QDialog::accepted, this, [this, fw, dir]() {
      const QString target = fw->selectedUrl().toLocalFile();
      if (target.isEmpty())
        return;
      bool ok;
      QString name = DialogUtils::getText(this, tr("Verknüpfungsname"),
                                          tr("Name:"), tr("Link"), &ok);
      if (!ok || name.isEmpty())
        return;
      auto *job = KIO::symlink(target, QUrl::fromLocalFile(dir + "/" + name),
                               KIO::HideProgressInfo);
      job->setUiDelegate(KIO::createDefaultJobUiDelegate(
          KJobUiDelegate::AutoHandlingEnabled, this));
    });
    dlg->open();
  });

  // (Verbindungen wurden bereits oben eingerichtet)

  hamburgerBtn->setMenu(hamburgerMenu);
}

// --- PaneWidget::initSearchPanel ---
