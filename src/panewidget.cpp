#include "panewidget.h"
#include "mainwindow.h"
#include <QShortcut>
#include <QKeySequence>
#include "config.h"
#include "dialogutils.h"
#include "panecomponents.h"
#include "scglobal.h"
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
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

// --- PaneWidget ---
PaneWidget::PaneWidget(const QString &settingsKey, QWidget *parent)
    : QWidget(parent), m_settingsKey(settingsKey) {
  setStyleSheet(QString("background:%1;").arg(TM().colors().bgDeep));
  auto *rootLay = new QVBoxLayout(this);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  initTabBar(rootLay);
  initSearchPanel(rootLay);
  initSplitter(rootLay);
  initConnections();
  buildFooter(rootLay);
}

// --- PaneWidget::initTabBar ---
void PaneWidget::initTabBar(QVBoxLayout *rootLay) {
  auto *tabBar = new QWidget();
  tabBar->setFixedHeight(46);
  tabBar->setStyleSheet(
      QString("background:%1; border:none;").arg(TM().colors().bgMain));
  auto *tabLay = new QHBoxLayout(tabBar);
  tabLay->setContentsMargins(4, 0, 4, 0);
  tabLay->setSpacing(2);

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
          [breadcrumbBtn](const QString &p) {
            QUrl url(p);
            if (p == "__drives__") {
              breadcrumbBtn->setText(tr("Dieser PC"));
            } else {
              QString name = url.fileName();
              if (name.isEmpty())
                name = p;
              breadcrumbBtn->setText(name);
            }
          });

  m_millerToggle = new QToolButton();
  m_millerToggle->setFixedSize(24, 24);
  m_millerToggle->setCheckable(true);
  m_millerToggle->setChecked(true);
  m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
  m_millerToggle->setIconSize(QSize(14, 14));
  m_millerToggle->setToolTip(tr("Miller-Columns ein-/ausklappen"));
  m_millerToggle->setStyleSheet(TM().ssToolBtn());

  m_searchBtn = new QToolButton();
  m_searchBtn->setFixedSize(30, 30);
  m_searchBtn->setIcon(QIcon::fromTheme("system-search"));
  m_searchBtn->setIconSize(QSize(18, 18));
  m_searchBtn->setToolTip(tr("Suchen"));
  m_searchBtn->setCheckable(true);
  m_searchBtn->setStyleSheet(TM().ssToolBtn());

  auto *layoutBtn = new QToolButton();
  layoutBtn->setFixedSize(30, 30);
  layoutBtn->setIcon(QIcon::fromTheme("view-split-left-right"));
  layoutBtn->setIconSize(QSize(18, 18));
  layoutBtn->setToolTip(tr("Layout wählen"));
  layoutBtn->setStyleSheet(TM().ssToolBtn());

  auto *hamburgerBtn = new QToolButton();
  hamburgerBtn->setFixedSize(30, 30);
  hamburgerBtn->setIcon(QIcon::fromTheme("application-menu"));
  hamburgerBtn->setIconSize(QSize(18, 18));
  hamburgerBtn->setToolTip(tr("Menü"));
  hamburgerBtn->setStyleSheet(TM().ssToolBtn() +
                              " QToolButton::menu-indicator { image: none; }");
  hamburgerBtn->setPopupMode(QToolButton::InstantPopup);

  initHamburgerMenu(hamburgerBtn, layoutBtn);

  tabLay->addWidget(m_millerToggle);
  tabLay->addWidget(m_pathStack, 1);
  tabLay->addWidget(m_searchBtn);
  tabLay->addWidget(layoutBtn);
  tabLay->addWidget(hamburgerBtn);
  rootLay->addWidget(tabBar);
}

// --- PaneWidget::initHamburgerMenu ---
void PaneWidget::initSplitter(QVBoxLayout *rootLay) {
  // --- Vertikaler Splitter ---
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

  m_miller = new MillerArea();
  m_miller->setMinimumHeight(150);
  m_miller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_vSplit->addWidget(m_miller);

  auto *lowerWidget = new QWidget();
  lowerWidget->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgDeep));
  auto *lowerLay = new QVBoxLayout(lowerWidget);
  lowerLay->setContentsMargins(0, 0, 0, 0);
  lowerLay->setSpacing(0);
  m_toolbar = new PaneToolbar();
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
  m_vSplit->addWidget(lowerWidget);
  m_vSplit->setSizes({200, 450});
  m_vSplit->setStretchFactor(0, 0);
  m_vSplit->setStretchFactor(1, 1);

  // Gespeicherte Position wiederherstellen (pane-spezifischer Key)
  {
    auto s = Config::group("UI");
    const QString key = m_settingsKey + "/vSplitState";
    const QByteArray state = s.readEntry(key, QByteArray());
    if (!state.isEmpty())
      m_vSplit->restoreState(state);
  }

  // Position beim Verschieben speichern — nur als Backup bei Drag
  connect(m_vSplit, &QSplitter::splitterMoved, this, [](int, int) {
    // m_millerCollapsed nicht über splitterMoved setzen — nur über Toggle
    // State wird beim App-Beenden in saveState() gespeichert
  });

  connect(m_millerToggle, &QToolButton::toggled, this, [this](bool checked) {
    if (checked) {
      // Aufklappen: gespeicherte Größe wiederherstellen
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

      m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
      m_millerToggle->setToolTip("Miller-Columns ausklappen");
      m_millerCollapsed = false;
    } else {
      // Einklappen: aktuelle Größe vorher sichern
      if (m_vSplit->sizes().value(0) > 0) {
        auto s = Config::group("UI");
        s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
        s.config()->sync();
      }

      m_vSplit->setSizes({0, 1});
      m_millerToggle->setIcon(QIcon::fromTheme("go-down"));
      m_millerToggle->setToolTip("Miller-Columns einblenden");
      m_millerCollapsed = true;
    }
  });

  // Miller-Toggle-State beim Start setzen
  {
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
}

// --- PaneWidget::initConnections ---
void PaneWidget::initConnections() {
  // Verbindungen
  connect(m_miller, &MillerArea::pathChanged, this,
          [this](const QString &path) { navigateTo(path, true, false); });
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
  connect(m_miller, &MillerArea::headerClicked, this,
          [this](const QString &path) {
            emit focusRequested();
            if (path == "__drives__") {
              m_miller->navigateTo("__drives__");
            } else if (!path.isEmpty()) {
              navigateTo(path);
            } else {
              m_pathEdit->setText(currentPath());
              m_pathEdit->selectAll();
              m_pathStack->setCurrentIndex(1);
              m_pathEdit->setFocus();
            }
          });
  connect(m_filePane, &FilePane::selectionChanged, this,
          [this](int count, const QString &path) {
            emit focusRequested();
            refreshFooter(path, count);
          });
  connect(&ThumbnailManager::instance(), &ThumbnailManager::thumbnailReady, this, [this](const QString &path, const QPixmap &pix) {
    if (path == m_lastPreviewPath) {
      m_lastPreviewPixmap = pix;
      const int h = m_footerBar->height();
      const int iconSize = qBound(120, h - 40, 1024);
      m_previewIcon->setPixmap(pix.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
  });
  connect(m_filePane, &FilePane::focusRequested, this,
          &PaneWidget::focusRequested);
  connect(m_filePane, &FilePane::fileActivated, this,
          [this](const QString &path) {
            emit focusRequested();
            // KIO-URL oder lokales Verzeichnis
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

  // Toolbar-Verbindungen
  connect(m_toolbar, &PaneToolbar::newFolderClicked, this,
          [this]() { emit newFolderRequested(); });
  connect(m_toolbar, &PaneToolbar::deleteClicked, this,
          [this]() { emit filePane() -> deleteRequested(); });

  connect(m_toolbar, &PaneToolbar::emptyTrashClicked, this, [this]() {
    if (!DialogUtils::question(
            this, tr("Papierkorb leeren"),
            tr("Möchten Sie den Papierkorb wirklich leeren?")))
      return;
    auto *job = KIO::emptyTrash();
    job->start();
    if (job->uiDelegate())
      job->uiDelegate()->setAutoErrorHandlingEnabled(true);
  });
  connect(m_toolbar, &PaneToolbar::copyClicked, this,
          [this]() { emit copyToOtherPaneRequested(); });

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

  // Toolbar bleibt synchron wenn FilePane den Mode selbst setzt (z.B. beim
  // Laden)
  connect(m_filePane, &FilePane::viewModeChanged, m_toolbar,
          &PaneToolbar::setViewMode);

  // Gespeicherte Ansicht nach erstem Laden wiederherstellen
  connect(
      m_filePane, &FilePane::directoryLoaded, this,
      [this]() {
        const QString key = QStringLiteral("FilePane/") + m_settingsKey +
                            QStringLiteral("/viewMode");
        const int savedMode = Config::group("UI").readEntry(key, 0);
        if (savedMode != 0)
          m_filePane->setViewMode(savedMode);
      },
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

          /* Header-Fix: Hintergrund geht bis zum Rand */
          "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
          "QHeaderView::section{background:%6;color:%7;border:none;"
          "border-bottom:1px solid %3;border-right:1px solid %3;"
          "padding:3px 6px;font-size:10px;}"
          "QHeaderView::section:last{border-right:none;}"

          "QTreeView::corner{background:transparent;border:none;}"

          /* Scrollbar komplett versteckt */
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

  const QUrl url = QUrl::fromUserInput(path);
  if (path == "__drives__") {
    // Liste nicht auf remote:/ zwingen, falls der User lieber home oder den alten Pfad sieht
    if (m_filePane->currentPath().isEmpty()) m_filePane->setRootPath(QDir::homePath());
    m_pathEdit->setText(tr("Dieser PC"));
    m_toolbar->setPath(path);
    if (updateMiller)
      m_miller->navigateTo("__drives__");
    return;
  }
  m_filePane->setRootPath(path);
  m_pathEdit->setText(url.isLocalFile() ? url.toLocalFile() : path);
  m_toolbar->setPath(path);
  if (updateMiller && !m_miller->cols().isEmpty())
    m_miller->navigateTo(path);

  m_toolbar->setCount(0, 0);
  emit pathUpdated(path);
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

void PaneWidget::saveState() const {
  if (m_settingsKey.isEmpty() || !m_vSplit)
    return;
  auto s = Config::group("UI");
  if (!m_millerCollapsed)
    s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
  s.writeEntry(m_settingsKey + "/millerCollapsed", m_millerCollapsed);
  s.writeEntry(m_settingsKey + "/currentPath", currentPath());
  s.config()->sync();
}

void PaneWidget::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFooterPanel();
}
