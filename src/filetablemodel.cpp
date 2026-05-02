#include "tagmanager.h"
#include "filetablemodel.h"
#include <QThread>
#include <QMutex>
#include <QDirIterator>
#include <QDateTime>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QSettings>

FileTableModel::FileTableModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_fs(new QFileSystemModel(this))
{
    m_fs->setRootPath("/");
    m_fs->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    connect(m_fs, &QFileSystemModel::directoryLoaded, this, [this](const QString &dir) {
        if (QDir(dir) == QDir(m_path)) {
            reload();
        }
    });
}

void FileTableModel::reload()
{
    beginResetModel();
    QDir dir(m_path);
    QDir::Filters f = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (m_showHidden) f |= QDir::Hidden;
    dir.setFilter(f);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QFileInfoList all = dir.entryInfoList();

    if (m_tagFilter.isEmpty()) {
        m_entries = all;
    } else {
        m_entries.clear();
        for (const auto &fi : all) {
            if (TagManager::instance().fileTag(fi.absoluteFilePath()) == m_tagFilter)
                m_entries.append(fi);
        }
    }
    endResetModel();
    emit directoryLoaded();
}

void FileTableModel::setTagFilter(const QString &tag)
{
    m_tagFilter = tag;
    reload();
}

void FileTableModel::setDirectory(const QString &path)
{
    m_path = path;
    // QFileSystemModel für Icons weiter nutzen
    m_fs->setRootPath(path);
    reload();
}

void FileTableModel::setShowHidden(bool show)
{
    if (m_showHidden == show) return;
    m_showHidden = show;
    reload();
}

int FileTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

int FileTableModel::columnCount(const QModelIndex &) const { return ColCount; }

QFileInfo FileTableModel::fileInfo(int row) const
{
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries.at(row);
}

bool FileTableModel::isDir(int row) const
{
    if (row < 0 || row >= m_entries.size()) return false;
    return m_entries.at(row).isDir();
}

QString FileTableModel::filePath(int row) const
{
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries.at(row).absoluteFilePath();
}

// Alter in Tagen → Farbe
static QString formatSize(qint64 s)
{
    if (s < 1024)           return QString("%1 B").arg(s);
    if (s < 1024*1024)      return QString("%1 KB").arg(s/1024.0,0,'f',1);
    if (s < 1024*1024*1024) return QString("%1 MB").arg(s/1048576.0,0,'f',1);
    return QString("%1 GB").arg(s/1073741824.0,0,'f',2);
}

static QColor ageColor(qint64 days)
{
    // Farben aus Einstellungen lesen (Fallback: Standardwerte)
    static const QStringList defaults = {
        "#00cc44", "#00aacc", "#ddaa00", "#ee6600", "#cc2200", "#6677aa"
    };
    auto col = [&](int idx) -> QColor {
        QSettings s("SplitCommander", "AgeBadge");
        return QColor(s.value(QString("color%1").arg(idx), defaults.at(idx)).toString());
    };
    if (days <  1)  return col(0);
    if (days <  7)  return col(1);
    if (days < 30)  return col(2);
    if (days < 180) return col(3);
    if (days < 365) return col(4);
    return col(5);
}

static QPixmap ageBadge(qint64 days, const QString &text)
{
    QColor col = ageColor(days);
    QPixmap px(40, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setBrush(col);
    p.setPen(Qt::NoPen);
    p.drawRect(1, 2, 37, 12);
    p.setPen(Qt::black);
    QFont f; f.setPixelSize(9); f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(1,2,37,12), Qt::AlignCenter, text);
    p.end();
    return px;
}

QVariant FileTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};

    const QFileInfo fi   = m_entries.at(index.row());
    const bool      dir  = fi.isDir();
    const qint64    days = fi.lastModified().daysTo(QDateTime::currentDateTime());

    if (role == Qt::DecorationRole && index.column() == IconCol) {
        // Erst Theme-Icon direkt - das ist immer sofort verfügbar
        if (fi.isDir())
            return QIcon::fromTheme("folder");
        QString mime = "text-x-" + fi.suffix().toLower();
        QIcon i = QIcon::fromTheme(mime);
        if (!i.isNull()) return i;
        // Fallback: QFileSystemModel (hat MIME-Datenbank)
        QModelIndex fsIdx = m_fs->index(fi.absoluteFilePath());
        QVariant ico = m_fs->data(fsIdx, Qt::DecorationRole);
        if (!ico.isNull()) return ico;
        return QIcon::fromTheme("text-x-generic");
    }

    if (role == Qt::DecorationRole && index.column() == TagCol) {
        QString tag = TagManager::instance().fileTag(fi.absoluteFilePath());
        if (tag.isEmpty()) return {};
        QString color = TagManager::instance().tagColor(tag);
        QPixmap px(12, 12);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(color));
        p.setPen(Qt::NoPen);
        p.drawEllipse(1, 1, 10, 10);
        p.end();
        return px;
    }

    if (role == Qt::DecorationRole && index.column() == AgeCol) {
        QString txt;
        if (days <  1)       txt = tr("heute");
        else if (days < 7)   txt = QString("%1d").arg(days);
        else if (days < 30)  txt = QString("%1w").arg(days/7);
        else if (days < 365) txt = QString("%1M").arg(days/30);
        else                 txt = QString("%1y").arg(days/365);
        return ageBadge(days, txt);
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case IconCol: return {};
        case NameCol: return fi.fileName();
        case TypeCol: return dir ? QStringLiteral("[DIR]") : fi.suffix().toUpper();
        case AgeCol:  return {};
        case DateCol: return fi.lastModified().toString("yyyy-MM-dd HH:mm");
        case TagCol: {
            QString tag = TagManager::instance().fileTag(fi.absoluteFilePath());
            return tag.isEmpty() ? QVariant{} : tag;
        }
        case PermsCol: {
            QFile::Permissions p = fi.permissions();
            QString s;
            s += (p & QFile::ReadOwner)  ? 'r' : '-';
            s += (p & QFile::WriteOwner) ? 'w' : '-';
            s += (p & QFile::ExeOwner)   ? 'x' : '-';
            s += (p & QFile::ReadGroup)  ? 'r' : '-';
            s += (p & QFile::WriteGroup) ? 'w' : '-';
            s += (p & QFile::ExeGroup)   ? 'x' : '-';
            s += (p & QFile::ReadOther)  ? 'r' : '-';
            s += (p & QFile::WriteOther) ? 'w' : '-';
            s += (p & QFile::ExeOther)   ? 'x' : '-';
            return s;
        }
        case SizeCol: {
            if (dir) {
                QString path = fi.absoluteFilePath();
                {
                    QMutexLocker lk(&m_sizeCacheMutex);
                    if (m_sizeCache.contains(path)) {
                        qint64 cached = m_sizeCache[path];
                        if (cached == -1) return tr("...");
                        return formatSize(cached);
                    }
                }
                m_sizeCacheMutex.lock();
                m_sizeCache[path] = -1;
                m_sizeCacheMutex.unlock();
                QModelIndex persistIdx = createIndex(index.row(), index.column());
                FileTableModel *self = const_cast<FileTableModel*>(this);
                QThread *t = QThread::create([self, path, persistIdx]() {
                    qint64 total = 0;
                    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                    while (it.hasNext()) { it.next(); total += it.fileInfo().size(); }
                    QMutexLocker lk(&self->m_sizeCacheMutex);
                    self->m_sizeCache[path] = total;
                    QMetaObject::invokeMethod(self, [self, persistIdx]() {
                        emit self->dataChanged(persistIdx, persistIdx);
                    }, Qt::QueuedConnection);
                });
                t->start();
                connect(t, &QThread::finished, t, &QObject::deleteLater);
                return tr("...");
            }
            return formatSize(fi.size());
        }
        default: return {};
        }
    }

    if (role == Qt::ToolTipRole && index.column() == NameCol)
        return fi.fileName();

    return {};
}

QVariant FileTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole || o != Qt::Horizontal) return {};
    switch (section) {
    case IconCol: return {};
    case NameCol: return tr("Name");
    case TypeCol: return tr("Typ");
    case AgeCol:  return tr("Alter");
    case DateCol: return tr("Datum");
    case SizeCol: return tr("Größe");
    case PermsCol: return tr("Rechte");
    case TagCol:   return tr("Tag");
    default: return {};
    }
}

void FileTableModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();
    auto cmp = [&](const QFileInfo &a, const QFileInfo &b) -> bool {
        // Verzeichnisse immer zuerst
        if (a.isDir() != b.isDir()) return a.isDir() > b.isDir();
        switch (column) {
        case NameCol: return order == Qt::AscendingOrder
                ? a.fileName().toLower() < b.fileName().toLower()
                : a.fileName().toLower() > b.fileName().toLower();
        case TypeCol: return order == Qt::AscendingOrder
                ? a.suffix() < b.suffix() : a.suffix() > b.suffix();
        case DateCol: return order == Qt::AscendingOrder
                ? a.lastModified() < b.lastModified()
                : a.lastModified() > b.lastModified();
        case SizeCol: return order == Qt::AscendingOrder
                ? a.size() < b.size() : a.size() > b.size();
        case PermsCol: return order == Qt::AscendingOrder
                ? a.permissions() < b.permissions() : a.permissions() > b.permissions();
        default: return false;
        }
    };
    std::sort(m_entries.begin(), m_entries.end(), cmp);
    endResetModel();
}
