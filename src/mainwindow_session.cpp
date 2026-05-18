// --- mainwindow_session.cpp -------------------------------------------------
// Session-Management: Wiederherstellen und Speichern von Fensterstatus,
// Tabs, Pfaden und Close-Event-Handling.
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

void MainWindow::restoreSession() {
  QString leftPath, rightPath;
  const int behavior = Config::startupBehavior();
  const QString configPath = Config::startupPath();
  const QString lastLeft = Config::lastLeftPath();
  const QString lastRight = Config::lastRightPath();

  if (behavior == 0) { // Letzte Sitzung
    leftPath = lastLeft;
    rightPath = lastRight;
  } else if (behavior == 1) { // Dieser PC
    leftPath = "__drives__";
    rightPath = "__drives__";
  } else if (behavior == 2) { // Fester Pfad
    leftPath = configPath;
    rightPath = configPath;
  }
  
  // Validierung lokaler Pfade
  if (behavior != 2) {
      if (leftPath.isEmpty() || (leftPath.startsWith("/") && !QFileInfo::exists(leftPath)))
        leftPath = QDir::homePath();
      if (rightPath.isEmpty() || (rightPath.startsWith("/") && !QFileInfo::exists(rightPath)))
        rightPath = QDir::homePath();
  }

  m_leftPane->navigateTo(leftPath);
  m_rightPane->navigateTo(rightPath);

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

