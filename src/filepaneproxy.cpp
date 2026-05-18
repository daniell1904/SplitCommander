// --- filepaneproxy.cpp ------------------------------------------------------
// FPColumnsProxy: Proxy-Model zur Verwaltung von Spaltenreihenfolge,
// Sichtbarkeit, Tag-Filter und zusätzlichen Daten-Rollen für FilePane.
// ---------------------------------------------------------------------------

#include "filepane.h"

#include <QApplication>
#include <KActionCollection>
#include <KFormat>
#include <QPointer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDirIterator>
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

#include "filepane_helpers.h"

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
                        : fp_fmtRwx(QFileInfo(lp).permissions());
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


