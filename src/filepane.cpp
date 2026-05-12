#include "filepane.h"
#include <QApplication>
#include <KActionCollection>
#include <QPointer>
#include "config.h"
#include "mainwindow.h"
#include "tagmanager.h"
#include "thememanager.h"
#include <KJob>

#include <QAction>
#include <QColor>
#include <QGraphicsDropShadowEffect>
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
#include <QSettings>

#include "drophandler.h"
#include <QStandardPaths>

#include <QDir>
#include <QResizeEvent>
#include <QTimer>

#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>

#include "terminalutils.h"
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

// Exakt wie DolphinRemoveAction:
// Delegiert an file_trash oder file_delete aus der ActionCollection.
// Shift-Taste schaltet live um solange das Menü offen ist.
class SCRemoveAction : public QAction
{
public:
  enum class ShiftState { Unknown, Pressed, Released };

  SCRemoveAction(KActionCollection *collection, QObject *parent)
    : QAction(parent), m_collection(collection)
  {
    update();
    connect(this, &QAction::triggered, this, [this]() {
      if (m_action) m_action->trigger();
    });
  }

  void update(ShiftState state = ShiftState::Unknown) {
    if (!m_collection) return;
    if (state == ShiftState::Unknown) {
      state = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
          ? ShiftState::Pressed : ShiftState::Released;
    }
    if (state == ShiftState::Pressed) {
      m_action = m_collection->action(QStringLiteral("file_delete"));
    } else {
      m_action = m_collection->action(QStringLiteral("file_trash"));
    }
    if (m_action) {
      setText(m_action->text());
      setIcon(m_action->icon());
      setEnabled(m_action->isEnabled());
      // Shortcut anzeigen wie Dolphin
      if (state == ShiftState::Pressed)
        setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
      else
        setShortcut(QKeySequence(Qt::Key_Delete));
    }
  }

private:
  QPointer<KActionCollection> m_collection;
  QPointer<QAction> m_action;
};

// Hilfsfunktion: FPColumnsProxy-Index -> KFileItem
// Berücksichtigt den rootIndex-Parent korrekt
static KFileItem fp_fileItemFromProxyIndex(
    const QModelIndex &proxyIdx,
    const QModelIndex &rootProxyIdx,
    KDirSortFilterProxyModel *sortProxy,
    KDirModel *dirModel)
{
  if (!proxyIdx.isValid()) return KFileItem();
  // rootIndex in sortProxy mappen
  QModelIndex rootSortIdx;
  if (rootProxyIdx.isValid()) {
    // rootProxyIdx ist ein FPColumnsProxy-Index ohne Parent
    // mapToSource: FPColumnsProxy -> sortProxy (row 1:1)
    rootSortIdx = sortProxy->index(rootProxyIdx.row(), 0);
  }
  // proxyIdx.row() ist relativ zum rootIndex-Parent
  QModelIndex sortIdx = sortProxy->index(proxyIdx.row(), 0, rootSortIdx);
  if (!sortIdx.isValid()) return KFileItem();
  QModelIndex dirIdx = sortProxy->mapToSource(sortIdx);
  if (!dirIdx.isValid()) return KFileItem();
  return dirModel->itemForIndex(dirIdx);
}


// --- Hilfsfunktionen ---

static QString menuStyle() {
  return TM().ssMenu() + "QMenu::separator{background:rgba(236,239,244,120);"
                         "height:1px;margin:4px 8px;}";
}

static void fp_applyMenuShadow(QMenu *menu) {
  if (!menu)
    return;
  auto *shadow = new QGraphicsDropShadowEffect(menu);
  shadow->setBlurRadius(20);
  shadow->setOffset(0, 4);
  shadow->setColor(QColor(0, 0, 0, 140));
  menu->setGraphicsEffect(shadow);
}

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

// --- ShiftMenuFilter: Aktualisiert Lösch-Aktion bei Shift-Tastendruck ---
class ShiftMenuFilter : public QObject {
public:
  ShiftMenuFilter(QAction *act, QObject *parent)
      : QObject(parent), m_action(act) {}

  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (ke->key() == Qt::Key_Shift) {
        updateAction();
      }
    }
    return QObject::eventFilter(obj, event);
  }

  void updateAction() {
    if (!m_action)
      return;
    bool shift = QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier;
    if (shift) {
      m_action->setText(QObject::tr("Löschen"));
      m_action->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
      m_action->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    } else {
      m_action->setText(QObject::tr("In den Papierkorb verschieben"));
      m_action->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
      m_action->setShortcut(QKeySequence(Qt::Key_Delete));
    }
  }

private:
  QAction *m_action;
};

static QString fmtSize(qint64 sz) {
  if (sz < 1024)
    return QString("%1 B").arg(sz);
  if (sz < 1024 * 1024)
    return QString("%1 KB").arg(sz / 1024);
  if (sz < 1024LL * 1024 * 1024)
    return QString("%1 MB").arg(sz / (1024 * 1024));
  return QString("%1 GB").arg(sz / (1024LL * 1024 * 1024));
}

static QString fmtRwx(QFileDevice::Permissions p) {
  QString s;
  s += (p & QFile::ReadOwner) ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteOwner) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeOwner) ? QLatin1String("x") : QLatin1String("-");
  s += (p & QFile::ReadGroup) ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteGroup) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeGroup) ? QLatin1String("x") : QLatin1String("-");
  s += (p & QFile::ReadOther) ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteOther) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeOther) ? QLatin1String("x") : QLatin1String("-");
  return s;
}

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

// --- FPColumnsProxy ---

FPColumnsProxy::FPColumnsProxy(QObject *parent) : QAbstractProxyModel(parent) {}

void FPColumnsProxy::setSourceModel(QAbstractItemModel *model) {
  if (sourceModel()) {
    disconnect(sourceModel(), nullptr, this, nullptr);
  }
  QAbstractProxyModel::setSourceModel(model);
  m_sortProxy = qobject_cast<KDirSortFilterProxyModel *>(model);
  m_kdirModel = m_sortProxy
                    ? qobject_cast<KDirModel *>(m_sortProxy->sourceModel())
                    : nullptr;
  if (model) {
    connect(model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int f, int l) {
              beginInsertRows({}, f, l);
              endInsertRows();
            });
    connect(model, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex &, int f, int l) {
              beginRemoveRows({}, f, l);
              endRemoveRows();
            });
    connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &) {
              emit dataChanged(index(0, 0),
                               index(rowCount() - 1, columnCount() - 1));
            });
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
      beginResetModel();
      endResetModel();
    });
    connect(model, &QAbstractItemModel::layoutChanged, this,
            [this]() { emit layoutChanged(); });
  }
}

void FPColumnsProxy::setVisibleCols(const QList<FPCol> &cols) {
  beginResetModel();
  m_visCols = cols;
  endResetModel();
}

void FPColumnsProxy::setTagFilter(const QString &tag) {
  beginResetModel();
  m_tagFilter = tag;
  m_globalTagMode = false;
  m_tagItems.clear();
  endResetModel();
}

void FPColumnsProxy::setGlobalTagMode(const QString &tagName,
                                      const QList<KFileItem> &items) {
  beginResetModel();
  m_tagFilter = tagName;
  m_globalTagMode = !tagName.isEmpty();
  m_tagItems = items;
  endResetModel();
}

QModelIndex FPColumnsProxy::mapToSource(const QModelIndex &proxyIndex) const {
  if (m_globalTagMode || !proxyIndex.isValid() || !m_sortProxy)
    return {};
  if (m_tagFilter.isEmpty())
    return m_sortProxy->index(proxyIndex.row(), 0);
  // Tag-Filter aktiv: proxyIndex.row() ist die n-te akzeptierte Row in m_sortProxy
  int accepted = 0;
  for (int i = 0; i < m_sortProxy->rowCount(); ++i) {
    if (acceptsRow(i, {})) {
      if (accepted == proxyIndex.row())
        return m_sortProxy->index(i, 0);
      ++accepted;
    }
  }
  return {};
}

QModelIndex
FPColumnsProxy::mapFromSource(const QModelIndex &sourceIndex) const {
  if (m_globalTagMode || !sourceIndex.isValid())
    return {};
  return createIndex(sourceIndex.row(), 0);
}

QModelIndex FPColumnsProxy::index(int row, int column,
                                  const QModelIndex &parent) const {
  if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 ||
      column >= columnCount())
    return {};
  return createIndex(row, column);
}

QModelIndex FPColumnsProxy::parent(const QModelIndex &) const { return {}; }

int FPColumnsProxy::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  if (m_globalTagMode)
    return m_tagItems.size();
  if (!m_sortProxy)
    return 0;
  if (m_tagFilter.isEmpty())
    return m_sortProxy->rowCount();
  // Tag-Filter: zähle gültige Rows
  int count = 0;
  for (int i = 0; i < m_sortProxy->rowCount(); ++i)
    if (acceptsRow(i, {}))
      ++count;
  return count;
}

bool FPColumnsProxy::acceptsRow(int sourceRow, const QModelIndex &) const {
  if (m_tagFilter.isEmpty())
    return true;
  if (!m_sortProxy || !m_kdirModel)
    return false;
  QModelIndex sortIdx = m_sortProxy->index(sourceRow, 0);
  QModelIndex dirIdx = m_sortProxy->mapToSource(sortIdx);
  KFileItem item = m_kdirModel->itemForIndex(dirIdx);
  if (item.isNull())
    return false;
  const QString localPath = item.localPath();
  if (localPath.isEmpty())
    return false;
  return TagManager::instance().fileTag(localPath) == m_tagFilter;
}

int FPColumnsProxy::columnCount(const QModelIndex &) const {
  return m_visCols.size();
}

Qt::ItemFlags FPColumnsProxy::flags(const QModelIndex &index) const {
  if (!index.isValid())
    return Qt::ItemIsDropEnabled;
  if (!m_sortProxy)
    return Qt::NoItemFlags;
  Qt::ItemFlags f = m_sortProxy->flags(m_sortProxy->index(index.row(), 0));
  return f | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled |
         Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

Qt::DropActions FPColumnsProxy::supportedDragActions() const {
  return m_sortProxy ? m_sortProxy->supportedDragActions()
                     : Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions FPColumnsProxy::supportedDropActions() const {
  return m_sortProxy ? m_sortProxy->supportedDropActions()
                     : Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

QMimeData *FPColumnsProxy::mimeData(const QModelIndexList &indexes) const {
  if (m_globalTagMode) {
    auto *mime = new QMimeData();
    QList<QUrl> urls;
    for (const auto &idx : indexes) {
      KFileItem item = fileItem(idx);
      if (!item.isNull())
        urls << item.url();
    }
    mime->setUrls(urls);
    return mime;
  }
  if (!m_sortProxy)
    return nullptr;
  QModelIndexList sourceIndices;
  for (const QModelIndex &idx : indexes) {
    sourceIndices << mapToSource(idx);
  }
  return m_sortProxy->mimeData(sourceIndices);
}

QStringList FPColumnsProxy::mimeTypes() const {
  return m_sortProxy ? m_sortProxy->mimeTypes() : QStringList();
}

KFileItem FPColumnsProxy::fileItem(const QModelIndex &proxyIdx) const {
  if (!proxyIdx.isValid())
    return KFileItem();
  if (m_globalTagMode) {
    if (proxyIdx.row() >= 0 && proxyIdx.row() < m_tagItems.size())
      return m_tagItems.at(proxyIdx.row());
    return KFileItem();
  }
  if (!m_sortProxy || !m_kdirModel)
    return KFileItem();
  // Korrekte Kette: FPColumnsProxy -> KDirSortFilterProxyModel -> KDirModel
  QModelIndex sortIdx = mapToSource(proxyIdx);
  if (!sortIdx.isValid())
    return KFileItem();
  QModelIndex dirIdx = m_sortProxy->mapToSource(sortIdx);
  return m_kdirModel->itemForIndex(dirIdx);
}

int FPColumnsProxy::kdirColumn(FPCol col) const {
  switch (col) {
  case FP_NAME:
    return KDirModel::Name;
  case FP_GROESSE:
    return KDirModel::Size;
  case FP_DATUM:
    return KDirModel::ModifiedTime;
  case FP_RECHTE:
    return KDirModel::Permissions;
  case FP_EIGENTUEMER:
    return KDirModel::Owner;
  case FP_GRUPPE:
    return KDirModel::Group;
  default:
    return -1;
  }
}

QVariant FPColumnsProxy::extraData(const KFileItem &item, FPCol col,
                                   int role) const {
  if (item.isNull())
    return {};
  if (role != Qt::DisplayRole && role != Qt::UserRole)
    return {};
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  switch (col) {
  case FP_TYP: {
    if (role != Qt::DisplayRole)
      return {};
    if (item.isDir())
      return QStringLiteral("[DIR]");
    QString ext = QFileInfo(item.text()).suffix().toUpper().left(4);
    return ext.isEmpty() ? QStringLiteral("[???]")
                         : QStringLiteral("[") + ext + QStringLiteral("]");
  }
  case FP_ALTER: {
    qint64 mtime = item.time(KFileItem::ModificationTime).toSecsSinceEpoch();
    qint64 age = mtime > 0 ? now - mtime : -1;
    if (role == Qt::UserRole)
      return age;
    return {};
  }
  case FP_ERSTELLT: {
    if (role != Qt::DisplayRole)
      return {};
    QDateTime dt = item.time(KFileItem::CreationTime);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd")) : QString();
  }
  case FP_ZUGRIFF: {
    if (role != Qt::DisplayRole)
      return {};
    QDateTime dt = item.time(KFileItem::AccessTime);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd")) : QString();
  }
  case FP_ERWEITERUNG:
    if (role != Qt::DisplayRole)
      return {};
    return QFileInfo(item.text()).suffix();
  case FP_PFAD:
    if (role != Qt::DisplayRole)
      return {};
    return item.localPath().isEmpty()
               ? item.url().path()
               : QFileInfo(item.localPath()).absolutePath();
  case FP_TAGS: {
    if (role != Qt::DisplayRole)
      return {};
    QString lp = item.localPath();
    return lp.isEmpty() ? QVariant() : TagManager::instance().fileTag(lp);
  }
  default:
    return {};
  }
}

QVariant FPColumnsProxy::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.column() >= m_visCols.size())
    return {};
  FPCol col = m_visCols.at(index.column());

  if (role == Qt::UserRole + 99)
    return col;

  KFileItem item = fileItem(index);

  if (role == Qt::UserRole + 2 && col == FP_NAME) {
    if (!item.isNull()) {
      qint64 mtime = item.time(KFileItem::ModificationTime).toSecsSinceEpoch();
      return mtime > 0 ? QDateTime::currentSecsSinceEpoch() - mtime : -1LL;
    }
    return -1LL;
  }
  if (role == Qt::UserRole && col == FP_ALTER) {
    if (!item.isNull()) {
      qint64 mtime = item.time(KFileItem::ModificationTime).toSecsSinceEpoch();
      return mtime > 0 ? QDateTime::currentSecsSinceEpoch() - mtime : -1LL;
    }
    return -1LL;
  }

  int kdc = kdirColumn(col);
  if (kdc < 0)
    return extraData(item, col, role);

  // KDirModel-Spalten direkt aus KFileItem lesen — kein mapToSource nötig
  if (item.isNull())
    return {};

  if (col == FP_NAME) {
    if (role == Qt::DecorationRole)
      return QIcon::fromTheme(item.iconName());
    if (role == Qt::DisplayRole) {
      if (!item.isDir() && !Config::showFileExtensions())
        return QFileInfo(item.text()).completeBaseName();
      return item.text();
    }
  }
  if (col == FP_GROESSE && role == Qt::DisplayRole) {
    if (item.isDir()) {
      QString lp = item.localPath();
      if (!lp.isEmpty())
        return QString("%1 El.").arg(
            QDir(lp)
                .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
                .count());
      return {};
    }
    return fmtSize(item.size());
  }
  if (col == FP_GROESSE && role == Qt::UserRole)
    return (qint64)item.size();
  if (col == FP_DATUM && role == Qt::DisplayRole) {
    QDateTime dt = item.time(KFileItem::ModificationTime);
    return dt.isValid() ? dt.toString(Config::dateFormat()) : QString();
  }
  if (col == FP_RECHTE && role == Qt::DisplayRole) {
    QString lp = item.localPath();
    return lp.isEmpty() ? item.permissionsString()
                        : fmtRwx(QFileInfo(lp).permissions());
  }
  if (col == FP_EIGENTUEMER && role == Qt::DisplayRole)
    return item.user();
  if (col == FP_GRUPPE && role == Qt::DisplayRole)
    return item.group();

  return {};
}

QVariant FPColumnsProxy::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (orientation != Qt::Horizontal || section >= m_visCols.size())
    return {};
  if (role == Qt::DisplayRole) {
    FPCol col = m_visCols.at(section);
    for (const auto &d : FilePane::colDefs())
      if (d.id == col)
        return d.label;
  }
  if (role == Qt::TextAlignmentRole)
    return m_visCols.at(section) == FP_NAME
               ? QVariant(Qt::AlignLeft | Qt::AlignVCenter)
               : QVariant(Qt::AlignCenter);
  return {};
}

// --- Delegate ---

FilePaneDelegate::FilePaneDelegate(QObject *par) : QStyledItemDelegate(par) {}

// Delegate für Kompakt/Symbole — holt Icon direkt in der gewünschten Größe
class ScaledIconDelegate : public QStyledItemDelegate {
public:
  explicit ScaledIconDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
  void initStyleOption(QStyleOptionViewItem *opt,
                       const QModelIndex &idx) const override {
    QStyledItemDelegate::initStyleOption(opt, idx);
    QIcon ico = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    if (!ico.isNull()) {
      int sz = opt->decorationSize.width();
      // Direkt in der Zielgröße holen — kein Skalieren
      QPixmap pm = ico.pixmap(QSize(sz, sz));
      if (!pm.isNull())
        opt->icon = QIcon(pm);
    }
  }
};

QString FilePaneDelegate::formatAge(qint64 s) {
  if (s < 0)
    return {};
  if (s < 60)
    return QString("%1s").arg(s);
  if (s < 3600)
    return QString("%1m").arg(s / 60);
  if (s < 86400)
    return QString("%1h").arg(s / 3600);
  if (s < 86400 * 30)
    return QString("%1t").arg(s / 86400);
  if (s < 86400 * 365)
    return QString("%1M").arg(s / 86400 / 30);
  return QString("%1J").arg(s / 86400 / 365);
}

QColor FilePaneDelegate::ageColor(qint64 s) {
  if (s < 3600)
    return Config::ageBadgeColor(0);
  if (s < 86400)
    return Config::ageBadgeColor(1);
  if (s < 86400 * 7)
    return Config::ageBadgeColor(2);
  if (s < 86400 * 30)
    return Config::ageBadgeColor(3);
  if (s < 86400 * 365)
    return Config::ageBadgeColor(4);
  return Config::ageBadgeColor(5);
}

void FilePaneDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                             const QModelIndex &idx) const {
  QStyleOptionViewItem o = opt;
  initStyleOption(&o, idx);

  bool sel = o.state & QStyle::State_Selected;
  bool hov = o.state & QStyle::State_MouseOver;

  QColor bg, bgAlt;
  if (focused) {
    bg = QColor(TM().colors().bgList);
    bgAlt = QColor(TM().colors().bgAlternate);
  } else {
    bg = QColor(TM().colors().bgDeep);
    bgAlt = QColor(TM().colors().bgBox);
  }

  QColor bgFinal = sel   ? QColor(TM().colors().bgSelect)
                   : hov ? QColor(TM().colors().bgHover)
                         : (idx.row() % 2 ? bgAlt : bg);
  p->fillRect(o.rect, bgFinal);

  int col = idx.data(Qt::UserRole + 99).toInt();
  QRect r = o.rect.adjusted(4, 0, -4, 0);
  QFont f = o.font;
  f.setPointSize(fontSize);
  p->setFont(f);

  QColor tc = (sel || hov) ? QColor(TM().colors().textLight)
                           : QColor(TM().colors().textPrimary);
  QColor dc = (sel || hov) ? QColor(TM().colors().textLight)
                           : QColor(TM().colors().textMuted);

  if (col == FP_NAME) {
    // New-File-Indicator
    if (Config::showNewIndicator()) {

      qint64 ageSecs = idx.data(Qt::UserRole + 2).toLongLong();
      if (ageSecs > 0 && ageSecs < 86400 * 2) {
        QRect strip(o.rect.left(), o.rect.top(), 3, o.rect.height());
        p->fillRect(strip, ageColor(ageSecs));
      }
    }
    QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    const int ic = qBound(12, rowHeight - 6, 48);
    if (!icon.isNull()) {
      QPixmap pm = icon.pixmap(QSize(32, 32));
      if (pm.isNull())
        pm = icon.pixmap(QSize(16, 16));
      if (!pm.isNull())
        p->drawPixmap(
            r.left(), r.top() + (r.height() - ic) / 2,
            pm.scaled(ic, ic, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    r.setLeft(r.left() + ic + 4);
    p->setPen(tc);
    p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight,
                                         r.width()));

  } else if (col == FP_ALTER) {
    qint64 secs = idx.data(Qt::UserRole).toLongLong();
    if (secs < 0)
      return;
    QString age = formatAge(secs);
    QColor bc = ageColor(secs);

    const int BW = 44, BH = 14;
    QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2,
             BW, BH);

    p->setRenderHint(QPainter::Antialiasing, false);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->setBrush(bc);
    p->setPen(Qt::NoPen);
    p->drawRect(br);

    QColor textCol =
        (bc.lightness() > 140) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    p->setPen(textCol);
    QFont fb = f;
    fb.setBold(false);
    fb.setPointSizeF(7.5);
    fb.setHintingPreference(QFont::PreferFullHinting);
    p->setFont(fb);
    p->drawText(br, Qt::AlignCenter, age);
    p->setFont(f);

  } else if (col == FP_TAGS) {
    QString tagName = idx.data().toString();
    if (!tagName.isEmpty()) {
      QString colorStr = TagManager::instance().tagColor(tagName);
      QColor tagCol =
          colorStr.isEmpty() ? QColor(TM().colors().accent) : QColor(colorStr);
      int textW = o.fontMetrics.horizontalAdvance(tagName);
      int BW = qMin(textW + 12, r.width()), BH = 16;
      QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2,
               BW, BH);

      p->setRenderHint(QPainter::Antialiasing, false);
      p->setBrush(tagCol.darker(180));
      p->setPen(QPen(tagCol, 1));
      p->drawRect(br);

      QFont fb = f;
      fb.setBold(false);
      fb.setPointSizeF(7.5);
      fb.setHintingPreference(QFont::PreferFullHinting);
      p->setFont(fb);
      p->setPen(QColor(255, 255, 255));
      p->drawText(br, Qt::AlignCenter,
                  o.fontMetrics.elidedText(tagName, Qt::ElideRight, BW - 6));
      p->setFont(f);
    }
  } else {
    p->setPen(col == FP_GROESSE ? tc : dc);
    if (col == FP_RECHTE) {
      QFont fm = f;
      fm.setFamily(QStringLiteral("monospace"));
      fm.setPointSize(9);
      p->setFont(fm);
    }
    p->drawText(r, Qt::AlignVCenter | Qt::AlignHCenter,
                o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight,
                                         r.width()));
    p->setFont(f);
  }

  p->setPen(QColor(TM().colors().bgHover));
  p->drawLine(o.rect.bottomLeft(), o.rect.bottomRight());
}

QSize FilePaneDelegate::sizeHint(const QStyleOptionViewItem &,
                                 const QModelIndex &) const {
  return QSize(0, rowHeight);
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
  m_settingsKey =
      QStringLiteral("FilePane/") + settingsKey + QStringLiteral("/");
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);

  m_stack = new QStackedWidget(this);
  lay->addWidget(m_stack);

  // --- KDE Model Stack ---
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

  // --- TreeView ---
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
      QSize(qBound(12, savedHeight - 6, 48), qBound(12, savedHeight - 6, 48)));

  setupColumns();

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
  hdr->setStretchLastSection(false);
  hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  hdr->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(hdr, &QHeaderView::customContextMenuRequested, this,
          &FilePane::showHeaderMenu);
  connect(hdr, &QHeaderView::sectionDoubleClicked, this,
          [this](int col) { m_view->resizeColumnToContents(col); });

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
          "QScrollBar::handle:vertical{background:rgba(136,192,208,40);border-"
          "radius:5px;min-height:20px;margin:2px;}"
          "QTreeView "
          "QScrollBar::handle:vertical:hover{background:rgba(136,192,208,100);}"
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
               TM().colors().textAccent));
  m_view->viewport()->setStyleSheet("background:transparent;");
  m_view->viewport()->setAttribute(Qt::WA_TranslucentBackground);

  // --- Overlay Scrollbars ---
  m_overlayBar = new QScrollBar(Qt::Vertical, this);
  m_overlayBar->setStyleSheet(
      "QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:"
      "none;}"
      "QScrollBar::handle:vertical{background:rgba(136,192,208,60);border-"
      "radius:4px;min-height:20px;margin:1px;}"
      "QScrollBar::handle:vertical:hover{background:rgba(136,192,208,140);}"
      "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
      "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:"
      "transparent;}");
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
      "QScrollBar:horizontal{background:transparent;height:6px;margin:0px;"
      "border:none;}"
      "QScrollBar::handle:horizontal{background:rgba(136,192,208,80);border-"
      "radius:3px;min-width:20px;margin:1px;}"
      "QScrollBar::handle:horizontal:hover{background:rgba(136,192,208,160);}"
      "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:"
      "0px;}"
      "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{"
      "background:transparent;}");
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

  // --- IconView ---
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
          "11px;}"
          "QListView::item{padding:4px;border-radius:4px;}"
          "QListView::item:hover{background:%3;}"
          "QListView::item:selected{background:%4;color:%5;}"
          "QListView "
          "QScrollBar:vertical{width:0px;background:transparent;border:none;}")
          .arg(TM().colors().bgList, TM().colors().textPrimary,
               TM().colors().bgHover, TM().colors().bgSelect,
               TM().colors().textLight));

  m_stack->addWidget(m_view);
  m_stack->addWidget(m_iconView);
  m_stack->setCurrentWidget(m_view);

  // --- Signale ---
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_view, &QTreeView::customContextMenuRequested, this,
          &FilePane::showContextMenu);
  connect(m_view, &QTreeView::activated, this, &FilePane::onItemActivated);
  connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
    auto gs = Config::group("General");
    if (gs.readEntry("singleClick", false))
      onItemActivated(idx);
  });

  connect(m_iconView, &QListView::clicked, this,
          [this](const QModelIndex &idx) {
            auto gs = Config::group("General");
            if (gs.readEntry("singleClick", false))
              onItemActivated(idx);
          });

  connect(m_iconView, &QListView::customContextMenuRequested, this,
          &FilePane::showContextMenu);
  connect(m_iconView, &QListView::activated, this, &FilePane::onItemActivated);

  // QTimer::singleShot(100, this, [this]() {
  //    if (m_view && m_view->viewport())
  //    m_view->viewport()->installEventFilter(this); if (m_iconView &&
  //    m_iconView->viewport())
  //    m_iconView->viewport()->installEventFilter(this);
  // });

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
            KFileItem item = fp_fileItemFromProxyIndex(
                cur, m_view->rootIndex(), m_sortProxy, m_dirModel);
            QString path = item.isNull() ? QString()
                                         : (item.localPath().isEmpty()
                                                ? item.url().toString()
                                                : item.localPath());
            // Anzahl selektierter Rows (Spalten-unabhängig)
            QSet<int> seenRows;
            for (const auto &idx : selectedIdx)
              seenRows.insert(idx.row());
            emit selectionChanged(seenRows.count(), path);
          });

  // Tag-Änderungen: Model aktualisieren (KDirModel refresht von selbst,
  // aber Tags-Spalte muss manuell angestoßen werden)
  connect(&TagManager::instance(), &TagManager::fileTagChanged, this,
          [this](const QString &) {
            // Alle sichtbaren Indizes der Tags-Spalte neu zeichnen
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
          });

  // KNewFileMenu
  m_newFileMenu = new KNewFileMenu(this);
  connect(m_newFileMenu, &KNewFileMenu::fileCreated, this,
          &FilePane::onNewFileCreated);

  setRootPath(QDir::homePath());
}

// --- Navigation ---

void FilePane::setRootPath(const QString &path) {
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
      m_lister->openUrl(url);
      connect(
          m_lister, &KDirLister::completed, this,
          [this, url]() {
            QModelIndex dirIdx = m_dirModel->indexForUrl(url);
            if (dirIdx.isValid()) {
              QModelIndex proxyIdx =
                  m_proxy->mapFromSource(m_sortProxy->mapFromSource(dirIdx));
              m_view->setRootIndex(proxyIdx);
              m_iconView->setRootIndex(proxyIdx);
            }
          },
          Qt::SingleShotConnection);
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
  m_proxy->setTagFilter(QString()); // Tag-Filter zurücksetzen bei Navigation

  QUrl url = QUrl::fromLocalFile(path);
  m_lister->openUrl(url);
  // Root-Index setzen sobald Lister fertig ist
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
  m_kioMode = true;
  m_currentUrl = url;
  m_currentPath = url.toString();
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

void FilePane::setShowHiddenFiles(bool show) {
  if (m_lister->showHiddenFiles() == show)
    return;
  m_lister->setShowHiddenFiles(show);
  // Erneutes Laden erzwingen um Filter anzuwenden
  m_lister->openUrl(m_lister->url(), KDirLister::NoFlags);
}

const QString &FilePane::currentPath() const { return m_currentPath; }

bool FilePane::hasFocus() const { return m_view->hasFocus(); }

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
  m_filter = pattern;
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
  const QModelIndex rootProxy = m_view->rootIndex();
  const auto indexes = m_view->selectionModel()->selectedIndexes();
  QSet<int> seenRows;
  for (const auto &idx : indexes) {
    if (idx.column() != 0) continue;
    if (seenRows.contains(idx.row())) continue;
    seenRows.insert(idx.row());
    KFileItem item = fp_fileItemFromProxyIndex(idx, rootProxy, m_sortProxy, m_dirModel);
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
    m_iconView->setIconSize(QSize(48, 48));
    m_iconView->setGridSize(QSize(96, 80));
    m_iconView->setSpacing(8);
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

  // remoteViewMap: UDS_NAME → Ziel-URL aus /usr/share/remoteview/*.desktop
  static QHash<QString, QString> remoteViewMap;
  static bool remoteViewLoaded = false;
  if (!remoteViewLoaded) {
    remoteViewLoaded = true;
    const QDir remoteDir("/usr/share/remoteview");
    for (const QFileInfo &fi :
         remoteDir.entryInfoList({"*.desktop"}, QDir::Files)) {
      QSettings ds(fi.absoluteFilePath(), QSettings::IniFormat);
      ds.beginGroup(QStringLiteral("Desktop Entry"));
      const QString urlVal = ds.value(QStringLiteral("URL")).toString();
      const QString baseName = fi.completeBaseName();
      if (!urlVal.isEmpty())
        remoteViewMap.insert(baseName, urlVal);
      ds.endGroup();
    }
  }

  // UDS_NAME für remote:/ Einträge auflösen
  const QString udsName = item.text();
  if (remoteViewMap.contains(udsName)) {
    const QString mapped = remoteViewMap.value(udsName);
    emit fileActivated(mapped);
    return;
  }

  // Verzeichnis oder KIO-Navigation → navigieren
  // Wir prüfen explizit auf isDir(), damit Dateien nicht als Ordner "geöffnet"
  // werden
  if (item.isDir()) {
    emit fileActivated(path);
    return;
  }

  // Datei öffnen
  QDesktopServices::openUrl(item.url());
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
}

bool FilePane::eventFilter(QObject *obj, QEvent *e) {
  return QWidget::eventFilter(obj, e);
}

void FilePane::onNewFileCreated(const QUrl &) { reload(); }

// --- openWithApp ---
void FilePane::openWithApp(const QString &entry, const QString &path) {
  auto svc = KService::serviceByDesktopName(entry);
  if (!svc)
    return;
  QString exec = svc->exec();
  exec.replace("%f", "\"" + path + "\"");
  exec.replace("%F", "\"" + path + "\"");
  exec.replace("%u", QUrl::fromLocalFile(path).toString());
  exec.replace("%U", QUrl::fromLocalFile(path).toString());
  exec.replace("%i", "");
  exec.replace("%c", svc->name());
  exec = exec.trimmed();
  QProcess::startDetached("sh", {"-c", exec});
}

// --- showHeaderMenu ---
void FilePane::showHeaderMenu(const QPoint &pos) {
  QMenu menu;
  menu.setStyleSheet("QMenu::item{padding:5px 20px 5px 8px;}"
                     "QMenu::indicator{width:14px;height:14px;}");
  fp_applyMenuShadow(&menu);

  QMap<QString, QMenu *> subMenus;

  for (const auto &d : colDefs()) {
    QMenu *target = &menu;
    if (!d.group.isEmpty()) {
      if (!subMenus.contains(d.group)) {
        auto *sub = menu.addMenu(d.group);
        sub->setStyleSheet(menu.styleSheet());
        subMenus[d.group] = sub;
      }
      target = subMenus[d.group];
    }
    auto *act = target->addAction(d.label);
    act->setCheckable(true);
    act->setChecked(m_colVisible[d.id]);
    connect(act, &QAction::toggled, this,
            [this, id = d.id](bool v) { setColumnVisible(id, v); });
  }
  menu.exec(m_view->header()->mapToGlobal(pos));
}

// --- showContextMenu — volles Menü mit KFileItemActions + KNewFileMenu ---
void FilePane::showContextMenu(const QPoint &pos) {
  emit focusRequested();
  auto *view = qobject_cast<QAbstractItemView *>(sender());
  if (!view)
    view = m_view;

  const QModelIndex proxyIndex = view->indexAt(pos);
  bool hasItem = proxyIndex.isValid();

  if (hasItem) {
    // Dolphin-Verhalten: Nur wenn das Element NICHT ausgewählt ist, Auswahl ändern
    if (!view->selectionModel()->isSelected(proxyIndex)) {
      view->selectionModel()->select(proxyIndex,
                                     QItemSelectionModel::ClearAndSelect |
                                         QItemSelectionModel::Rows);
    }
    // NoUpdate: Current setzen ohne die Mehrfachauswahl zu zerstören
    view->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::NoUpdate);
  } else {
    view->selectionModel()->clearSelection();
  }

  // selectedItems mit korrektem rootIndex-Parent mappen
  KFileItemList selectedItems;
  {
    const QModelIndex rootProxy = view->rootIndex();
    const QModelIndexList sel = view->selectionModel()->selectedIndexes();
    QSet<int> seenRows;
    for (const auto &idx : sel) {
      if (idx.column() != 0) continue;
      if (seenRows.contains(idx.row())) continue;
      seenRows.insert(idx.row());
      KFileItem it = fp_fileItemFromProxyIndex(idx, rootProxy, m_sortProxy, m_dirModel);
      if (!it.isNull())
        selectedItems << it;
    }
  }

  KFileItem item;
  emit focusRequested();
  QString path;
  QUrl itemUrl;

  if (hasItem) {
    item = fp_fileItemFromProxyIndex(proxyIndex, view->rootIndex(), m_sortProxy, m_dirModel);
    if (item.isNull()) {
      hasItem = false;
    } else {
      path =
          item.localPath().isEmpty() ? item.url().toString() : item.localPath();
      itemUrl = item.url();
    }
  }

  const bool isKioPath =
      m_kioMode || (!m_currentPath.startsWith("/") &&
                    m_currentPath.contains(QStringLiteral(":/")));
  const QUrl dirUrl =
      isKioPath ? QUrl(m_currentPath)
                : QUrl::fromLocalFile(m_currentPath.isEmpty()
                                          ? QFileInfo(path).absolutePath()
                                          : m_currentPath);

  // CWD Fix für Service-Menüs (Ark etc.)
  QString oldCwd = QDir::currentPath();
  if (!isKioPath && !m_currentPath.isEmpty()) {
    QDir::setCurrent(m_currentPath);
  }

  QMenu menu(this);
  fp_applyMenuShadow(&menu);
  menu.setStyleSheet(menuStyle());

  // --- 1. KIO-Aktionen (Öffnen, Dienste, Bearbeiten, Löschen, Papierkorb,
  // Eigenschaften) ---
  // Wie Dolphin: selectedItems bereits oben gesammelt, hier nur zuweisen
  KFileItemList items;
  if (hasItem) {
    items = selectedItems;
    if (items.isEmpty())
      items << item;
  } else {
    items << KFileItem(dirUrl);
  }

  KFileItemActions actions(&menu);
  KFileItemListProperties props(items);
  actions.setItemListProperties(props);
  actions.setParentWidget(this);

  const bool isTrash = dirUrl.scheme() == QStringLiteral("trash");

  if (isTrash) {
    if (hasItem) {
      // --- PAPIERKORB ITEM (Bild 1) ---
      menu.addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
                     tr("An ursprünglichem Ort wiederherstellen"), this,
                     [items]() {
                       QList<QUrl> urls;
                       urls.reserve(items.count());
                       // Wie Dolphin: item.url() für restoreFromTrash
                       for (const KFileItem &it : items)
                         urls << it.url();
                       if (urls.isEmpty()) return;
                       auto *job = KIO::restoreFromTrash(urls);
                       job->uiDelegate()->setAutoErrorHandlingEnabled(true);
                     });
      menu.addSeparator();

      if (auto *mw = MW()) {
        if (auto *a =
                mw->actionCollection()->action(QStringLiteral("file_move")))
          menu.addAction(a);
        if (auto *a =
                mw->actionCollection()->action(QStringLiteral("file_copy")))
          menu.addAction(a);
      }
      menu.addSeparator();

      if (auto *mw = MW()) {
        if (auto *a =
                mw->actionCollection()->action(QStringLiteral("file_delete"))) {
          a->setText(tr("Löschen"));
          a->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
          a->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
          menu.addAction(a);
        }
      }
    } else {
      // --- PAPIERKORB HINTERGRUND (Bild 2) ---
      auto *sortMenu = menu.addMenu(
          QIcon::fromTheme(QStringLiteral("view-sort")), tr("Sortieren nach"));
      if (auto *tree = qobject_cast<QTreeView *>(view)) {
        auto *hdr = tree->header();
        for (int i = 0; i < hdr->count(); ++i) {
          if (hdr->isSectionHidden(i))
            continue;
          QString label =
              tree->model()->headerData(i, Qt::Horizontal).toString();
          QAction *a = sortMenu->addAction(label);
          a->setCheckable(true);
          a->setChecked(hdr->sortIndicatorSection() == i);
          connect(a, &QAction::triggered, this,
                  [i, tree]() { tree->sortByColumn(i, Qt::AscendingOrder); });
        }
      }

      auto *modeMenu =
          menu.addMenu(QIcon::fromTheme(QStringLiteral("view-mode")),
                       tr("Ansichtsmodus ändern"));
      auto addMode = [&](const QString &label, const QString &icon, int m) {
        QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
        a->setCheckable(true);
        a->setChecked(m_viewMode == m);
        connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
      };
      addMode(tr("Details"), "view-list-details", 0);
      addMode(tr("Symbole"), "view-list-icons", 1);
      menu.addSeparator();

      menu.addAction(QIcon::fromTheme(QStringLiteral("trash-empty")),
                     tr("Papierkorb leeren"), this, []() {
                       auto *job = KIO::emptyTrash();
                       job->uiDelegate()->setAutoErrorHandlingEnabled(true);
                     });
    }
  } else if (hasItem) {
    KFileItem contextItem = items.first();

    // --- 1. ÖFFNEN-BLOCK ---
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")),
                   tr("Öffnen"), this, [this, contextItem]() {
                     if (contextItem.isDir())
                       setRootPath(contextItem.localPath().isEmpty()
                                       ? contextItem.url().toString()
                                       : contextItem.localPath());
                     else
                       QDesktopServices::openUrl(contextItem.url());
                   });
    actions.insertOpenWithActionsTo(
        nullptr, &menu, QStringList{QCoreApplication::applicationName()});
    menu.addSeparator();

    // --- 2. NEU ERSTELLEN (Nur Ordner) ---
    if (props.isDirectory() && m_newFileMenu) {
      m_newFileMenu->setWorkingDirectory(itemUrl);
      m_newFileMenu->checkUpToDate();
      auto *newMenu = new QMenu(tr("Neu erstellen"), &menu);
      newMenu->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
      for (QAction *act : m_newFileMenu->menu()->actions())
        newMenu->addAction(act);
      menu.addMenu(newMenu);
      menu.addSeparator();
    }

    // --- 3. BEARBEITEN-BLOCK ---
    if (auto *mw = MW()) {
      if (auto *a = mw->actionCollection()->action(QStringLiteral("file_move")))
        menu.addAction(a);
      if (auto *a = mw->actionCollection()->action(QStringLiteral("file_copy")))
        menu.addAction(a);
    }
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy-path")),
                   tr("Adresse kopieren"), this, [itemUrl]() {
                     QGuiApplication::clipboard()->setText(
                         itemUrl.isLocalFile() ? itemUrl.toLocalFile()
                                               : itemUrl.toString());
                   });
    if (props.isDirectory()) {
      if (auto *mw = MW())
        if (auto *a =
                mw->actionCollection()->action(QStringLiteral("file_paste")))
          menu.addAction(a);
    }
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                   tr("Hier duplizieren"), this, [this]() {
                     const QList<QUrl> urls = selectedUrls();
                     if (urls.isEmpty())
                       return;
                     for (const QUrl &u : urls) {
                       QUrl dest = u;
                       QString p = u.path();
                       if (p.endsWith("/"))
                         p.chop(1);
                       dest.setPath(p + "_copy");
                       auto *job = KIO::copy(u, dest);
                       if (auto *mw = MW())
                         mw->registerJob(job, tr("Dupliziere Datei..."));
                     }
                   });
    if (auto *mw = MW())
      if (auto *a =
              mw->actionCollection()->action(QStringLiteral("file_rename")))
        menu.addAction(a);

    // Hinzufügen zu (Immer verfügbar)
    if (!isTrash) {
      auto *mw = MW();
      if (mw && mw->sidebar()) {
        QStringList groups = mw->sidebar()->groupNames();
        if (groups.isEmpty())
          groups << tr("Favoriten");
        auto *addToMenu = new QMenu(tr("Hinzufügen zu"), &menu);
        addToMenu->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
        const QString scheme = itemUrl.scheme().toLower();
        if (!itemUrl.isLocalFile() && (scheme == "smb" || scheme == "sftp" ||
                                       scheme == "mtp" || scheme == "gdrive")) {
          QAction *a = addToMenu->addAction(QIcon::fromTheme("network-server"),
                                            tr("Laufwerke"));
          connect(a, &QAction::triggered, this, [mw, path, contextItem]() {
            mw->sidebar()->addNetworkPlace(path, contextItem.text());
          });
          addToMenu->addSeparator();
        }
        for (const QString &grp : groups) {
          QAction *a = addToMenu->addAction(QIcon::fromTheme("folder"), grp);
          connect(a, &QAction::triggered, this, [mw, grp, path]() {
            mw->sidebar()->addPathToGroup(grp, path);
          });
        }
        menu.addMenu(addToMenu);
      }
    }
    menu.addSeparator();

    // --- 4. PAPIERKORB-BLOCK --- (exakt wie Dolphin: DolphinRemoveAction)
    if (auto *mw = MW()) {
      auto *removeAct = new SCRemoveAction(mw->actionCollection(), &menu);
      menu.addAction(removeAct);

      // Wie Dolphin: KeyEvent-Filter auf dem Menü
      // Shift-Press/Release schaltet die Action live um
      struct ShiftFilter : public QObject {
        SCRemoveAction *act;
        explicit ShiftFilter(SCRemoveAction *a, QObject *parent)
            : QObject(parent), act(a) {}
        bool eventFilter(QObject *, QEvent *e) override {
          if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
            auto *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Shift) {
              act->update(e->type() == QEvent::KeyPress
                              ? SCRemoveAction::ShiftState::Pressed
                              : SCRemoveAction::ShiftState::Released);
            }
          }
          return false;
        }
      };
      menu.installEventFilter(new ShiftFilter(removeAct, &menu));
    }
    menu.addSeparator();

    // --- 5. AKTIONEN & SYSTEM (Dienste & Extras) ---
    if (!isTrash) {
      QList<QAction *> additionalActions;
      if (props.isLocal() && props.isDirectory()) {
        auto *termAct =
            new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                        tr("Terminal hier öffnen"), &menu);
        termAct->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F4));
        connect(termAct, &QAction::triggered, this,
                [path]() { sc_openTerminal(path); });
        additionalActions << termAct;
      }
      if (props.isDirectory()) {
        auto *slideAct =
            new QAction(QIcon::fromTheme(QStringLiteral("view-presentation")),
                        tr("Diaschau starten"), &menu);
        connect(slideAct, &QAction::triggered, this, [itemUrl]() {
          QProcess::startDetached(
              QStringLiteral("gwenview"),
              {QStringLiteral("--slideshow"), itemUrl.toLocalFile()});
        });
        additionalActions << slideAct;
      }

      actions.addActionsTo(&menu, KFileItemActions::MenuActionSource::All,
                           additionalActions);

      // Unser eigenes Tag-Menü (Vorbereiten zum Verschieben)
      auto *customTagMenu = new QMenu(tr("Tag"), &menu);
      customTagMenu->setIcon(QIcon::fromTheme(QStringLiteral("tag")));
      QString curTag = TagManager::instance().fileTag(path);
      for (const auto &t : TagManager::instance().tags()) {
        QPixmap dot(14, 14);
        dot.fill(Qt::transparent);
        QPainter dp(&dot);
        dp.setRenderHint(QPainter::Antialiasing);
        dp.setBrush(QColor(t.second));
        dp.setPen(Qt::NoPen);
        dp.drawEllipse(1, 1, 12, 12);
        QAction *act = customTagMenu->addAction(QIcon(dot), t.first);
        act->setCheckable(true);
        act->setChecked(t.first == curTag);
        connect(act, &QAction::triggered, this, [path, t](bool) {
          TagManager::instance().setFileTag(path, t.first);
        });
      }
      if (!curTag.isEmpty()) {
        customTagMenu->addSeparator();
        customTagMenu->addAction(
            QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Tag entfernen"),
            this, [path]() { TagManager::instance().clearFileTag(path); });
      }

      // --- Filter-Logik für Aktionen-Untermenü ---
      auto isFolderColor = [](QAction *a) {
        if (!a)
          return false;
        QString t = a->text().toLower();
        QString tt = a->toolTip().toLower();
        QString in = a->icon().name().toLower();
        QString className = QString::fromLatin1(a->metaObject()->className());

        // Keywords für foldercolor Plugin
        static const QStringList kws = {
            "ordnersymbol", "farbe",   "symbol", "weitere", "standard",
            "color",        "emblem",  "rot",    "gelb",    "grün",
            "blau",         "violett", "braun",  "grau",    "schwarz",
            "weiß",         "orange",  "red",    "yellow",  "green",
            "blue",         "purple",  "brown",  "grey",    "black",
            "white"};

        if (className.contains("WidgetAction"))
          return true;
        for (const QString &kw : kws) {
          if (t.contains(kw) || tt.contains(kw) || in.contains(kw))
            return true;
        }
        if (t.isEmpty() && (!a->icon().isNull() || in.contains("folder")))
          return true;

        if (a->menu()) {
          for (QAction *sa : a->menu()->actions()) {
            QString st = sa->text().toLower();
            for (const QString &kw : kws) {
              if (st.contains(kw))
                return true;
            }
          }
        }
        return false;
      };

      QMenu *actionsSubMenu = nullptr;
      for (QAction *a : menu.actions()) {
        QString mt = a->text().remove('&');
        if (a->menu() &&
            (mt.contains(tr("Aktionen")) || mt.contains("Actions"))) {
          actionsSubMenu = a->menu();
          break;
        }
      }
      if (!actionsSubMenu) {
        actionsSubMenu = new QMenu(tr("Aktionen"), &menu);
        actionsSubMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
      }

      QStringList seen;
      QList<QAction *> toMain;
      for (QAction *a : menu.actions()) {
        if (a->isSeparator() || a->menu() == actionsSubMenu)
          continue;
        QString txt = a->text().toLower();

        // A) Ordnersymbole / Farbauswahl / Dubletten weg
        if (isFolderColor(a)) {
          menu.removeAction(a);
          continue;
        }

        // B) Dubletten von "Tag" oder "Stichwörter" entfernen (außer unser
        // EIGENES Menü)
        if ((txt.contains("tag") || txt.contains("stichwört") ||
             txt.contains("assign tags")) &&
            a != customTagMenu->menuAction()) {
          menu.removeAction(a);
          continue;
        }

        // C) Aktivitäten und unser eigenes Tag-Menü im Hauptmenü lassen
        if (txt.contains("aktivität") || a == customTagMenu->menuAction()) {
          toMain << a;
          continue;
        }

        bool isService =
            (txt.contains("verschlüssel") || txt.contains("signier") ||
             txt.contains("packen") || txt.contains("entpacken") ||
             txt.contains("teilen") || txt.contains("diaschau"));
        if (isService) {
          menu.removeAction(a);
          QString key = txt.simplified();
          if (!seen.contains(key)) {
            actionsSubMenu->addAction(a);
            seen << key;
          }
        }
      }

      // Finale Bereinigung des Untermenüs
      if (actionsSubMenu) {
        QList<QAction *> toDel;
        for (QAction *a : actionsSubMenu->actions()) {
          if (isFolderColor(a) || a->text().toLower().contains("stichwört") ||
              (a->text().toLower().contains("tag") &&
               a != customTagMenu->menuAction()))
            toDel << a;
        }
        for (QAction *a : toDel)
          actionsSubMenu->removeAction(a);
      }
      // "In neuen Ordner verschieben"
      if (props.supportsMoving()) {
        auto *moveAct =
            new QAction(QIcon::fromTheme(QStringLiteral("folder-new")),
                        tr("In neuen Ordner verschieben ..."), actionsSubMenu);
        connect(moveAct, &QAction::triggered, this, [this, items, dirUrl]() {
          bool ok;
          QString name = QInputDialog::getText(
              this, tr("In neuen Ordner verschieben"), tr("Ordnername:"),
              QLineEdit::Normal, tr("Neuer Ordner"), &ok);
          if (ok && !name.isEmpty()) {
            QUrl destDir = dirUrl;
            destDir.setPath(destDir.path() +
                            (destDir.path().endsWith('/') ? "" : "/") + name);
            KIO::Job *mkdirJob = KIO::mkdir(destDir);
            connect(
                mkdirJob, &KIO::Job::result, this, [items, destDir](KJob *job) {
                  if (job->error() == 0) {
                    KIO::CopyJob *moveJob = KIO::move(items.urlList(), destDir);
                    KIO::FileUndoManager::self()->recordCopyJob(moveJob);
                  }
                });
          }
        });
        actionsSubMenu->addAction(moveAct);
      }

      // Alles wieder ins Hauptmenü sortieren
      QAction *propPos = nullptr;
      for (QAction *ma : menu.actions())
        if (ma->text().contains(tr("Eigenschaften"))) {
          propPos = ma;
          break;
        }
      if (actionsSubMenu && !actionsSubMenu->actions().isEmpty())
        menu.insertMenu(propPos, actionsSubMenu);
      for (QAction *a : toMain)
        menu.insertAction(propPos, a);
    }

    // --- 6. SPLITCOMMANDER-EXTRAS ---
    menu.addSeparator();
    // --- 7. SPLITCOMMANDER-EXTRAS ---
    if (auto *mw = MW()) {
      if (auto *a = mw->actionCollection()->action(
              QStringLiteral("file_copy_to_other")))
        menu.addAction(a);
      if (auto *a = mw->actionCollection()->action(
              QStringLiteral("file_move_to_other")))
        menu.addAction(a);
    }

    // --- 8. TAGS ---
    if (!isKioPath) {
      auto *tagMenu =
          menu.addMenu(QIcon::fromTheme(QStringLiteral("tag")), tr("Tag"));
      QString currentTag = TagManager::instance().fileTag(path);
      for (const auto &t : TagManager::instance().tags()) {
        QPixmap dot(14, 14);
        dot.fill(Qt::transparent);
        QPainter dp(&dot);
        dp.setRenderHint(QPainter::Antialiasing);
        dp.setBrush(QColor(t.second));
        dp.setPen(Qt::NoPen);
        dp.drawEllipse(1, 1, 12, 12);
        QAction *act = tagMenu->addAction(QIcon(dot), t.first);
        act->setCheckable(true);
        act->setChecked(t.first == currentTag);
        connect(act, &QAction::triggered, this, [path, t](bool) {
          TagManager::instance().setFileTag(path, t.first);
        });
      }
      if (!currentTag.isEmpty()) {
        tagMenu->addSeparator();
        tagMenu->addAction(
            QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Tag entfernen"),
            this, [path]() { TagManager::instance().clearFileTag(path); });
      }
    }
  } else {
    // --- 1. NEU / ÖFFNEN-BLOCK (Viewport) ---
    if (m_newFileMenu && dirUrl.scheme() != "trash") {
      m_newFileMenu->setWorkingDirectory(dirUrl);
      m_newFileMenu->checkUpToDate();
      auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("list-add")),
                                   tr("Neu erstellen"));
      for (QAction *act : m_newFileMenu->menu()->actions())
        newMenu->addAction(act);
    }
    actions.insertOpenWithActionsTo(
        nullptr, &menu, QStringList{QCoreApplication::applicationName()});
    menu.addSeparator();

    // --- 2. BEARBEITEN / EINFÜGEN ---
    if (auto *mw = MW()) {
      if (auto *a =
              mw->actionCollection()->action(QStringLiteral("file_paste")))
        menu.addAction(a);
    }
    // Zu Laufwerken hinzufügen (Dolphin: add_to_places)
    if (isKioPath) {
      auto netCheck = Config::group("NetworkPlaces");
      if (!netCheck.readEntry("places", QStringList())
               .contains(m_currentPath)) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                       tr("Zu Laufwerken hinzufügen"), this, [this, dirUrl]() {
                         emit addToPlacesRequested(m_currentPath,
                                                   dirUrl.host());
                       });
      }
    }
    menu.addSeparator();

    // --- 3. ANSICHT-BLOCK ---
    auto *sortMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-sort")),
                                  tr("Sortieren nach"));
    if (auto *tree = qobject_cast<QTreeView *>(view)) {
      auto *hdr = tree->header();
      for (int i = 0; i < hdr->count(); ++i) {
        if (hdr->isSectionHidden(i))
          continue;
        QString label = tree->model()->headerData(i, Qt::Horizontal).toString();
        QAction *a = sortMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(hdr->sortIndicatorSection() == i);
        connect(a, &QAction::triggered, this,
                [i, tree]() { tree->sortByColumn(i, Qt::AscendingOrder); });
      }
    }

    auto *modeMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-mode")),
                                  tr("Ansichtsmodus ändern"));
    auto addMode = [&](const QString &label, const QString &icon, int m) {
      QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
      a->setCheckable(true);
      a->setChecked(m_viewMode == m);
      connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
    };
    addMode(tr("Details"), "view-list-details", 0);
    addMode(tr("Symbole"), "view-list-icons", 1);
    menu.addSeparator();
  }

  // --- 5. EIGENSCHAFTEN (Immer ganz unten) ---
  menu.addSeparator();
  menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                 tr("Eigenschaften"), this, [itemUrl]() {
                   auto *dlg = new KPropertiesDialog(itemUrl, nullptr);
                   dlg->setAttribute(Qt::WA_DeleteOnClose);
                   dlg->show();
                 });

  // --- 8. FINALES STYLING FÜR ALLE UNTERMENÜS (Robust & Rekursiv) ---
  auto applyStyle = [](QMenu *m, auto &self) -> void {
    if (!m)
      return;
    m->setStyleSheet(menuStyle());
    fp_applyMenuShadow(m);

    auto styleChildren = [m, &self]() {
      // 1. Suche über findChildren (für bereits existierende Widgets)
      for (auto *sub : m->findChildren<QMenu *>()) {
        self(sub, self);
      }
      // 2. Suche über Aktionen (für KActionMenus / KFileItemActions)
      for (auto *a : m->actions()) {
        if (a->menu())
          self(a->menu(), self);
      }
    };

    styleChildren();
    QObject::connect(m, &QMenu::aboutToShow, styleChildren);
  };

  applyStyle(&menu, applyStyle);

  menu.exec(view->mapToGlobal(pos));

  if (!isKioPath) {
    QDir::setCurrent(oldCwd);
  }
}
