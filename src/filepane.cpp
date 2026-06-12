#include "filepane.h"

#include <QApplication>
#include <KActionCollection>
#include <KFormat>
#include <QPointer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include "config.h"
#include "tagmanager.h"
#include "thumbnailmanager.h"
#include "thememanager.h"
#include <KJob>

#include <QAction>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>

#include <QFileDialog>
#include <QKeyEvent>
#include <QScrollBar>

#include "drophandler.h"
#include <QStandardPaths>

#include <QDir>
#include <QResizeEvent>
#include <QTimer>

#include <QClipboard>
#include <KIO/OpenUrlJob>
#include <KDesktopFile>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>

#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <KApplicationTrader>
#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFileUtils>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/Job>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/PasteJob>
#include <KIO/RenameDialog>
#include <KIO/RestoreJob>
#include <KJobWidgets>
#include <KNewFileMenu>
#include <KPropertiesDialog>
#include <KService>
#include "scremoveaction.h"





// --- Hilfsfunktionen ---



// --- SCTreeView: QTreeView-Subklasse die Drag vs. Rubber-Band wie Dolphin steuert ---
// startDrag wird nur ausgeführt wenn das beim Press angeklickte Item selektiert war.
// Sonst: kein Drag → Qt fällt auf Rubber-Band-Selektion zurück.
class SCTreeView : public QTreeView {
public:
  explicit SCTreeView(QWidget *parent = nullptr) : QTreeView(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
  }

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton) {
      const QModelIndex idx = indexAt(e->pos());
      // Drag erlaubt nur wenn Item bereits selektiert
      m_dragAllowed = idx.isValid() && selectionModel()->isSelected(idx);
    }
    QTreeView::mousePressEvent(e);
  }

  void startDrag(Qt::DropActions supportedActions) override {
    if (!m_dragAllowed)
      return; // kein Drag → Rubber-Band
    QTreeView::startDrag(supportedActions);
  }

private:
  bool m_dragAllowed = false;
};

// --- SCListView: QListView-Subklasse mit Drag-Schutz wie SCTreeView ---
class SCListView : public QListView {
public:
  explicit SCListView(QWidget *parent = nullptr) : QListView(parent) {}

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton) {
      const QModelIndex idx = indexAt(e->pos());
      m_dragAllowed = idx.isValid() && selectionModel()->isSelected(idx);
    }
    QListView::mousePressEvent(e);
  }

  void startDrag(Qt::DropActions supportedActions) override {
    if (!m_dragAllowed)
      return;
    QListView::startDrag(supportedActions);
  }

private:
  bool m_dragAllowed = false;
};




// --- Spalten-Definitionen ---
const QList<FPColDef> &FilePane::colDefs() {
  static QList<FPColDef> defs = {
      {FP_NAME, QCoreApplication::translate("SplitCommander", "Name"), "", true, 220},
      {FP_TYP, QCoreApplication::translate("SplitCommander", "Typ"), "", true, 48},
      {FP_ALTER, QCoreApplication::translate("SplitCommander", "Alter"), "", true, 48},
      {FP_DATUM, QCoreApplication::translate("SplitCommander", "Geändert"), "", true, 80},
      {FP_ERSTELLT, QCoreApplication::translate("SplitCommander", "Erstellt"), "", false, 80},
      {FP_ZUGRIFF, QCoreApplication::translate("SplitCommander", "Letzter Zugriff"), "", false, 80},
      {FP_GROESSE, QCoreApplication::translate("SplitCommander", "Größe"), "", true, 60},
      {FP_RECHTE, QCoreApplication::translate("SplitCommander", "Rechte"), "", true, 68},
      {FP_EIGENTUEMER, QCoreApplication::translate("SplitCommander", "Eigentümer"), QCoreApplication::translate("SplitCommander", "Weitere"), false, 70},
      {FP_GRUPPE, QCoreApplication::translate("SplitCommander", "Benutzergruppe"), QCoreApplication::translate("SplitCommander", "Weitere"), false, 80},
      {FP_PFAD, QCoreApplication::translate("SplitCommander", "Pfad"), QCoreApplication::translate("SplitCommander", "Weitere"), false, 120},
      {FP_ERWEITERUNG, QCoreApplication::translate("SplitCommander", "Dateierweiterung"), QCoreApplication::translate("SplitCommander", "Weitere"), false, 80},
      {FP_TAGS, QCoreApplication::translate("SplitCommander", "Tags"), "", false, 50},
      {FP_IMG_DATUM, QCoreApplication::translate("SplitCommander", "Datum der Aufnahme"), QCoreApplication::translate("SplitCommander", "Bild"), false, 80},
      {FP_IMG_ABMESS, QCoreApplication::translate("SplitCommander", "Abmessungen"), QCoreApplication::translate("SplitCommander", "Bild"), false, 80},
      {FP_IMG_BREITE, QCoreApplication::translate("SplitCommander", "Breite"), QCoreApplication::translate("SplitCommander", "Bild"), false, 50},
      {FP_IMG_HOEHE, QCoreApplication::translate("SplitCommander", "Höhe"), QCoreApplication::translate("SplitCommander", "Bild"), false, 50},
      {FP_IMG_AUSRICHT, QCoreApplication::translate("SplitCommander", "Ausrichtung"), QCoreApplication::translate("SplitCommander", "Bild"), false, 60},
      {FP_AUD_KUENSTLER, QCoreApplication::translate("SplitCommander", "Künstler"), QCoreApplication::translate("SplitCommander", "Audio"), false, 80},
      {FP_AUD_GENRE, QCoreApplication::translate("SplitCommander", "Genre"), QCoreApplication::translate("SplitCommander", "Audio"), false, 60},
      {FP_AUD_ALBUM, QCoreApplication::translate("SplitCommander", "Album"), QCoreApplication::translate("SplitCommander", "Audio"), false, 80},
      {FP_AUD_DAUER, QCoreApplication::translate("SplitCommander", "Dauer"), QCoreApplication::translate("SplitCommander", "Audio"), false, 50},
      {FP_AUD_BITRATE, QCoreApplication::translate("SplitCommander", "Bitrate"), QCoreApplication::translate("SplitCommander", "Audio"), false, 60},
      {FP_AUD_STUECK, QCoreApplication::translate("SplitCommander", "Stück"), QCoreApplication::translate("SplitCommander", "Audio"), false, 40},
      {FP_VID_SEITENVERH, QCoreApplication::translate("SplitCommander", "Seitenverhältnis"), QCoreApplication::translate("SplitCommander", "Video"), false, 60},
      {FP_VID_FRAMERATE, QCoreApplication::translate("SplitCommander", "Bildwiederholrate"), QCoreApplication::translate("SplitCommander", "Video"), false, 60},
      {FP_VID_DAUER, QCoreApplication::translate("SplitCommander", "Dauer"), QCoreApplication::translate("SplitCommander", "Video"), false, 50},
      {FP_DOC_TITEL, QCoreApplication::translate("SplitCommander", "Titel"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 80},
      {FP_DOC_AUTOR, QCoreApplication::translate("SplitCommander", "Autor"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 70},
      {FP_DOC_HERAUSGEBER, QCoreApplication::translate("SplitCommander", "Herausgeber"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 80},
      {FP_DOC_SEITEN, QCoreApplication::translate("SplitCommander", "Seitenanzahl"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 50},
      {FP_DOC_WOERTER, QCoreApplication::translate("SplitCommander", "Wortanzahl"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 60},
      {FP_DOC_ZEILEN, QCoreApplication::translate("SplitCommander", "Zeilenanzahl"), QCoreApplication::translate("SplitCommander", "Dokument"), false, 60},
  };
  return defs;
}



// --- FilePane::setupColumns ---
void FilePane::setupColumns() {
  m_colVisible.resize(FP_COUNT);
  for (int i = 0; i < FP_COUNT; i++)
    m_colVisible[i] = false;
  for (const auto &d : colDefs())
    m_colVisible[d.id] = d.defaultVisible;

  auto s = Config::group("UI").group("columns");
  for (const auto &d : colDefs())
    if (s.hasKey(QString::number(d.id)))
      m_colVisible[d.id] = s.readEntry(QString::number(d.id), d.defaultVisible);

  // Sichtbare Spalten zusammenstellen
  QList<FPCol> visCols;
  for (const auto &d : colDefs())
    if (m_colVisible[d.id])
      visCols << d.id;

  m_proxy->setVisibleCols(visCols);
}

// --- FilePane Konstruktor ---
FilePane::FilePane(QWidget *parent, const QString &settingsKey)
    : QWidget(parent) {
  m_settingsKey = QStringLiteral("FilePane/") + settingsKey + QStringLiteral("/");
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);

  m_stack = new QStackedWidget(this);
  lay->addWidget(m_stack);

  setupModel();
  setupView();
  setupConnections();
  setRootPath(QDir::homePath());
}


// --- FilePane::setupModel ---
void FilePane::setupModel() {
  m_lister = new KDirLister(this);
  m_lister->setAutoUpdate(true);
  m_lister->setMainWindow(window());
  m_lister->setShowHiddenFiles(Config::showHiddenFiles());
  connect(m_lister, &KDirLister::completed, this, [this]() {
    QTimer::singleShot(0, this, [this]() { emit directoryLoaded(); });
  });

  m_dirModel = new KDirModel(this);
  m_dirModel->setDirLister(m_lister);

  m_sortProxy = new KDirSortFilterProxyModel(this);
  m_sortProxy->setSourceModel(m_dirModel);
  m_sortProxy->setSortFoldersFirst(true);
  m_sortProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

  m_proxy = new FPColumnsProxy(this);
  m_proxy->setSourceModel(m_sortProxy);

  connect(m_proxy, &QAbstractItemModel::rowsInserted, this,
          &FilePane::modelUpdated);
  connect(m_proxy, &QAbstractItemModel::rowsRemoved, this,
          &FilePane::modelUpdated);
  connect(m_proxy, &QAbstractItemModel::modelReset, this,
          &FilePane::modelUpdated);
  connect(m_proxy, &QAbstractItemModel::layoutChanged, this,
          &FilePane::modelUpdated);

}


void FilePane::buildTreeView() {
  m_view = new SCTreeView(this);
  m_view->setRootIsDecorated(false);
  m_view->setItemsExpandable(false);
  m_view->setUniformRowHeights(true);
  m_view->setSortingEnabled(true);
  m_view->setAlternatingRowColors(false);
  m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setMouseTracking(true);
  m_view->setFrameStyle(QFrame::NoFrame);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_view->header()->setMinimumSectionSize(0);
  m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
  m_view->setDragEnabled(true);
  m_view->setAcceptDrops(true);
  m_view->setDropIndicatorShown(true);
  m_view->setDragDropMode(QAbstractItemView::DragDrop);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_view->setModel(m_proxy);

  m_delegate = new FilePaneDelegate(this);
  m_view->setItemDelegate(m_delegate);

  auto s = Config::group("General");
  const int savedHeight = s.readEntry(m_settingsKey + "rowHeight", 26);

  m_delegate->rowHeight = savedHeight;
  m_delegate->fontSize = qBound(9, savedHeight / 3, 16);
  m_view->setIconSize(
      QSize(Config::listIconSize(), Config::listIconSize()));

  setupColumns();
  setupTreeViewAppearance();
}

void FilePane::setupTreeViewAppearance() {
  m_view->setStyleSheet(
      QString(
          "QTreeView{background:%1;border:none;color:%2;outline:none;font-size:"
          "10px;}"
          "QTreeView::item{padding:2px 4px;}"
          "QTreeView::item:hover{background:%3;}"
          "QTreeView::item:selected{background:%4;color:%5;}"
          "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
          "QHeaderView::section{background:%6;color:%7;border:none;"
          "border-bottom:1px solid %3;"
          "padding:3px 6px;font-size:10px;}"
          "QTreeView "
          "QScrollBar:vertical{background:transparent;width:0px;margin:0px;"
          "border:none;}"
          "QTreeView "
          "QScrollBar::handle:vertical{background:%8;border-"
          "radius:5px;min-height:20px;margin:2px;}"
          "QTreeView "
          "QScrollBar::handle:vertical:hover{background:%9;}"
          "QTreeView QScrollBar::add-line:vertical,QTreeView "
          "QScrollBar::sub-line:vertical{height:0px;}"
          "QTreeView QScrollBar::add-page:vertical,QTreeView "
          "QScrollBar::sub-page:vertical{background:transparent;}"
          "QTreeView "
          "QScrollBar:horizontal{background:transparent;height:0px;margin:0px;"
          "border:none;}")
          .arg(TM().colors().bgList, TM().colors().textPrimary,
               TM().colors().bgHover, TM().colors().bgSelect,
               TM().colors().textLight, TM().colors().bgBox,
               TM().colors().textAccent,
               TM().colors().separator, TM().colors().accent));
  m_view->viewport()->setStyleSheet("background:transparent;");
  m_view->viewport()->setAttribute(Qt::WA_TranslucentBackground);
}

void FilePane::setupTreeViewHeader() {
  auto *hdr = m_view->header();
  const QList<FPCol> &visCols = m_proxy->visibleCols();
  for (int i = 0; i < visCols.size(); ++i) {
    FPCol col = visCols.at(i);
    bool isName = (col == FP_NAME);
    hdr->setSectionResizeMode(i, isName ? QHeaderView::Stretch
                                        : QHeaderView::Interactive);
    if (!isName) {
      for (const auto &d : colDefs())
        if (d.id == col) {
          hdr->resizeSection(i, d.defaultWidth);
          break;
        }
    }
  }
  hdr->setSectionsClickable(true);
  hdr->setSortIndicatorShown(true);
  hdr->setSortIndicator(0, Qt::AscendingOrder);
  m_sortProxy->sort(KDirModel::Name, Qt::AscendingOrder);
  connect(m_lister, &KDirLister::completed, this, [hdr]() {
    if (hdr->sortIndicatorSection() == 0)
      hdr->setSortIndicator(0, hdr->sortIndicatorOrder());
  }, Qt::SingleShotConnection);
  hdr->setStretchLastSection(false);
  hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  hdr->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(hdr, &QHeaderView::customContextMenuRequested, this,
          &FilePane::showHeaderMenu);
  {
    auto s = Config::group("General");
    for (int i = 0; i < visCols.size(); ++i) {
      if (visCols.at(i) == FP_NAME) continue;
      const int saved = s.readEntry(m_settingsKey + "colW_" + QString::number(visCols.at(i)), 0);
      if (saved > 0) hdr->resizeSection(i, saved);
    }
  }
  connect(hdr, &QHeaderView::sectionResized, this,
          [this](int idx, int, int newSize) {
            const QList<FPCol> &vc = m_proxy->visibleCols();
            if (idx < 0 || idx >= vc.size() || vc.at(idx) == FP_NAME) return;
            auto s = Config::group("General");
            s.writeEntry(m_settingsKey + "colW_" + QString::number(vc.at(idx)), newSize);
            s.config()->sync();
          });
  connect(hdr, &QHeaderView::sectionDoubleClicked, this,
          [this, hdr](int col) {
            if (m_proxy->visibleCols().value(col) == FP_NAME) return;
            QFontMetrics fm(m_view->font());
            int maxW = hdr->sectionSizeHint(col);
            const int rows = m_proxy->rowCount(m_view->rootIndex());
            for (int r = 0; r < qMin(rows, 500); ++r) {
              const QString t = m_proxy->index(r, col, m_view->rootIndex())
                                    .data(Qt::DisplayRole).toString();
              if (!t.isEmpty()) maxW = qMax(maxW, fm.horizontalAdvance(t) + 32);
            }
            hdr->resizeSection(col, qMax(maxW, 40));
          });
}

void FilePane::setupOverlayScrollbars() {
  m_overlayBar = new QScrollBar(Qt::Vertical, this);
  m_overlayBar->setStyleSheet(
      QString("QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:none;}"
              "QScrollBar::handle:vertical{background:%1;border-radius:4px;min-height:20px;margin:1px;}"
              "QScrollBar::handle:vertical:hover{background:%2;}"
              "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
              "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}")
          .arg(TM().colors().separator, TM().colors().accent));
  m_overlayBar->hide();
  m_overlayBar->raise();
  auto *native = m_view->verticalScrollBar();
  connect(native, &QScrollBar::rangeChanged, m_overlayBar,
          &QScrollBar::setRange);
  connect(native, &QScrollBar::valueChanged, m_overlayBar,
          &QScrollBar::setValue);
  connect(native, &QScrollBar::rangeChanged, this,
          [this](int, int max) { m_overlayBar->setVisible(max > 0); });
  connect(m_overlayBar, &QScrollBar::valueChanged, native,
          &QScrollBar::setValue);

  m_overlayHBar = new QScrollBar(Qt::Horizontal, this);
  m_overlayHBar->setStyleSheet(
      QString("QScrollBar:horizontal{background:transparent;height:6px;margin:0px;border:none;}"
              "QScrollBar::handle:horizontal{background:%1;border-radius:3px;min-width:20px;margin:1px;}"
              "QScrollBar::handle:horizontal:hover{background:%2;}"
              "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0px;}"
              "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}")
          .arg(TM().colors().separator, TM().colors().accent));
  m_overlayHBar->hide();
  m_overlayHBar->raise();
  auto *nativeH = m_view->horizontalScrollBar();
  connect(nativeH, &QScrollBar::rangeChanged, m_overlayHBar,
          &QScrollBar::setRange);
  connect(nativeH, &QScrollBar::valueChanged, m_overlayHBar,
          &QScrollBar::setValue);
  connect(nativeH, &QScrollBar::rangeChanged, this,
          [this](int, int max) { m_overlayHBar->setVisible(max > 0); });
  connect(m_overlayHBar, &QScrollBar::valueChanged, nativeH,
          &QScrollBar::setValue);
}

void FilePane::buildIconView() {
  m_iconView = new SCListView(this);
  m_iconView->setModel(m_proxy);
  m_iconView->setItemDelegate(new ScaledIconDelegate(m_iconView));
  m_iconView->setSelectionModel(m_view->selectionModel());
  m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_iconView->setMouseTracking(true);
  m_iconView->setDragEnabled(true);
  m_iconView->setAcceptDrops(true);
  m_iconView->setDropIndicatorShown(true);
  m_iconView->setDragDropMode(QAbstractItemView::DragDrop);
  m_iconView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_iconView->setContextMenuPolicy(Qt::CustomContextMenu);
  m_iconView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_iconView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_iconView->verticalScrollBar()->setSingleStep(26);
  m_iconView->setWordWrap(true);
  m_iconView->setStyleSheet(
      QString(
          "QListView{background:%1;border:none;color:%2;outline:none;font-size:"
          "12px;}"
          "QListView::item{padding:4px;border-radius:4px;}"
          "QListView::item:hover{background:%3;}"
          "QListView::item:selected{background:%4;color:%5;}"
          "QListView "
          "QScrollBar:vertical{width:0px;background:transparent;border:none;}")
          .arg(TM().colors().bgList, TM().colors().textPrimary,
               TM().colors().bgHover, TM().colors().bgSelect,
               TM().colors().textLight));
}

void FilePane::setupView() {
  buildTreeView();
  setupTreeViewHeader();
  setupOverlayScrollbars();
  buildIconView();

  m_stack->addWidget(m_view);
  m_stack->addWidget(m_iconView);
  m_stack->setCurrentWidget(m_view);
}


// --- FilePane::setupConnections ---
void FilePane::setupConnections() {
  connectTreeViewSignals();
  connectIconViewSignals();
  setupDropHandlers();
  connectSelectionSignals();
  connectMiscSignals();
}

void FilePane::connectTreeViewSignals() {
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_view, &QTreeView::customContextMenuRequested, this,
          &FilePane::showContextMenu);
  connect(m_view, &QTreeView::activated, this, &FilePane::onItemActivated);
  connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
    auto gs = Config::group("General");
    if (gs.readEntry("singleClick", false))
      onItemActivated(idx);
  });
}

void FilePane::connectIconViewSignals() {
  connect(m_iconView, &QListView::clicked, this,
          [this](const QModelIndex &idx) {
            auto gs = Config::group("General");
            if (gs.readEntry("singleClick", false))
              onItemActivated(idx);
          });

  connect(m_iconView, &QListView::customContextMenuRequested, this,
          &FilePane::showContextMenu);
  connect(m_iconView, &QListView::activated, this, &FilePane::onItemActivated);
}

void FilePane::setupDropHandlers() {
  auto resolver = [this](const QModelIndex &idx) -> QUrl {
    QUrl dest = QUrl::fromUserInput(m_currentPath);
    if (idx.isValid() && m_proxy) {
      KFileItem item = m_proxy->fileItem(idx);
      if (!item.isNull() && item.isDir())
        dest = item.url();
    }
    return dest;
  };
  m_view->viewport()->installEventFilter(
      new DropHandler(m_view, resolver, m_view));
  m_iconView->viewport()->installEventFilter(
      new DropHandler(m_iconView, resolver, m_iconView));
}

void FilePane::connectSelectionSignals() {
  connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &cur, const QModelIndex &) {
            KFileItem item = m_proxy->fileItem(cur);
            if (!item.isNull())
              emit fileSelected(item.localPath().isEmpty()
                                    ? item.url().toString()
                                    : item.localPath());
          });

  connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this]() {
            const auto selectedIdx =
                m_view->selectionModel()->selectedIndexes();
            const QModelIndex cur = m_view->selectionModel()->currentIndex();
            KFileItem item = m_proxy->fileItem(cur);
            QString path = item.isNull() ? QString()
                                         : (item.localPath().isEmpty()
                                                ? item.url().toString()
                                                : item.localPath());
            QSet<int> seenRows;
            for (const auto &idx : selectedIdx)
              seenRows.insert(idx.row());
            emit selectionChanged(seenRows.count(), path);
          });
}

void FilePane::connectMiscSignals() {
  connect(&TagManager::instance(), &TagManager::fileTagChanged, this,
          [this](const QString &) {
            if (!m_currentTagFilter.isEmpty()) {
              showTaggedFiles(m_currentTagFilter);
              return;
            }
            const QList<FPCol> &visCols = m_proxy->visibleCols();
            int tagColIdx = -1;
            for (int i = 0; i < visCols.size(); ++i)
              if (visCols.at(i) == FP_TAGS) {
                tagColIdx = i;
                break;
              }
            if (tagColIdx >= 0 && m_proxy->rowCount() > 0)
              emit m_proxy->dataChanged(
                  m_proxy->index(0, tagColIdx),
                  m_proxy->index(m_proxy->rowCount() - 1, tagColIdx));
            emit m_proxy->layoutChanged();
          });

  connect(&ThumbnailManager::instance(), &ThumbnailManager::thumbnailReady, this, [this](const QString &path) {
      Q_UNUSED(path)
      if (!m_proxy) return;
      m_proxy->layoutChanged();
  });

  m_newFileMenu = new KNewFileMenu(this);
  connect(m_newFileMenu, &KNewFileMenu::fileCreated, this,
          &FilePane::onNewFileCreated);
}

// --- Navigation ---

void FilePane::setRootPath(const QString &path) {
  m_proxy->clearDirSizeCache();
  if (path.isEmpty())
    return;

  // Globalen Tag-Modus beenden wenn wir normal navigieren
  if (m_proxy->isGlobalTagMode()) {
    m_proxy->setGlobalTagMode(QString(), {});
    m_currentTagFilter.clear();
  }

  // KIO-URL oder URL-artiger Pfad erkennen
  if (!path.startsWith("/") && !path.startsWith("~") && !path.isEmpty()) {
    const QUrl url(path);
    const QString scheme = url.scheme().toLower();
    if (scheme == "file") {
      // Lokale file:// URL in normalen Pfad umwandeln und fortfahren
      m_kioMode = false;
      m_currentUrl = QUrl();
      m_currentPath = url.toLocalFile();
      executeLocalPathLoad(url);
      return;
    }

    static const QStringList kioSchemes = {"gdrive",
                                           "smb",
                                           "sftp",
                                           "ftp",
                                           "ftps",
                                           "mtp",
                                           "remote",
                                           "network",
                                           "bluetooth",
                                           "davs",
                                           "dav",
                                           "nfs",
                                           "fish",
                                           "webdav",
                                           "webdavs",
                                           "afc",
                                           "zeroconf",
                                           "trash",
                                           "recentdocuments",
                                           "tags"};
    if (!scheme.isEmpty() && kioSchemes.contains(scheme)) {
      setRootUrl(url);
      return;
    }
  }

  m_kioMode = false;
  m_currentUrl = QUrl();
  m_currentPath = path;
  m_proxy->setTagFilter(QString());

  QUrl url = QUrl::fromLocalFile(path);
  executeLocalPathLoad(url);
}

void FilePane::executeLocalPathLoad(const QUrl &url) {
  m_lister->stop();
  m_lister->openUrl(url);
  connect(
      m_lister, &KDirLister::completed, this,
      [this, url]() {
        QModelIndex dirIdx = m_dirModel->indexForUrl(url);
        if (dirIdx.isValid()) {
          QModelIndex sortIdx = m_sortProxy->mapFromSource(dirIdx);
          QModelIndex proxyIdx = m_proxy->mapFromSource(sortIdx);
          m_view->setRootIndex(proxyIdx);
          m_iconView->setRootIndex(proxyIdx);
        }
      },
      Qt::SingleShotConnection);
}

void FilePane::setRootUrl(const QUrl &url) {
  QUrl safeUrl = url;
  if (safeUrl.path().isEmpty())
    safeUrl.setPath(QStringLiteral("/"));

  m_kioMode = true;
  m_currentUrl = safeUrl;
  m_currentPath = safeUrl.toString();
  m_lister->stop();
  m_lister->openUrl(safeUrl);

  connect(
      m_lister, &KDirLister::completed, this,
      [this, url]() {
        // Nach Auth: m_currentUrl mit evtl. vorhandenen Credentials aktualisieren
        const QUrl listerUrl = m_lister->url();
        if (!listerUrl.userInfo().isEmpty() && m_currentUrl.userInfo().isEmpty()) {
          m_currentUrl = listerUrl;
          m_currentPath = listerUrl.toString();
          // Gespeicherte NetworkPlace URL aktualisieren
          auto s = Config::group("NetworkPlaces");
          QStringList saved = s.readEntry("places", QStringList());
          const QString oldUrl = url.toString();
          const QString newUrl = listerUrl.toString();
          if (saved.contains(oldUrl)) {
            saved.replaceInStrings(oldUrl, newUrl);
            s.writeEntry("places", saved);
            // Keys umbenennen
            const QString oldKey = QString(oldUrl).replace("/","_").replace(":","_");
            const QString newKey = QString(newUrl).replace("/","_").replace(":","_");
            const QString name = s.readEntry("name_" + oldKey, QString());
            const QString icon = s.readEntry("icon_" + oldKey, QString());
            if (!name.isEmpty()) s.writeEntry("name_" + newKey, name);
            if (!icon.isEmpty()) s.writeEntry("icon_" + newKey, icon);
            s.deleteEntry("name_" + oldKey);
            s.deleteEntry("icon_" + oldKey);
            s.config()->sync();
          }
        }

        QModelIndex dirIdx = m_dirModel->indexForUrl(url);
        if (dirIdx.isValid()) {
          QModelIndex sortIdx = m_sortProxy->mapFromSource(dirIdx);
          QModelIndex proxyIdx = m_proxy->mapFromSource(sortIdx);
          m_view->setRootIndex(proxyIdx);
          m_iconView->setRootIndex(proxyIdx);
        }
      },
      Qt::SingleShotConnection);
}

void FilePane::setShowHiddenFiles(bool show) {
  if (m_lister->showHiddenFiles() == show)
    return;
  m_lister->setShowHiddenFiles(show);
  // Erneutes Laden erzwingen um Filter anzuwenden
  m_lister->openUrl(m_lister->url(), KDirLister::NoFlags);
}

const QString &FilePane::currentPath() const { return m_currentPath; }

QUrl FilePane::currentUrl() const {
  if (m_kioMode && m_currentUrl.isValid())
    return m_currentUrl;
  if (!m_currentPath.isEmpty())
    return QUrl::fromLocalFile(m_currentPath);
  return {};
}


qint64 FilePane::currentTotalSize() const {
  qint64 size = 0;
  QModelIndex root = m_view->rootIndex();
  int count = m_proxy->rowCount(root);
  for (int i = 0; i < count; ++i) {
    QModelIndex idx = m_proxy->index(i, 0, root);
    QModelIndex srcIdx = m_proxy->mapToSource(idx);
    QModelIndex dirIdx = m_sortProxy->mapToSource(srcIdx);
    KFileItem item = m_dirModel->itemForIndex(dirIdx);
    if (!item.isNull()) {
      size += item.size();
    }
  }
  return size;
}

void FilePane::reload() {
  if (m_kioMode)
    m_lister->openUrl(m_currentUrl);
  else
    m_lister->openUrl(QUrl::fromLocalFile(m_currentPath));
}

void FilePane::setNameFilter(const QString &pattern) {
  m_lister->setNameFilter(pattern);
  reload();
}

void FilePane::setFoldersFirst(bool on) {
  m_foldersFirst = on;
  m_sortProxy->setSortFoldersFirst(on);
}

void FilePane::setRowHeight(int height) {
  // Nur für Detailliste
  if (m_delegate) {
    m_delegate->rowHeight = height;
    m_delegate->fontSize = qBound(9, height / 3, 16);
    m_view->setIconSize(
        QSize(qBound(12, height - 6, 48), qBound(12, height - 6, 48)));
  }
  auto s = Config::group("General");
  s.writeEntry(m_settingsKey + "rowHeight", height);
  s.config()->sync();

  m_view->update();
}

void FilePane::showTaggedFiles(const QString &tagName) {
  m_currentTagFilter = tagName;

  QStringList paths = TagManager::instance().filesWithTag(tagName);
  QList<KFileItem> items;
  for (const QString &p : paths) {
    if (QFileInfo::exists(p)) {
      // Wir erstellen KFileItems direkt aus den Pfaden.
      // KFileItem wird versuchen die Metadaten zu lesen.
      items.append(KFileItem(QUrl::fromLocalFile(p)));
    }
  }
  m_proxy->setGlobalTagMode(tagName, items);
}

QList<QUrl> FilePane::selectedUrls() const {
  QList<QUrl> urls;

  const auto indexes = m_view->selectionModel()->selectedIndexes();
  QSet<int> seenRows;
  for (const auto &idx : indexes) {
    if (idx.column() != 0) continue;
    if (seenRows.contains(idx.row())) continue;
    seenRows.insert(idx.row());
    KFileItem item = m_proxy->fileItem(idx);
    if (!item.isNull())
      urls << item.url();
  }
  return urls;
}


// --- Spalten-Sichtbarkeit ---
void FilePane::setColumnVisible(int colId, bool visible) {
  if (colId < 0 || colId >= FP_COUNT)
    return;
  if (m_colVisible[colId] == (bool)visible)
    return;
  m_colVisible[colId] = visible;

  auto s = Config::group("UI").group("columns");
  s.writeEntry(QString::number(colId), visible);
  s.config()->sync();

  QList<FPCol> visCols;
  for (const auto &d : colDefs())
    if (m_colVisible[d.id])
      visCols << d.id;
  m_proxy->setVisibleCols(visCols);

  // Header-Breiten neu setzen
  auto *hdr = m_view->header();
  for (int i = 0; i < visCols.size(); ++i) {
    FPCol col = visCols.at(i);
    bool isName = (col == FP_NAME);
    hdr->setSectionResizeMode(i, isName ? QHeaderView::Stretch
                                        : QHeaderView::Interactive);
    if (!isName)
      for (const auto &d : colDefs())
        if (d.id == col) {
          hdr->resizeSection(i, d.defaultWidth);
          break;
        }
  }

  emit columnsChanged(colId, visible);
}

void FilePane::setViewMode(int mode) {
  m_viewMode = mode;
  Config::group("UI").writeEntry(m_settingsKey + "viewMode", mode);
  emit viewModeChanged(mode);
  switch (mode) {
  case 0: // Details — TreeView
    m_stack->setCurrentWidget(m_view);
    break;
  case 1: // Kompakt — TopToBottom Flow, Wrapping = neue Spalte wenn voll
    m_stack->setCurrentWidget(m_iconView);
    m_iconView->setViewMode(QListView::ListMode);
    m_iconView->setFlow(QListView::TopToBottom);
    m_iconView->setWrapping(true);
    m_iconView->setResizeMode(QListView::Fixed);
    m_iconView->setIconSize(QSize(38, 38));
    m_iconView->setGridSize(QSize(280, 44));
    m_iconView->setSpacing(0);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setWordWrap(false);
    m_iconView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_iconView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    break;
  case 2: // Symbole — IconMode, 48px Icons
    m_stack->setCurrentWidget(m_iconView);
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setFlow(QListView::LeftToRight);
    m_iconView->setWrapping(true);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setIconSize(QSize(100, 100));
    m_iconView->setGridSize(QSize(130, 130));
    m_iconView->setSpacing(12);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setWordWrap(true);
    m_iconView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_iconView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    break;
  }
  // rootIndex synchron halten
  if (mode != 0) {
    QModelIndex root = m_view->rootIndex();
    if (root.isValid())
      m_iconView->setRootIndex(root);
  }
}

// --- onItemActivated ---
void FilePane::onItemActivated(const QModelIndex &index) {
  KFileItem item = m_proxy->fileItem(index);
  if (item.isNull())
    return;

  QString path =
      item.localPath().isEmpty() ? item.url().toString() : item.localPath();

  if (path.contains(QStringLiteral("new-account"))) {
    QProcess::startDetached("kcmshell6", {"kcm_kaccounts"});
    return;
  }

  if (handleRemoteViewItem(item))
    return;

  // Verzeichnis oder KIO-Navigation → navigieren
  // Wir prüfen explizit auf isDir(), damit Dateien nicht als Ordner "geöffnet"
  // werden
  if (item.isDir()) {
    emit fileActivated(path);
    return;
  }

  // Datei öffnen
  auto *job = new KIO::OpenUrlJob(item.url());
  job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
  job->start();
}

bool FilePane::handleRemoteViewItem(const KFileItem &item) {
  // remoteViewMap: UDS_NAME -> Ziel-URL aus /usr/share/remoteview/*.desktop
  static QHash<QString, QString> remoteViewMap;
  static bool remoteViewLoaded = false;
  if (!remoteViewLoaded) {
    remoteViewLoaded = true;
    const QDir remoteDir(QStringLiteral("/usr/share/remoteview"));
    for (const QFileInfo &fi :
         remoteDir.entryInfoList({QStringLiteral("*.desktop")}, QDir::Files)) {
      KDesktopFile df(fi.absoluteFilePath());
      const QString urlVal = df.readUrl();
      const QString baseName = fi.completeBaseName();
      if (!urlVal.isEmpty())
        remoteViewMap.insert(baseName, urlVal);
    }
  }

  // Priorität 1: UDS_TARGET_URL — kio-gdrive setzt das direkt auf gdrive:/
  const QUrl targetUrl = item.targetUrl();
  if (targetUrl.isValid() && targetUrl != item.url()) {
    emit fileActivated(targetUrl.toString());
    return true;
  }

  // Priorität 2: remoteViewMap über UDS_NAME
  const QString udsName = item.text();
  if (remoteViewMap.contains(udsName)) {
    emit fileActivated(remoteViewMap.value(udsName));
    return true;
  }

  // Priorität 3: baseName aus URL (z.B. "gdrive-network" → "gdrive")
  const QString urlBaseName = item.url().path().section('/', -1).section('-', 0, 0);
  if (remoteViewMap.contains(urlBaseName)) {
    emit fileActivated(remoteViewMap.value(urlBaseName));
    return true;
  }

  return false;
}

// --- resizeEvent / eventFilter ---
void FilePane::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  if (m_overlayBar) {
    const int w = 8;
    const int hdrH = m_view->header()->height();
    m_overlayBar->setGeometry(m_view->width() - w, m_view->y() + hdrH, w,
                              m_view->height() - hdrH);
  }
  if (m_overlayHBar) {
    const int h = 6;
    const int rsvd = (m_overlayBar && m_overlayBar->isVisible()) ? 8 : 0;
    m_overlayHBar->setGeometry(m_view->x(), m_view->y() + m_view->height() - h,
                               m_view->width() - rsvd, h);
    m_overlayHBar->raise();
  }
  // Spaltenbreiten anpassen, damit das Pane ausgefüllt bleibt
  onSectionResized(-1, 0, 0);
}

bool FilePane::eventFilter(QObject *obj, QEvent *e) {
  return QWidget::eventFilter(obj, e);
}

void FilePane::onNewFileCreated(const QUrl &) { reload(); }

void FilePane::setActionCollection(KActionCollection *ac) {
  m_actionCollection = ac;
}

void FilePane::stopLister() {
  m_lister->stop();
}

// --- showHeaderMenu ---
void FilePane::onSectionResized(int, int, int) {
  if (m_inSectionResized || !m_view || !m_view->header())
    return;

  m_inSectionResized = true;
  QHeaderView *hdr = m_view->header();
  const int viewW = m_view->viewport()->width();

  // Wir nehmen die Name-Spalte (Index 0) als "elastische" Spalte.
  // Wenn eine andere Spalte geändert wird, passt sich der Name an.
  // Wenn das ganze Fenster (index -1) resized wird, passt sich der Name ebenfalls an.

  int otherWidths = 0;
  int nameIdx = -1;

  for (int i = 0; i < hdr->count(); ++i) {
    if (hdr->isSectionHidden(i))
      continue;
    
    // Wir suchen den Index der Name-Spalte (FP_NAME)
    // Da der User Spalten verschieben kann, prüfen wir die logische ID
    int logicalIdx = hdr->logicalIndex(i);
    FPCol colId = m_proxy->visibleCols().at(logicalIdx);
    
    if (colId == FP_NAME) {
      nameIdx = logicalIdx;
    } else {
      otherWidths += hdr->sectionSize(logicalIdx);
    }
  }

  if (nameIdx != -1) {
    int newNameW = viewW - otherWidths;
    if (newNameW < 100)
      newNameW = 100; // Mindestbreite für den Namen
    
    if (hdr->sectionSize(nameIdx) != newNameW) {
      hdr->resizeSection(nameIdx, newNameW);
    }
  }

  m_inSectionResized = false;
}
