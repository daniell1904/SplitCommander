#pragma once

#include <QMainWindow>
#include <QApplication>
#include <QSplitter>
#include <QTimer>
#include <KActionCollection>
#include <KDirWatch>
#include <KJob>
#include <functional>
#include "sidebar.h"
#include "panewidget.h"
#include "filemanager1.h"

class JobOverlay;
class Sidebar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void registerJob(KJob *job, const QString &title);
    void doDelete(PaneWidget *pane = nullptr, bool permanent = false);
    void registerShortcuts();
    
    [[nodiscard]] KActionCollection *actionCollection() const { return m_actionCollection; }
    [[nodiscard]] PaneWidget *activePane() const;
    [[nodiscard]] PaneWidget *leftPane()   const { return m_leftPane; }
    [[nodiscard]] PaneWidget *rightPane()  const { return m_rightPane; }
    [[nodiscard]] Sidebar    *sidebar()    const { return m_sidebar; }

public:
    void openSettings(int page = -1);
#ifdef SC_PLUGIN_GIT
    void openGitManager();
#endif
#ifdef SC_PLUGIN_PAPERLESS
    void openPaperlessManager();
#endif

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void applyLayout(int mode);

private:
    void saveWindowState();
    void initUI();
    void initConnections();
    void restoreSession();
    void resolveSessionPaths(int behavior, const QString &configPath, const QString &lastLeft, const QString &lastRight, QString &leftPath, QString &rightPath);
    void applySessionNavigation(int behavior, const QString &leftPath, const QString &rightPath);
    void initTimers();
    void refreshAllDrives();
    void scheduleDriveRefresh();  // Laufwerke entprellt aktualisieren

    // --- UI Hilfsfunktionen ---
    void buildWindowProperties();
    void buildSidebar(QHBoxLayout *rootLay, QWidget *central);
    void buildPanes(QHBoxLayout *rootLay, QWidget *central);

    // --- Connections Hilfsfunktionen (NASA Rule 4) ---
    void connectPaneSignals(PaneWidget *pane, PaneWidget *other);
    void connectSidebarSignals();
    void connectSystemNotifications();
    void connectFileWatcher();
    
    // Unterfunktionen für connectSidebarSignals
    void connectSidebarDriveClicks();
    void mountAndNavigateDrive(const QString &path, bool leftPane);
    void handleSolidDeviceMount(const QString &path, const std::function<void(const QString&)> &navigate);
    void connectPaneOpenRequests();
    void connectSidebarMiscSignals();
    
    // Unterfunktionen für connectFileWatcher
    void connectUnmountRequested();
    void connectTeardownRequested();
    void executeDeviceTeardown(const QString &udi);
    void connectDriveSettingsAndThemes();
    void connectDeviceNotifierAndWatcher();

    // --- Shortcuts Hilfsfunktionen (NASA Rule 4) ---
    struct ShortcutRegistrar
    {
        std::function<QAction*(const QString&, const QString&, const QString&, const QKeySequence&, std::function<void()>, const QKeySequence&)> addActRaw;
        QAction* operator()(const QString &id, const QString &label, const QString &icon, const QKeySequence &defKey, std::function<void()> fn, const QKeySequence &altKey = {}) const
        {
            return addActRaw(id, label, icon, defKey, fn, altKey);
        }
    };
    void registerNavigationShortcuts(const ShortcutRegistrar &addAct);
    void registerTabShortcuts(const ShortcutRegistrar &addAct);
    void registerPaneShortcuts(const ShortcutRegistrar &addAct);
    void registerFileShortcuts(const ShortcutRegistrar &addAct);
    void registerFileRenameShortcut(const ShortcutRegistrar &addAct);
    void registerFileDeleteShortcuts(const ShortcutRegistrar &addAct);
    void registerClipboardShortcuts(const ShortcutRegistrar &addAct);
    void registerViewShortcuts(const ShortcutRegistrar &addAct);

    Sidebar    *m_sidebar          = nullptr;
    JobOverlay *m_jobOverlay       = nullptr;
    PaneWidget *m_leftPane         = nullptr;
    PaneWidget *m_rightPane        = nullptr;
    QSplitter  *m_panesSplitter    = nullptr;
    QSplitter  *m_vSplit           = nullptr;
    int         m_currentMode      = 1;
    bool        m_panesSplitterRestored = false;
    KActionCollection *m_actionCollection = nullptr;
    FileManager1 *m_fileManager1 = nullptr;
    KDirWatch  *m_fsWatcher        = nullptr;
    QTimer     *m_driveRefreshTimer = nullptr;
};

[[nodiscard]] inline MainWindow *MW()
{
    for (auto *w : qApp->topLevelWidgets())
    {
        if (auto *mw = qobject_cast<MainWindow*>(w))
        {
            return mw;
        }
    }
    return nullptr;
}
