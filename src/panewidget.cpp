#include "panewidget.h"
#include "config.h"
#include "dialogutils.h"
#include "thememanager.h"
#include "thumbnailmanager.h"
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KDialogJobUiDelegate>
#include <KFileItem>
#include <KFileWidget>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/EmptyTrashJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/StoredTransferJob>
#include <KPropertiesDialog>
#include <KShortcutsDialog>
#include <KShortcutsEditor>
#include <KTerminalLauncherJob>
#include <KUrlRequester>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

PaneWidget::PaneWidget(const QString &settingsKey, QWidget *parent)
    : QWidget(parent), m_settingsKey(settingsKey) {
  setStyleSheet(QString("background:%1;").arg(TM().colors().bgDeep));
  auto *rootLay = new QVBoxLayout(this);
  rootLay->setContentsMargins(0, 2, 0, 0);
  rootLay->setSpacing(0);

  m_toolbar = new PaneToolbar(this);

  initTabBar();
  initSplitter(rootLay);
  initConnections();
  buildFooter(rootLay);

  // Globalen Event-Filter registrieren, um Klicks auf Kind-Widgets zu erkennen
  if (qApp) {
    qApp->installEventFilter(this);
  }
}

PaneWidget::~PaneWidget() {
  if (qApp) {
    qApp->removeEventFilter(this);
  }
}

static void sc_styleToolButton(QToolButton *btn) {
  btn->setStyleSheet(
      QString("QToolButton { background:%1; border:none; border-radius:0px; }"
              "QToolButton:hover { background:%2; }"
              "QToolButton:checked { background:%3; }"
              "QToolButton::menu-indicator { image: none; }")
          .arg(TM().colors().bgBox, TM().colors().bgHover,
               TM().colors().bgList));
}

void PaneWidget::setupPathStackConnections(QPushButton *breadcrumbBtn) {
  connect(breadcrumbBtn, &QPushButton::clicked, this, [this]() {
    m_pathEdit->setText(currentPath());
    m_pathEdit->selectAll();
    m_pathStack->setCurrentIndex(1);
    m_pathEdit->setFocus();
  });

  auto commitPath = [breadcrumbBtn, this]() {
    const QString p = m_pathEdit->text().trimmed();
    if (!p.isEmpty() && QFileInfo::exists(p))
      navigateTo(p);
    breadcrumbBtn->setText(currentPath());
    m_pathStack->setCurrentIndex(0);
  };
  connect(m_pathEdit, &QLineEdit::returnPressed, this, commitPath);

  connect(m_pathEdit, &QLineEdit::editingFinished, this, [this, commitPath]() {
    if (m_pathStack->currentIndex() == 1)
      commitPath();
  });
  connect(this, &PaneWidget::pathUpdated, this,
          [breadcrumbBtn, this](const QString &p) {
            QUrl url(p);
            QString name;
            QIcon icon;
            if (p == "__drives__") {
              name = tr("Dieser PC");
              icon = QIcon::fromTheme("computer");
            } else {
              name = url.fileName();
              if (name.isEmpty())
                name = p;
              icon = QIcon::fromTheme("folder");
            }
            breadcrumbBtn->setText(name);
            if (m_currentTab == 0 && m_activeHeaderBtn) {
              m_activeHeaderBtn->setText(name);
              m_activeHeaderBtn->setIcon(icon);
            } else if (m_currentTab > 0) {
              const int btnIdx = m_currentTab - 1;
              if (btnIdx < m_tabButtons.size()) {
                m_tabButtons[btnIdx]->setText(name);
                m_tabButtons[btnIdx]->setIcon(icon);
              }
            }
          });
}

void PaneWidget::buildPathStack(QHBoxLayout *tabLay) {
  m_pathStack = new QStackedWidget();
  m_pathStack->setFixedHeight(26);
  m_pathStack->setStyleSheet(
      QString("background-color:%1; border:1px solid %2; border-radius:0px;")
          .arg(TM().colors().bgBox, TM().colors().borderAlt));

  m_pathEdit = new QLineEdit(QDir::homePath());
  m_pathEdit->setStyleSheet(
      QString("QLineEdit{background:transparent; border:none; color:%1; "
              "font-size:14px; padding:2px 8px; border-radius:0px;}")
          .arg(TM().colors().textPrimary));

  auto *breadcrumbBtn = new QPushButton(QDir::homePath());
  breadcrumbBtn->setStyleSheet(
      QString("QPushButton { background:transparent; border:none; color:%1; "
              "font-size:15px; font-weight:300; padding:2px 10px; "
              "border-radius:0px; }")
          .arg(TM().colors().textAccent));
  m_pathStack->addWidget(breadcrumbBtn);
  m_pathStack->addWidget(m_pathEdit);
  m_pathStack->setCurrentIndex(0);

  setupPathStackConnections(breadcrumbBtn);
  tabLay->addWidget(m_pathStack, 1);
}

void PaneWidget::buildActionButtons() {
  m_millerToggle = new QToolButton();
  m_millerToggle->setFixedSize(24, Config::millerHeaderHeight());
  m_millerToggle->setCheckable(true);
  m_millerToggle->setChecked(true);
  m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
  m_millerToggle->setIconSize(QSize(18, 18));
  m_millerToggle->setToolTip(tr("Miller-Columns ein-/ausklappen"));
  m_millerToggle->setStyleSheet(
      QString("QToolButton { background:%1; border:none; border-radius:0px; }"
              "QToolButton:hover { background:%2; }"
              "QToolButton:checked { background:%1; }"
              "QToolButton::menu-indicator { image: none; }")
          .arg(TM().colors().bgBox, TM().colors().bgHover));

  m_searchBtn = new QToolButton();
  m_searchBtn->setIcon(QIcon::fromTheme("system-search"));
  m_searchBtn->setIconSize(QSize(18, 18));
  m_searchBtn->setFixedSize(Config::millerHeaderHeight() + 4,
                            Config::millerHeaderHeight() + 4);
  m_searchBtn->setToolTip(tr("Suchen"));
  m_searchBtn->setCheckable(true);
  sc_styleToolButton(m_searchBtn);

  m_hamburgerBtn = new QToolButton();
  m_hamburgerBtn->setIcon(QIcon::fromTheme("application-menu"));
  m_hamburgerBtn->setIconSize(QSize(18, 18));
  m_hamburgerBtn->setFixedSize(Config::millerHeaderHeight() + 4,
                               Config::millerHeaderHeight() + 4);
  m_hamburgerBtn->setToolTip(tr("Menü"));
  m_hamburgerBtn->setPopupMode(QToolButton::InstantPopup);
  sc_styleToolButton(m_hamburgerBtn);

  auto *layoutBtn = new QToolButton();
  layoutBtn->setFixedSize(30, 30);
  layoutBtn->setIcon(QIcon::fromTheme("view-split-left-right"));
  layoutBtn->setIconSize(QSize(18, 18));
  layoutBtn->setToolTip(tr("Layout wählen"));
  layoutBtn->setStyleSheet(TM().ssToolBtn());

  initHamburgerMenu(m_hamburgerBtn, layoutBtn);
}

// --- PaneWidget::initTabBar ---
// UI-Bereich: Toolbar-Zeile (Miller-Toggle, Pfad-Stack, Suche, Hamburger)
void PaneWidget::initTabBar() {
  auto *tabBar = new QWidget(this);
  tabBar->hide();
  tabBar->setFixedHeight(46);
  tabBar->setStyleSheet(
      QString("background:%1; border:none;").arg(TM().colors().bgMain));
  auto *tabLay = new QHBoxLayout(tabBar);
  tabLay->setContentsMargins(4, 0, 4, 0);
  tabLay->setSpacing(2);

  buildPathStack(tabLay);
  buildActionButtons();
}

// Hilfsfunktion: Tab-Button-Style
// bg: Hintergrundfarbe (bgList=aktiv, bgDeep=inaktiv)
static QString tabBtnStyle(const QString &bg, const QString &textColor) {
  return QString("QPushButton#MillerHeader { background:%1;"
                 "  border:none;"
                 "  border-top-right-radius:6px;"
                 "  border-top-left-radius:0px;"
                 "  border-bottom-left-radius:0px;"
                 "  border-bottom-right-radius:0px;"
                 "  color:%2;"
                 "  font-family:'Segoe UI Semilight','Roboto Light',sans-serif;"
                 "  font-weight:300; font-size:14px; padding:4px 12px 4px 8px; "
                 "text-align:left; }"
                 "QPushButton#MillerHeader:hover { background:%1; }")
      .arg(bg, textColor);
}

// --- PaneWidget::initHamburgerMenu ---
// UI-Bereich: Hamburger-Menü (Einstellungen, Aktionen)


void PaneWidget::buildActiveHeader(QVBoxLayout *millerWrapLay) {
  auto *activeHeader = new QWidget();
  activeHeader->setFixedHeight(Config::millerHeaderHeight() + 4);
  activeHeader->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgMain));
  auto *activeHeaderLay = new QHBoxLayout(activeHeader);
  activeHeaderLay->setContentsMargins(0, 0, 0, 0);
  activeHeaderLay->setSpacing(0);

  buildActiveHeaderButtons(activeHeaderLay);

  auto *headerFill = new QWidget();
  headerFill->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  headerFill->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgMain));
  activeHeaderLay->addWidget(headerFill, 1);

  activeHeaderLay->addWidget(m_searchBtn);
  activeHeaderLay->addWidget(m_hamburgerBtn);

  m_miller = new MillerArea();
  m_miller->setMinimumHeight(150);
  m_miller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  millerWrapLay->addWidget(m_miller, 1);
  m_activeHeader = activeHeader;
}

void PaneWidget::buildActiveHeaderButtons(QHBoxLayout *activeHeaderLay) {
  m_activeHeaderBtn = new QPushButton();
  m_activeHeaderBtn->setObjectName("MillerHeader");
  m_activeHeaderBtn->setFlat(true);
  m_activeHeaderBtn->setAttribute(Qt::WA_StyledBackground, true);
  m_activeHeaderBtn->setFixedHeight(Config::millerHeaderHeight() + 4);
  m_activeHeaderBtn->setMinimumWidth(50);
  m_activeHeaderBtn->setMaximumWidth(130);
  m_activeHeaderBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_activeHeaderBtn->setIconSize(
      QSize(Config::millerIconSize(), Config::millerIconSize()));
  m_activeHeaderBtn->setIcon(QIcon::fromTheme("computer"));
  m_activeHeaderBtn->setText(tr("Dieser PC"));
  m_activeHeaderBtn->setStyleSheet(
      tabBtnStyle(TM().colors().bgList, TM().colors().textPrimary));

  m_tabs.append(TabState{QStringLiteral("__drives__"), {}, {}});
  m_currentTab = 0;

  m_tabStrip = new QWidget();
  m_tabStrip->setAttribute(Qt::WA_TranslucentBackground);
  m_tabStrip->setStyleSheet(QStringLiteral("background:transparent;"));
  m_tabStripLay = new QHBoxLayout(m_tabStrip);
  m_tabStripLay->setContentsMargins(0, 0, 0, 0);
  m_tabStripLay->setSpacing(1);
  m_tabStrip->hide();

  auto *addTabBtn = new QToolButton();
  addTabBtn->setText(QStringLiteral("+"));
  addTabBtn->setFixedSize(36, Config::millerHeaderHeight());
  addTabBtn->setToolTip(tr("Neuer Tab (Strg+T)"));
  addTabBtn->setStyleSheet(
      QString("QToolButton { background:%1; color:#ffffff; border:none;"
              "  font-size:22px; font-weight:300; border-radius:6px; }"
              "QToolButton:hover { background:%2; }")
          .arg(TM().colors().bgDeep, TM().colors().bgList));
  connect(addTabBtn, &QToolButton::clicked, this, [this]() {
    const QString path = (m_currentTab >= 0 && m_currentTab < m_tabs.size())
                             ? m_tabs[m_currentTab].path
                             : QStringLiteral("__drives__");
    addTab(path);
  });

  connect(m_activeHeaderBtn, &QPushButton::clicked, this,
          [this]() { switchTab(0); });

  m_millerToggle->setFixedSize(Config::millerHeaderHeight() + 4,
                               Config::millerHeaderHeight() + 4);
  m_millerToggle->setIconSize(QSize(16, 16));

  activeHeaderLay->addWidget(m_millerToggle);
  activeHeaderLay->addWidget(m_activeHeaderBtn, 0);
  activeHeaderLay->addSpacing(1);
  activeHeaderLay->addWidget(m_tabStrip, 0);
  activeHeaderLay->addWidget(addTabBtn);
}

void PaneWidget::buildMillerBreadcrumbBar(QVBoxLayout *rootLay, int insertIdx) {
  m_breadcrumbBar = new QWidget();
  m_breadcrumbBar->setFixedHeight(Config::millerHeaderHeight());
  m_breadcrumbBar->setObjectName("breadcrumbBar");
  m_breadcrumbBar->setStyleSheet(
      QString("QWidget#breadcrumbBar { background:%1; }"
              "QPushButton { background:transparent; color:%2; font-size:14px;"
              "  border:none; padding:0 2px; margin:0; }"
              "QPushButton:hover { color:%2; }"
              "QLabel#bcSep { color:%2; font-size:14px;"
              "  background:transparent; padding:0; margin:0; }")
          .arg(TM().colors().bgList, TM().colors().textAccent));
  auto *bcLay = new QHBoxLayout(m_breadcrumbBar);
  bcLay->setContentsMargins(12, 0, 12, 0);
  bcLay->setSpacing(0);
  bcLay->addStretch();
  m_breadcrumbBar->hide();

  rootLay->insertWidget(insertIdx, m_activeHeader);
  rootLay->insertWidget(insertIdx + 1, m_breadcrumbBar);
}

void PaneWidget::buildLowerFilePane(QWidget *lowerWidget) {
  auto *lowerLay = new QVBoxLayout(lowerWidget);
  lowerLay->setContentsMargins(0, 0, 0, 0);
  lowerLay->setSpacing(0);
  m_filePane = new FilePane(nullptr, m_settingsKey);
  m_filePane->setStyleSheet(
      QString("border:none;background:%1;").arg(TM().colors().bgDeep));
  auto *updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  updateTimer->setInterval(50); // 50ms debounce
  connect(updateTimer, &QTimer::timeout, this, [this]() {
    const int count =
        m_filePane->view()->model()->rowCount(m_filePane->view()->rootIndex());
    const qint64 sz = m_filePane->currentTotalSize();
    m_toolbar->setCount(count, sz);
    refreshFooter(currentPath(),
                  m_filePane->view()->selectionModel()->selectedRows().count());
  });
  connect(m_filePane, &FilePane::modelUpdated, this,
          [updateTimer]() { updateTimer->start(); });
  lowerLay->addWidget(m_toolbar);
  lowerLay->addWidget(m_filePane, 1);
}

void PaneWidget::setupMillerToggleAndSplitter() {
  connect(m_vSplit, &QSplitter::splitterMoved, this, [this](int, int) {
    if (m_searchOverlay && m_searchOverlay->isVisible()) {
      const QPoint topLeft = m_vSplit->mapTo(this, QPoint(0, 0));
      m_searchOverlay->setGeometry(topLeft.x(), topLeft.y(), m_vSplit->width(),
                                   qMin(300, m_vSplit->height()));
    }
  });

  connect(m_millerToggle, &QToolButton::toggled, this, [this](bool checked) {
    if (checked) {
      auto s = Config::group("UI");
      const QByteArray saved =
          s.readEntry(m_settingsKey + "/vSplitState", QByteArray());
      if (!saved.isEmpty()) {
        m_vSplit->restoreState(saved);
        if (m_vSplit->sizes().value(0) == 0)
          m_vSplit->setSizes({200, 450});
      } else {
        m_vSplit->setSizes({200, 450});
      }
      m_miller->setCollapsed(false);
      if (m_breadcrumbBar)
        m_breadcrumbBar->hide();
      m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
      m_millerToggle->setToolTip("Miller-Columns ausklappen");
      m_millerCollapsed = false;
    } else {
      if (m_vSplit->sizes().value(0) > 0) {
        auto s = Config::group("UI");
        s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
        s.config()->sync();
      }
      m_vSplit->setSizes({0, 1});
      m_miller->setCollapsed(true);
      if (m_breadcrumbBar) {
        updateBreadcrumb(currentPath());
        m_breadcrumbBar->show();
      }
      m_millerToggle->setIcon(QIcon::fromTheme("go-down"));
      m_millerToggle->setToolTip("Miller-Columns einblenden");
      m_millerCollapsed = true;
    }
  });

  auto s = Config::group("UI");
  if (s.readEntry(m_settingsKey + "/millerCollapsed", false)) {
    m_millerCollapsed = true;
    m_millerToggle->blockSignals(true);
    m_millerToggle->setChecked(false);
    m_millerToggle->setIcon(QIcon::fromTheme("go-down"));
    m_millerToggle->setToolTip("Miller-Columns einblenden");
    m_millerToggle->blockSignals(false);
    m_vSplit->setSizes({0, 1});
  }
}

void PaneWidget::initSplitter(QVBoxLayout *rootLay) {
  m_vSplit = new QSplitter(Qt::Vertical);
  m_vSplit->setChildrenCollapsible(true);
  m_vSplit->setHandleWidth(4);
  m_vSplit->setStyleSheet(QString("QSplitter::handle { background:%1; }"
                                  "QSplitter::handle:hover { background:%2; }"
                                  "QSplitter { background:%3; }")
                              .arg(TM().colors().splitter,
                                   TM().colors().colActive,
                                   TM().colors().bgDeep));
  rootLay->addWidget(m_vSplit, 1);
  m_vSplit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_vSplit->setMinimumHeight(200);

  auto *millerWrapper = new QWidget();
  millerWrapper->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgDeep));
  auto *millerWrapLay = new QVBoxLayout(millerWrapper);
  millerWrapLay->setContentsMargins(0, 0, 0, 0);
  millerWrapLay->setSpacing(0);

  buildActiveHeader(millerWrapLay);
  m_vSplit->addWidget(millerWrapper);

  const int vSplitIdx = rootLay->indexOf(m_vSplit);
  buildMillerBreadcrumbBar(rootLay, vSplitIdx);

  auto *lowerWidget = new QWidget();
  lowerWidget->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgDeep));
  buildLowerFilePane(lowerWidget);

  m_vSplit->addWidget(lowerWidget);
  m_vSplit->setSizes({200, 450});
  m_vSplit->setStretchFactor(0, 0);
  m_vSplit->setStretchFactor(1, 1);

  {
    auto s = Config::group("UI");
    const QString key = m_settingsKey + "/vSplitState";
    const QByteArray state = s.readEntry(key, QByteArray());
    if (!state.isEmpty())
      m_vSplit->restoreState(state);
  }

  setupMillerToggleAndSplitter();
  initSearchPanel(rootLay);
}

// --- PaneWidget::initConnections ---
// Verbindet Signale: Pfad-Updates, Navigation, Footer, Toolbar
void PaneWidget::connectMillerSignals() {
  connect(m_miller, &MillerArea::pathChanged, this,
          [this](const QString &path) { navigateTo(path, true, false); });
  connect(m_miller, &MillerArea::pathChanged, this,
          [this](const QString &path) { updateActiveTabLabel(path); });
  connect(m_miller, &MillerArea::kioPathRequested, this,
          [this](const QString &path) { navigateTo(path); });
  connect(m_miller, &MillerArea::openInLeft, this,
          &PaneWidget::openInLeftRequested);
  connect(m_miller, &MillerArea::openInRight, this,
          &PaneWidget::openInRightRequested);
  connect(m_miller, &MillerArea::propertiesRequested, this,
          [](const QString &path) {
            auto *dlg =
                new KPropertiesDialog(QUrl::fromUserInput(path), nullptr);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
          });
  connect(m_miller, &MillerArea::focusRequested, this,
          &PaneWidget::focusRequested);
  connect(m_miller, &MillerArea::editPathRequested, this,
          [this]() { openPathEditOverlay(); });
  connect(m_miller, &MillerArea::headerClicked, this,
          [this](const QString &path) {
            emit focusRequested();
            if (path == "__drives__") {
              m_miller->navigateTo("__drives__");
              if (m_activeHeaderBtn) {
                m_activeHeaderBtn->setIcon(QIcon::fromTheme("computer"));
                m_activeHeaderBtn->setText(tr("Dieser PC"));
              }
            } else if (!path.isEmpty()) {
              navigateTo(path);
            } else {
              m_pathEdit->setText(currentPath());
              m_pathEdit->selectAll();
              m_pathStack->setCurrentIndex(1);
              m_pathEdit->setFocus();
            }
          });
}

void PaneWidget::updateActiveTabLabel(const QString &path) {
  const QString name =
      (path == QStringLiteral("__drives__") || path.isEmpty())
          ? tr("Dieser PC")
          : (QUrl(path).fileName().isEmpty() ? path : QUrl(path).fileName());
  const QIcon icon =
      (path == QStringLiteral("__drives__") || path.isEmpty())
          ? QIcon::fromTheme("computer")
          : QIcon::fromTheme("folder");
  if (m_currentTab == 0 && m_activeHeaderBtn) {
    m_activeHeaderBtn->setIcon(icon);
    m_activeHeaderBtn->setText(name);
  } else if (m_currentTab > 0) {
    const int btnIdx = m_currentTab - 1;
    if (btnIdx < m_tabButtons.size()) {
      m_tabButtons[btnIdx]->setIcon(icon);
      m_tabButtons[btnIdx]->setText(name);
    }
  }
}

void PaneWidget::connectFilePaneSignals() {
  connect(m_filePane, &FilePane::selectionChanged, this,
          [this](int count, const QString &path) {
            emit focusRequested();
            refreshFooter(path, count);
          });
  connect(&ThumbnailManager::instance(), &ThumbnailManager::thumbnailReady,
          this, [this](const QString &path, const QPixmap &pix) {
            if (path == m_lastPreviewPath) {
              m_lastPreviewPixmap = pix;
              const int h = m_footerBar->height();
              const int iconSize = qBound(120, h - 40, 1024);
              m_previewIcon->setPixmap(pix.scaled(iconSize, iconSize,
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
            }
          });
  connect(m_filePane, &FilePane::focusRequested, this,
          &PaneWidget::focusRequested);
  connect(m_filePane, &FilePane::fileActivated, this,
          [this](const QString &path) {
            emit focusRequested();
            const bool isKio = !path.startsWith("/") && path.contains(":/");
            if (isKio || QFileInfo(path).isDir()) {
              navigateTo(path);
            } else {
              auto *job = new KIO::OpenUrlJob(QUrl::fromUserInput(path));
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(
                  KJobUiDelegate::AutoHandlingEnabled, this));
              job->start();
            }
          });
}

void PaneWidget::connectToolbarSignals() {
  connect(m_toolbar, &PaneToolbar::newFolderClicked, this,
          [this]() { emit newFolderRequested(); });
  connect(m_toolbar, &PaneToolbar::deleteClicked, this,
          [this]() { emit filePane() -> deleteRequested(); });

  connect(m_toolbar, &PaneToolbar::emptyTrashClicked, this, [this]() { handleEmptyTrash(); });

  connect(m_toolbar, &PaneToolbar::sortClicked, this, [this]() {
    auto *hdr = m_filePane->view()->header();
    m_filePane->view()->sortByColumn(
        hdr->sortIndicatorSection(),
        hdr->sortIndicatorOrder() == Qt::AscendingOrder ? Qt::DescendingOrder
                                                        : Qt::AscendingOrder);
  });
  connect(m_toolbar, &PaneToolbar::actionsClicked, this, [this]() {
    QModelIndex cur = m_filePane->view()->currentIndex();
    if (!cur.isValid())
      return;
    m_filePane->view()->customContextMenuRequested(
        m_filePane->view()->visualRect(cur).center());
  });

  connect(m_toolbar, &PaneToolbar::upClicked, this, [this]() {
    QDir d(currentPath());
    if (d.cdUp())
      navigateTo(d.absolutePath());
  });
  connect(m_toolbar, &PaneToolbar::foldersFirstToggled, this,
          [this](bool on) { m_filePane->setFoldersFirst(on); });
  connect(m_toolbar, &PaneToolbar::viewModeChanged, this,
          [this](int mode) { m_filePane->setViewMode(mode); });

  connect(m_filePane, &FilePane::viewModeChanged, m_toolbar,
          &PaneToolbar::setViewMode);

  connect(
      m_filePane, &FilePane::directoryLoaded, this,
      [this]() { handleDirectoryLoaded(); },
      Qt::SingleShotConnection);
  connect(m_toolbar, &PaneToolbar::backClicked, this, [this]() {
    if (!m_histBack.isEmpty()) {
      m_histFwd.push(currentPath());
      navigateTo(m_histBack.pop(), false);
    }
  });
  connect(m_toolbar, &PaneToolbar::forwardClicked, this, [this]() {
    if (!m_histFwd.isEmpty()) {
      m_histBack.push(currentPath());
      navigateTo(m_histFwd.pop(), false);
    }
  });
}

void PaneWidget::handleDirectoryLoaded() {
  const QString key = QStringLiteral("FilePane/") + m_settingsKey +
                      QStringLiteral("/viewMode");
  const int savedMode = Config::group("UI").readEntry(key, 0);
  if (savedMode != 0)
    m_filePane->setViewMode(savedMode);
}

void PaneWidget::handleEmptyTrash() {
  if (!DialogUtils::question(
          this, tr("Papierkorb leeren"),
          tr("Möchten Sie den Papierkorb wirklich leeren?")))
    return;
  auto *job = KIO::emptyTrash();
  job->start();
  if (job->uiDelegate())
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
}

void PaneWidget::initConnections() {
  connectMillerSignals();
  connectFilePaneSignals();
  connectToolbarSignals();

  connect(m_pathEdit, &QLineEdit::returnPressed, this,
          [this]() { navigateTo(m_pathEdit->text()); });

  const QString startPath = "__drives__";
  m_filePane->setRootPath(QDir::homePath());
  m_pathEdit->setText(tr("Dieser PC"));
  m_toolbar->setPath(startPath);
  m_miller->init();
}

void PaneWidget::setFocused(bool f) {
  m_focused = f;
  setStyleSheet(QString("background:%1;")
                    .arg(f ? TM().colors().bgBox : TM().colors().bgDeep));

  if (auto *d =
          static_cast<FilePaneDelegate *>(m_filePane->view()->itemDelegate())) {
    d->focused = f;
    m_filePane->view()->viewport()->update();
  }

  auto *v = m_filePane->view();
  // FrameStyle auf NoFrame setzen minimiert die Ränder automatisch
  v->setFrameStyle(QFrame::NoFrame);
  v->viewport()->setAttribute(Qt::WA_TranslucentBackground);
  v->verticalScrollBar()->hide();
  v->horizontalScrollBar()->hide();

  v->setStyleSheet(
      QString(
          "QTreeView{background:%1;border:none;color:%2;outline:none;font-size:"
          "10px;}"
          "QTreeView::item{padding:2px 4px;}"
          "QTreeView::item:hover{background:%3;}"
          "QTreeView::item:selected{background:%4;color:%5;}"

          // Header-Fix: Hintergrund geht bis zum Rand
          "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
          "QHeaderView::section{background:%6;color:%7;border:none;"
          "border-bottom:1px solid %3;border-right:1px solid %3;"
          "padding:3px 6px;font-size:10px;}"
          "QHeaderView::section:last{border-right:none;}"

          "QTreeView::corner{background:transparent;border:none;}"

          // Scrollbar komplett versteckt
          "QTreeView "
          "QScrollBar:vertical{width:0px;background:transparent;border:none;}"
          "QTreeView "
          "QScrollBar:horizontal{height:0px;background:transparent;border:none;"
          "}")
          .arg(f ? TM().colors().bgList : TM().colors().bgDeep)
          .arg(TM().colors().textPrimary, TM().colors().bgHover,
               TM().colors().bgSelect)
          .arg(TM().colors().textLight, TM().colors().bgBox,
               TM().colors().textAccent));
}

QString PaneWidget::currentPath() const {
  // m_filePane->currentPath() gibt den echten Pfad zurück (nicht den
  // Anzeigenamen)
  if (m_filePane)
    return m_filePane->currentPath();
  return m_pathEdit ? m_pathEdit->text() : QDir::homePath();
}

void PaneWidget::navigateTo(const QString &path, bool clearForward,
                            bool updateMiller) {
  const bool isKio = !path.startsWith("/") && path.contains(":/");
  if (path.isEmpty())
    return;
  // Lokale Pfade die nicht existieren überspringen; KIO-URLs und Spezialpfade
  // durchlassen
  if (!isKio && path != "__drives__" && !QFileInfo::exists(path))
    return;

  const QString cur = currentPath();
  if (!cur.isEmpty() && cur != path) {
    if (clearForward)
      m_histFwd.clear();
    m_histBack.push(cur);
  }
  m_toolbar->setNavState(!m_histBack.isEmpty(), !m_histFwd.isEmpty());

  // Aktiven Tab-State aktuell halten
  if (m_currentTab >= 0 && m_currentTab < m_tabs.size())
    m_tabs[m_currentTab].path = path;

  const QUrl url = QUrl::fromUserInput(path);
  if (path == "__drives__") {
    // Liste nicht auf remote:/ zwingen, falls der User lieber home oder den
    // alten Pfad sieht
    if (m_filePane->currentPath().isEmpty())
      m_filePane->setRootPath(QDir::homePath());
    m_pathEdit->setText(tr("Dieser PC"));
    m_toolbar->setPath(path);
    if (updateMiller)
      m_miller->navigateTo("__drives__");
    if (m_millerCollapsed && m_breadcrumbBar)
      updateBreadcrumb(QStringLiteral("__drives__"));
    emit pathUpdated(QStringLiteral("__drives__"));
    return;
  }
  m_filePane->setRootPath(path);
  m_pathEdit->setText(url.isLocalFile() ? url.toLocalFile() : path);
  m_toolbar->setPath(path);
  if (updateMiller && !m_miller->cols().isEmpty())
    m_miller->navigateTo(path);

  m_toolbar->setCount(0, 0);
  if (m_millerCollapsed && m_breadcrumbBar)
    updateBreadcrumb(path);
  emit pathUpdated(path);
}

void PaneWidget::updateBreadcrumb(const QString &path) {
  if (!m_breadcrumbBar)
    return;

  auto *lay = qobject_cast<QHBoxLayout *>(m_breadcrumbBar->layout());
  if (!lay)
    return;

  // Alle alten Widgets entfernen (außer dem abschließenden Stretch)
  while (lay->count() > 0) {
    auto *item = lay->takeAt(0);
    if (item->widget())
      item->widget()->deleteLater();
    delete item;
  }

  // Hilfslambda: Trenner
  auto addSep = [&]() {
    auto *sep = new QLabel(QStringLiteral("\\"), m_breadcrumbBar);
    sep->setObjectName("bcSep");
    lay->addWidget(sep);
  };

  // Segment "Dieser PC" — navigiert zu __drives__
  auto *driveBtn = new QPushButton(tr("Dieser PC"), m_breadcrumbBar);
  driveBtn->setCursor(Qt::PointingHandCursor);
  connect(driveBtn, &QPushButton::clicked, this,
          [this]() { navigateTo(QStringLiteral("__drives__")); });
  lay->addWidget(driveBtn);

  if (path != QStringLiteral("__drives__") && !path.isEmpty()) {
    // Pfad in Segmente aufteilen
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString accumulated;
    for (int i = 0; i < parts.size(); ++i) {
      accumulated += QLatin1Char('/') + parts[i];
      const QString segPath = accumulated;
      const bool isLast = (i == parts.size() - 1);

      addSep();

      auto *btn = new QPushButton(parts[i], m_breadcrumbBar);
      btn->setCursor(Qt::PointingHandCursor);

      if (isLast) {
        // Letztes Segment: öffnet Pfadeingabe
        connect(btn, &QPushButton::clicked, this,
                [this]() { openPathEditOverlay(); });
      } else {
        connect(btn, &QPushButton::clicked, this,
                [this, segPath]() { navigateTo(segPath); });
      }
      lay->addWidget(btn);
    }
  }

  lay->addStretch();
}

void PaneWidget::openPathEditOverlay() {
  auto *overlay = new QWidget(this);
  const int y = m_activeHeader ? m_activeHeader->geometry().bottom() : 0;
  const int h = Config::millerHeaderHeight();
  overlay->setGeometry(0, y, width(), h);
  overlay->setStyleSheet(
      QString("QWidget { background:%1; }"
              "QLineEdit { background:#000000; color:#ffffff; border:1px "
              "solid %2;"
              "  border-radius:0; padding:0 8px; min-height:30px; "
              "max-height:30px; }"
              "KUrlRequester { background:%1; }")
          .arg(TM().colors().bgList, TM().colors().separator));
  overlay->raise();
  overlay->show();

  auto *lay = new QHBoxLayout(overlay);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(2);

  auto *urlReq =
      new KUrlRequester(QUrl::fromUserInput(currentPath()), overlay);
  urlReq->setMode(KFile::Directory | KFile::ExistingOnly);
  urlReq->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  auto *copyBtn = new QToolButton(overlay);
  copyBtn->setIcon(QIcon::fromTheme("edit-copy"));
  copyBtn->setFixedSize(28, 28);
  copyBtn->setToolTip(tr("Pfad kopieren"));
  copyBtn->setStyleSheet(TM().ssToolBtn());

  auto *closeBtn = new QToolButton(overlay);
  closeBtn->setIcon(QIcon::fromTheme("dialog-close"));
  closeBtn->setFixedSize(24, 24);
  closeBtn->setToolTip(tr("Schließen"));
  closeBtn->setStyleSheet(TM().ssToolBtn());

  lay->addWidget(urlReq, 1);
  lay->addWidget(copyBtn);
  lay->addWidget(closeBtn);

  urlReq->setFocus();

  setupPathEditOverlayConnections(overlay, urlReq, copyBtn, closeBtn);
}

void PaneWidget::setupPathEditOverlayConnections(QWidget *overlay, KUrlRequester *urlReq, QToolButton *copyBtn, QToolButton *closeBtn) {
  auto cleanup = [overlay]() { overlay->deleteLater(); };

  connect(urlReq, &KUrlRequester::returnPressed, this,
          [this, urlReq, cleanup]() {
            navigateTo(urlReq->url().toLocalFile().isEmpty()
                           ? urlReq->url().toString()
                           : urlReq->url().toLocalFile());
            cleanup();
          });
  connect(urlReq, &KUrlRequester::urlSelected, this,
          [this, cleanup](const QUrl &url) {
            navigateTo(url.toLocalFile().isEmpty() ? url.toString()
                                                   : url.toLocalFile());
            cleanup();
          });
  connect(closeBtn, &QToolButton::clicked, this, cleanup);
  connect(copyBtn, &QToolButton::clicked, this,
          [urlReq]() { QApplication::clipboard()->setText(urlReq->text()); });

  auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), overlay);
  connect(esc, &QShortcut::activated, this, cleanup);

  connect(qApp, &QApplication::focusChanged, overlay,
          [overlay](QWidget *, QWidget *now) {
            if (overlay &&
                (!now || (!overlay->isAncestorOf(now) && now != overlay)))
              overlay->deleteLater();
          });
}

void PaneWidget::setActionCollection(KActionCollection *ac) {
  m_actionCollection = ac;
  if (m_filePane)
    m_filePane->setActionCollection(ac);
}

void PaneWidget::setMillerVisible(bool visible) {
  if (m_millerToggle)
    m_millerToggle->setChecked(visible);
}

void PaneWidget::setViewMode(int mode) {
  m_toolbar->setViewMode(mode);
  m_filePane->setViewMode(mode);
}

// --- Tab-Methoden ---

void PaneWidget::addTab(const QString &path) {
  const QString newPath = path.isEmpty() ? QStringLiteral("__drives__") : path;
  TabState state;
  state.path = newPath;
  m_tabs.append(state);

  const QString label =
      (newPath == QStringLiteral("__drives__")) ? tr("Dieser PC")
      : QUrl(newPath).fileName().isEmpty()      ? newPath
                                                : QUrl(newPath).fileName();
  const QIcon icon = (newPath == QStringLiteral("__drives__"))
                         ? QIcon::fromTheme("computer")
                         : QIcon::fromTheme("folder");

  const int btnIdx = m_tabButtons.size();

  QFrame *frame = nullptr;
  QToolButton *btn = nullptr;
  buildTabButtonUI(label, icon, btnIdx, frame, btn);

  m_tabButtons.append(btn);
  m_tabStripLay->addWidget(frame);

  m_tabStrip->show();

  switchTab(m_tabs.size() - 1);
}

void PaneWidget::buildTabButtonUI(const QString &label, const QIcon &icon, int btnIdx, QFrame *&frame, QToolButton *&btn) {
  frame = new QFrame();
  frame->setFixedHeight(Config::millerHeaderHeight());
  frame->setMinimumWidth(40);
  frame->setMaximumWidth(130);
  frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  frame->setStyleSheet(
      QString(
          "QFrame { background:%1; border:none; border-top-right-radius:6px; }")
          .arg(TM().colors().bgDeep));
  auto *fLay = new QHBoxLayout(frame);
  fLay->setContentsMargins(0, 0, 0, 0);
  fLay->setSpacing(0);

  btn = new QToolButton();
  btn->setObjectName(QStringLiteral("MillerTabBtn"));
  btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  btn->setFixedHeight(Config::millerHeaderHeight());
  btn->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
  btn->setIcon(icon);
  btn->setText(label);
  btn->setMinimumWidth(24);
  btn->setMaximumWidth(110);
  btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btn->setStyleSheet(
      QString("QToolButton#MillerTabBtn { background:transparent; border:none;"
              "  color:%1; font-family:'Segoe UI Semilight','Roboto "
              "Light',sans-serif;"
              "  font-weight:300; font-size:14px; padding:4px 8px; "
              "text-align:left; }"
              "QToolButton#MillerTabBtn::menu-indicator { image:none; }")
          .arg(TM().colors().textPrimary));
  connect(btn, &QToolButton::clicked, this,
          [this, btnIdx]() { switchTab(btnIdx + 1); });

  auto *closeBtn = new QToolButton();
  closeBtn->setText(QStringLiteral("×"));
  closeBtn->setFixedSize(16, Config::millerHeaderHeight());
  closeBtn->setProperty("tabCloseBtn", true);
  closeBtn->setStyleSheet(
      QString("QToolButton { background:transparent; color:%1; border:none;"
              "  font-size:13px; font-weight:600; }"
              "QToolButton:hover { color:#ffffff; }")
          .arg(TM().colors().textMuted));
  closeBtn->setToolTip(tr("Tab schließen"));
  connect(closeBtn, &QToolButton::clicked, this,
          [this, btnIdx]() { closeTab(btnIdx + 1); });

  fLay->addWidget(btn, 1);
  fLay->addWidget(closeBtn);
}

void PaneWidget::closeTab(int index) {
  if (m_tabs.size() <= 1 || index < 0 || index >= m_tabs.size())
    return;

  if (index > 0) {
    const int btnIdx = index - 1;
    if (btnIdx < m_tabButtons.size()) {
      auto *btn = m_tabButtons.takeAt(btnIdx);
      // frame ist parent von btn
      auto *frame = btn->parentWidget();
      if (frame)
        frame->deleteLater();
      else
        btn->deleteLater();

      // Reconnect folgende Buttons
      for (int i = btnIdx; i < m_tabButtons.size(); ++i) {
        auto *b = m_tabButtons[i];
        const int ni = i;
        b->disconnect(SIGNAL(clicked()));
        connect(b, &QToolButton::clicked, this,
                [this, ni]() { switchTab(ni + 1); });
        auto *f = b->parentWidget();
        if (f) {
          for (auto *cb : f->findChildren<QToolButton *>()) {
            if (cb->property("tabCloseBtn").toBool()) {
              cb->disconnect(SIGNAL(clicked()));
              connect(cb, &QToolButton::clicked, this,
                      [this, ni]() { closeTab(ni + 1); });
            }
          }
        }
      }
    }
  }

  m_tabs.removeAt(index);

  if (m_tabs.size() <= 1)
    m_tabStrip->hide();

  m_currentTab = qMin(m_currentTab, m_tabs.size() - 1);
  switchTab(m_currentTab);
}

void PaneWidget::switchTab(int index) {
  if (index < 0 || index >= m_tabs.size())
    return;
  // Aktuellen State sichern — logischen Pfad aus m_tabs nehmen falls __drives__
  if (m_currentTab >= 0 && m_currentTab < m_tabs.size()) {
    // path wird bereits in navigateTo aktuell gehalten
    m_tabs[m_currentTab].histBack = m_histBack;
    m_tabs[m_currentTab].histFwd = m_histFwd;
  }
  m_currentTab = index;
  m_histBack = m_tabs[index].histBack;
  m_histFwd = m_tabs[index].histFwd;
  m_toolbar->setNavState(!m_histBack.isEmpty(), !m_histFwd.isEmpty());

  // m_activeHeaderBtn: aktiv=bgList, inaktiv=bgDeep
  if (m_activeHeaderBtn) {
    const bool active0 = (index == 0);
    m_activeHeaderBtn->setStyleSheet(
        tabBtnStyle(active0 ? TM().colors().bgList : TM().colors().bgDeep,
                    TM().colors().textPrimary));
  }

  // Extra-Tab-Buttons: aktiv=bgList, inaktiv=bgDeep
  for (int i = 0; i < m_tabButtons.size(); ++i) {
    const bool active = (i + 1 == index);
    const QString bg = active ? TM().colors().bgList : TM().colors().bgDeep;
    auto *frame = m_tabButtons[i]->parentWidget();
    if (frame) {
      frame->setStyleSheet(QString("QFrame { background:%1; border:none; "
                                   "border-top-right-radius:6px; }")
                               .arg(bg));
    }
  }

  navigateTo(m_tabs[index].path, false);
}

void PaneWidget::saveState() const {
  if (m_settingsKey.isEmpty() || !m_vSplit)
    return;
  auto s = Config::group("UI");
  if (!m_millerCollapsed)
    s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
  s.writeEntry(m_settingsKey + "/millerCollapsed", m_millerCollapsed);
  s.writeEntry(m_settingsKey + "/currentPath", currentPath());
  QStringList tabPaths;
  for (const auto &t : m_tabs)
    tabPaths << (t.path.isEmpty() ? QStringLiteral("__drives__") : t.path);
  s.writeEntry(m_settingsKey + "/tabs", tabPaths);
  s.writeEntry(m_settingsKey + "/currentTab", m_currentTab);
  s.config()->sync();
}

void PaneWidget::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFooterPanel();
  if (m_searchOverlay && m_searchOverlay->isVisible()) {
    const QPoint topLeft = m_vSplit->mapTo(this, QPoint(0, 0));
    m_searchOverlay->setGeometry(topLeft.x(), topLeft.y(), m_vSplit->width(),
                                 qMin(300, m_vSplit->height()));
  }
}
