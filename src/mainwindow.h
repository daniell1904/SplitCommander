#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
private:
    QListWidget  *m_list;
    QPushButton  *m_header;
    QString       m_path;
};

// ── Miller-Bereich ────────────────────────────────────────────────────────────
class MillerArea : public QWidget {
    Q_OBJECT
public:
    explicit MillerArea(QWidget *parent = nullptr);
    void init();
    void refreshDrives();
    void navigateTo(const QString &path); // neues API
    QString activePath() const;
    void setFocused(bool f);
    const QList<MillerColumn*>& cols() const { return m_cols; }
signals:
    void pathChanged(const QString &path);
    void focusRequested();
    void headerClicked(const QString &path);
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
    void navigateTo(const QString &path);
    void updateFooter(const QString &path);
    MillerArea *miller() { return m_miller; }
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void resizeEvent(QResizeEvent *e) override;
signals:
    void focusRequested();
    void pathUpdated(const QString &path);
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
    PaneWidget *activePane() const { return m_rightPane->isFocused() ? m_rightPane : m_leftPane; }
    QSplitter  *m_panesSplitter;
    QSplitter  *m_vSplit = nullptr;
    int         m_currentMode = 1;
    void registerShortcuts();
    QList<QShortcut*> m_shortcuts;
};

#endif
