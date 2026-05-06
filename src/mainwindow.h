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
#include "sidebar.h"
#include "thememanager.h"
#include "filepane.h"

// ── Toolbar über der Dateiliste ───────────────────────────────────────────────
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
private:
    QLabel *m_pathLabel;
    QLabel *m_countLabel;
    QLabel *m_selectedLabel;
    QLabel *m_sizeLabel;
};

// ── Einzelne Miller-Spalte ────────────────────────────────────────────────────
class MillerColumn : public QWidget {
    Q_OBJECT
public:
    void setGdriveAccounts(const QStringList &accounts) { m_gdriveAccounts = accounts; }
    explicit MillerColumn(QWidget *parent = nullptr);
    void populateDrives();
    void populateDir(const QString &path);
    void setActive(bool active);
    void refreshStyle();
    QString path() const { return m_path; }
    QListWidget *list()  { return m_list; }
signals:
    void entryClicked(const QString &path, MillerColumn *self);
    void activated(MillerColumn *self);
    void headerClicked(const QString &path);
    void teardownRequested(const QString &udi);   // Laufwerk aushängen
    void setupRequested(const QString &udi);      // Laufwerk einhängen
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);
private:
    QListWidget  *m_list;
    QPushButton  *m_header;
    QString       m_path;
    QStringList   m_gdriveAccounts;
};

// ── Miller-Bereich ────────────────────────────────────────────────────────────
class MillerArea : public QWidget {
    Q_OBJECT
public:
    explicit MillerArea(QWidget *parent = nullptr);
    void init();
    void refreshDrives();
    void navigateTo(const QString &path, bool clearForward = true); // neues API
    QString activePath() const;
    void setFocused(bool f);
    const QList<MillerColumn*>& cols() const { return m_cols; }
signals:
    void pathChanged(const QString &path);
    void focusRequested();
    void headerClicked(const QString &path);
    void kioPathRequested(const QString &path);
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);
protected:
    void resizeEvent(QResizeEvent *e) override;
private:
    void appendColumn(const QString &path);
    void trimAfter(MillerColumn *col);
    void redistributeWidths();
    void updateVisibleColumns();

    QHBoxLayout          *m_rowLayout;
    QWidget              *m_rowWidget;
    QFrame               *m_stripDivider;
    QHBoxLayout          *m_colLayout;
    QWidget              *m_colContainer;
    QList<QWidget*>       m_strips;
    QList<QFrame*>        m_colSeparators;
    QList<MillerColumn*>  m_cols;
    MillerColumn         *m_activeCol = nullptr;
    bool                  m_focused   = false;
};

// ── Eine komplette Pane ───────────────────────────────────────────────────────
class PaneWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaneWidget(QWidget *parent = nullptr);
    void setFocused(bool f);
    bool isFocused() const { return m_focused; }
    QString currentPath() const;
    FilePane *filePane() { return m_filePane; }
    void navigateTo(const QString &path, bool clearForward = true);
    void updateFooter(const QString &path);
    MillerArea *miller() { return m_miller; }
    PaneToolbar *toolbar() { return m_toolbar; }
    QStack<QString> &histBack() { return m_histBack; }
    QStack<QString> &histFwd()  { return m_histFwd; }
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void resizeEvent(QResizeEvent *e) override;
signals:
    void focusRequested();
    void pathUpdated(const QString &path);
    void openInLeftRequested(const QString &path);
    void openInRightRequested(const QString &path);
    void openSettingsRequested(int page);
    void hiddenFilesToggled(bool show);
    void extensionsToggled(bool show);
    void newFolderRequested();
    void layoutChangeRequested(int mode);
private:
    void buildFooter(QVBoxLayout *rootLay);
    void positionFooterPanel();

    MillerArea  *m_miller;
    PaneToolbar *m_toolbar;
    FilePane    *m_filePane;
    QLineEdit   *m_pathEdit;
    QSplitter   *m_vSplit = nullptr;
    bool         m_focused = false;
    QStack<QString> m_histBack, m_histFwd;

    // Footer
    QWidget  *m_footerBar      = nullptr; // eingeklappte Leiste
    QWidget  *m_footerPanel    = nullptr; // ausgeklapptes Panel
    QLabel   *m_footerCount    = nullptr;
    QLabel   *m_footerSelected = nullptr;
    QLabel   *m_footerSize     = nullptr;
    QLabel   *m_previewIcon    = nullptr;
    QLabel   *m_previewInfo    = nullptr;
    bool      m_footerExpanded = false;
    QString   m_lastPreviewPath;
    QProcess *m_searchProc = nullptr;
};

// ── MainWindow ────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private slots:
    void applyLayout(int mode);
private:
    Sidebar    *m_sidebar;
    PaneWidget *m_leftPane;
    PaneWidget *m_rightPane;
    QSplitter  *m_panesSplitter;
    QSplitter  *m_vSplit = nullptr;
    int         m_currentMode = 1;
public:
    void registerShortcuts();
    PaneWidget *activePane() const { return m_rightPane->isFocused() ? m_rightPane : m_leftPane; }
    PaneWidget *leftPane()   const { return m_leftPane; }
    PaneWidget *rightPane()  const { return m_rightPane; }
private:
    QList<QShortcut*> m_shortcuts;
};

