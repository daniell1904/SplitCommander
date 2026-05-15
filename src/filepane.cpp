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

// Exakt wie DolphinRemoveAction:
// Delegiert an file_trash oder file_delete aus der ActionCollection.
// Shift-Taste schaltet live um solange das Menü offen ist.
class SCRemoveAction : public QAction
{
public:
  enum class ShiftState { Unknown, Pressed, Released };

  SCRemoveAction(KActionCollection *collection, QMenu *menu)
      : QAction(menu), m_collection(collection), m_menu(menu) {
    connect(this, &QAction::triggered, this, [this]() {
      if (m_action)
        m_action->trigger();
    });
    if (m_menu)
      m_menu->installEventFilter(this);
    if (auto *app = QGuiApplication::instance())
      app->installEventFilter(this);
    m_shiftPressed = QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier;
    update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
  }

  ~SCRemoveAction() override {
    if (m_menu)
      m_menu->removeEventFilter(this);
    if (auto *app = QGuiApplication::instance())
      app->removeEventFilter(this);
  }

  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (ke->key() == Qt::Key_Shift && !ke->isAutoRepeat()) {
        m_shiftPressed = (event->type() == QEvent::KeyPress);
        update(m_shiftPressed ? ShiftState::Pressed : ShiftState::Released);
      }
    }
    return QObject::eventFilter(obj, event);
  }

  void update(ShiftState state = ShiftState::Unknown) {
    if (!m_collection)
      return;
    if (state == ShiftState::Unknown) {
      state = m_shiftPressed ? ShiftState::Pressed : ShiftState::Released;
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
      setShortcut(state == ShiftState::Pressed
                      ? QKeySequence(Qt::SHIFT | Qt::Key_Delete)
                      : QKeySequence(Qt::Key_Delete));
      if (m_menu)
        m_menu->update();
    }
  }

private:
  QPointer<KActionCollection> m_collection;
  QPointer<QAction> m_action;
  QPointer<QMenu> m_menu;
  bool m_shiftPressed = false;
};




// --- Hilfsfunktionen ---

static QString menuStyle() {
  return TM().ssMenu() + "QMenu::separator{background:rgba(236,239,244,120);"
                         "height:1px;margin:4px 8px;}";
}

static void fp_applyMenuShadow(QMenu *menu) {
  Q_UNUSED(menu)
  // QGraphicsDropShadowEffect auf QMenu zerstört Submenü-Positionierung — nicht verwenden
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
    if (item.isDir()) {
      if (role == Qt::DisplayRole) return QStringLiteral("[DIR]");
      if (role == Qt::UserRole)    return QStringLiteral("");  // Ordner immer zuerst
      return {};
    }
    const QString ext = QFileInfo(item.text()).suffix().toUpper().left(4);
    const QString display = ext.isEmpty() ? QStringLiteral("[???]")
                                           : QStringLiteral("[") + ext + QStringLiteral("]");
    if (role == Qt::DisplayRole) return display;
    if (role == Qt::UserRole)    return ext; // Sortierung nach reiner Erweiterung
    return {};
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
  if (role == Qt::UserRole + 1)
    return QVariant::fromValue(item);

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
      const QString lp = item.localPath();
      if (lp.isEmpty()) return {};

      // Cache prüfen
      if (m_dirSizeCache.contains(lp))
        return KFormat().formatByteSize(m_dirSizeCache.value(lp));

      // Noch nicht berechnet: async starten
      if (!m_dirSizePending.contains(lp)) {
        m_dirSizePending.insert(lp);
        auto *watcher = new QFutureWatcher<qint64>(const_cast<FPColumnsProxy*>(this));
        QObject::connect(watcher, &QFutureWatcher<qint64>::finished,
                         const_cast<FPColumnsProxy*>(this),
                         [this, lp, watcher]() {
                           m_dirSizeCache.insert(lp, watcher->result());
                           m_dirSizePending.remove(lp);
                           watcher->deleteLater();
                           emit const_cast<FPColumnsProxy*>(this)->layoutChanged();
                         });
        watcher->setFuture(QtConcurrent::run([lp]() -> qint64 {
          qint64 total = 0;
          QDirIterator it(lp, QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
          int count = 0;
          while (it.hasNext() && count < 50000) {
            it.next();
            total += it.fileInfo().size();
            ++count;
          }
          return total;
        }));
      }
      return QStringLiteral("…");
    }
    return KFormat().formatByteSize(item.size());
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


// --- FilePane::setupView ---
void FilePane::setupView() {
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
  // Sort-Indicator auch nach dem ersten Laden setzen
  connect(m_lister, &KDirLister::completed, this, [hdr]() {
    if (hdr->sortIndicatorSection() == 0)
      hdr->setSortIndicator(0, hdr->sortIndicatorOrder());
  }, Qt::SingleShotConnection);
  hdr->setStretchLastSection(false);
  hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  hdr->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(hdr, &QHeaderView::customContextMenuRequested, this,
          &FilePane::showHeaderMenu);
  // Gespeicherte Breiten laden (nicht Name = Stretch)
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

  // --- Overlay Scrollbars ---
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
          "12px;}"
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
}


// --- FilePane::setupConnections ---
void FilePane::setupConnections() {
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
            KFileItem item = m_proxy->fileItem(cur);
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
            if (!m_currentTagFilter.isEmpty()) {
              showTaggedFiles(m_currentTagFilter);
              return;
            }
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
            emit m_proxy->layoutChanged();
          });

  connect(&ThumbnailManager::instance(), &ThumbnailManager::thumbnailReady, this, [this](const QString &path) {
      Q_UNUSED(path)
      if (!m_proxy) return;
      // Wir könnten hier gezielt den Index suchen, aber ein layoutChanged ist sicherer 
      // und Qt-modelle cachen das Zeichnen sowieso.
      m_proxy->layoutChanged();
  });

  // KNewFileMenu
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
      m_lister->stop();
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
  m_proxy->setTagFilter(QString());

  QUrl url = QUrl::fromLocalFile(path);
  m_lister->stop();
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

  // remoteViewMap: UDS_NAME → Ziel-URL aus /usr/share/remoteview/*.desktop
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
    return;
  }

  // Priorität 2: remoteViewMap über UDS_NAME
  const QString udsName = item.text();
  if (remoteViewMap.contains(udsName)) {
    emit fileActivated(remoteViewMap.value(udsName));
    return;
  }

  // Priorität 3: baseName aus URL (z.B. "gdrive-network" → "gdrive")
  const QString urlBaseName = item.url().path().section('/', -1).section('-', 0, 0);
  if (remoteViewMap.contains(urlBaseName)) {
    emit fileActivated(remoteViewMap.value(urlBaseName));
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
  auto *job = new KIO::OpenUrlJob(item.url());
  job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
  job->start();
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

FilePane::ContextMenuState FilePane::buildContextMenuState(const QPoint &pos) {
  ContextMenuState ctx;
  ctx.view = qobject_cast<QAbstractItemView *>(sender());
  if (!ctx.view)
    ctx.view = m_view;

  const QModelIndex proxyIndex = ctx.view->indexAt(pos);
  ctx.hasItem = proxyIndex.isValid();

  if (ctx.hasItem) {
    if (!ctx.view->selectionModel()->isSelected(proxyIndex)) {
      ctx.view->selectionModel()->select(proxyIndex,
                                         QItemSelectionModel::ClearAndSelect |
                                             QItemSelectionModel::Rows);
    }
    ctx.view->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::NoUpdate);
  } else {
    ctx.view->selectionModel()->clearSelection();
  }

  {

    const QModelIndexList sel = ctx.view->selectionModel()->selectedIndexes();
    QSet<int> seenRows;
    for (const auto &idx : sel) {
      if (idx.column() != 0) continue;
      if (seenRows.contains(idx.row())) continue;
      seenRows.insert(idx.row());
      KFileItem it = m_proxy->fileItem(idx);
      if (!it.isNull())
        ctx.selectedItems << it;
    }
  }

  if (ctx.hasItem) {
    ctx.item = m_proxy->fileItem(proxyIndex);
    if (ctx.item.isNull()) {
      ctx.hasItem = false;
    } else {
      ctx.path = ctx.item.localPath().isEmpty() ? ctx.item.url().toString() : ctx.item.localPath();
      ctx.itemUrl = ctx.item.url();
    }
  }

  ctx.isKioPath = m_kioMode || (!m_currentPath.startsWith("/") &&
                                 m_currentPath.contains(QStringLiteral(":/")));
  ctx.dirUrl = ctx.isKioPath
      ? (m_currentUrl.isValid() ? m_currentUrl : QUrl(m_currentPath))
      : QUrl::fromLocalFile(m_currentPath.isEmpty()
                                ? QFileInfo(ctx.path).absolutePath()
                                : m_currentPath);
  ctx.isTrash = ctx.dirUrl.scheme() == QStringLiteral("trash");

  ctx.items = ctx.hasItem ? ctx.selectedItems : KFileItemList{KFileItem(ctx.dirUrl)};
  if (ctx.hasItem && ctx.items.isEmpty()) ctx.items << ctx.item;

  return ctx;
}

void FilePane::applyMenuStyling(QMenu &menu) {
  auto applyStyle = [](QMenu *m, auto &self) -> void {
    if (!m) return;
    m->setStyleSheet(menuStyle());
    fp_applyMenuShadow(m);
    auto styleChildren = [m, &self]() {
      for (auto *sub : m->findChildren<QMenu *>())
        self(sub, self);
      for (auto *act : m->actions()) {
        if (auto *sub = act->menu())
          self(sub, self);
      }
    };
    QObject::connect(m, &QMenu::aboutToShow, m, styleChildren);
    styleChildren();
  };
  applyStyle(&menu, applyStyle);
}

void FilePane::showContextMenu(const QPoint &pos) {
  emit focusRequested();

  ContextMenuState ctx = buildContextMenuState(pos);

  QString oldCwd = QDir::currentPath();
  if (!ctx.isKioPath && !m_currentPath.isEmpty())
    QDir::setCurrent(m_currentPath);

  QMenu menu(this);
  fp_applyMenuShadow(&menu);
  menu.setStyleSheet(menuStyle());

  KFileItemActions actions(&menu);
  KFileItemListProperties props(ctx.items);
  actions.setItemListProperties(props);
  actions.setParentWidget(this);

  if (ctx.isTrash)
    populateTrashMenu(menu, ctx, actions, props);
  else if (ctx.hasItem)
    populateItemMenu(menu, ctx, actions, props);
  else
    populateBackgroundMenu(menu, ctx, actions, props);

  const QUrl propsUrl = ctx.hasItem ? ctx.itemUrl : ctx.dirUrl;
  menu.addSeparator();
  menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                 tr("Eigenschaften"), this, [propsUrl]() {
                   auto *dlg = new KPropertiesDialog(propsUrl, nullptr);
                   dlg->setAttribute(Qt::WA_DeleteOnClose);
                   dlg->show();
                 });

  applyMenuStyling(menu);

  menu.exec(ctx.view->viewport()->mapToGlobal(pos));
  QDir::setCurrent(oldCwd);
}


void FilePane::populateTrashMenu(QMenu &menu, const ContextMenuState &ctx,
                                 KFileItemActions &actions, KFileItemListProperties &props) {
  Q_UNUSED(actions); Q_UNUSED(props);
  const bool hasItem        = ctx.hasItem;
  const KFileItemList &items = ctx.items;
  QAbstractItemView *view   = ctx.view;
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

      if (m_actionCollection) {
        if (auto *a = m_actionCollection->action(QStringLiteral("file_move")))
          menu.addAction(a);
        if (auto *a = m_actionCollection->action(QStringLiteral("file_copy")))
          menu.addAction(a);
      }
      menu.addSeparator();

      if (m_actionCollection) {
        if (auto *a = m_actionCollection->action(QStringLiteral("file_delete"))) {
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
}

void FilePane::populateItemMenu(QMenu &menu, const ContextMenuState &ctx,
                                KFileItemActions &actions, KFileItemListProperties &props) {
  Q_UNUSED(props);
  const QUrl &dirUrl         = ctx.dirUrl;
  const bool isKioPath       = ctx.isKioPath;
  QAbstractItemView *view    = ctx.view;
    // --- 1. NEU / ÖFFNEN-BLOCK (Viewport) ---
    if (m_newFileMenu && dirUrl.scheme() != "trash") {
      m_newFileMenu->setWorkingDirectory(dirUrl);
      m_newFileMenu->checkUpToDate();
      auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-new")),
                                   tr("Neu erstellen"));
      for (QAction *act : m_newFileMenu->menu()->actions())
        newMenu->addAction(act);
    }
    actions.insertOpenWithActionsTo(
        nullptr, &menu, QStringList{QCoreApplication::applicationName()});
    menu.addSeparator();

    // --- 2. BEARBEITEN / EINFÜGEN ---
    if (m_actionCollection) {
      if (auto *a = m_actionCollection->action(QStringLiteral("file_move")))
        menu.addAction(a);
      if (auto *a = m_actionCollection->action(QStringLiteral("file_copy")))
        menu.addAction(a);
      if (auto *a = m_actionCollection->action(QStringLiteral("file_paste")))
        menu.addAction(a);
      menu.addSeparator();
      if (auto *a = m_actionCollection->action(QStringLiteral("file_rename")))
        menu.addAction(a);

      auto *trashDeleteAction = new SCRemoveAction(m_actionCollection, &menu);
      menu.addAction(trashDeleteAction);
    }
    // Zu Laufwerken hinzufügen (Dolphin: add_to_places)
    if (isKioPath) {
      // Immer m_currentUrl verwenden wenn KIO-Modus — enthält User/Auth-Info
      // z.B. smb://root@192.168.0.152/Daten statt smb://192.168.0.152/Daten
      QUrl resolvedUrl = (ctx.hasItem && ctx.itemUrl.isValid())
                             ? ctx.itemUrl
                             : (m_kioMode && m_currentUrl.isValid()
                                    ? m_currentUrl
                                    : QUrl::fromUserInput(m_currentPath));

      // Sicherstellen dass User-Info erhalten bleibt
      if (!m_currentUrl.userInfo().isEmpty() && resolvedUrl.userInfo().isEmpty()
          && resolvedUrl.host() == m_currentUrl.host()) {
        resolvedUrl.setUserInfo(m_currentUrl.userInfo());
      }

      const QString placeUrl = resolvedUrl.toString();
      const QUrl placeQUrl(placeUrl);
      auto netCheck = Config::group("NetworkPlaces");
      if (!netCheck.readEntry("places", QStringList()).contains(placeUrl)) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                       tr("Zu Laufwerken hinzufügen"), this, [this, placeUrl, placeQUrl]() {
                         const QString scheme = placeQUrl.scheme().toLower();
                         QString name;
                         if (scheme == QStringLiteral("gdrive"))
                           name = placeQUrl.path().section('/', 1, 1); // "google18"
                         if (name.isEmpty() && !placeQUrl.fileName().isEmpty())
                           name = placeQUrl.fileName(); // "Daten" für smb://.../Daten
                         if (name.isEmpty() && !placeQUrl.host().isEmpty())
                           name = placeQUrl.host();
                         if (name.isEmpty())
                           name = placeUrl;
                         emit addToPlacesRequested(placeUrl, name);
                       });
      }
    }
    menu.addSeparator();

    // --- 3. ANSICHT-BLOCK ---
    auto *sortMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-sort-ascending")),
                                  tr("Sortieren nach"));

    // Festes Sort-Set wie Dolphin
    struct SortEntry { QString label; int col; QString icon; };
    const QList<SortEntry> sortEntries = {
        { tr("Name"),            0, "sort-name"             },
        { tr("Größe"),           2, "sort-size"             },
        { tr("Geändert"),        3, "sort-time"             },
        { tr("Erstellt"),        3, ""                      },
        { tr("Letzter Zugriff"), 3, ""                      },
        { tr("Typ"),             1, "sort-file-type"        },
        { tr("Bewertung"),      -1, "rating"                },
    };
    auto *tree = qobject_cast<QTreeView *>(view);
    for (const SortEntry &e : sortEntries) {
        if (e.col < 0) continue; // Baloo-Felder — nur wenn vorhanden
        QAction *a = sortMenu->addAction(e.icon.isEmpty()
            ? QIcon() : QIcon::fromTheme(e.icon), e.label);
        a->setCheckable(true);
        if (tree) a->setChecked(tree->header()->sortIndicatorSection() == e.col
                                && e.label == tr("Name") ? tree->header()->sortIndicatorSection() == 0
                                : false);
        const int col = e.col;
        connect(a, &QAction::triggered, this, [col, tree]() {
            if (tree) tree->sortByColumn(col, Qt::AscendingOrder);
        });
    }
    sortMenu->addSeparator();
    // A-Z / Z-A
    auto *azAct = sortMenu->addAction(tr("A-Z"));
    azAct->setCheckable(true);
    auto *zaAct = sortMenu->addAction(tr("Z-A"));
    zaAct->setCheckable(true);
    if (tree) {
        azAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::AscendingOrder);
        zaAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::DescendingOrder);
    }
    connect(azAct, &QAction::triggered, this, [tree]() {
        if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::AscendingOrder);
    });
    connect(zaAct, &QAction::triggered, this, [tree]() {
        if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::DescendingOrder);
    });
    sortMenu->addSeparator();
    // Ordner zuerst
    auto *folderFirstAct = sortMenu->addAction(tr("Ordner zuerst"));
    folderFirstAct->setCheckable(true);
    folderFirstAct->setChecked(m_foldersFirst);
    connect(folderFirstAct, &QAction::triggered, this, [this](bool checked) {
        setFoldersFirst(checked);
    });
    // Versteckte Dateien zuletzt
    auto *hiddenLastAct = sortMenu->addAction(tr("Versteckte Dateien zuletzt"));
    hiddenLastAct->setCheckable(true);
    connect(hiddenLastAct, &QAction::triggered, this, [](bool) {});

    auto *modeMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-list-details")),
                                  tr("Ansichtsmodus ändern"));
    auto addMode = [&](const QString &label, const QString &icon, int m) {
      QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
      a->setCheckable(true);
      a->setChecked(m_viewMode == m);
      connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
    };
    addMode(tr("Details"), "view-list-details", 0);
    addMode(tr("Symbole"), "view-list-icons", 1);

    // --- 4. KIO-AKTIONEN (Aktionen, Stichwörter, Komprimieren, Aktivitäten) ---
    menu.addSeparator();

    // Aktionen in temporäres Menü sammeln
    QMenu tempMenu;
    actions.addActionsTo(&tempMenu, KFileItemActions::MenuActionSource::All, {});

    // Bereits vorhandene Einträge nach Text sammeln, um Duplikate zu vermeiden
    QSet<QString> existingActionTexts;
    for (QAction *act : menu.actions()) {
      if (!act->isSeparator() && !act->text().isEmpty())
        existingActionTexts.insert(act->text().trimmed());
    }

    // Submenüs nach Titel aus tempMenu holen
    auto findSubMenu = [&](const QString &title) -> QAction* {
        for (QAction *act : tempMenu.actions()) {
            if (!act->isSeparator() && act->text().contains(title, Qt::CaseInsensitive))
                return act;
        }
        return nullptr;
    };

    // Aktionen-Submenü bauen — Terminal + restliche Einträge
    auto *actMenu = new QMenu(tr("Aktionen"), &menu);
    actMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    // Terminal hier öffnen als ersten Eintrag
    auto *termAct = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                                tr("Terminal hier öffnen"), actMenu);
    termAct->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F4));
    connect(termAct, &QAction::triggered, this, [this, dirUrl]() {
        auto *job = new KTerminalLauncherJob(QString());
        job->setWorkingDirectory(dirUrl.toLocalFile());
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    actMenu->addAction(termAct);

    // Restliche KIO-Aktionen die nicht manuell im Menü hinzugefügt werden sollen
    static const QStringList skipTitles = {
        tr("Stichwörter zuweisen"), tr("Komprimieren"), tr("Aktivitäten"),
        tr("In den Papierkorb verschieben"), tr("Löschen"), tr("Umbenennen"),
        tr("Kopieren"), tr("Ausschneiden"), tr("Einfügen")
    };
    for (QAction *act : tempMenu.actions()) {
        if (act->isSeparator()) continue;
        // Farbige Ordner-Widget-Aktionen (haben kein Text oder sind Widgets) überspringen
        const QString actText = act->text().trimmed();
        if (actText.isEmpty()) continue;
        if (existingActionTexts.contains(actText)) continue;
        bool skip = false;
        for (const QString &t : skipTitles)
            if (actText.contains(t, Qt::CaseInsensitive)) { skip = true; break; }
        if (!skip) actMenu->addAction(act);
    }

    // Reihenfolge wie Dolphin: Aktionen, Stichwörter, Komprimieren, Aktivitäten
    menu.addMenu(actMenu);
    if (auto *a = findSubMenu(tr("Stichwörter zuweisen"))) menu.addAction(a);
    if (auto *a = findSubMenu(tr("Komprimieren")))          menu.addAction(a);
    if (auto *a = findSubMenu(tr("Aktivitäten")))           menu.addAction(a);

    // --- 5. EIGENE TAGS ---
    if (!TagManager::instance().tags().isEmpty()) {
      auto *tagMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("tag")), tr("Tag setzen"));
      for (const auto &tag : TagManager::instance().tags()) {
        const QString &tagName = tag.first;
        const QString &tagColor = tag.second;
        QPixmap pix(12, 12);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(tagColor));
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 12, 12);
        QAction *a = tagMenu->addAction(QIcon(pix), tagName);
        connect(a, &QAction::triggered, this, [this, ctx, tagName]() {
          if (!m_proxy) return;
          QStringList paths;
          for (const KFileItem &item : ctx.selectedItems) {
            QString path = item.localPath();
            if (!path.isEmpty()) paths << path;
          }
          if (!paths.isEmpty()) {
            TagManager::instance().setFileTags(paths, tagName);
          }
        });
      }
      tagMenu->addSeparator();
      QAction *clearAct = tagMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Tag entfernen"));
      connect(clearAct, &QAction::triggered, this, [this, ctx]() {
        if (!m_proxy) return;
        QStringList paths;
        for (const KFileItem &item : ctx.selectedItems) {
          QString path = item.localPath();
          if (!path.isEmpty()) paths << path;
        }
        if (!paths.isEmpty()) {
          TagManager::instance().clearFileTags(paths);
        }
      });
    }

    menu.addSeparator();
  }


void FilePane::populateBackgroundMenu(QMenu &menu, const ContextMenuState &ctx,
                                       KFileItemActions &actions,
                                       KFileItemListProperties &props) {
  const QUrl &dirUrl      = ctx.dirUrl;
  QAbstractItemView *view = ctx.view;
  Q_UNUSED(props);

  // --- 1. NEU ERSTELLEN ---
  if (m_newFileMenu && dirUrl.scheme() != "trash") {
    m_newFileMenu->setWorkingDirectory(dirUrl);
    m_newFileMenu->checkUpToDate();
    auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-new")),
                                 tr("Neu erstellen"));
    for (QAction *act : m_newFileMenu->menu()->actions())
      newMenu->addAction(act);
  }

  // --- 2. ORDNER ÖFFNEN MIT ---
  actions.insertOpenWithActionsTo(nullptr, &menu,
                                  QStringList{QCoreApplication::applicationName()});
  menu.addSeparator();

  // --- 3. EINFÜGEN ---
  if (m_actionCollection) {
    if (auto *a = m_actionCollection->action(QStringLiteral("file_paste")))
      menu.addAction(a);
  }
  menu.addSeparator();

  // --- 4. ANSICHT ---
  auto *sortMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-sort-ascending")),
                                tr("Sortieren nach"));
  auto *tree = qobject_cast<QTreeView *>(view);
  if (tree) {
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
  sortMenu->addSeparator();
  auto *azAct = sortMenu->addAction(tr("A-Z"));
  azAct->setCheckable(true);
  auto *zaAct = sortMenu->addAction(tr("Z-A"));
  zaAct->setCheckable(true);
  if (tree) {
    azAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::AscendingOrder);
    zaAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::DescendingOrder);
  }
  connect(azAct, &QAction::triggered, this, [tree]() {
    if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::AscendingOrder);
  });
  connect(zaAct, &QAction::triggered, this, [tree]() {
    if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::DescendingOrder);
  });
  sortMenu->addSeparator();
  auto *folderFirstAct = sortMenu->addAction(tr("Ordner zuerst"));
  folderFirstAct->setCheckable(true);
  folderFirstAct->setChecked(m_foldersFirst);
  connect(folderFirstAct, &QAction::triggered, this, [this](bool checked) {
    setFoldersFirst(checked);
  });

  auto *modeMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-list-details")),
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

  // --- 5. KIO-AKTIONEN ---
  QMenu tempMenu;
  actions.addActionsTo(&tempMenu, KFileItemActions::MenuActionSource::All, {});

  auto findAction = [&](const QString &text) -> QAction* {
    for (QAction *act : tempMenu.actions()) {
      if (act->isSeparator())
        continue;
      if (act->text().contains(text, Qt::CaseInsensitive))
        return act;
    }
    return nullptr;
  };

  auto *actMenu = new QMenu(tr("Aktionen"), &menu);
  actMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
  auto *termAct = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                              tr("Terminal hier öffnen"), actMenu);
  termAct->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F4));
  connect(termAct, &QAction::triggered, this, [this, dirUrl]() {
    auto *job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(dirUrl.toLocalFile());
    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
  });
  actMenu->addAction(termAct);

  static const QStringList skipTitles = {
    tr("Stichwörter zuweisen"), tr("Komprimieren"), tr("Aktivitäten")
  };
  for (QAction *act : tempMenu.actions()) {
    if (act->isSeparator())
      continue;
    const QString actText = act->text().trimmed();
    if (actText.isEmpty())
      continue;
    bool skip = false;
    for (const QString &skipText : skipTitles) {
      if (actText.contains(skipText, Qt::CaseInsensitive)) {
        skip = true;
        break;
      }
    }
    if (!skip)
      actMenu->addAction(act);
  }

  menu.addMenu(actMenu);
  if (auto *action = findAction(tr("Stichwörter zuweisen")))
    menu.addAction(action);
  if (auto *action = findAction(tr("Komprimieren")))
    menu.addAction(action);
  if (auto *action = findAction(tr("Aktivitäten")))
    menu.addAction(action);
  menu.addSeparator();
}
void FilePane::onSectionResized(int /*index*/, int, int) {
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
