// --- mainwindow.cpp — SplitCommander Hauptfenster ---

#include "mainwindow.h"
#include "agebadgedialog.h"
#include "config.h"
#include "filepane.h"
#include "joboverlay.h"
#include "panewidgets.h"
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include "thememanager.h"
#include <KActionCollection>
#include <KShortcutsDialog>
#include <KStandardShortcut>

#include "drophandler.h"
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


#include "panetoolbar.h"
#include "millercolumn.h"
#include "scglobal.h"

#include "panewidget.h"

// --- MainWindow ---
MainWindow::~MainWindow() {}

void MainWindow::registerJob(KJob *job, const QString &title) {
  if (m_jobOverlay) {
    m_jobOverlay->addJob(job, title);
  }
}

PaneWidget *MainWindow::activePane() const {
  if (m_leftPane && m_leftPane->isFocused())
    return m_leftPane;
  return m_rightPane;
}

void MainWindow::doDelete(PaneWidget *pane, bool permanent) {
  if (!pane)
    pane = activePane();
  const QList<QUrl> urls = pane->filePane()->selectedUrls();
  if (urls.isEmpty())
    return;

  // Exakt wie Dolphin: DefaultConfirmation + AutoWarningHandlingEnabled
  auto *job = new KIO::DeleteOrTrashJob(
      urls,
      permanent ? KIO::AskUserActionInterface::Delete
                : KIO::AskUserActionInterface::Trash,
      KIO::AskUserActionInterface::DefaultConfirmation,
      this);
  job->setUiDelegate(KIO::createDefaultJobUiDelegate(
      KJobUiDelegate::AutoWarningHandlingEnabled, this));
  registerJob(job, tr("Löschen..."));
  job->start();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("SplitCommander");
  {
    auto s = Config::group("UI");
    const QByteArray geo = s.readEntry("windowGeometry", QByteArray());
    if (!geo.isEmpty())
      restoreGeometry(geo);
    else
      resize(1280, 900);
  }

  auto *central = new QWidget(this);
  setCentralWidget(central);
  auto *rootLay = new QHBoxLayout(central);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  m_sidebar = new Sidebar(this);
  m_jobOverlay = new JobOverlay(this);
  {
    auto s = Config::group("UI");
    const int sidebarW = s.readEntry("sidebarWidth", 250);
    const bool sidebarVis = s.readEntry("sidebarVisible", true);
    m_sidebar->setFixedWidth(sidebarW);
    m_sidebar->setVisible(sidebarVis);
  }

  rootLay->addWidget(m_sidebar);
  auto *sidebarHandle = new SidebarHandle(m_sidebar, central);
  {
    auto s = Config::group("UI");
    const bool sidebarVis = s.readEntry("sidebarVisible", true);
    sidebarHandle->setFixedWidth(sidebarVis ? 10 : 32);
    if (!sidebarVis)
      sidebarHandle->setCursor(Qt::PointingHandCursor);
  }

  rootLay->addWidget(sidebarHandle);

  m_panesSplitter = new PaneSplitter(Qt::Horizontal, central);
  m_panesSplitter->setHandleWidth(12);
  m_panesSplitter->setChildrenCollapsible(true);
  m_panesSplitter->setStyleSheet(TM().ssSplitter());

  m_leftPane = new PaneWidget("leftPane");
  m_rightPane = new PaneWidget("rightPane");
  m_panesSplitter->addWidget(m_leftPane);
  m_panesSplitter->addWidget(m_rightPane);
  rootLay->addWidget(m_panesSplitter, 1);

  connect(m_leftPane, &PaneWidget::focusRequested, this, [this]() {
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
  });
  connect(m_rightPane, &PaneWidget::focusRequested, this, [this]() {
    m_rightPane->setFocused(true);
    m_leftPane->setFocused(false);
  });

  auto connectHamburger = [this](PaneWidget *pane) {
    connect(pane, &PaneWidget::newFolderRequested, this, [this, pane]() {
      bool ok;
      QString name = DialogUtils::getText(
          this, tr("Neuer Ordner"), tr("Ordnername:"), tr("Neuer Ordner"), &ok);
      if (!ok || name.isEmpty())
        return;
      auto *job = KIO::mkdir(QUrl::fromLocalFile(pane->currentPath() + "/" + name));
      job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
      connect(job, &KJob::result, this, [this, job]() {
        if (job->error())
          DialogUtils::message(this, tr("Fehler"), tr("Ordner konnte nicht erstellt werden."));
      });
    });
    connect(pane, &PaneWidget::hiddenFilesToggled, this, [this](bool show) {
      emit m_sidebar->hiddenFilesChanged(show);
      m_leftPane->filePane()->setShowHiddenFiles(show);
      m_rightPane->filePane()->setShowHiddenFiles(show);
      m_leftPane->miller()->refresh();
      m_rightPane->miller()->refresh();
    });
    connect(pane, &PaneWidget::extensionsToggled, this, [this](bool on) {
      Config::setShowFileExtensions(on);
      m_leftPane->filePane()->setRootPath(m_leftPane->currentPath());
      m_rightPane->filePane()->setRootPath(m_rightPane->currentPath());
    });
  };
  connectHamburger(m_leftPane);
  connectHamburger(m_rightPane);

  auto mountAndNavigate = [this](const QString &path, bool leftPane) {
    auto navigate = [this, leftPane](const QString &p) {
      if (leftPane) {
        m_leftPane->navigateTo(p);
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
      } else {
        m_rightPane->navigateTo(p);
        m_rightPane->setFocused(true);
        m_leftPane->setFocused(false);
      }
    };

    if (path == "remote:/") {
      PaneWidget *pane = leftPane ? m_leftPane : m_rightPane;
      pane->setViewMode(0);                // Details
      pane->navigateTo(path, true, false); // false = don't update miller
      pane->setFocused(true);
      if (leftPane)
        m_rightPane->setFocused(false);
      else
        m_leftPane->setFocused(false);
      return;
    }

    if (path.startsWith("solid:")) {
      Solid::Device dev(path.mid(6));
      auto *acc = dev.as<Solid::StorageAccess>();
      if (!acc)
        return;
      if (acc->isAccessible()) {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
        m_sidebar->updateDrives();
      } else {
        connect(
            acc, &Solid::StorageAccess::setupDone, this,
            [this, acc](Solid::ErrorType, QVariant, const QString &) {
              if (acc->isAccessible()) {
                m_leftPane->miller()->refreshDrives();
                m_rightPane->miller()->refreshDrives();
                m_sidebar->updateDrives();
              }
            },
            Qt::SingleShotConnection);
        acc->setup();
      }
    } else {
      navigate(path);
    }
  };

  connect(m_sidebar, &Sidebar::driveClicked, this,
          [this, mountAndNavigate](const QString &path) {
            mountAndNavigate(path, !m_rightPane->isFocused());
          });
  connect(m_sidebar, &Sidebar::driveClickedLeft, this,
          [mountAndNavigate](const QString &path) {
            mountAndNavigate(path, true); // immer links
          });
  connect(m_sidebar, &Sidebar::driveClickedRight, this,
          [mountAndNavigate](const QString &path) {
            mountAndNavigate(path, false); // immer rechts
          });
  // Öffnen in Linke/Rechte Ansicht – fix, unabhängig von der Quell-Pane
  for (auto *pane : {m_leftPane, m_rightPane}) {
    connect(pane, &PaneWidget::openInLeftRequested, this,
            [this](const QString &path) {
              m_leftPane->navigateTo(path);
              m_leftPane->setFocused(true);
              m_rightPane->setFocused(false);
            });
    connect(pane, &PaneWidget::openInRightRequested, this,
            [this](const QString &path) {
              m_rightPane->navigateTo(path);
              m_rightPane->setFocused(true);
              m_leftPane->setFocused(false);
            });
  }
  connect(m_sidebar, &Sidebar::addCurrentPathToPlaces, this,
          [this]() { m_sidebar->addPlace(m_leftPane->currentPath()); });
  connect(m_sidebar, &Sidebar::requestActivePath, this, [this](QString *out) {
    if (out)
      *out = m_leftPane->currentPath();
  });
  connect(m_sidebar, &Sidebar::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_leftPane, &PaneWidget::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_rightPane, &PaneWidget::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_sidebar, &Sidebar::tagClicked, this,
          [this](const QString &tagName) {
            auto *pane = activePane();
            const QString cur = pane->currentPath();
            const bool isKio = !cur.startsWith("/") && cur.contains(":/");
            if (isKio) {
              // KIO-Modus verlassen ohne History-Eintrag
              pane->filePane()->setRootPath(QDir::homePath());
            }
            pane->filePane()->showTaggedFiles(tagName);
          });
  connect(m_sidebar, &Sidebar::drivesChanged, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
  });

  // NetworkPlace aus Millers entfernen — sofort refreshen
  auto doRemoveFromPlaces = [this](const QString &url) {
    if (!url.isEmpty()) {
      const QString normUrl = mw_normalizePath(url);
      for (auto *pane : {m_leftPane, m_rightPane}) {
        const QString current = mw_normalizePath(pane->currentPath());
        if (current == normUrl || current.startsWith(normUrl + "/")) {
          pane->navigateTo("__drives__");
        }
      }
    }
    m_sidebar->updateDrives();
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    emit m_sidebar->drivesChanged();
  };
  connect(m_leftPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(m_rightPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(m_sidebar, &Sidebar::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(
      m_sidebar, &Sidebar::unmountRequested, this, [this](const QString &path) {
        const QString normPath = mw_normalizePath(path);

        // Pfade VOR dem Aushängen erfassen
        const bool leftPaneOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_leftPane->currentPath()) == normPath ||
            mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + "/"));
        const bool rightPaneOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_rightPane->currentPath()) == normPath ||
            mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + "/"));
        const bool leftMillerOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_leftPane->miller()->activePath()) == normPath ||
            mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + "/"));
        const bool rightMillerOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_rightPane->miller()->activePath()) == normPath ||
            mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + "/"));

        auto *proc = new QProcess(this);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, leftPaneOnMount, rightPaneOnMount,
                       leftMillerOnMount, rightMillerOnMount](int exitCode, QProcess::ExitStatus) {
                  if (exitCode == 0) {
                    if (leftPaneOnMount)  m_leftPane->navigateTo("__drives__");
                    if (rightPaneOnMount) m_rightPane->navigateTo("__drives__");
                    if (leftMillerOnMount)
                      m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
                    else
                      m_leftPane->miller()->refreshDrives();
                    if (rightMillerOnMount)
                      m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));
                    else
                      m_rightPane->miller()->refreshDrives();
                  }
                  m_sidebar->updateDrives();
                  proc->deleteLater();
                });
        proc->start("umount", {path});
      });

  auto doTeardown = [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;

    const QString mountPoint = acc->filePath();
    const QString normPath = mw_normalizePath(mountPoint);

    // Pfade VOR dem Aushängen erfassen
    const bool leftPaneOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_leftPane->currentPath()) == normPath ||
        mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + "/"));
    const bool rightPaneOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_rightPane->currentPath()) == normPath ||
        mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + "/"));
    const bool leftMillerOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_leftPane->miller()->activePath()) == normPath ||
        mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + "/"));
    const bool rightMillerOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_rightPane->miller()->activePath()) == normPath ||
        mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + "/"));

    connect(
        acc, &Solid::StorageAccess::teardownDone, this,
        [this, leftPaneOnMount, rightPaneOnMount,
         leftMillerOnMount, rightMillerOnMount](Solid::ErrorType, QVariant, const QString &) {
          if (leftPaneOnMount)  m_leftPane->navigateTo(QDir::homePath());
          if (rightPaneOnMount) m_rightPane->navigateTo(QDir::homePath());
          if (leftMillerOnMount)
            m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
          else
            m_leftPane->miller()->refreshDrives();
          if (rightMillerOnMount)
            m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));
          else
            m_rightPane->miller()->refreshDrives();
          m_sidebar->updateDrives();
        },
        Qt::SingleShotConnection);
    acc->teardown();
  };
  connect(m_leftPane->miller(), &MillerArea::teardownRequested, this,
          doTeardown);
  connect(m_rightPane->miller(), &MillerArea::teardownRequested, this,
          doTeardown);
  // Sidebar Aushängen ebenfalls über doTeardown koordinieren
  connect(m_sidebar, &Sidebar::teardownRequested, this, doTeardown);

  // Miller Ein-/Aushängen -> Sidebar synchronisieren
  connect(m_leftPane->miller(), &MillerArea::drivesChanged,
          m_sidebar, &Sidebar::updateDrives);
  connect(m_rightPane->miller(), &MillerArea::drivesChanged,
          m_sidebar, &Sidebar::updateDrives);
  QTimer::singleShot(2000, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
  });

  // --- Settings-Änderungen live anwenden ---
  connect(m_sidebar, &Sidebar::hiddenFilesChanged, this, [this](bool) {
    // Beide Panes neu laden — populate() liest showHidden selbst aus Settings
    m_leftPane->navigateTo(m_leftPane->currentPath());
    m_rightPane->navigateTo(m_rightPane->currentPath());
    // Miller-Columns ebenfalls aktualisieren
    for (auto *col : m_leftPane->miller()->cols())
      col->populateDir(col->path());
    for (auto *col : m_rightPane->miller()->cols())
      col->populateDir(col->path());
  });
  connect(m_sidebar, &Sidebar::settingsChanged, this, [this]() {
    // ThemeManager neu anwenden — setzt qApp->setStyleSheet und emittiert
    // themeChanged
    TM().apply();
    // Shortcuts neu registrieren falls geändert
    registerShortcuts();
  });
  connect(&TM(), &ThemeManager::themeChanged, this, [this]() {
    // Miller-Spalten neu stylen
    for (auto *col : m_leftPane->miller()->cols())
      col->refreshStyle();
    for (auto *col : m_rightPane->miller()->cols())
      col->refreshStyle();

    m_leftPane->setFocused(activePane() == m_leftPane);
    m_rightPane->setFocused(activePane() == m_rightPane);

    m_leftPane->filePane()->view()->viewport()->update();
    m_rightPane->filePane()->view()->viewport()->update();
    for (QWidget *w : QApplication::topLevelWidgets()) {
      w->style()->unpolish(w);
      w->style()->polish(w);
      w->update();
    }
  });

  // Hot-Plug
  connect(Solid::DeviceNotifier::instance(),
          &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &) {
            m_leftPane->miller()->refreshDrives();
            m_rightPane->miller()->refreshDrives();
            m_sidebar->updateDrives();
          });
  connect(Solid::DeviceNotifier::instance(),
          &Solid::DeviceNotifier::deviceRemoved, this, [this](const QString &) {
            m_leftPane->miller()->refreshDrives();
            m_rightPane->miller()->refreshDrives();
            m_sidebar->updateDrives();
          });

  // KDirWatch für Mount-Punkte (zusätzlich zu Solid)
  m_fsWatcher = new KDirWatch(this);
  if (QDir("/run/media").exists()) {
    QString userPath = "/run/media/" + QString::fromLocal8Bit(qgetenv("USER"));
    if (QDir(userPath).exists()) {
      m_fsWatcher->addDir(userPath, KDirWatch::WatchSubDirs);
    } else {
      m_fsWatcher->addDir("/run/media", KDirWatch::WatchSubDirs);
    }
  }
  if (QDir("/media").exists()) {
    m_fsWatcher->addDir("/media", KDirWatch::WatchSubDirs);
  }
  connect(m_fsWatcher, &KDirWatch::dirty, this, [this](const QString &) {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });
  connect(m_fsWatcher, &KDirWatch::created, this, [this](const QString &) {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });
  connect(m_fsWatcher, &KDirWatch::deleted, this, [this](const QString &) {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });

  // Fallback: Timer für regelmäßige Aktualisierung
  QTimer *driveTimer = new QTimer(this);
  connect(driveTimer, &QTimer::timeout, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });
  driveTimer->start(SC_DRIVE_REFRESH_MS); // Alle 5 Sekunden

  connect(m_leftPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_rightPane->filePane()->setColumnVisible(colId, visible);
          });
  connect(m_rightPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_leftPane->filePane()->setColumnVisible(colId, visible);
          });

  // Delete-Taste direkt aus dem View abfangen
  connect(m_leftPane->filePane(), &FilePane::deleteRequested, this,
          [this](bool perm) { doDelete(m_leftPane, perm); });
  connect(m_rightPane->filePane(), &FilePane::deleteRequested, this,
          [this](bool perm) { doDelete(m_rightPane, perm); });

  // KIO-Einträge zu Laufwerken hinzufügen
  auto doAddToPlaces = [this](const QString &url, const QString &name) {
    auto s = Config::group("NetworkPlaces");
    QStringList saved = s.readEntry("places", QStringList());
    if (!saved.contains(url)) {
      saved << url;
      s.writeEntry("places", saved);
      s.writeEntry("name_" + QString(url).replace("/", "_").replace(":", "_"),
                   name);
      s.config()->sync();
    }

    m_sidebar->updateDrives();
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    emit m_sidebar->drivesChanged();
  };
  connect(m_leftPane->filePane(), &FilePane::addToPlacesRequested, this,
          doAddToPlaces);
  connect(m_rightPane->filePane(), &FilePane::addToPlacesRequested, this,
          doAddToPlaces);

  auto s = Config::group("UI");
  const QString mode = s.readEntry("startupPathMode", "last");

  QString leftPath, rightPath;
  const int behavior = Config::startupBehavior();

  if (behavior == 1) { // Letzte Sitzung
    leftPath = Config::lastLeftPath();
    rightPath = Config::lastRightPath();
    // Validierung lokaler Pfade
    if (leftPath.startsWith("/") && !QFileInfo::exists(leftPath))
      leftPath = QDir::homePath();
    if (rightPath.startsWith("/") && !QFileInfo::exists(rightPath))
      rightPath = QDir::homePath();
  } else if (behavior == 2) { // Dieser PC (Root)
    leftPath = "__drives__";
    rightPath = "__drives__";
  } else { // Home (0)
    leftPath = QDir::homePath();
    rightPath = QDir::homePath();
  }

  // Sonderwunsch: Miller oben bei Laufwerken, Liste unten bei Home
  m_leftPane->navigateTo("__drives__");
  m_rightPane->navigateTo("__drives__");

  // Jetzt die Liste unten auf Home setzen, aber OHNE Miller zu aktualisieren
  m_leftPane->navigateTo(QDir::homePath(), true, false);
  m_rightPane->navigateTo(QDir::homePath(), true, false);

  auto sUI = Config::group("UI");
  m_currentMode = sUI.readEntry("layoutMode", 1);
  applyLayout(m_currentMode);

  connect(m_panesSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
    auto ss = Config::group("UI");
    ss.writeEntry("panesSplitterState", m_panesSplitter->saveState());
    ss.config()->sync();
  });

  m_leftPane->setFocused(true);
  m_rightPane->setFocused(false);

  QTimer::singleShot(100, this, [this]() {
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
  });

  registerShortcuts();
}

void MainWindow::registerShortcuts() {
  // Beim ersten Aufruf: KActionCollection anlegen
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
  } else {
    // Bereits initialisiert: nur Settings neu einlesen (z.B. nach
    // KShortcutsDialog)
    m_actionCollection->readSettings();
  }
}

void PaneWidget::setMillerVisible(bool visible) {
  if (m_millerToggle) {
    m_millerToggle->setChecked(visible);
  }
}

void PaneWidget::setViewMode(int mode) {
  m_toolbar->setViewMode(mode);
  m_filePane->setViewMode(mode);
}


void PaneWidget::saveState() const {
  if (m_settingsKey.isEmpty() || !m_vSplit)
    return;
  auto s = Config::group("UI");
  // Größe speichern — aber nur wenn nicht gerade eingeklappt
  if (!m_millerCollapsed)
    s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
  s.writeEntry(m_settingsKey + "/millerCollapsed", m_millerCollapsed);
  s.writeEntry(m_settingsKey + "/currentPath", currentPath());
  s.config()->sync();
}

void MainWindow::saveWindowState() {
  auto s = Config::group("UI");

  // Fenster-Geometrie
  s.writeEntry("windowGeometry", saveGeometry());

  // Sidebar
  s.writeEntry("sidebarVisible", m_sidebar->isVisible());
  s.writeEntry("sidebarWidth", m_sidebar->width());

  // Pane-Splitter (links/rechts bzw. oben/unten)
  s.writeEntry("panesSplitterState", m_panesSplitter->saveState());

  // Beide Panes: Miller-Größe und collapsed-State
  m_leftPane->saveState();
  m_rightPane->saveState();

  s.config()->sync();
}

void MainWindow::closeEvent(QCloseEvent *e) {
  saveWindowState();

  // Session speichern falls aktiviert
  if (Config::startupBehavior() == 1) {
    Config::setLastPaths(m_leftPane->currentPath(), m_rightPane->currentPath());
  }

  QMainWindow::closeEvent(e);
}

void MainWindow::applyLayout(int mode) {
  m_currentMode = mode;
  auto s = Config::group("UI");
  s.writeEntry("layoutMode", mode);
  s.config()->sync();

  const int total =
      m_panesSplitter->width() > 10 ? m_panesSplitter->width() : 1000;

  switch (mode) {
  case 0: // Klassisch
    m_panesSplitter->setOrientation(Qt::Horizontal);
    m_rightPane->hide();
    m_leftPane->show();
    m_panesSplitter->setSizes({total, 0});
    break;
  case 1: // Standard Dual
    m_panesSplitter->setOrientation(Qt::Horizontal);
    m_leftPane->show();
    m_rightPane->show();
    m_panesSplitter->setSizes({total / 2, total / 2});
    break;
  case 2: // Spalten Dual
    m_panesSplitter->setOrientation(Qt::Vertical);
    m_leftPane->show();
    m_rightPane->show();
    {
      const int h =
          m_panesSplitter->height() > 10 ? m_panesSplitter->height() : 800;
      m_panesSplitter->setSizes({h / 2, h / 2});
    }
    break;
  }

  // Gespeicherten State wiederherstellen — nur beim initialen Aufruf
  if (!m_panesSplitterRestored) {
    m_panesSplitterRestored = true;
    const QByteArray paneState =
        s.readEntry("panesSplitterState", QByteArray());
    if (!paneState.isEmpty())
      m_panesSplitter->restoreState(paneState);
  }
}
// EOF - Force IDE Refresh
