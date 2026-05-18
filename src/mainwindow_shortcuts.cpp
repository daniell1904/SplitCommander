// --- mainwindow_shortcuts.cpp -----------------------------------------------
// Tastatur-Shortcuts für MainWindow.
// ---------------------------------------------------------------------------

// --- mainwindow.cpp — SplitCommander Hauptfenster ---

#include "mainwindow.h"
#include "filemanager1.h"
// Removed agebadgedialog.h
#include "config.h"
#include "settingsdialog.h"
#ifdef SC_PLUGIN_GIT
#include "gitmanagerdialog.h"
#include "gitstatusmanager.h"
#endif
#include <QMessageBox>
#include "filepane.h"
#include "joboverlay.h"
#include "panecomponents.h"
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include "thememanager.h"
#include <KActionCollection>
#include <KShortcutsDialog>
#include <KStandardShortcut>

// Removed drophandler.h
#include <KFileItem>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/DeleteOrTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/SimpleJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KJobWidgets>
#include <KPropertiesDialog>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include "dialogutils.h"
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>

#include <QClipboard>
#include <QCloseEvent>
#include <KIO/OpenUrlJob>
#include <KAboutData>
#include <KAboutApplicationDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <KDirWatch>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSlider>

#include <QStandardPaths>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <functional>


// Removed panetoolbar.h
#include "millercolumn.h"
#include "scglobal.h"
#include "drivemanager.h"

#include "panewidget.h"

// --- MainWindow ---

void MainWindow::registerShortcuts() {
  if (!m_actionCollection) {
    m_actionCollection =
        new KActionCollection(this, QStringLiteral("splitcommander"));
    m_actionCollection->setComponentDisplayName(tr("SplitCommander"));

    // --- Hilfsmakro: Aktion anlegen ---
    // Qt::ApplicationShortcut: greift immer, egal welches Widget den Fokus hat
    auto addAct = [this](const QString &id, const QString &label,
                         const QString &icon, const QKeySequence &defKey,
                         std::function<void()> fn,
                         const QKeySequence &altKey = {}) -> QAction * {
      auto *a = m_actionCollection->addAction(id);
      a->setText(label);
      if (!icon.isEmpty())
        a->setIcon(QIcon::fromTheme(icon));
      a->setShortcutContext(Qt::ApplicationShortcut);
      if (altKey.isEmpty()) {
        m_actionCollection->setDefaultShortcut(a, defKey);
      } else {
        m_actionCollection->setDefaultShortcuts(a, {defKey, altKey});
      }
      connect(a, &QAction::triggered, this, fn);
      return a;
    };

    // Navigation
    addAct("nav_back", tr("Zurück"), "go-previous", Qt::ALT | Qt::Key_Left,
           [this]() {
             auto *p = activePane();
             if (!p->histBack().isEmpty()) {
               p->histFwd().push(p->currentPath());
               p->navigateTo(p->histBack().pop(), false);
             }
           });
    addAct("nav_forward", tr("Vorwärts"), "go-next", Qt::ALT | Qt::Key_Right,
           [this]() {
             auto *p = activePane();
             if (!p->histFwd().isEmpty()) {
               p->histBack().push(p->currentPath());
               p->navigateTo(p->histFwd().pop(), false);
             }
           });
    addAct("nav_up", tr("Übergeordneter Ordner"), "go-up", Qt::ALT | Qt::Key_Up,
           [this]() {
             QDir d(activePane()->currentPath());
             if (d.cdUp())
               activePane()->navigateTo(d.absolutePath());
           });
    addAct("nav_home", tr("Home-Verzeichnis"), "go-home",
           Qt::ALT | Qt::Key_Home,
           [this]() { activePane()->navigateTo(QDir::homePath()); });
    addAct(
        "nav_reload", tr("Neu laden"), "view-refresh", Qt::CTRL | Qt::Key_R,
        [this]() {
          m_leftPane->miller()->refreshDrives();
          m_rightPane->miller()->refreshDrives();
          m_leftPane->navigateTo(m_leftPane->currentPath());
          m_rightPane->navigateTo(m_rightPane->currentPath());
        },
        Qt::Key_F5);

    addAct("open_settings", tr("Einstellungen"), "settings-configure",
           Qt::CTRL | Qt::Key_Comma, [this]() {
             openSettings();
           });

#ifdef SC_PLUGIN_GIT
    addAct("open_git", tr("GitHub Manager"), "vcs-commit",
           Qt::CTRL | Qt::Key_G, [this]() {
             openGitManager();
           });
#endif

    // Pane-Fokus
    addAct("pane_focus_left", tr("Linke Pane fokussieren"), "go-first",
           Qt::CTRL | Qt::Key_Left, [this]() {
             m_leftPane->setFocused(true);
             m_rightPane->setFocused(false);
           });
    addAct("pane_focus_right", tr("Rechte Pane fokussieren"), "go-last",
           Qt::CTRL | Qt::Key_Right, [this]() {
             m_rightPane->setFocused(true);
             m_leftPane->setFocused(false);
           });
    addAct("pane_swap", tr("Panes tauschen"), "view-split-left-right",
           Qt::CTRL | Qt::Key_U, [this]() {
             const QString l = m_leftPane->currentPath();
             const QString r = m_rightPane->currentPath();
             m_leftPane->navigateTo(r);
             m_rightPane->navigateTo(l);
           });
    addAct("pane_sync", tr("Pfade synchronisieren"), "view-refresh",
           Qt::CTRL | Qt::SHIFT | Qt::Key_S,
           [this]() { m_rightPane->navigateTo(m_leftPane->currentPath()); });

    // Datei
    addAct(
        "file_rename", tr("Umbenennen"), "edit-rename", Qt::Key_F2, [this]() {
          const QList<QUrl> urls = activePane()->selectedUrls();
          if (urls.size() != 1)
            return;
          const QString path = urls.first().toLocalFile();
          bool ok;
          QString newName =
              DialogUtils::getText(this, tr("Umbenennen"), tr("Neuer Name:"),
                                   QFileInfo(path).fileName(), &ok);
          if (!ok || newName.isEmpty() || newName == QFileInfo(path).fileName())
            return;
          QUrl dest = QUrl::fromLocalFile(QFileInfo(path).dir().absolutePath() +
                                          "/" + newName);
          KIO::moveAs(urls.first(), dest, KIO::DefaultFlags);
        });
    // Wie Dolphin: zwei separate Actions für Trash und permanentes Löschen
    addAct("file_trash", tr("In den Papierkorb verschieben"), "user-trash",
           Qt::Key_Delete, [this]() {
             QWidget* fw = focusWidget();
             bool inMiller = false;
             while(fw) {
                 if (qobject_cast<MillerArea*>(fw)) { inMiller = true; break; }
                 fw = fw->parentWidget();
             }
             if (!inMiller)
                 doDelete(activePane(), false);
           });
    addAct("file_delete", tr("Löschen"), "edit-delete",
           QKeySequence(Qt::SHIFT | Qt::Key_Delete), [this]() {
             QWidget* fw = focusWidget();
             bool inMiller = false;
             while(fw) {
                 if (qobject_cast<MillerArea*>(fw)) { inMiller = true; break; }
                 fw = fw->parentWidget();
             }
             if (!inMiller)
                 doDelete(activePane(), true);
           });
    addAct("file_newfolder", tr("Neuer Ordner"), "folder-new", Qt::Key_F7,
           [this]() { emit activePane() -> newFolderRequested(); });
    addAct("file_copy", tr("Kopieren (Zwischenablage)"), "edit-copy",
           KStandardShortcut::copy().first(), [this]() {
             const QList<QUrl> urls = activePane()->selectedUrls();
             if (urls.isEmpty())
               return;
             auto *mime = new QMimeData();
             mime->setUrls(urls);
             mime->setData("x-kde-cut-selection", QByteArray("0"));
             QGuiApplication::clipboard()->setMimeData(mime);
           });
    addAct("file_move", tr("Ausschneiden (Zwischenablage)"), "edit-cut",
           KStandardShortcut::cut().first(), [this]() {
             const QList<QUrl> urls = activePane()->selectedUrls();
             if (urls.isEmpty())
               return;
             auto *mime = new QMimeData();
             mime->setUrls(urls);
             mime->setData("x-kde-cut-selection", QByteArray("1"));
             QGuiApplication::clipboard()->setMimeData(mime);
           });

    // Ansicht
    addAct("view_hidden", tr("Versteckte Dateien umschalten"), "view-hidden",
           Qt::CTRL | Qt::Key_H, [this]() {
             const bool cur = Config::showHiddenFiles();
             Config::setShowHiddenFiles(!cur);
             m_leftPane->navigateTo(m_leftPane->currentPath());
             m_rightPane->navigateTo(m_rightPane->currentPath());
             for (auto *col : m_leftPane->miller()->cols())
               col->populateDir(col->path());
             for (auto *col : m_rightPane->miller()->cols())
               col->populateDir(col->path());
           });

    addAct("view_layout", tr("Layout wechseln"), "view-choose",
           Qt::CTRL | Qt::Key_L, [this]() {
             int next = (m_currentMode + 1) % 3;
             auto gs = Config::group("UI");
             gs.writeEntry("layoutMode", next);
             gs.config()->sync();
             applyLayout(next);
           });

    // Einfügen
    addAct("file_paste", tr("Einfügen"), "edit-paste",
           KStandardShortcut::paste().first(), [this]() {
             const QMimeData *clip = QGuiApplication::clipboard()->mimeData();
             if (!clip || !clip->hasUrls())
               return;
             const bool isCut = clip->data("x-kde-cut-selection") == "1";
             const QList<QUrl> urls = clip->urls();
             const QUrl destUrl =
                 QUrl::fromLocalFile(activePane()->currentPath());
             if (isCut) {
               auto *job = KIO::move(urls, destUrl, KIO::DefaultFlags);
               job->uiDelegate()->setAutoErrorHandlingEnabled(true);
               registerJob(job, tr("Verschiebe Dateien..."));
               QGuiApplication::clipboard()->clear();
             } else {
               auto *job = KIO::copy(urls, destUrl, KIO::DefaultFlags);
               job->uiDelegate()->setAutoErrorHandlingEnabled(true);
               registerJob(job, tr("Kopiere Dateien..."));
             }
           });

    // Alles auswählen
    addAct("file_selectall", tr("Alles auswählen"), "edit-select-all",
           KStandardShortcut::selectAll().first(),
           [this]() { activePane()->filePane()->view()->selectAll(); });

    // Shortcuts aus KConfig laden (persistiert KShortcutsDialog-Änderungen)
    m_actionCollection->readSettings();

    // Alle Aktionen dem MainWindow zuweisen, damit sie feuern
    for (QAction *a : m_actionCollection->actions())
      addAction(a);

    // ActionCollection an Panes weitergeben — damit FilePane/PaneWidget
    // keine globalen MW()-Zugriffe mehr brauchen
    m_leftPane->setActionCollection(m_actionCollection);
    m_rightPane->setActionCollection(m_actionCollection);
  } else {
    // Bereits initialisiert: nur Settings neu einlesen (z.B. nach KShortcutsDialog)
    m_actionCollection->readSettings();
  }
}

