// --- mainwindow_session.cpp -------------------------------------------------
// Session-Management: Wiederherstellen und Speichern von Fensterstatus,
// Tabs, Pfaden und Close-Event-Handling.
// ---------------------------------------------------------------------------

#include "mainwindow.h" // Force IDE re-index
#include "config.h"
#include <QMessageBox>
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <KActionCollection>
#include <KShortcutsDialog>
#include <KStandardShortcut>

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

#include "panewidget.h"

// --- MainWindow ---

void MainWindow::resolveSessionPaths(int behavior, const QString &configPath, const QString &lastLeft, const QString &lastRight, QString &leftPath, QString &rightPath)
{
    if (behavior == 0) // Letzte Sitzung
    {
        leftPath  = lastLeft;
        rightPath = lastRight;
    }
    else if (behavior == 1) // Dieser PC
    {
        leftPath  = QStringLiteral("__drives__");
        rightPath = QStringLiteral("__drives__");
    }
    else if (behavior == 2) // Fester Pfad
    {
        leftPath  = configPath;
        rightPath = configPath;
    }

    // Validierung lokaler Pfade
    if (behavior != 2)
    {
        if (leftPath.isEmpty() || (leftPath.startsWith(QLatin1Char('/')) && !QFileInfo::exists(leftPath)))
        {
            leftPath = QDir::homePath();
        }
        if (rightPath.isEmpty() || (rightPath.startsWith(QLatin1Char('/')) && !QFileInfo::exists(rightPath)))
        {
            rightPath = QDir::homePath();
        }
    }
}

void MainWindow::applySessionNavigation(int behavior, const QString &leftPath, const QString &rightPath)
{
    auto sUI = Config::group("UI");
    if (behavior == 0)
    {
        // Tabs aus letzter Sitzung laden
        const QStringList leftTabs  = sUI.readEntry("left/tabs",  QStringList());
        const QStringList rightTabs = sUI.readEntry("right/tabs", QStringList());
        m_leftPane->navigateTo(leftTabs.isEmpty()  ? leftPath  : leftTabs.first(),  false);
        m_rightPane->navigateTo(rightTabs.isEmpty() ? rightPath : rightTabs.first(), false);
    }
    else
    {
        m_leftPane->navigateTo(leftPath);
        m_rightPane->navigateTo(rightPath);
    }
    m_currentMode = sUI.readEntry("layoutMode", 1);
    applyLayout(m_currentMode);
}

void MainWindow::restoreSession()
{
    Q_ASSERT(m_leftPane != nullptr && m_rightPane != nullptr && m_panesSplitter != nullptr);
    const int behavior      = Config::startupBehavior();
    const QString configPath = Config::startupPath();
    const QString lastLeft   = Config::lastLeftPath();
    const QString lastRight  = Config::lastRightPath();

    QString leftPath;
    QString rightPath;
    resolveSessionPaths(behavior, configPath, lastLeft, lastRight, leftPath, rightPath);
    applySessionNavigation(behavior, leftPath, rightPath);

    connect(m_panesSplitter, &QSplitter::splitterMoved, this, [this](int, int)
    {
        auto ss = Config::group("UI");
        ss.writeEntry("panesSplitterState", m_panesSplitter->saveState());
        ss.config()->sync();
    });

    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
    QTimer::singleShot(100, this, [this]()
    {
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
    });
    registerShortcuts();
}

void MainWindow::saveWindowState()
{
    Q_ASSERT(m_sidebar != nullptr && m_panesSplitter != nullptr && m_leftPane != nullptr && m_rightPane != nullptr);
    auto s = Config::group("UI");

    // Fenster-Geometrie und maximierter Zustand
    s.writeEntry("windowGeometry", saveGeometry());
    s.writeEntry("windowMaximized", isMaximized());

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

void MainWindow::closeEvent(QCloseEvent *e)
{
    saveWindowState();

    // Session speichern falls aktiviert
    if (Config::startupBehavior() == 1)
    {
        Config::setLastPaths(m_leftPane->currentPath(), m_rightPane->currentPath());
    }

    QMainWindow::closeEvent(e);
}
