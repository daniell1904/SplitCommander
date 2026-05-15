#pragma once
#include <QMainWindow>
#include <QApplication>
#include <QSplitter>
#include <QTimer>
#include <KActionCollection>
#include <KDirWatch>
#include <KJob>
#include "sidebar.h"
#include "panewidget.h"
#include "filemanager1.h"

class JobOverlay;
class Sidebar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();

    void registerJob(KJob *job, const QString &title);
    void doDelete(PaneWidget *pane = nullptr, bool permanent = false);
    void registerShortcuts();
    KActionCollection *actionCollection() const { return m_actionCollection; }

    PaneWidget *activePane() const;
    PaneWidget *leftPane()   const { return m_leftPane; }
    PaneWidget *rightPane()  const { return m_rightPane; }
    Sidebar    *sidebar()    const { return m_sidebar; }

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void applyLayout(int mode);

private:
    void saveWindowState();
    void initUI();
    void initConnections();
    void initTimers();
    void refreshAllDrives();
    void scheduleDriveRefresh();  // debounced refreshAllDrives

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

inline MainWindow *MW() {
    for (auto *w : qApp->topLevelWidgets())
        if (auto *mw = qobject_cast<MainWindow*>(w)) return mw;
    return nullptr;
}
