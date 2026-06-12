// --- mainwindow.cpp — SplitCommander Hauptfenster ---

#include "mainwindow.h"
#include "filemanager1.h"
#include "config.h"
#include "settingsdialog.h"
#ifdef SC_PLUGIN_GIT
#include "plugins/git/gitmanagerdialog.h"
#include "plugins/git/gitstatusmanager.h"
#endif
#ifdef SC_PLUGIN_PAPERLESS
#include "plugins/paperless/paperlessmanager.h"
#endif
#include <QMessageBox>
#include "filepane.h"
#include "joboverlay.h"
#include "panecomponents.h"
#include <KDialogJobUiDelegate>
#include "thememanager.h"
#include <KActionCollection>

#include <KFileItem>
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/DeleteOrTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KJobWidgets>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>

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

#include "scglobal.h"
#include "drivemanager.h"
#include "panewidget.h"

// --- MainWindow ---
MainWindow::~MainWindow()
{
}

void MainWindow::registerJob(KJob *job, const QString &title)
{
    if (m_jobOverlay != nullptr)
    {
        m_jobOverlay->addJob(job, title);
    }

    connect(job, &KJob::result, this, [title](KJob *finishedJob)
    {
        if (finishedJob->error() == 0)
        {
            sc_notify(title, tr("Vorgang erfolgreich abgeschlossen."));
        }
        else
        {
            sc_notify(tr("Fehler bei: %1").arg(title), finishedJob->errorString(), QStringLiteral("dialog-error"));
        }
    });
}

void MainWindow::refreshAllDrives()
{
    Q_ASSERT(m_leftPane != nullptr && m_rightPane != nullptr && m_sidebar != nullptr);
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
}

void MainWindow::scheduleDriveRefresh()
{
    if (m_driveRefreshTimer == nullptr)
    {
        m_driveRefreshTimer = new QTimer(this);
        Q_ASSERT(m_driveRefreshTimer != nullptr);
        m_driveRefreshTimer->setSingleShot(true);
        m_driveRefreshTimer->setInterval(400);
        connect(m_driveRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshAllDrives);
    }
    m_driveRefreshTimer->start(); // Neustart wenn bereits läuft
}

PaneWidget *MainWindow::activePane() const
{
    if (m_leftPane != nullptr && m_leftPane->isFocused())
    {
        return m_leftPane;
    }
    return m_rightPane;
}

void MainWindow::doDelete(PaneWidget *pane, bool permanent)
{
    if (pane == nullptr)
    {
        pane = activePane();
    }
    Q_ASSERT(pane != nullptr);
    const QList<QUrl> urls = pane->filePane()->selectedUrls();
    if (urls.isEmpty())
    {
        return;
    }

    // Für KIO-Pfade (gdrive, smb etc.) immer direkt löschen — kein lokaler Trash
    const bool hasKioUrl = std::any_of(urls.begin(), urls.end(), [](const QUrl &u)
    {
        const QString s = u.scheme();
        return s != QStringLiteral("file") && s != QStringLiteral("trash") && !s.isEmpty();
    });
    const bool useDelete = permanent || hasKioUrl;

    auto *job = new KIO::DeleteOrTrashJob(urls, useDelete ? KIO::AskUserActionInterface::Delete : KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation, this);
    Q_ASSERT(job != nullptr);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoWarningHandlingEnabled, this));
    registerJob(job, tr("Löschen..."));
    job->start();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    initUI();
    initConnections();
    initTimers();
    QTimer::singleShot(50, this, &MainWindow::restoreSession);
}

void MainWindow::buildWindowProperties()
{
    setWindowTitle(QStringLiteral("SplitCommander"));
    auto s = Config::group("UI");
    const QByteArray geo = s.readEntry("windowGeometry", QByteArray());
    if (!geo.isEmpty())
    {
        restoreGeometry(geo);
    }
    else
    {
        resize(1280, 900);
    }

    // Maximierten Zustand wiederherstellen
    if (s.readEntry("windowMaximized", false))
    {
        showMaximized();
    }
}

void MainWindow::buildSidebar(QHBoxLayout *rootLay, QWidget *central)
{
    m_sidebar = new Sidebar(this);
    Q_ASSERT(m_sidebar != nullptr);
    m_jobOverlay = new JobOverlay(this);
    Q_ASSERT(m_jobOverlay != nullptr);
    
    auto s = Config::group("UI");
    const int sidebarW = s.readEntry("sidebarWidth", 250);
    const bool sidebarVis = s.readEntry("sidebarVisible", true);
    m_sidebar->setFixedWidth(sidebarW);
    m_sidebar->setVisible(sidebarVis);

    rootLay->addWidget(m_sidebar);
    auto *sidebarHandle = new SidebarHandle(m_sidebar, central);
    Q_ASSERT(sidebarHandle != nullptr);
    
    sidebarHandle->setFixedWidth(sidebarVis ? 10 : 32);
    if (!sidebarVis)
    {
        sidebarHandle->setCursor(Qt::PointingHandCursor);
    }
    rootLay->addWidget(sidebarHandle);
}

void MainWindow::buildPanes(QHBoxLayout *rootLay, QWidget *central)
{
    m_panesSplitter = new PaneSplitter(Qt::Horizontal, central);
    Q_ASSERT(m_panesSplitter != nullptr);
    m_panesSplitter->setHandleWidth(12);
    m_panesSplitter->setChildrenCollapsible(true);
    m_panesSplitter->setStyleSheet(TM().ssSplitter());

    m_leftPane = new PaneWidget(QStringLiteral("leftPane"));
    Q_ASSERT(m_leftPane != nullptr);
    m_rightPane = new PaneWidget(QStringLiteral("rightPane"));
    Q_ASSERT(m_rightPane != nullptr);
    m_panesSplitter->addWidget(m_leftPane);
    m_panesSplitter->addWidget(m_rightPane);
    rootLay->addWidget(m_panesSplitter, 1);
}

void MainWindow::initUI()
{
    buildWindowProperties();

    auto *central = new QWidget(this);
    Q_ASSERT(central != nullptr);
    setCentralWidget(central);
    
    auto *rootLay = new QHBoxLayout(central);
    Q_ASSERT(rootLay != nullptr);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    buildSidebar(rootLay, central);
    buildPanes(rootLay, central);
}

void MainWindow::initConnections()
{
    m_fileManager1 = new FileManager1(this);
    Q_ASSERT(m_fileManager1 != nullptr);
    connect(m_fileManager1, &FileManager1::showFoldersRequested, this, [this](const QStringList &uriList)
    {
        if (uriList.isEmpty())
        {
            return;
        }
        raise();
        activateWindow();
        const QString path = QUrl(uriList.first()).toLocalFile();
        if (!path.isEmpty())
        {
            activePane()->navigateTo(path);
        }
    });
    connect(m_fileManager1, &FileManager1::showItemsRequested, this, [this](const QStringList &uriList)
    {
        if (uriList.isEmpty())
        {
            return;
        }
        raise();
        activateWindow();
        const QString path = QUrl(uriList.first()).toLocalFile();
        if (!path.isEmpty())
        {
            activePane()->navigateTo(QFileInfo(path).absolutePath());
        }
    });

    connect(m_leftPane, &PaneWidget::focusRequested, this, [this]()
    {
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
    });
    connect(m_rightPane, &PaneWidget::focusRequested, this, [this]()
    {
        m_rightPane->setFocused(true);
        m_leftPane->setFocused(false);
    });

    connectPaneSignals(m_leftPane, m_rightPane);
    connectPaneSignals(m_rightPane, m_leftPane);

    connectSidebarSignals();
    connectSystemNotifications();
    connectFileWatcher();
}

void MainWindow::connectPaneSignals(PaneWidget *pane, PaneWidget *other)
{
    Q_ASSERT(pane != nullptr && other != nullptr);
    connect(pane, &PaneWidget::newFolderRequested, this, [this, pane]()
    {
        bool ok;
        QString name = DialogUtils::getText(this, tr("Neuer Ordner"), tr("Ordnername:"), tr("Neuer Ordner"), &ok);
        if (!ok || name.isEmpty())
        {
            return;
        }
        QUrl mkdirUrl = pane->currentUrl();
        mkdirUrl.setPath(mkdirUrl.path().chopped(mkdirUrl.path().endsWith('/') ? 1 : 0) + '/' + name);
        auto *job = KIO::mkdir(mkdirUrl);
        Q_ASSERT(job != nullptr);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        connect(job, &KJob::result, this, [this, job]()
        {
            if (job->error())
            {
                DialogUtils::message(this, tr("Fehler"), tr("Ordner konnte nicht erstellt werden."));
            }
        });
    });
    connect(pane, &PaneWidget::hiddenFilesToggled, this, [this](bool show)
    {
        emit m_sidebar->hiddenFilesChanged(show);
        m_leftPane->filePane()->setShowHiddenFiles(show);
        m_rightPane->filePane()->setShowHiddenFiles(show);
        m_leftPane->miller()->refresh();
        m_rightPane->miller()->refresh();
    });
    connect(pane, &PaneWidget::extensionsToggled, this, [this](bool on)
    {
        Config::setShowFileExtensions(on);
        m_leftPane->filePane()->setRootPath(m_leftPane->currentPath());
        m_rightPane->filePane()->setRootPath(m_rightPane->currentPath());
    });
    connect(pane, &PaneWidget::settingsChanged, this, [this]()
    {
        emit m_sidebar->settingsChanged();
    });
    connect(pane, &PaneWidget::copyToOtherPaneRequested, this, [this, pane, other]()
    {
        const QList<QUrl> urls = pane->filePane()->selectedUrls();
        if (urls.isEmpty() || other->currentPath().isEmpty())
        {
            return;
        }
        auto *job = KIO::copy(urls, other->currentUrl(), KIO::DefaultFlags);
        Q_ASSERT(job != nullptr);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
        registerJob(job, tr("Kopiere Dateien..."));
    });
}

void MainWindow::handleSolidDeviceMount(const QString &path, const std::function<void(const QString&)> &navigate)
{
    Solid::Device dev(path.mid(6));
    auto *acc = dev.as<Solid::StorageAccess>();
    if (acc == nullptr) return;
    if (acc->isAccessible())
    {
        refreshAllDrives();
        navigate(acc->filePath());
    }
    else
    {
        connect(acc, &Solid::StorageAccess::setupDone, this, [acc, navigate](Solid::ErrorType, QVariant errData, const QString &)
        {
            if (acc->isAccessible())
            {
                DriveManager::instance()->refreshAll();
                navigate(acc->filePath());
                sc_notify(tr("Laufwerk bereit"), tr("Das Laufwerk wurde erfolgreich eingebunden."), QStringLiteral("media-removable"));
            }
            else
            {
                sc_notify(tr("Fehler beim Einbinden"), tr("Das Laufwerk konnte nicht eingebunden werden:\n%1").arg(errData.toString()), QStringLiteral("dialog-warning"));
            }
        }, Qt::SingleShotConnection);
        acc->setup();
    }
}

void MainWindow::mountAndNavigateDrive(const QString &path, bool leftPane)
{
    auto navigate = [this, leftPane](const QString &p)
    {
        if (leftPane)
        {
            m_leftPane->navigateTo(p);
            m_leftPane->setFocused(true);
            m_rightPane->setFocused(false);
        }
        else
        {
            m_rightPane->navigateTo(p);
            m_rightPane->setFocused(true);
            m_leftPane->setFocused(false);
        }
    };

    if (path == QStringLiteral("remote:/"))
    {
        PaneWidget *pane = leftPane ? m_leftPane : m_rightPane;
        Q_ASSERT(pane != nullptr);
        pane->setViewMode(0);                // Details
        pane->navigateTo(path, true, false); // false = Miller-Spalten nicht aktualisieren
        pane->setFocused(true);
        if (leftPane) m_rightPane->setFocused(false);
        else m_leftPane->setFocused(false);
        return;
    }

    if (path.startsWith(QStringLiteral("solid:")))
    {
        handleSolidDeviceMount(path, navigate);
    }
    else
    {
        navigate(path);
    }
}

void MainWindow::connectSidebarDriveClicks()
{
    connect(m_sidebar, &Sidebar::driveClicked, this, [this](const QString &path)
    {
        mountAndNavigateDrive(path, !m_rightPane->isFocused());
    });
    connect(m_sidebar, &Sidebar::driveClickedLeft, this, [this](const QString &path)
    {
        mountAndNavigateDrive(path, true); // immer links
    });
    connect(m_sidebar, &Sidebar::driveClickedRight, this, [this](const QString &path)
    {
        mountAndNavigateDrive(path, false); // immer rechts
    });
}

void MainWindow::connectPaneOpenRequests()
{
    for (auto *pane : {m_leftPane, m_rightPane})
    {
        Q_ASSERT(pane != nullptr);
        connect(pane, &PaneWidget::openInLeftRequested, this, [this](const QString &path)
        {
            m_leftPane->navigateTo(path);
            m_leftPane->setFocused(true);
            m_rightPane->setFocused(false);
        });
        connect(pane, &PaneWidget::openInRightRequested, this, [this](const QString &path)
        {
            m_rightPane->navigateTo(path);
            m_rightPane->setFocused(true);
            m_leftPane->setFocused(false);
        });
    }
}

void MainWindow::connectSidebarMiscSignals()
{
    connect(m_sidebar, &Sidebar::addCurrentPathToPlaces, this, [this]()
    {
        m_sidebar->addPlace(m_leftPane->currentPath());
    });
    connect(m_sidebar, &Sidebar::requestActivePath, this, [this](QString *out)
    {
        if (out != nullptr)
        {
            *out = m_leftPane->currentPath();
        }
    });
    connect(m_sidebar, &Sidebar::layoutChangeRequested, this, &MainWindow::applyLayout);
    connect(m_leftPane, &PaneWidget::layoutChangeRequested, this, &MainWindow::applyLayout);
    connect(m_rightPane, &PaneWidget::layoutChangeRequested, this, &MainWindow::applyLayout);
}

void MainWindow::connectSidebarSignals()
{
    Q_ASSERT(m_sidebar != nullptr && m_leftPane != nullptr && m_rightPane != nullptr);
    connectSidebarDriveClicks();
    connectPaneOpenRequests();
    connectSidebarMiscSignals();
}

void MainWindow::connectSystemNotifications()
{
    Q_ASSERT(m_sidebar != nullptr && m_leftPane != nullptr && m_rightPane != nullptr);
    connect(m_sidebar, &Sidebar::tagClicked, this, [this](const QString &tagName)
    {
        auto *pane = activePane();
        Q_ASSERT(pane != nullptr);
        const QString cur = pane->currentPath();
        const bool isKio = !cur.startsWith(QLatin1Char('/')) && cur.contains(QStringLiteral(":/"));
        if (isKio)
        {
            pane->filePane()->setRootPath(QDir::homePath());
        }
        pane->filePane()->showTaggedFiles(tagName);
    });
    connect(m_sidebar, &Sidebar::drivesChanged, this, [this]()
    {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
    });

    auto doRemoveFromPlaces = [this](const QString &url)
    {
        if (!url.isEmpty())
        {
            auto s = Config::group("NetworkPlaces");
            QStringList saved = s.readEntry("places", QStringList());
            saved.removeAll(url);
            QUrl qurl(url);
            qurl.setUserInfo(QString());
            saved.removeAll(qurl.toString());
            s.writeEntry("places", saved);
            s.config()->sync();
        }
        if (!url.isEmpty())
        {
            const QString normUrl = mw_normalizePath(url);
            for (auto *pane : {m_leftPane, m_rightPane})
            {
                Q_ASSERT(pane != nullptr);
                const QString current = mw_normalizePath(pane->currentPath());
                if (current == normUrl || current.startsWith(normUrl + QLatin1Char('/')))
                {
                    pane->navigateTo(QStringLiteral("__drives__"));
                }
            }
        }
        refreshAllDrives();
        emit m_sidebar->drivesChanged();
    };

    connect(m_leftPane->miller(), &MillerArea::removeFromPlacesRequested, this, doRemoveFromPlaces);
    connect(m_rightPane->miller(), &MillerArea::removeFromPlacesRequested, this, doRemoveFromPlaces);
    connect(m_sidebar, &Sidebar::removeFromPlacesRequested, this, doRemoveFromPlaces);
}

void MainWindow::connectUnmountRequested()
{
    connect(m_sidebar, &Sidebar::unmountRequested, this, [this](const QString &path)
    {
        const QString normPath = mw_normalizePath(path);
        const bool leftPaneOnMount = !normPath.isEmpty() && (mw_normalizePath(m_leftPane->currentPath()) == normPath || mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + QLatin1Char('/')));
        const bool rightPaneOnMount = !normPath.isEmpty() && (mw_normalizePath(m_rightPane->currentPath()) == normPath || mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + QLatin1Char('/')));
        const bool leftMillerOnMount = !normPath.isEmpty() && (mw_normalizePath(m_leftPane->miller()->activePath()) == normPath || mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + QLatin1Char('/')));
        const bool rightMillerOnMount = !normPath.isEmpty() && (mw_normalizePath(m_rightPane->miller()->activePath()) == normPath || mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + QLatin1Char('/')));

        auto *proc = new QProcess(this);
        Q_ASSERT(proc != nullptr);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, proc, leftPaneOnMount, rightPaneOnMount, leftMillerOnMount, rightMillerOnMount](int exitCode, QProcess::ExitStatus)
        {
            if (exitCode == 0)
            {
                if (leftPaneOnMount) m_leftPane->navigateTo(QStringLiteral("__drives__"));
                if (rightPaneOnMount) m_rightPane->navigateTo(QStringLiteral("__drives__"));
                if (leftMillerOnMount) m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
                else m_leftPane->miller()->refreshDrives();
                if (rightMillerOnMount) m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));
                else m_rightPane->miller()->refreshDrives();
            }
            m_sidebar->updateDrives();
            proc->deleteLater();
        });
        proc->start(QStringLiteral("umount"), {path});
    });
}

void MainWindow::connectTeardownRequested()
{
    auto doTeardown = [this](const QString &udi) { executeDeviceTeardown(udi); };

    connect(m_leftPane->miller(), &MillerArea::teardownRequested, this, doTeardown);
    connect(m_rightPane->miller(), &MillerArea::teardownRequested, this, doTeardown);
    connect(m_sidebar, &Sidebar::teardownRequested, this, doTeardown);

    connect(m_leftPane->miller(), &MillerArea::drivesChanged, m_sidebar, &Sidebar::updateDrives);
    connect(m_rightPane->miller(), &MillerArea::drivesChanged, m_sidebar, &Sidebar::updateDrives);
    QTimer::singleShot(2000, this, [this]()
    {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
    });
}

void MainWindow::executeDeviceTeardown(const QString &udi)
{
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (acc == nullptr)
    {
        return;
    }

    const QString mountPoint = acc->filePath();
    const QString normPath = mw_normalizePath(mountPoint);

    const bool leftPaneOnMount = !normPath.isEmpty() && (mw_normalizePath(m_leftPane->currentPath()) == normPath || mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + QLatin1Char('/')));
    const bool rightPaneOnMount = !normPath.isEmpty() && (mw_normalizePath(m_rightPane->currentPath()) == normPath || mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + QLatin1Char('/')));
    const bool leftMillerOnMount = !normPath.isEmpty() && (mw_normalizePath(m_leftPane->miller()->activePath()) == normPath || mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + QLatin1Char('/')));
    const bool rightMillerOnMount = !normPath.isEmpty() && (mw_normalizePath(m_rightPane->miller()->activePath()) == normPath || mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + QLatin1Char('/')));

    m_leftPane->filePane()->stopLister();
    m_rightPane->filePane()->stopLister();

    if (leftPaneOnMount) m_leftPane->navigateTo(QDir::homePath());
    if (rightPaneOnMount) m_rightPane->navigateTo(QDir::homePath());
    if (leftMillerOnMount) m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
    if (rightMillerOnMount) m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));

    connect(acc, &Solid::StorageAccess::teardownDone, this, [this, leftMillerOnMount, rightMillerOnMount](Solid::ErrorType err, QVariant errData, const QString &)
    {
        if (err != Solid::NoError) sc_notify(tr("Aushängen fehlgeschlagen"), tr("Das Laufwerk konnte nicht ausgehängt werden:\n%1").arg(errData.toString()), QStringLiteral("dialog-warning"));
        else sc_notify(tr("Laufwerk sicher entfernt"), tr("Sie können das Gerät jetzt sicher abziehen."), QStringLiteral("media-eject"));
        
        if (!leftMillerOnMount) m_leftPane->miller()->refreshDrives();
        if (!rightMillerOnMount) m_rightPane->miller()->refreshDrives();
        m_sidebar->updateDrives();
    }, Qt::SingleShotConnection);

    auto *timer = new QTimer(this);
    Q_ASSERT(timer != nullptr);
    timer->setSingleShot(true);
    timer->setInterval(8000);
    connect(timer, &QTimer::timeout, this, [this, mountPoint, timer]()
    {
        timer->deleteLater();
        auto *proc = new QProcess(this);
        Q_ASSERT(proc != nullptr);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, proc](int, QProcess::ExitStatus)
        {
            refreshAllDrives();
            proc->deleteLater();
        });
        proc->start(QStringLiteral("umount"), {mountPoint});
    });
    connect(acc, &Solid::StorageAccess::teardownDone, timer, &QTimer::stop, Qt::SingleShotConnection);
    timer->start();

    acc->teardown();
}

void MainWindow::connectDriveSettingsAndThemes()
{
    connect(m_sidebar, &Sidebar::hiddenFilesChanged, this, [this](bool)
    {
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
        for (auto *col : m_leftPane->miller()->cols()) col->populateDir(col->path());
        for (auto *col : m_rightPane->miller()->cols()) col->populateDir(col->path());
    });
    
    connect(m_sidebar, &Sidebar::settingsChanged, this, [this]()
    {
        m_sidebar->applyIconSizes();
#ifdef SC_PLUGIN_GIT
        GitStatusManager::instance().reloadConfig();
        m_sidebar->refreshGitSection();
#endif
        for (auto *col : m_leftPane->miller()->cols()) col->refreshStyle();
        for (auto *col : m_rightPane->miller()->cols()) col->refreshStyle();
    });
    
    connect(&TM(), &ThemeManager::themeChanged, this, [this]()
    {
        for (auto *col : m_leftPane->miller()->cols()) col->refreshStyle();
        for (auto *col : m_rightPane->miller()->cols()) col->refreshStyle();

        m_leftPane->setFocused(activePane() == m_leftPane);
        m_rightPane->setFocused(activePane() == m_rightPane);

        m_leftPane->filePane()->view()->viewport()->update();
        m_rightPane->filePane()->view()->viewport()->update();
        for (QWidget *w : QApplication::topLevelWidgets())
        {
            w->style()->unpolish(w);
            w->style()->polish(w);
            w->update();
        }
    });
}

void MainWindow::connectDeviceNotifierAndWatcher()
{
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &)
    {
        scheduleDriveRefresh();
    });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, this, [this](const QString &)
    {
        scheduleDriveRefresh();
    });

    m_fsWatcher = new KDirWatch(this);
    Q_ASSERT(m_fsWatcher != nullptr);
    if (QDir(QStringLiteral("/run/media")).exists())
    {
        QString userPath = QStringLiteral("/run/media/") + QString::fromLocal8Bit(qgetenv("USER"));
        if (QDir(userPath).exists())
        {
            m_fsWatcher->addDir(userPath, KDirWatch::WatchSubDirs);
        }
        else
        {
            m_fsWatcher->addDir(QStringLiteral("/run/media"), KDirWatch::WatchSubDirs);
        }
    }
    if (QDir(QStringLiteral("/media")).exists())
    {
        m_fsWatcher->addDir(QStringLiteral("/media"), KDirWatch::WatchSubDirs);
    }
    connect(m_fsWatcher, &KDirWatch::dirty, this, [this](const QString &) { scheduleDriveRefresh(); });
    connect(m_fsWatcher, &KDirWatch::created, this, [this](const QString &) { scheduleDriveRefresh(); });
    connect(m_fsWatcher, &KDirWatch::deleted, this, [this](const QString &) { scheduleDriveRefresh(); });
}

void MainWindow::connectFileWatcher()
{
    Q_ASSERT(m_sidebar != nullptr && m_leftPane != nullptr && m_rightPane != nullptr);
    connectUnmountRequested();
    connectTeardownRequested();
    connectDriveSettingsAndThemes();
    connectDeviceNotifierAndWatcher();
}

void MainWindow::initTimers()
{
    auto *driveTimer = new QTimer(this);
    Q_ASSERT(driveTimer != nullptr);
    connect(driveTimer, &QTimer::timeout, this, [this]()
    {
        refreshAllDrives();
    });
    driveTimer->start(30000); // 30s Fallback — Hot-Plug kommt über Solid-Events

    connect(m_leftPane->filePane(), &FilePane::columnsChanged, this, [this](int colId, bool visible)
    {
        m_rightPane->filePane()->setColumnVisible(colId, visible);
    });
    connect(m_rightPane->filePane(), &FilePane::columnsChanged, this, [this](int colId, bool visible)
    {
        m_leftPane->filePane()->setColumnVisible(colId, visible);
    });

    // Delete-Taste direkt aus dem View abfangen
    connect(m_leftPane->filePane(), &FilePane::deleteRequested, this, [this](bool perm)
    {
        doDelete(m_leftPane, perm);
    });
    connect(m_rightPane->filePane(), &FilePane::deleteRequested, this, [this](bool perm)
    {
        doDelete(m_rightPane, perm);
    });

    // KIO-Einträge zu Laufwerken hinzufügen
    auto doAddToPlaces = [this](const QString &url, const QString &name)
    {
        auto s = Config::group("NetworkPlaces");
        QStringList saved = s.readEntry("places", QStringList());
        if (!saved.contains(url))
        {
            saved << url;
            s.writeEntry("places", saved);
            s.writeEntry("name_" + QString(url).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')), name);
            s.config()->sync();
        }

        refreshAllDrives();
        emit m_sidebar->drivesChanged();
    };
    connect(m_leftPane->filePane(), &FilePane::addToPlacesRequested, this, doAddToPlaces);
    connect(m_rightPane->filePane(), &FilePane::addToPlacesRequested, this, doAddToPlaces);
}

void MainWindow::applyLayout(int mode)
{
    m_currentMode = mode;
    auto s = Config::group("UI");
    s.writeEntry("layoutMode", mode);
    s.config()->sync();

    const int total = m_panesSplitter->width() > 10 ? m_panesSplitter->width() : 1000;

    switch (mode)
    {
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
                const int h = m_panesSplitter->height() > 10 ? m_panesSplitter->height() : 800;
                m_panesSplitter->setSizes({h / 2, h / 2});
            }
            break;
    }

    // Gespeicherten State wiederherstellen — nur beim initialen Aufruf
    if (!m_panesSplitterRestored)
    {
        m_panesSplitterRestored = true;
        const QByteArray paneState = s.readEntry("panesSplitterState", QByteArray());
        if (!paneState.isEmpty())
        {
            m_panesSplitter->restoreState(paneState);
        }
    }
}

void MainWindow::openSettings(int page)
{
    auto *dlg = new SettingsDialog(this);
    Q_ASSERT(dlg != nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::settingsChanged, this, [this]()
    {
        emit m_sidebar->settingsChanged();
        
        const int iconSize = Config::listIconSize();
        m_leftPane->filePane()->view()->setIconSize(QSize(iconSize, iconSize));
        m_rightPane->filePane()->view()->setIconSize(QSize(iconSize, iconSize));
        
        // Vollständige Aktualisierung um versteckte Dateien, Erweiterungen und Verhalten zu übernehmen
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
        
        for (auto *col : m_leftPane->miller()->cols())
        {
            col->populateDir(col->path());
        }
        for (auto *col : m_rightPane->miller()->cols())
        {
            col->populateDir(col->path());
        }
        
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();

        m_leftPane->filePane()->view()->viewport()->update();
        m_rightPane->filePane()->view()->viewport()->update();
    });
    if (page >= 0)
    {
        dlg->showPage(static_cast<SettingsDialog::Page>(page));
    }
    else
    {
        dlg->show();
    }
}

#ifdef SC_PLUGIN_GIT
void MainWindow::openGitManager()
{
    auto *dlg = new GitManagerDialog(activePane()->currentPath(), this);
    Q_ASSERT(dlg != nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &GitManagerDialog::settingsChanged, m_sidebar, &Sidebar::settingsChanged);
    dlg->show();
}
#endif // SC_PLUGIN_GIT

#ifdef SC_PLUGIN_PAPERLESS
void MainWindow::openPaperlessManager()
{
    auto *dlg = new PaperlessManagerDialog(activePane()->currentPath(), this);
    Q_ASSERT(dlg != nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
#endif // SC_PLUGIN_PAPERLESS
