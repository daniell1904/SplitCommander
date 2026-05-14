#pragma once
#include <QProcess>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QStack>
#include <QToolButton>
#include <QTimer>
#include <QPushButton>
#include <QStackedWidget>
#include <QFutureWatcher>
#include <KDirWatch>
#include <KDirLister>
#include <KActionCollection>
#include <QApplication>
#include <QButtonGroup>
#include <KJob>
#include <KIO/Job>
#include "sidebar.h"
#include "filepane.h"
#include "panetoolbar.h"
#include "millercolumn.h"
#include "panewidget.h"

// --- PaneWidget ---
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
    Sidebar    *m_sidebar;
    JobOverlay *m_jobOverlay;
    PaneWidget *m_leftPane;
    PaneWidget *m_rightPane;
    QSplitter  *m_panesSplitter;
    QSplitter  *m_vSplit = nullptr;
    int         m_currentMode = 1;
    bool        m_panesSplitterRestored = false;
    KActionCollection *m_actionCollection = nullptr;
    KDirWatch *m_fsWatcher = nullptr;
};

inline MainWindow *MW() { 
    for (auto *w : qApp->topLevelWidgets()) {
        if (auto *mw = qobject_cast<MainWindow*>(w)) return mw;
    }
    return nullptr;
}
