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
#include <KDirLister>
#include <QShortcut>
#include <QApplication>
#include <QButtonGroup>
#include <KJob>
#include <KIO/Job>
#include "sidebar.h"
#include "filepane.h"

// --- PaneToolbar ---
class PaneToolbar : public QWidget {
    Q_OBJECT
public:
    explicit PaneToolbar(QWidget *parent = nullptr);
    void setPath(const QString &path);
    void setCount(int count, qint64 totalBytes);
    void setSelected(int count);
signals:
    void foldersFirstToggled(bool on);
    void backClicked();
    void forwardClicked();
    void upClicked();
    void viewModeChanged(int mode);
    void newFolderClicked();
    void deleteClicked();
    void sortClicked();
    void actionsClicked();
    void copyClicked();
    void emptyTrashClicked();
private:
    QLabel *m_pathLabel;
    QLabel *m_countLabel;
    QLabel *m_selectedLabel;
    QLabel *m_sizeLabel;
    QToolButton *m_newFolderBtn;
    QToolButton *m_copyBtn;
    QToolButton *m_emptyTrashBtn;
};

// --- MillerColumn ---
class MillerColumn : public QWidget {
    Q_OBJECT
public:
    explicit MillerColumn(QWidget *parent = nullptr);
    void setGdriveAccounts(const QStringList &accounts) { m_gdriveAccounts = accounts; }
    void populateDrives();
    void populateDir(const QString &path);
    void setActive(bool active);
    void refreshStyle();
    const QString& path() const { return m_path; }
    QListWidget *list()  { return m_list; }
signals:
    void entryClicked(const QString &path, MillerColumn *self);
    void activated(MillerColumn *self);
    void headerClicked(const QString &path);
    void teardownRequested(const QString &udi);
    void setupRequested(const QString &udi);
    void removeFromPlacesRequested(const QString &url);
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);
private:
    QListWidget  *m_list;
    QPushButton  *m_header;
    QString       m_path;
    QStringList   m_gdriveAccounts;
    KDirLister   *m_lister = nullptr;
    QList<KDirLister*> m_discoveryListers;
};

// --- MillerArea ---
class MillerArea : public QWidget {
    Q_OBJECT
public:
    explicit MillerArea(QWidget *parent = nullptr);
    void init();
    void refreshDrives();
    void navigateTo(const QString &path, bool clearForward = true);
    void refresh();
    QString activePath() const;
    void setFocused(bool f);
    void redistributeWidths();
    const QList<MillerColumn*>& cols() const { return m_cols; }
signals:
    void pathChanged(const QString &path);
    void focusRequested();
    void headerClicked(const QString &path);
    void kioPathRequested(const QString &path);
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);
    void removeFromPlacesRequested(const QString &url);
protected:
    void resizeEvent(QResizeEvent *e) override;
private:
    void appendColumn(const QString &path);
    void updateVisibleColumns();
    void trimAfter(MillerColumn *col);
    QList<MillerColumn*> m_cols;
    MillerColumn* m_activeCol = nullptr;
    QHBoxLayout *m_rowLayout = nullptr;
    QWidget *m_rowWidget = nullptr;
    QHBoxLayout *m_colLayout = nullptr;
    QWidget *m_colContainer = nullptr;
    QList<QFrame*> m_colSeparators;
    QList<QWidget*> m_strips;
    QFrame *m_stripDivider = nullptr;
    bool m_focused = false;
};

// --- PaneWidget ---
class JobOverlay;
class PaneWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaneWidget(const QString &settingsKey, QWidget *parent = nullptr);
    void setFocused(bool f);
    bool isFocused() const { return m_focused; }
    QString currentPath() const;
    void navigateTo(const QString &path, bool clearForward = true);
    FilePane *filePane() const { return m_filePane; }
    void saveState() const;

    QStack<QString>& histBack() { return m_histBack; }
    QStack<QString>& histFwd()  { return m_histFwd; }
    MillerArea *miller() const { return m_miller; }

signals:
    void pathUpdated(const QString &path);
    void focusRequested();
    void newFolderRequested();
    void hiddenFilesToggled(bool show);
    void extensionsToggled(bool show);
    void openSettingsRequested(int page);
    void openInLeftRequested(const QString &path);
    void openInRightRequested(const QString &path);
    void layoutChangeRequested(int mode);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void buildFooter(QVBoxLayout *rootLay);
    void positionFooterPanel();
    void updateFooter(const QString &path);

    QString m_settingsKey;
    QLineEdit *m_pathEdit;
    FilePane *m_filePane;
    MillerArea *m_miller;
    PaneToolbar *m_toolbar;
    QWidget *m_footerBar = nullptr;
    QLabel *m_footerCount = nullptr;
    QLabel *m_footerSelected = nullptr;
    QLabel *m_footerSize = nullptr;
    QLabel *m_previewIcon = nullptr;
    QLabel *m_previewInfo = nullptr;
    QString m_lastPreviewPath;
    QSplitter *m_vSplit = nullptr;
    bool m_millerCollapsed = false;
    bool m_focused = false;
    QStack<QString> m_histBack;
    QStack<QString> m_histFwd;
    QProcess *m_searchProc = nullptr;
};

class Sidebar;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();
    
    void registerJob(KJob *job, const QString &title);
    void registerShortcuts();
    
    PaneWidget *activePane() const;
    PaneWidget *leftPane()   const { return m_leftPane; }
    PaneWidget *rightPane()  const { return m_rightPane; }

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
    QList<QShortcut*> m_shortcuts;
};

inline MainWindow *MW() { 
    for (auto *w : qApp->topLevelWidgets()) {
        if (auto *mw = qobject_cast<MainWindow*>(w)) return mw;
    }
    return nullptr;
}
