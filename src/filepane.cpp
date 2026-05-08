#include "filepane.h"
#include "thememanager.h"
#include "settingsdialog.h"
#include "tagmanager.h"
#include "agebadgedialog.h"
#include "terminalutils.h"

#include <QKeyEvent>
#include <QFileDialog>
#include <QScrollBar>
#include <QStandardPaths>
#include <QResizeEvent>
#include <QTimer>
#include <QDir>
#include <QSettings>
#include <QMenu>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMimeData>
#include <QGuiApplication>
#include <QClipboard>
#include <QGraphicsDropShadowEffect>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <KApplicationTrader>
#include <KService>
#include <KIO/CopyJob>
#include <KFileUtils>
#include <KPropertiesDialog>
#include <KIO/DeleteOrTrashJob>
#include <KIO/RenameDialog>
#include <KIO/PasteJob>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>
#include <KFileItemActions>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KNewFileMenu>
#include <KDirModel>
#include <KDirLister>
#include <KDirSortFilterProxyModel>

// --- Hilfsfunktionen ---

static QString menuStyle() {
    return TM().ssMenu() +
    "QMenu::item{padding:6px 20px;}"
    "QMenu::separator{background:rgba(236,239,244,120);height:1px;margin:4px 8px;}";
}

static void fp_applyMenuShadow(QMenu *menu) {
    if (!menu) return;
    auto *shadow = new QGraphicsDropShadowEffect(menu);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 140));
    menu->setGraphicsEffect(shadow);
}

static QString fp_getText(QWidget *parent, const QString &title, const QString &label,
                           const QString &defaultText = QString())
{
    QDialog dlg(nullptr);
    dlg.setAttribute(Qt::WA_StyledBackground, true);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    auto *lay = new QVBoxLayout(&dlg);
    lay->setSpacing(4);
    lay->setContentsMargins(12, 10, 12, 10);
    auto *lbl  = new QLabel(label, &dlg);
    auto *edit = new QLineEdit(defaultText, &dlg);
    auto *btnRow = new QHBoxLayout();
    auto *ok  = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok")),     "OK",                    &dlg);
    auto *can = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QObject::tr("Abbrechen"), &dlg);
    ok->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    btnRow->addWidget(can);
    lay->addWidget(lbl);
    lay->addWidget(edit);
    lay->addLayout(btnRow);
    QObject::connect(ok,  &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(can, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.adjustSize();
    if (parent) {
        QPoint center = parent->mapToGlobal(parent->rect().center());
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    if (dlg.exec() != QDialog::Accepted) return {};
    return edit->text();
}

static QString fmtSize(qint64 sz) {
    if (sz < 1024)             return QString("%1 B").arg(sz);
    if (sz < 1024*1024)        return QString("%1 KB").arg(sz/1024);
    if (sz < 1024LL*1024*1024) return QString("%1 MB").arg(sz/(1024*1024));
    return QString("%1 GB").arg(sz/(1024LL*1024*1024));
}

static QString fmtRwx(QFileDevice::Permissions p) {
    QString s;
    s += (p & QFile::ReadOwner)  ? QLatin1String("r") : QLatin1String("-");
    s += (p & QFile::WriteOwner) ? QLatin1String("w") : QLatin1String("-");
    s += (p & QFile::ExeOwner)   ? QLatin1String("x") : QLatin1String("-");
    s += (p & QFile::ReadGroup)  ? QLatin1String("r") : QLatin1String("-");
    s += (p & QFile::WriteGroup) ? QLatin1String("w") : QLatin1String("-");
    s += (p & QFile::ExeGroup)   ? QLatin1String("x") : QLatin1String("-");
    s += (p & QFile::ReadOther)  ? QLatin1String("r") : QLatin1String("-");
    s += (p & QFile::WriteOther) ? QLatin1String("w") : QLatin1String("-");
    s += (p & QFile::ExeOther)   ? QLatin1String("x") : QLatin1String("-");
    return s;
}

// --- Spalten-Definitionen ---
const QList<FPColDef>& FilePane::colDefs() {
    static QList<FPColDef> defs = {
        {FP_NAME,          "Name",               "",         true,  220},
        {FP_TYP,           "Typ",                "",         true,  48 },
        {FP_ALTER,         "Alter",              "",         true,  48 },
        {FP_DATUM,         "Geändert",           "",         true,  80 },
        {FP_ERSTELLT,      "Erstellt",           "",         false, 80 },
        {FP_ZUGRIFF,       "Letzter Zugriff",    "",         false, 80 },
        {FP_GROESSE,       "Größe",              "",         true,  60 },
        {FP_RECHTE,        "Rechte",             "",         true,  68 },
        {FP_EIGENTUEMER,   "Eigentümer",         "Weitere",  false, 70 },
        {FP_GRUPPE,        "Benutzergruppe",     "Weitere",  false, 80 },
        {FP_PFAD,          "Pfad",               "Weitere",  false, 120},
        {FP_ERWEITERUNG,   "Dateierweiterung",   "Weitere",  false, 80 },
        {FP_TAGS,          "Tags",               "",         false, 50 },
        {FP_IMG_DATUM,     "Datum der Aufnahme", "Bild",     false, 80 },
        {FP_IMG_ABMESS,    "Abmessungen",        "Bild",     false, 80 },
        {FP_IMG_BREITE,    "Breite",             "Bild",     false, 50 },
        {FP_IMG_HOEHE,     "Höhe",               "Bild",     false, 50 },
        {FP_IMG_AUSRICHT,  "Ausrichtung",        "Bild",     false, 60 },
        {FP_AUD_KUENSTLER, "Künstler",           "Audio",    false, 80 },
        {FP_AUD_GENRE,     "Genre",              "Audio",    false, 60 },
        {FP_AUD_ALBUM,     "Album",              "Audio",    false, 80 },
        {FP_AUD_DAUER,     "Dauer",              "Audio",    false, 50 },
        {FP_AUD_BITRATE,   "Bitrate",            "Audio",    false, 60 },
        {FP_AUD_STUECK,    "Stück",              "Audio",    false, 40 },
        {FP_VID_SEITENVERH,"Seitenverhältnis",   "Video",    false, 60 },
        {FP_VID_FRAMERATE, "Bildwiederholrate",  "Video",    false, 60 },
        {FP_VID_DAUER,     "Dauer",              "Video",    false, 50 },
        {FP_DOC_TITEL,     "Titel",              "Dokument", false, 80 },
        {FP_DOC_AUTOR,     "Autor",              "Dokument", false, 70 },
        {FP_DOC_HERAUSGEBER,"Herausgeber",       "Dokument", false, 80 },
        {FP_DOC_SEITEN,    "Seitenanzahl",       "Dokument", false, 50 },
        {FP_DOC_WOERTER,   "Wortanzahl",         "Dokument", false, 60 },
        {FP_DOC_ZEILEN,    "Zeilenanzahl",       "Dokument", false, 60 },
    };
    return defs;
}

// --- FPColumnsProxy ---

FPColumnsProxy::FPColumnsProxy(QObject *parent)
    : QAbstractProxyModel(parent)
{}

void FPColumnsProxy::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel()) {
        disconnect(sourceModel(), nullptr, this, nullptr);
    }
    QAbstractProxyModel::setSourceModel(model);
    m_sortProxy = qobject_cast<KDirSortFilterProxyModel*>(model);
    m_kdirModel = m_sortProxy
        ? qobject_cast<KDirModel*>(m_sortProxy->sourceModel())
        : nullptr;
    if (model) {
        connect(model, &QAbstractItemModel::rowsInserted,    this, [this](const QModelIndex&,int f,int l){ beginInsertRows({},f,l); endInsertRows(); });
        connect(model, &QAbstractItemModel::rowsRemoved,     this, [this](const QModelIndex&,int f,int l){ beginRemoveRows({},f,l); endRemoveRows(); });
        connect(model, &QAbstractItemModel::dataChanged,     this, [this](const QModelIndex&,const QModelIndex&){ emit dataChanged(index(0,0),index(rowCount()-1,columnCount()-1)); });
        connect(model, &QAbstractItemModel::modelReset,      this, [this](){ beginResetModel(); endResetModel(); });
        connect(model, &QAbstractItemModel::layoutChanged,   this, [this](){ emit layoutChanged(); });
    }
}

void FPColumnsProxy::setVisibleCols(const QList<FPCol> &cols)
{
    beginResetModel();
    m_visCols = cols;
    endResetModel();
}

void FPColumnsProxy::setTagFilter(const QString &tag)
{
    beginResetModel();
    m_tagFilter = tag;
    endResetModel();
}

QModelIndex FPColumnsProxy::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_sortProxy) return {};
    return m_sortProxy->index(proxyIndex.row(), 0);
}

QModelIndex FPColumnsProxy::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid()) return {};
    return createIndex(sourceIndex.row(), 0);
}

QModelIndex FPColumnsProxy::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 || column >= columnCount())
        return {};
    return createIndex(row, column);
}

QModelIndex FPColumnsProxy::parent(const QModelIndex &) const
{
    return {};
}

int FPColumnsProxy::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_sortProxy) return 0;
    if (m_tagFilter.isEmpty()) return m_sortProxy->rowCount();
    // Tag-Filter: zähle gültige Rows
    int count = 0;
    for (int i = 0; i < m_sortProxy->rowCount(); ++i)
        if (acceptsRow(i, {})) ++count;
    return count;
}

bool FPColumnsProxy::acceptsRow(int sourceRow, const QModelIndex &) const
{
    if (m_tagFilter.isEmpty()) return true;
    if (!m_sortProxy || !m_kdirModel) return false;
    QModelIndex sortIdx = m_sortProxy->index(sourceRow, 0);
    QModelIndex dirIdx  = m_sortProxy->mapToSource(sortIdx);
    KFileItem item = m_kdirModel->itemForIndex(dirIdx);
    if (item.isNull()) return false;
    const QString localPath = item.localPath();
    if (localPath.isEmpty()) return false;
    return TagManager::instance().fileTag(localPath) == m_tagFilter;
}

int FPColumnsProxy::columnCount(const QModelIndex &) const
{
    return m_visCols.size();
}

Qt::ItemFlags FPColumnsProxy::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

KFileItem FPColumnsProxy::fileItem(const QModelIndex &proxyIdx) const
{
    if (!proxyIdx.isValid() || !m_sortProxy || !m_kdirModel) return KFileItem();
    int row = proxyIdx.row();
    // Tag-Filter: row im Proxy != row im sortProxy
    if (!m_tagFilter.isEmpty()) {
        int accepted = 0;
        for (int i = 0; i < m_sortProxy->rowCount(); ++i) {
            if (acceptsRow(i, {})) {
                if (accepted == row) {
                    QModelIndex sortIdx = m_sortProxy->index(i, 0);
                    QModelIndex dirIdx  = m_sortProxy->mapToSource(sortIdx);
                    return m_kdirModel->itemForIndex(dirIdx);
                }
                ++accepted;
            }
        }
        return KFileItem();
    }
    QModelIndex sortIdx = m_sortProxy->index(row, 0);
    QModelIndex dirIdx  = m_sortProxy->mapToSource(sortIdx);
    return m_kdirModel->itemForIndex(dirIdx);
}

int FPColumnsProxy::kdirColumn(FPCol col) const
{
    switch (col) {
    case FP_NAME:        return KDirModel::Name;
    case FP_GROESSE:     return KDirModel::Size;
    case FP_DATUM:       return KDirModel::ModifiedTime;
    case FP_RECHTE:      return KDirModel::Permissions;
    case FP_EIGENTUEMER: return KDirModel::Owner;
    case FP_GRUPPE:      return KDirModel::Group;
    default:             return -1;
    }
}

QVariant FPColumnsProxy::extraData(const KFileItem &item, FPCol col, int role) const
{
    if (item.isNull()) return {};
    if (role != Qt::DisplayRole && role != Qt::UserRole) return {};
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    switch (col) {
    case FP_TYP: {
        if (role != Qt::DisplayRole) return {};
        if (item.isDir()) return QStringLiteral("[DIR]");
        QString ext = QFileInfo(item.name()).suffix().toUpper().left(4);
        return ext.isEmpty() ? QStringLiteral("[???]") : QStringLiteral("[") + ext + QStringLiteral("]");
    }
    case FP_ALTER: {
        qint64 mtime = item.time(KFileItem::ModificationTime).toSecsSinceEpoch();
        qint64 age   = mtime > 0 ? now - mtime : -1;
        if (role == Qt::UserRole) return age;
        return {};
    }
    case FP_ERSTELLT: {
        if (role != Qt::DisplayRole) return {};
        QDateTime dt = item.time(KFileItem::CreationTime);
        return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd")) : QString();
    }
    case FP_ZUGRIFF: {
        if (role != Qt::DisplayRole) return {};
        QDateTime dt = item.time(KFileItem::AccessTime);
        return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd")) : QString();
    }
    case FP_ERWEITERUNG:
        if (role != Qt::DisplayRole) return {};
        return QFileInfo(item.name()).suffix();
    case FP_PFAD:
        if (role != Qt::DisplayRole) return {};
        return item.localPath().isEmpty() ? item.url().path()
                                          : QFileInfo(item.localPath()).absolutePath();
    case FP_TAGS: {
        if (role != Qt::DisplayRole) return {};
        QString lp = item.localPath();
        return lp.isEmpty() ? QVariant() : TagManager::instance().fileTag(lp);
    }
    default: return {};
    }
}

QVariant FPColumnsProxy::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() >= m_visCols.size()) return {};
    FPCol col = m_visCols.at(index.column());

    if (role == Qt::UserRole + 99) return col;

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
    if (kdc < 0) return extraData(item, col, role);

    // KDirModel-Spalten direkt aus KFileItem lesen — kein mapToSource nötig
    if (item.isNull()) return {};

    if (col == FP_NAME) {
        if (role == Qt::DecorationRole) return QIcon::fromTheme(item.iconName());
        if (role == Qt::DisplayRole) {
            if (!item.isDir() && !SettingsDialog::showFileExtensions())
                return QFileInfo(item.name()).completeBaseName();
            return item.name();
        }
    }
    if (col == FP_GROESSE && role == Qt::DisplayRole) {
        if (item.isDir()) {
            QString lp = item.localPath();
            if (!lp.isEmpty())
                return QString("%1 El.").arg(
                    QDir(lp).entryList(QDir::AllEntries|QDir::NoDotAndDotDot).count());
            return {};
        }
        return fmtSize(item.size());
    }
    if (col == FP_GROESSE && role == Qt::UserRole)
        return (qint64)item.size();
    if (col == FP_DATUM && role == Qt::DisplayRole) {
        QDateTime dt = item.time(KFileItem::ModificationTime);
        return dt.isValid() ? dt.toString(SettingsDialog::dateFormat()) : QString();
    }
    if (col == FP_RECHTE && role == Qt::DisplayRole) {
        QString lp = item.localPath();
        return lp.isEmpty() ? item.permissionsString() : fmtRwx(QFileInfo(lp).permissions());
    }
    if (col == FP_EIGENTUEMER && role == Qt::DisplayRole) return item.user();
    if (col == FP_GRUPPE      && role == Qt::DisplayRole) return item.group();

    return {};
}

QVariant FPColumnsProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section >= m_visCols.size()) return {};
    if (role == Qt::DisplayRole) {
        FPCol col = m_visCols.at(section);
        for (const auto &d : FilePane::colDefs())
            if (d.id == col) return d.label;
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
    void initStyleOption(QStyleOptionViewItem *opt, const QModelIndex &idx) const override {
        QStyledItemDelegate::initStyleOption(opt, idx);
        QIcon ico = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
        if (!ico.isNull()) {
            int sz = opt->decorationSize.width();
            // Direkt in der Zielgröße holen — kein Skalieren
            QPixmap pm = ico.pixmap(QSize(sz, sz));
            if (!pm.isNull()) opt->icon = QIcon(pm);
        }
    }
};

QString FilePaneDelegate::formatAge(qint64 s) {
    if (s < 0)         return {};
    if (s < 60)        return QString("%1s").arg(s);
    if (s < 3600)      return QString("%1m").arg(s/60);
    if (s < 86400)     return QString("%1h").arg(s/3600);
    if (s < 86400*30)  return QString("%1t").arg(s/86400);
    if (s < 86400*365) return QString("%1M").arg(s/86400/30);
    return QString("%1J").arg(s/86400/365);
}

QColor FilePaneDelegate::ageColor(qint64 s) {
    if (s < 3600)       return SettingsDialog::ageBadgeColor(0);
    if (s < 86400)      return SettingsDialog::ageBadgeColor(1);
    if (s < 86400*7)    return SettingsDialog::ageBadgeColor(2);
    if (s < 86400*30)   return SettingsDialog::ageBadgeColor(3);
    if (s < 86400*365)  return SettingsDialog::ageBadgeColor(4);
    return SettingsDialog::ageBadgeColor(5);
}

void FilePaneDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);

    bool sel = o.state & QStyle::State_Selected;
    bool hov = o.state & QStyle::State_MouseOver;

    QColor bg, bgAlt;
    if (focused) {
        bg    = QColor(TM().colors().bgList);
        bgAlt = QColor(TM().colors().bgAlternate);
    } else {
        bg    = QColor(TM().colors().bgDeep);
        bgAlt = QColor(TM().colors().bgBox);
    }

    QColor bgFinal = sel ? QColor(TM().colors().bgSelect)
                         : hov ? QColor(TM().colors().bgHover)
                               : (idx.row() % 2 ? bgAlt : bg);
    p->fillRect(o.rect, bgFinal);

    int col = idx.data(Qt::UserRole + 99).toInt();
    QRect r = o.rect.adjusted(4, 0, -4, 0);
    QFont f = o.font;
    f.setPointSize(fontSize);
    p->setFont(f);

    QColor tc = (sel || hov) ? QColor(TM().colors().textLight) : QColor(TM().colors().textPrimary);
    QColor dc = (sel || hov) ? QColor(TM().colors().textLight) : QColor(TM().colors().textMuted);

    if (col == FP_NAME) {
        // New-File-Indicator
        if (AgeBadgeDialog::showNewIndicator()) {
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
            if (pm.isNull()) pm = icon.pixmap(QSize(16, 16));
            if (!pm.isNull())
                p->drawPixmap(r.left(), r.top() + (r.height() - ic) / 2,
                    pm.scaled(ic, ic, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        r.setLeft(r.left() + ic + 4);
        p->setPen(tc);
        p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                    o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight, r.width()));

    } else if (col == FP_ALTER) {
        qint64 secs = idx.data(Qt::UserRole).toLongLong();
        if (secs < 0) return;
        QString age = formatAge(secs);
        QColor  bc  = ageColor(secs);

        const int BW = 44, BH = 14;
        QRect br(r.left() + (r.width()-BW)/2, r.top() + (r.height()-BH)/2, BW, BH);

        p->setRenderHint(QPainter::Antialiasing, false);
        p->setRenderHint(QPainter::TextAntialiasing, true);
        p->setBrush(bc);
        p->setPen(Qt::NoPen);
        p->drawRect(br);

        QColor textCol = (bc.lightness() > 140) ? QColor(0,0,0) : QColor(255,255,255);
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
            QColor tagCol = colorStr.isEmpty() ? QColor(TM().colors().accent) : QColor(colorStr);
            int textW = o.fontMetrics.horizontalAdvance(tagName);
            int BW = qMin(textW + 12, r.width()), BH = 16;
            QRect br(r.left() + (r.width()-BW)/2, r.top() + (r.height()-BH)/2, BW, BH);

            p->setRenderHint(QPainter::Antialiasing, false);
            p->setBrush(tagCol.darker(180));
            p->setPen(QPen(tagCol, 1));
            p->drawRect(br);

            QFont fb = f;
            fb.setBold(false);
            fb.setPointSizeF(7.5);
            fb.setHintingPreference(QFont::PreferFullHinting);
            p->setFont(fb);
            p->setPen(QColor(255,255,255));
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
                    o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight, r.width()));
        p->setFont(f);
    }

    p->setPen(QColor(TM().colors().bgHover));
    p->drawLine(o.rect.bottomLeft(), o.rect.bottomRight());
}

QSize FilePaneDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(0, rowHeight);
}

// --- FilePane::setupColumns ---
void FilePane::setupColumns()
{
    m_colVisible.resize(FP_COUNT);
    for (int i = 0; i < FP_COUNT; i++) m_colVisible[i] = false;
    for (const auto &d : colDefs()) m_colVisible[d.id] = d.defaultVisible;

    QSettings s(QStringLiteral("SplitCommander"), QStringLiteral("UI"));
    s.beginGroup(QStringLiteral("columns"));
    for (const auto &d : colDefs())
        if (s.contains(QString::number(d.id)))
            m_colVisible[d.id] = s.value(QString::number(d.id)).toBool();
    s.endGroup();

    // Sichtbare Spalten zusammenstellen
    QList<FPCol> visCols;
    for (const auto &d : colDefs())
        if (m_colVisible[d.id]) visCols << d.id;

    m_proxy->setVisibleCols(visCols);
}

// --- FilePane Konstruktor ---
FilePane::FilePane(QWidget *parent, const QString &settingsKey) : QWidget(parent)
{
    m_settingsKey = QStringLiteral("FilePane/") + settingsKey + QStringLiteral("/");
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    lay->addWidget(m_stack);

    // --- KDE Model Stack ---
    m_lister = new KDirLister(this);
    m_lister->setAutoUpdate(true);
    m_lister->setMainWindow(window());

    m_dirModel = new KDirModel(this);
    m_dirModel->setDirLister(m_lister);

    m_sortProxy = new KDirSortFilterProxyModel(this);
    m_sortProxy->setSourceModel(m_dirModel);
    m_sortProxy->setSortFoldersFirst(true);
    m_sortProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_proxy = new FPColumnsProxy(this);
    m_proxy->setSourceModel(m_sortProxy);
    // --- TreeView ---
    m_view = new QTreeView(this);
    m_view->setRootIsDecorated(false);
    m_view->setItemsExpandable(false);
    m_view->setUniformRowHeights(true);
    m_view->setSortingEnabled(true);
    m_view->setAlternatingRowColors(false);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setMouseTracking(true);
    m_view->setFrameStyle(QFrame::NoFrame);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->header()->setMinimumSectionSize(0);
    m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_view->setModel(m_proxy);

    m_delegate = new FilePaneDelegate(this);
    m_view->setItemDelegate(m_delegate);
    m_view->installEventFilter(this);

    const int savedHeight = QSettings().value(m_settingsKey + "rowHeight", 26).toInt();
    m_delegate->rowHeight = savedHeight;
    m_delegate->fontSize  = qBound(9, savedHeight / 3, 16);
    m_view->setIconSize(QSize(qBound(12, savedHeight-6, 48), qBound(12, savedHeight-6, 48)));

    setupColumns();

    auto *hdr = m_view->header();
    const QList<FPCol> &visCols = m_proxy->visibleCols();
    for (int i = 0; i < visCols.size(); ++i) {
        FPCol col = visCols.at(i);
        bool isName = (col == FP_NAME);
        hdr->setSectionResizeMode(i, isName ? QHeaderView::Stretch : QHeaderView::Interactive);
        if (!isName) {
            for (const auto &d : colDefs())
                if (d.id == col) { hdr->resizeSection(i, d.defaultWidth); break; }
        }
    }
    hdr->setSectionsClickable(true);
    hdr->setSortIndicatorShown(true);
    hdr->setSortIndicator(0, Qt::AscendingOrder);
    m_sortProxy->sort(KDirModel::Name, Qt::AscendingOrder);
    hdr->setStretchLastSection(false);
    hdr->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hdr->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hdr, &QHeaderView::customContextMenuRequested, this, &FilePane::showHeaderMenu);
    connect(hdr, &QHeaderView::sectionDoubleClicked, this, [this](int col) {
        m_view->resizeColumnToContents(col);
    });

    m_view->setStyleSheet(QString(
        "QTreeView{background:%1;border:none;color:%2;outline:none;font-size:10px;}"
        "QTreeView::item{padding:2px 4px;}"
        "QTreeView::item:hover{background:%3;}"
        "QTreeView::item:selected{background:%4;color:%5;}"
        "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
        "QHeaderView::section{background:%6;color:%7;border:none;"
        "border-bottom:1px solid %3;"
        "padding:3px 6px;font-size:10px;}"
        "QTreeView QScrollBar:vertical{background:transparent;width:0px;margin:0px;border:none;}"
        "QTreeView QScrollBar::handle:vertical{background:rgba(136,192,208,40);border-radius:5px;min-height:20px;margin:2px;}"
        "QTreeView QScrollBar::handle:vertical:hover{background:rgba(136,192,208,100);}"
        "QTreeView QScrollBar::add-line:vertical,QTreeView QScrollBar::sub-line:vertical{height:0px;}"
        "QTreeView QScrollBar::add-page:vertical,QTreeView QScrollBar::sub-page:vertical{background:transparent;}"
        "QTreeView QScrollBar:horizontal{background:transparent;height:0px;margin:0px;border:none;}")
        .arg(TM().colors().bgList, TM().colors().textPrimary,
             TM().colors().bgHover, TM().colors().bgSelect, TM().colors().textLight,
             TM().colors().bgBox,   TM().colors().textAccent));
    m_view->viewport()->setStyleSheet("background:transparent;");
    m_view->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    // --- Overlay Scrollbars ---
    m_overlayBar = new QScrollBar(Qt::Vertical, this);
    m_overlayBar->setStyleSheet(
        "QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:none;}"
        "QScrollBar::handle:vertical{background:rgba(136,192,208,60);border-radius:4px;min-height:20px;margin:1px;}"
        "QScrollBar::handle:vertical:hover{background:rgba(136,192,208,140);}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
    m_overlayBar->hide();
    m_overlayBar->raise();
    auto *native = m_view->verticalScrollBar();
    connect(native, &QScrollBar::rangeChanged, m_overlayBar, &QScrollBar::setRange);
    connect(native, &QScrollBar::valueChanged, m_overlayBar, &QScrollBar::setValue);
    connect(native, &QScrollBar::rangeChanged, this, [this](int, int max) {
        m_overlayBar->setVisible(max > 0);
    });
    connect(m_overlayBar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);

    m_overlayHBar = new QScrollBar(Qt::Horizontal, this);
    m_overlayHBar->setStyleSheet(
        "QScrollBar:horizontal{background:transparent;height:6px;margin:0px;border:none;}"
        "QScrollBar::handle:horizontal{background:rgba(136,192,208,80);border-radius:3px;min-width:20px;margin:1px;}"
        "QScrollBar::handle:horizontal:hover{background:rgba(136,192,208,160);}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0px;}"
        "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}");
    m_overlayHBar->hide();
    m_overlayHBar->raise();
    auto *nativeH = m_view->horizontalScrollBar();
    connect(nativeH, &QScrollBar::rangeChanged, m_overlayHBar, &QScrollBar::setRange);
    connect(nativeH, &QScrollBar::valueChanged, m_overlayHBar, &QScrollBar::setValue);
    connect(nativeH, &QScrollBar::rangeChanged, this, [this](int, int max) {
        m_overlayHBar->setVisible(max > 0);
    });
    connect(m_overlayHBar, &QScrollBar::valueChanged, nativeH, &QScrollBar::setValue);

    // --- IconView ---
    m_iconView = new QListView(this);
    m_iconView->setModel(m_proxy);
    m_iconView->setItemDelegate(new ScaledIconDelegate(m_iconView));
    m_iconView->setSelectionModel(m_view->selectionModel());
    m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_iconView->setMouseTracking(true);
    m_iconView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_iconView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_iconView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_iconView->verticalScrollBar()->setSingleStep(26);
    m_iconView->setWordWrap(true);
    m_iconView->setStyleSheet(QString(
        "QListView{background:%1;border:none;color:%2;outline:none;font-size:11px;}"
        "QListView::item{padding:4px;border-radius:4px;}"
        "QListView::item:hover{background:%3;}"
        "QListView::item:selected{background:%4;color:%5;}"
        "QListView QScrollBar:vertical{width:0px;background:transparent;border:none;}")
        .arg(TM().colors().bgList, TM().colors().textPrimary,
             TM().colors().bgHover, TM().colors().bgSelect, TM().colors().textLight));

    m_stack->addWidget(m_view);
    m_stack->addWidget(m_iconView);
    m_stack->setCurrentWidget(m_view);

    // --- Signale ---
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTreeView::customContextMenuRequested, this, &FilePane::showContextMenu);
    connect(m_view, &QTreeView::activated, this, &FilePane::onItemActivated);
    connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
        QSettings gs(QStringLiteral("SplitCommander"), QStringLiteral("General"));
        if (gs.value(QStringLiteral("singleClick"), false).toBool()) onItemActivated(idx);
    });
    connect(m_iconView, &QListView::clicked, this, [this](const QModelIndex &idx) {
        QSettings gs(QStringLiteral("SplitCommander"), QStringLiteral("General"));
        if (gs.value(QStringLiteral("singleClick"), false).toBool()) onItemActivated(idx);
    });
    connect(m_iconView, &QListView::customContextMenuRequested, this, &FilePane::showContextMenu);
    connect(m_iconView, &QListView::activated, this, &FilePane::onItemActivated);

    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex &cur, const QModelIndex&) {
        KFileItem item = m_proxy->fileItem(cur);
        if (!item.isNull())
            emit fileSelected(item.localPath().isEmpty()
                ? item.url().toString() : item.localPath());
    });

    // Tag-Änderungen: Model aktualisieren (KDirModel refresht von selbst,
    // aber Tags-Spalte muss manuell angestoßen werden)
    connect(&TagManager::instance(), &TagManager::fileTagChanged, this, [this](const QString &) {
        // Alle sichtbaren Indizes der Tags-Spalte neu zeichnen
        const QList<FPCol> &visCols = m_proxy->visibleCols();
        int tagColIdx = -1;
        for (int i = 0; i < visCols.size(); ++i)
            if (visCols.at(i) == FP_TAGS) { tagColIdx = i; break; }
        if (tagColIdx >= 0)
            emit m_proxy->dataChanged(
                m_proxy->index(0, tagColIdx),
                m_proxy->index(m_proxy->rowCount()-1, tagColIdx));
    });

    // KNewFileMenu
    m_newFileMenu = new KNewFileMenu(this);
    connect(m_newFileMenu, &KNewFileMenu::fileCreated, this, &FilePane::onNewFileCreated);

    setRootPath(QDir::homePath());
}

// --- Navigation ---

void FilePane::setRootPath(const QString &path)
{
    if (path.isEmpty()) return;

    // KIO-URL erkennen
    if (!path.startsWith("/") && !path.startsWith("~") && !path.isEmpty()) {
        const QUrl url(path);
        const QString scheme = url.scheme().toLower();
        static const QStringList kioSchemes = {
            "gdrive","smb","sftp","ftp","ftps","mtp","remote","network",
            "bluetooth","davs","dav","nfs","fish","webdav","webdavs","afc","zeroconf"
        };
        if (!scheme.isEmpty() && kioSchemes.contains(scheme)) {
            setRootUrl(url);
            return;
        }
    }

    m_kioMode    = false;
    m_currentUrl = QUrl();
    m_currentPath = path;
    m_proxy->setTagFilter(QString()); // Tag-Filter zurücksetzen bei Navigation

    QUrl url = QUrl::fromLocalFile(path);
    m_lister->openUrl(url);
    // Root-Index setzen sobald Lister fertig ist
    connect(m_lister, &KDirLister::completed, this, [this, url]() {
        QModelIndex dirIdx = m_dirModel->indexForUrl(url);
        if (dirIdx.isValid()) {
            QModelIndex sortIdx  = m_sortProxy->mapFromSource(dirIdx);
            QModelIndex proxyIdx = m_proxy->mapFromSource(sortIdx);
            m_view->setRootIndex(proxyIdx);
            m_iconView->setRootIndex(proxyIdx);
        }
    }, Qt::SingleShotConnection);
}

void FilePane::setRootUrl(const QUrl &url)
{
    m_kioMode    = true;
    m_currentUrl = url;
    m_currentPath = url.toString();
    m_proxy->setTagFilter(QString()); // Tag-Filter zurücksetzen

    m_lister->openUrl(url);
    connect(m_lister, &KDirLister::completed, this, [this, url]() {
        QModelIndex dirIdx = m_dirModel->indexForUrl(url);
        if (dirIdx.isValid()) {
            QModelIndex sortIdx  = m_sortProxy->mapFromSource(dirIdx);
            QModelIndex proxyIdx = m_proxy->mapFromSource(sortIdx);
            m_view->setRootIndex(proxyIdx);
            m_iconView->setRootIndex(proxyIdx);
        }
        // Fallback: wenn indexForUrl nichts findet (root-level), kein rootIndex nötig
    }, Qt::SingleShotConnection);
}

const QString& FilePane::currentPath() const
{
    return m_currentPath;
}

bool FilePane::hasFocus() const { return m_view->hasFocus(); }

void FilePane::reload()
{
    if (m_kioMode)
        m_lister->openUrl(m_currentUrl);
    else
        m_lister->openUrl(QUrl::fromLocalFile(m_currentPath));
}

void FilePane::setNameFilter(const QString &pattern)
{
    m_filter = pattern;
    m_lister->setNameFilter(pattern);
    reload();
}

void FilePane::setFoldersFirst(bool on)
{
    m_foldersFirst = on;
    m_sortProxy->setSortFoldersFirst(on);
}

void FilePane::setRowHeight(int height)
{
    // Nur für Detailliste
    if (m_delegate) {
        m_delegate->rowHeight = height;
        m_delegate->fontSize  = qBound(9, height/3, 16);
        m_view->setIconSize(QSize(qBound(12,height-6,48), qBound(12,height-6,48)));
    }
    QSettings().setValue(m_settingsKey + "rowHeight", height);
    m_view->update();
}

void FilePane::showTaggedFiles(const QString &tagName)
{
    m_currentTagFilter = tagName;
    m_proxy->setTagFilter(tagName);
}

QList<QUrl> FilePane::selectedUrls() const
{
    QList<QUrl> urls;
    const auto indexes = m_view->selectionModel()->selectedRows();
    for (const auto &idx : indexes) {
        KFileItem item = m_proxy->fileItem(idx);
        if (!item.isNull())
            urls << item.url();
    }
    return urls;
}

// --- Spalten-Sichtbarkeit ---
void FilePane::setColumnVisible(int colId, bool visible)
{
    if (colId < 0 || colId >= FP_COUNT) return;
    m_colVisible[colId] = visible;

    QSettings s(QStringLiteral("SplitCommander"), QStringLiteral("UI"));
    s.beginGroup(QStringLiteral("columns"));
    s.setValue(QString::number(colId), visible);
    s.endGroup();
    s.sync();

    QList<FPCol> visCols;
    for (const auto &d : colDefs())
        if (m_colVisible[d.id]) visCols << d.id;
    m_proxy->setVisibleCols(visCols);

    // Header-Breiten neu setzen
    auto *hdr = m_view->header();
    for (int i = 0; i < visCols.size(); ++i) {
        FPCol col = visCols.at(i);
        bool isName = (col == FP_NAME);
        hdr->setSectionResizeMode(i, isName ? QHeaderView::Stretch : QHeaderView::Interactive);
        if (!isName)
            for (const auto &d : colDefs())
                if (d.id == col) { hdr->resizeSection(i, d.defaultWidth); break; }
    }

    emit columnsChanged(colId, visible);
}

void FilePane::setViewMode(int mode)
{
    m_viewMode = mode;
    switch (mode) {
    case 0: // Details — TreeView
        m_stack->setCurrentWidget(m_view);
        break;
    case 1: // Kompakt — ListMode, 32px Icons fest
        m_stack->setCurrentWidget(m_iconView);
        m_iconView->setViewMode(QListView::ListMode);
        m_iconView->setIconSize(QSize(32, 32));
        m_iconView->setGridSize(QSize());
        m_iconView->setSpacing(0);
        m_iconView->setUniformItemSizes(true);
        break;
    case 2: // Symbole — IconMode, 48px Icons fest
        m_stack->setCurrentWidget(m_iconView);
        m_iconView->setViewMode(QListView::IconMode);
        m_iconView->setIconSize(QSize(48, 48));
        m_iconView->setGridSize(QSize(96, 80));
        m_iconView->setSpacing(8);
        m_iconView->setResizeMode(QListView::Adjust);
        m_iconView->setUniformItemSizes(true);
        m_iconView->setWordWrap(true);
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
void FilePane::onItemActivated(const QModelIndex &index)
{
    KFileItem item = m_proxy->fileItem(index);
    if (item.isNull()) return;

    QString path = item.localPath().isEmpty()
        ? item.url().toString() : item.localPath();

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
        for (const QFileInfo &fi : remoteDir.entryInfoList({"*.desktop"}, QDir::Files)) {
            QSettings ds(fi.absoluteFilePath(), QSettings::IniFormat);
            ds.beginGroup(QStringLiteral("Desktop Entry"));
            const QString urlVal  = ds.value(QStringLiteral("URL")).toString();
            const QString baseName = fi.completeBaseName();
            if (!urlVal.isEmpty())
                remoteViewMap.insert(baseName, urlVal);
            ds.endGroup();
        }
    }

    // UDS_NAME für remote:/ Einträge auflösen
    const QString udsName = item.name();
    if (remoteViewMap.contains(udsName)) {
        const QString mapped = remoteViewMap.value(udsName);
        emit fileActivated(mapped);
        return;
    }

    // file:// → lokaler Pfad
    if (item.url().scheme() == "file") {
        const QString localPath = item.url().toLocalFile();
        if (!localPath.isEmpty()) emit fileActivated(localPath);
        return;
    }

    // KIO oder Verzeichnis → navigieren
    const bool isKio = !path.startsWith("/") && path.contains(QStringLiteral(":/"));
    if (isKio || item.isDir()) {
        emit fileActivated(path);
        return;
    }

    // Datei öffnen
    QDesktopServices::openUrl(item.url());
}

// --- resizeEvent / eventFilter ---
void FilePane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlayBar) {
        const int w    = 8;
        const int hdrH = m_view->header()->height();
        m_overlayBar->setGeometry(m_view->width() - w, m_view->y() + hdrH,
                                  w, m_view->height() - hdrH);
    }
    if (m_overlayHBar) {
        const int h    = 6;
        const int rsvd = (m_overlayBar && m_overlayBar->isVisible()) ? 8 : 0;
        m_overlayHBar->setGeometry(m_view->x(),
                                   m_view->y() + m_view->height() - h,
                                   m_view->width() - rsvd, h);
        m_overlayHBar->raise();
    }
}

bool FilePane::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_view && e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Delete) {
            emit deleteRequested();
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void FilePane::onNewFileCreated(const QUrl &)
{
    reload();
}

// --- openWithApp ---
void FilePane::openWithApp(const QString &entry, const QString &path)
{
    auto svc = KService::serviceByDesktopName(entry);
    if (!svc) return;
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
void FilePane::showHeaderMenu(const QPoint &pos)
{
    QMenu menu;
    menu.setStyleSheet(
        TM().ssMenu() +
        "QMenu::item{padding:5px 20px 5px 8px;}"
        "QMenu::separator{background:rgba(236,239,244,120);height:1px;margin:3px 8px;}"
        "QMenu::indicator{width:14px;height:14px;}");

    QMap<QString, QMenu*> subMenus;

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
        connect(act, &QAction::toggled, this, [this, id=d.id](bool v) {
            setColumnVisible(id, v);
        });
    }
    menu.exec(m_view->header()->mapToGlobal(pos));
}

// --- showContextMenu — volles Menü mit KFileItemActions + KNewFileMenu ---
void FilePane::showContextMenu(const QPoint &pos)
{
    QModelIndex proxyIndex = m_view->indexAt(pos);
    bool hasItem = proxyIndex.isValid();

    KFileItem item;
    QString   path;
    QUrl      itemUrl;

    if (hasItem) {
        item    = m_proxy->fileItem(proxyIndex);
        if (item.isNull()) { hasItem = false; }
        else {
            path    = item.localPath().isEmpty() ? item.url().toString() : item.localPath();
            itemUrl = item.url();
        }
    }

    if (hasItem && path.contains(QStringLiteral("new-account"))) return;

    // Aktuelles Verzeichnis
    const bool   isKioPath = m_kioMode || (!m_currentPath.startsWith("/") && m_currentPath.contains(QStringLiteral(":/")));
    const QString dirPath  = isKioPath ? m_currentPath
                             : (hasItem && item.isDir()
                                ? path : QFileInfo(path).absolutePath());
    const QUrl   dirUrl    = isKioPath ? QUrl(m_currentPath)
                             : QUrl::fromLocalFile(m_currentPath.isEmpty()
                               ? QFileInfo(path).absolutePath() : m_currentPath);
    const bool   isDir     = !hasItem || item.isDir();
    const bool   isArchive = [&]() -> bool {
        if (!hasItem) return false;
        QMimeDatabase mdb;
        const QString mt = mdb.mimeTypeForUrl(itemUrl).name();
        return mt.contains(QStringLiteral("zip")) || mt.contains(QStringLiteral("tar")) || mt.contains(QStringLiteral("rar"))
            || mt.contains(QStringLiteral("7z"))  || mt.contains(QStringLiteral("gzip"))|| mt.contains(QStringLiteral("bzip"))
            || mt.contains(QStringLiteral("xz"))  || mt.contains(QStringLiteral("archive"))|| mt.contains(QStringLiteral("compressed"));
    }();

    QMenu menu(this);
    fp_applyMenuShadow(&menu);
    menu.setStyleSheet(menuStyle());

    // --- 1. Öffnen mit (KFileItemActions) ---
    if (hasItem) {
        KFileItemList items;
        items << item;
        KFileItemActions *actions = new KFileItemActions(&menu);
        KFileItemListProperties props(items);
        actions->setItemListProperties(props);

        // "Öffnen mit" Untermenü
        auto *openWithMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-open")), tr("Öffnen mit"));
        openWithMenu->setStyleSheet(menuStyle());
        // Standard-Aktion
        openWithMenu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("Standard"), this,
            [this, item]() {
                if (item.isDir()) setRootPath(item.localPath().isEmpty()
                    ? item.url().toString() : item.localPath());
                else QDesktopServices::openUrl(item.url());
            });
        openWithMenu->addSeparator();
        // KDE-Apps aus KFileItemActions
        actions->insertOpenWithActionsTo(nullptr, openWithMenu, {});
        openWithMenu->addSeparator();
        openWithMenu->addAction(QIcon::fromTheme(QStringLiteral("application-x-executable")),
            tr("Andere Anwendung …"), this, [this, path]() {
                QProcess::startDetached("kioclient6",
                    {"openWith", QUrl::fromLocalFile(path).toString()});
            });
        menu.addSeparator();

        // KDE-Dienste — unerwünschte Aktionen filtern
        {
            QMenu tmpMenu;
            actions->addActionsTo(&tmpMenu);
            for (QAction *act : tmpMenu.actions()) {
                // Widget-Actions (Ordner-Farb-Picker) rausfiltern
                if (qobject_cast<QWidgetAction*>(act)) continue;
                const QString t = act->text().toLower();
                if (t.contains(QStringLiteral("symbol"))      || t.contains(QStringLiteral("icon"))     ||
                    t.contains(QStringLiteral("emblem"))      || t.contains(QStringLiteral("stichwort")) ||
                    t.contains(QStringLiteral("keyword"))     || t.contains(QStringLiteral("farb"))      ||
                    t.contains(QStringLiteral("colour"))      || t.contains(QStringLiteral("color")))
                    continue;
                menu.addAction(act);
            }
        }
        menu.addSeparator();
    }

    // --- 2. Neu (KNewFileMenu) ---
    if (m_newFileMenu) {
        m_newFileMenu->setWorkingDirectory(dirUrl);
        m_newFileMenu->checkUpToDate();
        auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-new")), tr("Neu"));
        newMenu->setStyleSheet(menuStyle());
        for (QAction *act : m_newFileMenu->menu()->actions())
            newMenu->addAction(act);
    }

    // --- 3. Umbenennen ---
    if (hasItem) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen …"), this,
            [this, item, dirPath, isKioPath]() {
                const QString currentName = isKioPath
                    ? item.url().fileName() : item.name();
                QString newName = fp_getText(this, tr("Umbenennen"), tr("Neuer Name:"), currentName);
                if (newName.isEmpty() || newName == currentName) return;
                QUrl dest = isKioPath
                    ? QUrl(dirPath.endsWith('/') ? QString(dirPath+newName) : QString(dirPath+'/'+newName))
                    : QUrl::fromLocalFile(dirPath + "/" + newName);
                auto *job = KIO::moveAs(item.url(), dest, KIO::DefaultFlags);
                job->uiDelegate()->setAutoErrorHandlingEnabled(true);
            });
    }

    menu.addSeparator();

    // --- 4. In den Papierkorb / Löschen ---
    if (hasItem) {
        auto *removeAct = new QAction(&menu);
        auto setTrash  = [removeAct]() {
            removeAct->setText(QObject::tr("In den Papierkorb verschieben"));
            removeAct->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
        };
        auto setDelete = [removeAct]() {
            removeAct->setText(QObject::tr("Unwiderruflich löschen"));
            removeAct->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete-shred")));
        };
        if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) setDelete();
        else setTrash();

        struct ShiftFilter : public QObject {
            std::function<void()> onPress, onRelease;
            ShiftFilter(QObject *p, std::function<void()> pr, std::function<void()> re)
                : QObject(p), onPress(pr), onRelease(re) {}
            bool eventFilter(QObject*, QEvent *e) override {
                if (e->type()==QEvent::KeyPress || e->type()==QEvent::KeyRelease) {
                    auto *ke = static_cast<QKeyEvent*>(e);
                    if (ke->key() == Qt::Key_Shift) {
                        if (e->type()==QEvent::KeyPress) onPress();
                        else onRelease();
                    }
                }
                return false;
            }
        };
        menu.installEventFilter(new ShiftFilter(&menu, setDelete, setTrash));

        connect(removeAct, &QAction::triggered, this, [this, itemUrl]() {
            const bool shift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
            auto *job = new KIO::DeleteOrTrashJob({itemUrl},
                shift ? KIO::AskUserActionInterface::Delete
                      : KIO::AskUserActionInterface::Trash,
                KIO::AskUserActionInterface::DefaultConfirmation, this);
            job->start();
        });
        menu.addAction(removeAct);
        menu.addSeparator();
    }

    // --- 5. Senden an ---
    if (hasItem && !isKioPath) {
        auto *sendMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-send")), tr("Senden an"));
        sendMenu->setStyleSheet(menuStyle());
        sendMenu->addAction(QIcon::fromTheme(QStringLiteral("user-desktop")), tr("Desktop (Verknüpfung erstellen)"),
            this, [path]() {
                const QString desk = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                QFile::link(path, desk + "/" + QFileInfo(path).fileName());
            });
        sendMenu->addAction(QIcon::fromTheme(QStringLiteral("mail-send")), tr("E-Mail-Empfänger"),
            this, [path]() {
                QUrl mail(QString("mailto:?subject=%1&attachment=%2")
                    .arg(QFileInfo(path).fileName(), QUrl::fromLocalFile(path).toString()));
                QDesktopServices::openUrl(mail);
            });
        sendMenu->addAction(QIcon::fromTheme(QStringLiteral("application-zip")), tr("ZIP-komprimierter Ordner"),
            this, [path, dirPath]() {
                QProcess::startDetached("ark", {"--batch","--add-to",
                    dirPath+"/"+QFileInfo(path).fileName()+".zip", path});
            });
        sendMenu->addAction(QIcon::fromTheme(QStringLiteral("bluetooth")), tr("Bluetooth-Gerät"),
            this, [path]() {
                QProcess::startDetached("bluedevil-sendfile",
                    {"-u", QUrl::fromLocalFile(path).toString()});
            });
    }

    // --- 6. Bearbeiten ---
    {
        auto *editMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Bearbeiten"));
        editMenu->setStyleSheet(menuStyle());
        if (hasItem) {
            editMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-cut")), tr("Ausschneiden"), this,
                [itemUrl]() {
                    auto *mime = new QMimeData();
                    mime->setUrls({itemUrl});
                    mime->setData("x-kde-cut-selection", "1");
                    QGuiApplication::clipboard()->setMimeData(mime);
                });
            editMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Kopieren"), this,
                [itemUrl]() {
                    auto *mime = new QMimeData();
                    mime->setUrls({itemUrl});
                    QGuiApplication::clipboard()->setMimeData(mime);
                });
        }
        const QMimeData *clip = QGuiApplication::clipboard()->mimeData();
        bool canPaste = clip && clip->hasUrls();
        auto *pasteAct = editMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), tr("Einfügen"), this,
            [dirUrl, isKioPath, clip]() {
                if (!clip || !clip->hasUrls()) return;
                bool isCut = clip->data("x-kde-cut-selection") == "1";
                QList<QUrl> urls = clip->urls();
                if (isCut)
                    KIO::move(urls, dirUrl, KIO::DefaultFlags)
                        ->uiDelegate()->setAutoErrorHandlingEnabled(true);
                else
                    KIO::copy(urls, dirUrl, KIO::DefaultFlags)
                        ->uiDelegate()->setAutoErrorHandlingEnabled(true);
            });
        pasteAct->setEnabled(canPaste);
        if (hasItem) {
            editMenu->addSeparator();
            editMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Adresse kopieren"), this,
                [path]() { QGuiApplication::clipboard()->setText(path); });
            if (!isKioPath) {
                editMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Hier duplizieren"), this,
                    [this, path, dirPath, itemUrl]() {
                        QString baseName = QFileInfo(path).completeBaseName();
                        QString suffix   = QFileInfo(path).suffix();
                        QString copyName = suffix.isEmpty()
                            ? QString(baseName + tr(" (Kopie)"))
                            : QString(baseName + tr(" (Kopie).") + suffix);
                        KIO::copy({itemUrl}, QUrl::fromLocalFile(dirPath+"/"+copyName),
                                  KIO::DefaultFlags)
                            ->uiDelegate()->setAutoErrorHandlingEnabled(true);
                    });
            }
        }
    }

    menu.addSeparator();

    // --- 7. Komprimieren / Entpacken ---
    if (hasItem && !isKioPath) {
        const QString baseName = QFileInfo(path).fileName();
        const QUrl    dirU     = QUrl::fromLocalFile(dirPath);
        auto *compressMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("archive-insert")), tr("Komprimieren"));
        compressMenu->setStyleSheet(menuStyle());
        const QString tgzName   = KFileUtils::suggestName(dirU, baseName+".tar.gz");
        const QString zipName   = KFileUtils::suggestName(dirU, baseName+".zip");
        compressMenu->addAction(tr("Komprimieren nach '%1'").arg(tgzName), this,
            [path, dirPath, tgzName]() {
                QProcess::startDetached("ark",{"--batch","--add-to",dirPath+"/"+tgzName,path}); });
        compressMenu->addAction(tr("Komprimieren nach '%1'").arg(zipName), this,
            [path, dirPath, zipName]() {
                QProcess::startDetached("ark",{"--batch","--add-to",dirPath+"/"+zipName,path}); });
        compressMenu->addAction(tr("Komprimieren nach ..."), this,
            [path]() { QProcess::startDetached("ark",{"--add","--changetofirstpath","--dialog",path}); });
        if (isArchive) {
            auto *extractMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("archive-extract")), tr("Entpacken"));
            extractMenu->setStyleSheet(menuStyle());
            extractMenu->addAction(tr("Hierher entpacken"), this,
                [path]() { QProcess::startDetached("ark",{"--batch","--autodestination","--autosubfolder",path}); });
            extractMenu->addAction(tr("Entpacken nach ..."), this,
                [path]() { QProcess::startDetached("ark",{"--extract",path}); });
        }
    }

    // --- 8. Aktionen ---
    auto *actMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("system-run")), tr("Aktionen"));
    actMenu->setStyleSheet(menuStyle());
    actMenu->addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), tr("Im Terminal öffnen"),
        this, [dirPath]() { sc_openTerminal(dirPath); });
    if (hasItem && !isKioPath) {
        actMenu->addSeparator();
        actMenu->addAction(QIcon::fromTheme(QStringLiteral("document-encrypt")), tr("Datei verschlüsseln …"),
            this, [path]() { QProcess::startDetached("kleopatra",{"--encrypt",path}); });
        actMenu->addAction(QIcon::fromTheme(QStringLiteral("document-sign")),    tr("Datei signieren & verschlüsseln …"),
            this, [path]() { QProcess::startDetached("kleopatra",{"--sign-encrypt",path}); });
        actMenu->addAction(QIcon::fromTheme(QStringLiteral("document-sign")),    tr("Datei signieren …"),
            this, [path]() { QProcess::startDetached("kleopatra",{"--sign",path}); });
        actMenu->addSeparator();
        actMenu->addAction(QIcon::fromTheme(QStringLiteral("folder-new")), tr("In neuen Ordner verschieben …"),
            this, [this, itemUrl, dirPath]() {
                QString folderName = fp_getText(this, tr("In neuen Ordner verschieben"),
                    tr("Ordnername:"), QFileInfo(itemUrl.toLocalFile()).baseName());
                if (folderName.isEmpty()) return;
                QString dest = dirPath + "/" + folderName;
                QDir().mkdir(dest);
                KIO::move({itemUrl}, QUrl::fromLocalFile(dest), KIO::DefaultFlags);
            });
    }

    // --- 9. Tag ---
    if (hasItem && !isKioPath) {
        auto *tagMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("tag")), tr("Tag"));
        tagMenu->setStyleSheet(menuStyle());
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
            tagMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Tag entfernen"), this,
                [path]() { TagManager::instance().clearFileTag(path); });
        }
    }

    menu.addSeparator();

    // --- 10. Zu Laufwerken hinzufügen (nur KIO-Verzeichnisse) ---
    if (hasItem && isKioPath && isDir) {
        QSettings netCheck(QStringLiteral("SplitCommander"), QStringLiteral("NetworkPlaces"));
        if (!netCheck.value(QStringLiteral("places")).toStringList().contains(path)) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), tr("Zu Laufwerken hinzufügen"), this,
                [this, path, itemUrl, item]() {
                    QString displayName = item.name();
                    if (displayName.isEmpty())
                        displayName = itemUrl.host().isEmpty() ? itemUrl.scheme() : itemUrl.host();
                    emit addToPlacesRequested(path, displayName);
                });
            menu.addSeparator();
        }
    }

    // --- 11. Eigenschaften ---
    if (hasItem) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), tr("Eigenschaften"), this,
            [this, itemUrl]() {
                auto *dlg = new KPropertiesDialog(itemUrl, nullptr);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            });
    }

    menu.exec(m_view->mapToGlobal(pos));
}
