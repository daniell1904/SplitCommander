// --- mainwindow_shortcuts.cpp -----------------------------------------------
// Tastatur-Shortcuts für MainWindow.
// ---------------------------------------------------------------------------

#include "mainwindow.h"
#include "config.h"
#ifdef SC_PLUGIN_GIT
#include "plugins/git/gitmanagerdialog.h"
#include "plugins/git/gitstatusmanager.h"
#endif
#include <QMessageBox>
#include "filepane.h"
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

#include "dialogutils.h"
#include "batchrenamer.h"
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

#include "millercolumn.h"
#include "panewidget.h"

// --- MainWindow ---

void MainWindow::registerShortcuts()
{
    if (m_actionCollection != nullptr)
    {
        m_actionCollection->readSettings();
        return;
    }

    m_actionCollection = new KActionCollection(this, QStringLiteral("splitcommander"));
    Q_ASSERT(m_actionCollection != nullptr);
    m_actionCollection->setComponentDisplayName(tr("SplitCommander"));

    // --- Hilfsmakro: Aktion anlegen ---
    // Qt::ApplicationShortcut: greift immer, egal welches Widget den Fokus hat
    auto addAct = [this](const QString &id, const QString &label,
                         const QString &icon, const QKeySequence &defKey,
                         std::function<void()> fn,
                         const QKeySequence &altKey = {}) -> QAction *
    {
        auto *a = m_actionCollection->addAction(id);
        Q_ASSERT(a != nullptr);
        a->setText(label);
        if (!icon.isEmpty())
        {
            a->setIcon(QIcon::fromTheme(icon));
        }
        a->setShortcutContext(Qt::ApplicationShortcut);
        if (altKey.isEmpty())
        {
            m_actionCollection->setDefaultShortcut(a, defKey);
        }
        else
        {
            m_actionCollection->setDefaultShortcuts(a, {defKey, altKey});
        }
        connect(a, &QAction::triggered, this, fn);
        return a;
    };

    ShortcutRegistrar registrar{ addAct };
    registerNavigationShortcuts(registrar);
    registerTabShortcuts(registrar);
    registerPaneShortcuts(registrar);
    registerFileShortcuts(registrar);
    registerViewShortcuts(registrar);

    m_actionCollection->readSettings();

    for (QAction *a : m_actionCollection->actions())
    {
        Q_ASSERT(a != nullptr);
        addAction(a);
    }

    m_leftPane->setActionCollection(m_actionCollection);
    m_rightPane->setActionCollection(m_actionCollection);
}

void MainWindow::registerNavigationShortcuts(const ShortcutRegistrar &addAct)
{
    addAct(QStringLiteral("nav_back"), tr("Zurück"), QStringLiteral("go-previous"), Qt::ALT | Qt::Key_Left, [this]()
    {
        auto *p = activePane();
        Q_ASSERT(p != nullptr);
        if (!p->histBack().isEmpty())
        {
            p->histFwd().push(p->currentPath());
            p->navigateTo(p->histBack().pop(), false);
        }
    });
    addAct(QStringLiteral("nav_forward"), tr("Vorwärts"), QStringLiteral("go-next"), Qt::ALT | Qt::Key_Right, [this]()
    {
        auto *p = activePane();
        Q_ASSERT(p != nullptr);
        if (!p->histFwd().isEmpty())
        {
            p->histBack().push(p->currentPath());
            p->navigateTo(p->histFwd().pop(), false);
        }
    });
    addAct(QStringLiteral("nav_up"), tr("Übergeordneter Ordner"), QStringLiteral("go-up"), Qt::ALT | Qt::Key_Up, [this]()
    {
        const QUrl url = activePane()->currentUrl();
        if (url.isLocalFile())
        {
            QDir d(url.toLocalFile());
            if (d.cdUp())
            {
                activePane()->navigateTo(d.absolutePath());
            }
        }
        else
        {
            // KIO-URL: Elternpfad über URL-Manipulation
            QUrl parent = url.adjusted(QUrl::StripTrailingSlash | QUrl::RemoveFilename);
            if (parent.isValid() && parent != url)
            {
                activePane()->navigateTo(parent.toString());
            }
        }
    });
    addAct(QStringLiteral("nav_home"), tr("Home-Verzeichnis"), QStringLiteral("go-home"), Qt::ALT | Qt::Key_Home, [this]()
    {
        activePane()->navigateTo(QDir::homePath());
    });
    addAct(QStringLiteral("nav_reload"), tr("Neu laden"), QStringLiteral("view-refresh"), Qt::CTRL | Qt::Key_R, [this]()
    {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
    }, Qt::Key_F5);
}

void MainWindow::registerTabShortcuts(const ShortcutRegistrar &addAct)
{
    addAct(QStringLiteral("tab_new"), tr("Neuer Tab"), QStringLiteral("tab-new"), Qt::CTRL | Qt::Key_T, [this]()
    {
        activePane()->addTab(activePane()->currentPath());
    });
    addAct(QStringLiteral("tab_close"), tr("Tab schließen"), QString(), Qt::CTRL | Qt::Key_W, [this]()
    {
        activePane()->closeTab(activePane()->currentTabIndex());
    });
    for (int i = 1; i <= 9; ++i)
    {
        const int idx = i - 1;
        addAct(QStringLiteral("tab_%1").arg(i), tr("Tab %1").arg(i), QString(), QKeySequence(Qt::CTRL | (Qt::Key_1 + idx)), [this, idx]()
        {
            if (idx < activePane()->tabCount())
            {
                activePane()->switchTab(idx);
            }
        });
    }
}

void MainWindow::registerPaneShortcuts(const ShortcutRegistrar &addAct)
{
    addAct(QStringLiteral("pane_focus_left"), tr("Linke Pane fokussieren"), QStringLiteral("go-first"), Qt::CTRL | Qt::Key_Left, [this]()
    {
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
    });
    addAct(QStringLiteral("pane_focus_right"), tr("Rechte Pane fokussieren"), QStringLiteral("go-last"), Qt::CTRL | Qt::Key_Right, [this]()
    {
        m_rightPane->setFocused(true);
        m_leftPane->setFocused(false);
    });
    addAct(QStringLiteral("pane_swap"), tr("Panes tauschen"), QStringLiteral("view-split-left-right"), Qt::CTRL | Qt::Key_U, [this]()
    {
        const QString l = m_leftPane->currentPath();
        const QString r = m_rightPane->currentPath();
        m_leftPane->navigateTo(r);
        m_rightPane->navigateTo(l);
    });
    addAct(QStringLiteral("pane_sync"), tr("Pfade synchronisieren"), QStringLiteral("view-refresh"), Qt::CTRL | Qt::SHIFT | Qt::Key_S, [this]()
    {
        m_rightPane->navigateTo(m_leftPane->currentPath());
    });
}

void MainWindow::registerFileShortcuts(const ShortcutRegistrar &addAct)
{
    addAct(QStringLiteral("file_rename"), tr("Umbenennen"), QStringLiteral("edit-rename"), Qt::Key_F2, [this]()
    {
        const QList<QUrl> urls = activePane()->selectedUrls();
        if (urls.isEmpty())
        {
            return;
        }

        if (urls.size() == 1)
        {
            // Einzelne Datei: einfacher Inline-Dialog
            const QUrl &src = urls.first();
            const QString oldName = src.fileName();
            bool ok;
            QString newName = DialogUtils::getText(this, tr("Umbenennen"), tr("Neuer Name:"), oldName, &ok);
            if (!ok || newName.isEmpty() || newName == oldName)
            {
                return;
            }
            QUrl dest = src.adjusted(QUrl::RemoveFilename);
            dest.setPath(dest.path() + newName);
            KIO::moveAs(src, dest, KIO::DefaultFlags);
        }
        else
        {
            // Mehrfachauswahl: BatchRenamer
            QStringList paths;
            for (const QUrl &u : urls)
            {
                paths << (u.isLocalFile() ? u.toLocalFile() : u.toString());
            }
            BatchRenamer dlg(paths, this);
            if (dlg.exec() != QDialog::Accepted)
            {
                return;
            }
            const QStringList newNames = dlg.newNames();
            for (int i = 0; i < urls.size() && i < newNames.size(); ++i)
            {
                const QString &nn = newNames.at(i);
                const QString oldName = urls.at(i).fileName();
                if (nn.isEmpty() || nn == oldName)
                {
                    continue;
                }
                QUrl dest = urls.at(i).adjusted(QUrl::RemoveFilename);
                dest.setPath(dest.path() + nn);
                auto *job = KIO::moveAs(urls.at(i), dest, KIO::DefaultFlags);
                Q_ASSERT(job != nullptr);
                KJobWidgets::setWindow(job, this);
            }
        }
    });

    auto checkInMiller = [this]() -> bool
    {
        QWidget* fw = focusWidget();
        bool inMiller = false;
        while (fw != nullptr)
        {
            if (qobject_cast<MillerArea*>(fw) != nullptr)
            {
                inMiller = true;
                break;
            }
            fw = fw->parentWidget();
        }
        return inMiller;
    };

    addAct(QStringLiteral("file_trash"), tr("In den Papierkorb verschieben"), QStringLiteral("user-trash"), Qt::Key_Delete, [this, checkInMiller]()
    {
        if (!checkInMiller())
        {
            doDelete(activePane(), false);
        }
    });

    addAct(QStringLiteral("file_delete"), tr("Löschen"), QStringLiteral("edit-delete"), QKeySequence(Qt::SHIFT | Qt::Key_Delete), [this, checkInMiller]()
    {
        if (!checkInMiller())
        {
            doDelete(activePane(), true);
        }
    });

    addAct(QStringLiteral("file_newfolder"), tr("Neuer Ordner"), QStringLiteral("folder-new"), Qt::Key_F7, [this]()
    {
        emit activePane()->newFolderRequested();
    });

    addAct(QStringLiteral("file_copy"), tr("Kopieren (Zwischenablage)"), QStringLiteral("edit-copy"), KStandardShortcut::copy().first(), [this]()
    {
        const QList<QUrl> urls = activePane()->selectedUrls();
        if (urls.isEmpty())
        {
            return;
        }
        auto *mime = new QMimeData();
        Q_ASSERT(mime != nullptr);
        mime->setUrls(urls);
        mime->setData(QStringLiteral("x-kde-cut-selection"), QByteArray("0"));
        QGuiApplication::clipboard()->setMimeData(mime);
    });

    addAct(QStringLiteral("file_move"), tr("Ausschneiden (Zwischenablage)"), QStringLiteral("edit-cut"), KStandardShortcut::cut().first(), [this]()
    {
        const QList<QUrl> urls = activePane()->selectedUrls();
        if (urls.isEmpty())
        {
            return;
        }
        auto *mime = new QMimeData();
        Q_ASSERT(mime != nullptr);
        mime->setUrls(urls);
        mime->setData(QStringLiteral("x-kde-cut-selection"), QByteArray("1"));
        QGuiApplication::clipboard()->setMimeData(mime);
    });

    addAct(QStringLiteral("file_paste"), tr("Einfügen"), QStringLiteral("edit-paste"), KStandardShortcut::paste().first(), [this]()
    {
        const QMimeData *clip = QGuiApplication::clipboard()->mimeData();
        if (clip == nullptr || !clip->hasUrls())
        {
            return;
        }
        const bool isCut = clip->data(QStringLiteral("x-kde-cut-selection")) == "1";
        const QList<QUrl> urls = clip->urls();
        const QUrl destUrl = QUrl::fromLocalFile(activePane()->currentPath());
        if (isCut)
        {
            auto *job = KIO::move(urls, destUrl, KIO::DefaultFlags);
            Q_ASSERT(job != nullptr);
            job->uiDelegate()->setAutoErrorHandlingEnabled(true);
            registerJob(job, tr("Verschiebe Dateien..."));
            QGuiApplication::clipboard()->clear();
        }
        else
        {
            auto *job = KIO::copy(urls, destUrl, KIO::DefaultFlags);
            Q_ASSERT(job != nullptr);
            job->uiDelegate()->setAutoErrorHandlingEnabled(true);
            registerJob(job, tr("Kopiere Dateien..."));
        }
    });

    addAct(QStringLiteral("file_selectall"), tr("Alles auswählen"), QStringLiteral("edit-select-all"), KStandardShortcut::selectAll().first(), [this]()
    {
        activePane()->filePane()->view()->selectAll();
    });
}

void MainWindow::registerViewShortcuts(const ShortcutRegistrar &addAct)
{
    addAct(QStringLiteral("view_hidden"), tr("Versteckte Dateien umschalten"), QStringLiteral("view-hidden"), Qt::CTRL | Qt::Key_H, [this]()
    {
        const bool cur = Config::showHiddenFiles();
        Config::setShowHiddenFiles(!cur);
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
    });

    addAct(QStringLiteral("view_layout"), tr("Layout wechseln"), QStringLiteral("view-choose"), Qt::CTRL | Qt::Key_L, [this]()
    {
        int next = (m_currentMode + 1) % 3;
        auto gs = Config::group("UI");
        gs.writeEntry("layoutMode", next);
        gs.config()->sync();
        applyLayout(next);
    });

    addAct(QStringLiteral("open_settings"), tr("Einstellungen"), QStringLiteral("settings-configure"), Qt::CTRL | Qt::Key_Comma, [this]()
    {
        openSettings();
    });

#ifdef SC_PLUGIN_GIT
    addAct(QStringLiteral("open_git"), tr("GitHub Manager"), QStringLiteral("vcs-commit"), Qt::CTRL | Qt::Key_G, [this]()
    {
        openGitManager();
    });
#endif
}
