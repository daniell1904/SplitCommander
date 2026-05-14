#pragma once
#include <QWidget>
#include <QStack>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QFutureWatcher>
#include "filepane.h"
#include "millercolumn.h"
#include "panetoolbar.h"

class JobOverlay;

class PaneWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaneWidget(const QString &settingsKey, QWidget *parent = nullptr);
    void setFocused(bool f);
    bool isFocused() const { return m_focused; }
    QString currentPath() const;
    void navigateTo(const QString &path, bool clearForward = true, bool updateMiller = true);
    void setMillerVisible(bool visible);
    void setViewMode(int mode);
    FilePane *filePane() const { return m_filePane; }
    QList<QUrl> selectedUrls() const;
    void saveState() const;
    void refreshFooter(const QString &path, int selectedCount);

    QStack<QString>& histBack() { return m_histBack; }
    QStack<QString>& histFwd()  { return m_histFwd; }
    MillerArea *miller() const  { return m_miller; }

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

    QString       m_settingsKey;
    QLineEdit    *m_pathEdit        = nullptr;
    FilePane     *m_filePane        = nullptr;
    MillerArea   *m_miller          = nullptr;
    PaneToolbar  *m_toolbar         = nullptr;
    QToolButton  *m_millerToggle    = nullptr;
    QWidget      *m_footerBar       = nullptr;
    QLabel       *m_footerCount     = nullptr;
    QLabel       *m_footerSelected  = nullptr;
    QLabel       *m_footerSize      = nullptr;
    QLabel       *m_previewIcon     = nullptr;
    QLabel       *m_previewInfo     = nullptr;
    QString       m_lastPreviewPath;
    QSplitter    *m_vSplit          = nullptr;
    bool          m_millerCollapsed = false;
    bool          m_focused         = false;
    QStack<QString> m_histBack;
    QStack<QString> m_histFwd;
    QFutureWatcher<QStringList> *m_searchWatcher = nullptr;
};
