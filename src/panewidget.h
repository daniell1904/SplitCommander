#pragma once
#include "filepane.h"
#include "millerarea.h"
#include "panetoolbar.h"
#include <KActionCollection>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStack>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

class JobOverlay;

class PaneWidget : public QWidget {
  Q_OBJECT
public:
  explicit PaneWidget(const QString &settingsKey, QWidget *parent = nullptr);
  ~PaneWidget() override;
  void setFocused(bool f);
  bool isFocused() const { return m_focused; }
  QString currentPath() const;
  QUrl    currentUrl() const { return m_filePane->currentUrl(); }
  void navigateTo(const QString &path, bool clearForward = true,
                  bool updateMiller = true);
  void setMillerVisible(bool visible);
  void setViewMode(int mode);
  void setActionCollection(KActionCollection *ac);
  FilePane *filePane() const { return m_filePane; }
  QList<QUrl> selectedUrls() const;
  void saveState() const;
  void refreshFooter(const QString &path, int selectedCount);

  // --- Tab-API ---
  void addTab(const QString &path = QString());
  void closeTab(int index);
  void switchTab(int index);
  int  tabCount()        const { return m_tabs.size(); }
  int  currentTabIndex() const { return m_currentTab; }



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
  void buildPathStack(class QHBoxLayout *tabLay);
  void setupPathStackConnections(class QPushButton *breadcrumbBtn);
  void buildActionButtons();
  void initTabBar();
  void initHamburgerMenu(class QToolButton *hamburgerBtn, class QToolButton *layoutBtn);
  void initSearchPanel(QVBoxLayout *rootLay);
  void buildSearchTopRow(class QVBoxLayout *spVLay, class QToolButton *&searchCloseBtn, std::shared_ptr<bool> searchByName);
  void setupSearchFilterMenu(class QToolButton *filterBtn, std::shared_ptr<bool> searchByName);
  void buildSearchTabRow(class QVBoxLayout *spVLay, class QWidget *&spTabRow);
  void buildSearchOverlay();
  void connectSearchSignals(class QWidget *searchPanel, class QWidget *spTabRow, class QToolButton *searchCloseBtn, std::shared_ptr<bool> searchByName);
  void executeBalooSearch(const QString &term);
  void buildActiveHeader(class QVBoxLayout *millerWrapLay);
  void buildActiveHeaderButtons(class QHBoxLayout *activeHeaderLay);
  void buildMillerBreadcrumbBar(class QVBoxLayout *rootLay, int insertIdx);
  void buildLowerFilePane(class QWidget *lowerWidget);
  void setupMillerToggleAndSplitter();
  void initSplitter(QVBoxLayout *rootLay);
  void connectMillerSignals();
  void updateActiveTabLabel(const QString &path);
  void connectFilePaneSignals();
  void connectToolbarSignals();
  void handleDirectoryLoaded();
  void handleEmptyTrash();
  void initConnections();

  // Fußzeile
  void buildFooter(QVBoxLayout *rootLay);
  void positionFooterPanel();
  void refreshFooterForDirectory(int selectedCount);
  void refreshFooterForLocalPath(const QString &path);
  void refreshFooterForRemotePath(const QString &path, const QUrl &url);
  void refreshFooterPreviewAndInfo(const QString &path, const QFileInfo &fi);
  QString buildPermissionsString(const QFileInfo &fi);
  QPushButton *m_activeHeaderBtn = nullptr;
  QWidget     *m_activeHeader    = nullptr;
  QWidget     *m_breadcrumbBar   = nullptr;

  void updateBreadcrumb(const QString &path);
  void openPathEditOverlay();
  void setupPathEditOverlayConnections(class QWidget *overlay, class KUrlRequester *urlReq, class QToolButton *copyBtn, class QToolButton *closeBtn);

  // --- Tabs ---
  void buildTabButtonUI(const QString &label, const QIcon &icon, int btnIdx, class QFrame *&frame, class QToolButton *&btn);
  struct TabState {
    QString path;
    QStack<QString> histBack;
    QStack<QString> histFwd;
  };
  QList<TabState>       m_tabs;
  int                   m_currentTab = 0;
  QWidget              *m_tabStrip   = nullptr;  // Container für zusätzliche Tab-Schaltflächen
  QHBoxLayout          *m_tabStripLay = nullptr;
  QList<QToolButton *>  m_tabButtons; // Index = Tabs-Index - 1

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
  QToolButton *m_hamburgerBtn = nullptr;
  QWidget *m_searchOverlay = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QTreeWidget *m_searchResults = nullptr;
};
