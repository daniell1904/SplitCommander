#pragma once
#include "filepane.h"
#include "millercolumn.h"
#include "panetoolbar.h"
#include <KActionCollection>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStack>
#include <QTreeWidget>
#include <QWidget>

class JobOverlay;

class PaneWidget : public QWidget {
  Q_OBJECT
public:
  explicit PaneWidget(const QString &settingsKey, QWidget *parent = nullptr);
  void setFocused(bool f);
  bool isFocused() const { return m_focused; }
  QString currentPath() const;
  void navigateTo(const QString &path, bool clearForward = true,
                  bool updateMiller = true);
  void setMillerVisible(bool visible);
  void setViewMode(int mode);
  void setActionCollection(KActionCollection *ac);
  FilePane *filePane() const { return m_filePane; }
  QList<QUrl> selectedUrls() const;
  void saveState() const;
  void refreshFooter(const QString &path, int selectedCount);

  QStack<QString> &histBack() { return m_histBack; }
  QStack<QString> &histFwd() { return m_histFwd; }
  MillerArea *miller() const { return m_miller; }

signals:
  void pathUpdated(const QString &path);
  void focusRequested();
  void newFolderRequested();
  void hiddenFilesToggled(bool show);
  void extensionsToggled(bool show);
  void settingsChanged();
  void openInLeftRequested(const QString &path);
  void openInRightRequested(const QString &path);
  void layoutChangeRequested(int mode);
  void copyToOtherPaneRequested();

protected:
  bool eventFilter(QObject *obj, QEvent *ev) override;
  void resizeEvent(QResizeEvent *e) override;

private:
  // Konstruktor-Helfer
  void initTabBar(QVBoxLayout *rootLay);
  void initHamburgerMenu(QToolButton *hamburgerBtn, QToolButton *layoutBtn);
  void initSearchPanel(QVBoxLayout *rootLay);
  void initSplitter(QVBoxLayout *rootLay);
  void initConnections();

  // Footer
  void buildFooter(QVBoxLayout *rootLay);
  void positionFooterPanel();
  void refreshFooterForDirectory(int selectedCount);
  void refreshFooterForLocalPath(const QString &path);
  void refreshFooterForRemotePath(const QString &path, const QUrl &url);

  QString m_settingsKey;
  QLineEdit *m_pathEdit = nullptr;
  FilePane *m_filePane = nullptr;
  MillerArea *m_miller = nullptr;
  PaneToolbar *m_toolbar = nullptr;
  QToolButton *m_millerToggle = nullptr;
  QWidget *m_footerBar = nullptr;
  QLabel *m_footerCount = nullptr;
  QLabel *m_footerSelected = nullptr;
  QLabel *m_footerSize = nullptr;
  QLabel *m_previewIcon = nullptr;
  QLabel *m_previewInfo = nullptr;
  QString m_lastPreviewPath;
  QPixmap m_lastPreviewPixmap;
  QSplitter *m_vSplit = nullptr;
  bool m_millerCollapsed = false;
  bool m_focused = false;
  QStack<QString> m_histBack;
  QStack<QString> m_histFwd;
  QFutureWatcher<QStringList> *m_searchWatcher = nullptr;
  KActionCollection *m_actionCollection = nullptr;

  // Zwischen init*-Methoden geteilte Widgets
  QStackedWidget *m_pathStack = nullptr;
  QToolButton *m_searchBtn = nullptr;
  QWidget *m_searchOverlay = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QTreeWidget *m_searchResults = nullptr;
};
