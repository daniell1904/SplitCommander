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
#include <QStandardItem>
#include <QGraphicsDropShadowEffect>
#include <KApplicationTrader>
#include <KService>
#include <KIO/CopyJob>
#include <KFileUtils>
#include <KPropertiesDialog>
#include <KIO/DeleteOrTrashJob>
#include <KIO/RenameDialog>
#include <KIO/PasteJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/ListJob>
#include <KIO/StatJob>
#include <KJobWidgets>

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

// ── Spalten-Definitionen ──────────────────────────────────────────────────────
const QList<FPColDef>& FilePane::colDefs() {
    static QList<FPColDef> defs = {
        {FP_NAME,         "Name",                "",         true,  220},
        {FP_TYP,          "Typ",                 "",         true,  48 },
        {FP_ALTER,        "Alter",               "",         true,  48 },
        {FP_DATUM,        "Geändert",            "",         true,  80 },
        {FP_ERSTELLT,     "Erstellt",            "",         false, 80 },
        {FP_ZUGRIFF,      "Letzter Zugriff",     "",         false, 80 },
        {FP_GROESSE,      "Größe",               "",         true,  60 },
        {FP_RECHTE,       "Rechte",              "",         true,  68 },
        {FP_EIGENTUEMER,  "Eigentümer",          "Weitere",  false, 70 },
        {FP_GRUPPE,       "Benutzergruppe",      "Weitere",  false, 80 },
        {FP_PFAD,         "Pfad",                "Weitere",  false, 120},
        {FP_ERWEITERUNG,  "Dateierweiterung",    "Weitere",  false, 80 },
        {FP_TAGS,         "Tags",                "",         false, 50 },
        {FP_IMG_DATUM,    "Datum der Aufnahme",  "Bild",     false, 80 },
        {FP_IMG_ABMESS,   "Abmessungen",         "Bild",     false, 80 },
        {FP_IMG_BREITE,   "Breite",              "Bild",     false, 50 },
        {FP_IMG_HOEHE,    "Höhe",                "Bild",     false, 50 },
        {FP_IMG_AUSRICHT, "Ausrichtung",         "Bild",     false, 60 },
        {FP_AUD_KUENSTLER,"Künstler",            "Audio",    false, 80 },
        {FP_AUD_GENRE,    "Genre",               "Audio",    false, 60 },
        {FP_AUD_ALBUM,    "Album",               "Audio",    false, 80 },
        {FP_AUD_DAUER,    "Dauer",               "Audio",    false, 50 },
        {FP_AUD_BITRATE,  "Bitrate",             "Audio",    false, 60 },
        {FP_AUD_STUECK,   "Stück",               "Audio",    false, 40 },
        {FP_VID_SEITENVERH,"Seitenverhältnis",   "Video",    false, 60 },
        {FP_VID_FRAMERATE,"Bildwiederholrate",   "Video",    false, 60 },
        {FP_VID_DAUER,    "Dauer",               "Video",    false, 50 },
        {FP_DOC_TITEL,    "Titel",               "Dokument", false, 80 },
        {FP_DOC_AUTOR,    "Autor",               "Dokument", false, 70 },
        {FP_DOC_HERAUSGEBER,"Herausgeber",       "Dokument", false, 80 },
        {FP_DOC_SEITEN,   "Seitenanzahl",        "Dokument", false, 50 },
        {FP_DOC_WOERTER,  "Wortanzahl",          "Dokument", false, 60 },
        {FP_DOC_ZEILEN,   "Zeilenanzahl",        "Dokument", false, 60 },
    };
    return defs;
}

class FolderFirstProxy : public QSortFilterProxyModel {
public:
    bool foldersFirst = true;
    explicit FolderFirstProxy(QObject *p=nullptr) : QSortFilterProxyModel(p) {}
protected:
    bool lessThan(const QModelIndex &l, const QModelIndex &r) const override {
        if (foldersFirst) {
            auto *ml = qobject_cast<QStandardItemModel*>(sourceModel());
            if (ml) {
                int ld = ml->item(l.row(),0) ? ml->item(l.row(),0)->data(Qt::UserRole+10).toInt() : 1;
                int rd = ml->item(r.row(),0) ? ml->item(r.row(),0)->data(Qt::UserRole+10).toInt() : 1;
                if (ld != rd) return ld < rd;
            }
        }
        return QSortFilterProxyModel::lessThan(l, r);
    }
};

// ── Delegate ─────────────────────────────────────────────────────────────────
FilePaneDelegate::FilePaneDelegate(QObject *par) : QStyledItemDelegate(par) {}

QString FilePaneDelegate::formatAge(qint64 s) const {
    if(s<60)        return QString("%1s").arg(s);
    if(s<3600)      return QString("%1m").arg(s/60);
    if(s<86400)     return QString("%1h").arg(s/3600);
    if(s<86400*30)  return QString("%1t").arg(s/86400);
    if(s<86400*365) return QString("%1M").arg(s/86400/30);
    return QString("%1J").arg(s/86400/365);
}

QColor FilePaneDelegate::ageColor(qint64 s) const {
    // Kontinuierlicher Verlauf wie OC:
    // < 1h = Index 0 (Rot)
    // < 1Tag = Index 1 (Orange)
    // < 7 Tage = Index 2 (Grün)
    // < 1 Monat = Index 3 (Cyan)
    // < 1 Jahr = Index 4 (Blau)
    // > 1 Jahr = Index 5 (Grau/Lila)
    if(s < 3600)            return SettingsDialog::ageBadgeColor(0);   // < 1 Stunde
    if(s < 86400)           return SettingsDialog::ageBadgeColor(1);   // < 1 Tag
    if(s < 86400*7)         return SettingsDialog::ageBadgeColor(2);   // < 7 Tage
    if(s < 86400*30)        return SettingsDialog::ageBadgeColor(3);   // < 1 Monat
    if(s < 86400*365)       return SettingsDialog::ageBadgeColor(4);   // < 1 Jahr
    return SettingsDialog::ageBadgeColor(5);                            // > 1 Jahr
}

void FilePaneDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);

    bool sel = o.state & QStyle::State_Selected;
    bool hov = o.state & QStyle::State_MouseOver;
    QColor bg, bgAlt;

    // Hintergrundfarben basierend auf Fokus-Status
    // In FilePaneDelegate::paint (ca. Zeile 111):
    if (focused) {
        bg    = QColor(TM().colors().bgList);
        bgAlt = QColor(TM().colors().bgAlternate); // Geändert von bgHover
    } else {
        bg    = QColor(TM().colors().bgDeep);
        bgAlt = QColor(TM().colors().bgBox);
    }

    QColor bgFinal = sel ? QColor(TM().colors().bgSelect) : hov ? QColor(TM().colors().bgHover) : (idx.row() % 2 ? bgAlt : bg);
    p->fillRect(o.rect, bgFinal);

    int col = idx.data(Qt::UserRole + 99).toInt(); // logische Spalten-ID
    QRect r = o.rect.adjusted(4, 0, -4, 0);
    QFont f = o.font;
    f.setPointSize(fontSize);
    p->setFont(f);

    QColor tc = (sel || hov) ? QColor(TM().colors().textLight) : QColor(TM().colors().textPrimary);
    QColor dc = (sel || hov) ? QColor(TM().colors().textLight) : QColor(TM().colors().textMuted);

    if (col == FP_NAME) {
        // New-File-Indicator: vertikaler Streifen links
        if (AgeBadgeDialog::showNewIndicator()) {
            qint64 ageSecs = idx.data(Qt::UserRole + 2).toLongLong();
            if (ageSecs > 0 && ageSecs < 86400 * 2) {
                QColor stripColor = ageColor(ageSecs);
                QRect strip(o.rect.left(), o.rect.top(), 3, o.rect.height());
                p->fillRect(strip, stripColor);
            }
        }

        QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            p->drawPixmap(r.left(), r.top() + (r.height() - 16) / 2, icon.pixmap(16, 16));
        }
        r.setLeft(r.left() + 22);
        p->setPen(tc);
        p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                    o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight, r.width()));

    } else if (col == FP_ALTER) {
        qint64 secs = idx.data(Qt::UserRole).toLongLong();
        QString age = formatAge(secs);
        QColor bc = ageColor(secs); // Holt die Live-Farbe aus dem Dialog

        const int BW = 44, BH = 14;
        QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2, BW, BH);

        // Kein Antialiasing – scharfe Kanten, keine runden Ecken
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setRenderHint(QPainter::TextAntialiasing, true);
        p->setBrush(bc);
        p->setPen(Qt::NoPen);
        p->drawRect(br);

        // Reines Weiß oder Schwarz – kein subpixel blur
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
            QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2, BW, BH);

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
            p->drawText(br, Qt::AlignCenter, o.fontMetrics.elidedText(tagName, Qt::ElideRight, BW - 6));
            p->setFont(f);
        }
    } else {
        p->setPen(col == FP_GROESSE ? tc : dc);
        if (col == FP_RECHTE) {
            QFont fm = f;
            fm.setFamily("monospace");
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
// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
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
    auto *lbl = new QLabel(label, &dlg);
    auto *edit = new QLineEdit(defaultText, &dlg);
    auto *btnRow = new QHBoxLayout();
    auto *ok  = new QPushButton(QIcon::fromTheme("dialog-ok"),     "OK",        &dlg);
    auto *can = new QPushButton(QIcon::fromTheme("dialog-cancel"), QObject::tr("Abbrechen"), &dlg);
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
    if(sz<1024)             return QString("%1 B").arg(sz);
    if(sz<1024*1024)        return QString("%1 KB").arg(sz/1024);
    if(sz<1024LL*1024*1024) return QString("%1 MB").arg(sz/(1024*1024));
    return QString("%1 GB").arg(sz/(1024LL*1024*1024));
}

static QString fmtRwx(QFile::Permissions p) {
    QString s;
    s+=(p&QFile::ReadOwner)?"r":"-"; s+=(p&QFile::WriteOwner)?"w":"-"; s+=(p&QFile::ExeOwner)?"x":"-";
    s+=(p&QFile::ReadGroup)?"r":"-"; s+=(p&QFile::WriteGroup)?"w":"-"; s+=(p&QFile::ExeGroup)?"x":"-";
    s+=(p&QFile::ReadOther)?"r":"-"; s+=(p&QFile::WriteOther)?"w":"-"; s+=(p&QFile::ExeOther)?"x":"-";
    return s;
}

// ── FilePane ─────────────────────────────────────────────────────────────────
void FilePane::setupColumns() {
    m_colVisible.resize(FP_COUNT);
    // Defaults
    for(int i=0;i<FP_COUNT;i++) m_colVisible[i]=false;
    for(const auto &d : colDefs()) m_colVisible[d.id]=d.defaultVisible;

    // Aus QSettings laden
    QSettings s("SplitCommander","UI");
    s.beginGroup("columns");
    for(const auto &d : colDefs())
        if(s.contains(QString::number(d.id)))
            m_colVisible[d.id]=s.value(QString::number(d.id)).toBool();
    s.endGroup();

    // Model-Header aufbauen
    QStringList labels;
    for(const auto &d : colDefs())
        if(m_colVisible[d.id]) labels << d.label;
    m_model->setHorizontalHeaderLabels(labels);

    int visIdx=0;
    for(const auto &d : colDefs()) {
        if(!m_colVisible[d.id]) continue;
        auto *hi = m_model->horizontalHeaderItem(visIdx);
        if(hi) hi->setTextAlignment(d.id==FP_NAME ? Qt::AlignLeft|Qt::AlignVCenter : Qt::AlignCenter);
        visIdx++;
    }
}

void FilePane::buildRow(const QFileInfo &fi, QList<QStandardItem*> &items) {
    bool isDir=fi.isDir();
    qint64 secs=fi.lastModified().secsTo(QDateTime::currentDateTime());
    QString ext=fi.suffix().toUpper().left(4);
    QString typ=isDir?"[DIR]":(ext.isEmpty()?"[???]":"["+ext+"]");

    for(const auto &d : colDefs()) {
        if(!m_colVisible[d.id]) continue;
        auto *it=new QStandardItem();
        it->setEditable(false);
        it->setData(d.id, Qt::UserRole+99); // Spalten-ID für Delegate

        switch(d.id) {
        case FP_NAME: {
            const QString displayName = (!fi.isDir() && !SettingsDialog::showFileExtensions())
                ? fi.completeBaseName() : fi.fileName();
            it->setText(displayName);
            it->setIcon(m_iconProv.icon(fi));
            it->setData(fi.absoluteFilePath(), Qt::UserRole);
            it->setData(isDir?0:1, Qt::UserRole+10);
            it->setData(secs, Qt::UserRole+2); // Alter in Sekunden für New-File-Indicator
            break;
        }
        case FP_TYP:      it->setText(typ); break;
        case FP_ALTER:
            it->setData(secs, Qt::UserRole);
            it->setData(secs, Qt::UserRole+1); // für Sortierung
            break;
        case FP_DATUM:
            it->setText(fi.lastModified().toString(SettingsDialog::dateFormat()));
            it->setData(fi.lastModified(), Qt::UserRole);
            break;
        case FP_ERSTELLT:
            it->setText(fi.birthTime().toString("yyyy-MM-dd"));
            it->setData(fi.birthTime(), Qt::UserRole);
            break;
        case FP_ZUGRIFF:
            it->setText(fi.lastRead().toString("yyyy-MM-dd"));
            it->setData(fi.lastRead(), Qt::UserRole);
            break;
        case FP_GROESSE: {
            qint64 sortSz=isDir
                ? QDir(fi.absoluteFilePath()).entryList(QDir::AllEntries|QDir::NoDotAndDotDot).count()
                : fi.size();
            it->setText(isDir ? QString("%1 El.").arg(sortSz) : fmtSize(fi.size()));
            it->setData(sortSz, Qt::UserRole);
            break;
        }
        case FP_RECHTE:     it->setText(fmtRwx(fi.permissions())); break;
        case FP_EIGENTUEMER: it->setText(fi.owner()); break;
        case FP_GRUPPE:     it->setText(fi.group()); break;
        case FP_PFAD:       it->setText(fi.absolutePath()); break;
        case FP_ERWEITERUNG: it->setText(fi.suffix()); break;
        case FP_TAGS:       it->setText(TagManager::instance().fileTag(fi.absoluteFilePath())); break;
        default:            it->setText(""); break; // Meta: async befüllt
        }
        items << it;
    }
}

void FilePane::fetchMetaAsync(const QFileInfo &fi, int row) {
    // Nur für Media-Dateien
    static const QStringList imgExts={"jpg","jpeg","png","tif","tiff","webp","heic","raw","cr2","nef"};
    static const QStringList audExts={"mp3","flac","ogg","m4a","aac","wav","opus","wma"};
    static const QStringList vidExts={"mp4","mkv","avi","mov","webm","flv","wmv","m4v"};

    QString ext=fi.suffix().toLower();
    bool isImg=imgExts.contains(ext);
    bool isAud=audExts.contains(ext);
    bool isVid=vidExts.contains(ext);
    bool needExif=isImg||isAud||isVid;

    if(!needExif) return;

    auto *proc=new QProcess(this);
    QStringList args={"-json","-fast",
        "-DateTimeOriginal","-ImageWidth","-ImageHeight","-Orientation",
        "-Artist","-Genre","-Album","-Duration","-AudioBitrate","-TrackNumber",
        "-VideoFrameRate","-AspectRatio",
        "-Title","-Author","-Publisher","-PageCount",
        fi.absoluteFilePath()};
    proc->setProgram("exiftool");
    proc->setArguments(args);

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, row, isImg, isAud, isVid](int, QProcess::ExitStatus) {
        proc->deleteLater();
        QString out=proc->readAllStandardOutput().trimmed();
        if(out.isEmpty()) return;

        // Einfaches JSON-Parsing
        auto val=[&](const QString &key)->QString {
            int i=out.indexOf("\""+key+"\"");
            if(i<0) return {};
            int c=out.indexOf(":",i); if(c<0) return {};
            int s=out.indexOf("\"",c); int e=out.indexOf("\"",s+1);
            if(s<0) { // Zahl
                int ns=c+1; while(ns<out.size()&&(out[ns]==' '||out[ns]=='\n')) ns++;
                int ne=ns; while(ne<out.size()&&out[ne]!=','&&out[ne]!='\n'&&out[ne]!='}') ne++;
                return out.mid(ns,ne-ns).trimmed();
            }
            return out.mid(s+1,e-s-1);
        };

        // Sichtbare Spalten befüllen
        int visIdx=0;
        for(const auto &d : colDefs()) {
            if(!m_colVisible[d.id]) continue;
            QString v;
            if(isImg) switch(d.id) {
                case FP_IMG_DATUM:    v=val("DateTimeOriginal"); break;
                case FP_IMG_BREITE:   v=val("ImageWidth"); break;
                case FP_IMG_HOEHE:    v=val("ImageHeight"); break;
                case FP_IMG_ABMESS:   { auto w=val("ImageWidth"),h=val("ImageHeight"); if(!w.isEmpty()) v=w+"×"+h; } break;
                case FP_IMG_AUSRICHT: v=val("Orientation"); break;
                default: break;
            }
            if(isAud) switch(d.id) {
                case FP_AUD_KUENSTLER: v=val("Artist"); break;
                case FP_AUD_GENRE:     v=val("Genre"); break;
                case FP_AUD_ALBUM:     v=val("Album"); break;
                case FP_AUD_DAUER:     v=val("Duration"); break;
                case FP_AUD_BITRATE:   v=val("AudioBitrate"); break;
                case FP_AUD_STUECK:    v=val("TrackNumber"); break;
                default: break;
            }
            if(isVid) switch(d.id) {
                case FP_VID_FRAMERATE:  v=val("VideoFrameRate"); break;
                case FP_VID_SEITENVERH: v=val("AspectRatio"); break;
                case FP_VID_DAUER:      v=val("Duration"); break;
                default: break;
            }
            if(!v.isEmpty()) {
                auto *it=m_model->item(row, visIdx);
                if(it) it->setText(v);
            }
            visIdx++;
        }
    });
    proc->start();
}

void FilePane::populate(const QString &path) {
    m_model->removeRows(0, m_model->rowCount());
    QDir dir(path);
    if(!dir.exists()) return;

    QSettings gs("SplitCommander", "General");
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (gs.value("showHidden", false).toBool())
        filters |= QDir::Hidden;

    const auto entries=dir.entryInfoList(
        filters,
        m_foldersFirst ? QDir::DirsFirst|QDir::Name : QDir::Name);

    int row=0;
    for(const QFileInfo &fi : entries) {
        QList<QStandardItem*> items;
        buildRow(fi, items);
        m_model->appendRow(items);
        fetchMetaAsync(fi, row++);
    }
}

// ── KIO-URL Verzeichnis laden ──────────────────────────────────────────────
void FilePane::populateKio(const QUrl &url)
{
    m_model->removeRows(0, m_model->rowCount());

    auto *job = KIO::listDir(url, KIO::HideProgressInfo);
    job->setProperty("sc_url", url);

    connect(job, &KIO::ListJob::entries, this,
        [this](KIO::Job *, const KIO::UDSEntryList &entries) {
        QSettings gs("SplitCommander", "General");
        bool showHidden = gs.value("showHidden", false).toBool();

        for (const KIO::UDSEntry &entry : entries) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name == "." || name == "..") continue;
            if (!showHidden && name.startsWith('.')) continue;

            const bool isDir = entry.isDir();
            const qint64 size = entry.numberValue(KIO::UDSEntry::UDS_SIZE, 0);
            const qint64 mtime = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0);
            const QString linkDest = entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);
            const QString iconName = entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
            const QString mimeType = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);

            // URL zusammenbauen
            QUrl entryUrl = m_currentUrl;
            entryUrl.setPath(m_currentUrl.path().endsWith('/')
                ? m_currentUrl.path() + name
                : m_currentUrl.path() + '/' + name);

            QList<QStandardItem*> items;
            int visIdx = 0;
            for (const auto &d : colDefs()) {
                if (!m_colVisible[d.id]) continue;
                auto *it = new QStandardItem();
                it->setEditable(false);

                switch (d.id) {
                case FP_NAME: {
                    it->setText(name);
                    it->setData(entryUrl.toString(), Qt::UserRole);
                    it->setData(entryUrl.toString(), Qt::UserRole + 1);
                    // Icon
                    QIcon icon;
                    if (!iconName.isEmpty())
                        icon = QIcon::fromTheme(iconName);
                    if (icon.isNull())
                        icon = QIcon::fromTheme(isDir ? "folder" : "text-x-generic");
                    it->setIcon(icon);
                    it->setData(name.toLower(), Qt::UserRole + 5); // sort key
                    break;
                }
                case FP_TYP:
                    it->setText(isDir ? "Ordner" : (mimeType.isEmpty() ? "Datei" : mimeType.section('/', -1)));
                    break;
                case FP_GROESSE:
                    if (!isDir) {
                        it->setText(size < 1024 ? QString("%1 B").arg(size)
                            : size < 1048576 ? QString("%1 KiB").arg(size/1024.0, 0,'f',1)
                            : size < 1073741824 ? QString("%1 MiB").arg(size/1048576.0, 0,'f',1)
                            : QString("%1 GiB").arg(size/1073741824.0, 0,'f',2));
                        it->setData(size, Qt::UserRole);
                    }
                    break;
                case FP_DATUM:
                    if (mtime > 0) {
                        it->setText(QDateTime::fromSecsSinceEpoch(mtime)
                            .toString("dd.MM.yyyy hh:mm"));
                        it->setData(mtime, Qt::UserRole);
                    }
                    break;
                case FP_ALTER: {
                    qint64 ageSecs = mtime > 0
                        ? QDateTime::currentSecsSinceEpoch() - mtime : -1;
                    it->setData(ageSecs, Qt::UserRole + 3);
                    it->setData(ageSecs, Qt::UserRole);
                    break;
                }
                default:
                    break;
                }
                items << it;
                visIdx++;
            }
            if (!items.isEmpty())
                m_model->appendRow(items);
        }
    });

    connect(job, &KJob::result, this, [this](KJob *job) {
        if (job->error() && job->error() != KJob::KilledJobError) {
            qWarning() << "KIO listDir error:" << job->errorString();
        }
    });
}

FilePane::FilePane(QWidget *parent) : QWidget(parent) {
    auto *lay=new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);

    m_stack = new QStackedWidget(this);
    lay->addWidget(m_stack);

    m_model=new QStandardItemModel(0, 0, this);

    m_proxy = new FolderFirstProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);
    m_proxy->setDynamicSortFilter(true);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view=new QTreeView(this);
    m_view->setRootIsDecorated(false);
    m_view->setItemsExpandable(false);
    m_view->setUniformRowHeights(true);
    m_view->setSortingEnabled(true);
    m_view->setAlternatingRowColors(false);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setMouseTracking(true);

    // WICHTIG: Diese 4 Zeilen müssen exakt so dort stehen
    m_view->setFrameStyle(QFrame::NoFrame);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Das hier zwingt den Header, den Scrollbar-Platz zu ignorieren:
    m_view->header()->setMinimumSectionSize(0);

    m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_view->setModel(m_proxy);
    m_delegate = new FilePaneDelegate(this);
    m_view->setItemDelegate(m_delegate);
    // Gespeicherte Zeilenhöhe laden
    const int savedHeight = QSettings().value("FilePane/rowHeight", 26).toInt();
    m_delegate->rowHeight = savedHeight;
    m_delegate->fontSize  = qBound(9, savedHeight / 3, 16);
    m_view->setIconSize(QSize(qBound(12, savedHeight - 6, 48), qBound(12, savedHeight - 6, 48)));

    setupColumns();

    auto *hdr=m_view->header();
    // Spaltenbreiten setzen
    int visIdx=0;
    for(const auto &d : colDefs()) {
        if(!m_colVisible[d.id]) continue;
        hdr->setSectionResizeMode(visIdx, d.id==FP_NAME ? QHeaderView::Stretch : QHeaderView::Fixed);
        if(d.id!=FP_NAME) hdr->resizeSection(visIdx, d.defaultWidth);
        visIdx++;
    }
    m_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    hdr->setSectionsClickable(true);
    hdr->setSortIndicatorShown(true);
    hdr->setSortIndicator(0, Qt::AscendingOrder);
    m_proxy->sort(0, Qt::AscendingOrder);
    hdr->setStretchLastSection(false);
    hdr->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
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

        /* Fix für Header bis zum Rand */
        "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
        "QHeaderView::section{background:%6;color:%7;border:none;"
        "border-bottom:1px solid %3;"
        "padding:3px 6px;font-size:10px;}"

        /* Fix für Scrollbar Overlay */
        "QTreeView QScrollBar:vertical{background:transparent;width:0px;margin:0px;border:none;}"
        "QTreeView QScrollBar::handle:vertical{background:rgba(136,192,208,40);border-radius:5px;min-height:20px;margin:2px;}"
        "QTreeView QScrollBar::handle:vertical:hover{background:rgba(136,192,208,100);}"
        "QTreeView QScrollBar::add-line:vertical,QTreeView QScrollBar::sub-line:vertical{height:0px;}"
        "QTreeView QScrollBar::add-page:vertical,QTreeView QScrollBar::sub-page:vertical{background:transparent;}")
        .arg(TM().colors().bgList, TM().colors().textPrimary,
             TM().colors().bgHover, TM().colors().bgSelect, TM().colors().textLight,
             TM().colors().bgBox,   TM().colors().textAccent));
    m_view->viewport()->setStyleSheet("background:transparent;");
    m_view->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    // Overlay-Scrollbar: schwebt über dem Viewport, reserviert keinen Platz
    m_overlayBar = new QScrollBar(Qt::Vertical, this);
    m_overlayBar->setStyleSheet(
        "QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:none;}"
        "QScrollBar::handle:vertical{background:rgba(136,192,208,60);border-radius:4px;min-height:20px;margin:1px;}"
        "QScrollBar::handle:vertical:hover{background:rgba(136,192,208,140);}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
    m_overlayBar->hide();
    m_overlayBar->raise();

    // Overlay mit nativer Scrollbar synchronisieren
    auto *native = m_view->verticalScrollBar();
    connect(native, &QScrollBar::rangeChanged, m_overlayBar, &QScrollBar::setRange);
    connect(native, &QScrollBar::valueChanged, m_overlayBar, &QScrollBar::setValue);
    connect(native, &QScrollBar::rangeChanged, this, [this](int, int max) {
        m_overlayBar->setVisible(max > 0);
    });
    connect(m_overlayBar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);


    m_iconView = new QListView(this);
    m_iconView->setModel(m_proxy);
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
        "QListView QScrollBar:vertical{width:0px;background:transparent;border:none;}"
        "QListView QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-radius:2px;min-height:20px;}"
        "QListView:hover QScrollBar::handle:vertical{background:rgba(255,255,255,40);}"
        ).arg(TM().colors().bgList, TM().colors().textPrimary,
              TM().colors().bgHover, TM().colors().bgSelect, TM().colors().textLight));

    m_stack->addWidget(m_view);
    m_stack->addWidget(m_iconView);
    m_stack->setCurrentWidget(m_view);

    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view,&QTreeView::customContextMenuRequested,this,&FilePane::showContextMenu);
    connect(m_view,&QTreeView::activated,this,&FilePane::onItemActivated);
    connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
        QSettings gs("SplitCommander", "General");
        if (gs.value("singleClick", false).toBool()) onItemActivated(idx);
    });
    connect(m_iconView, &QListView::clicked, this, [this](const QModelIndex &idx) {
        QSettings gs("SplitCommander", "General");
        if (gs.value("singleClick", false).toBool()) onItemActivated(idx);
    });
    
    connect(m_iconView,&QListView::customContextMenuRequested,this,&FilePane::showContextMenu);
    connect(m_iconView,&QListView::activated,this,&FilePane::onItemActivated);
    connect(m_view->selectionModel(),&QItemSelectionModel::currentChanged,
        this,[this](const QModelIndex &cur,const QModelIndex&){
            QModelIndex src=m_proxy->mapToSource(cur);
            if(src.isValid()){
                auto *it=m_model->item(src.row(),0);
                if(it) emit fileSelected(it->data(Qt::UserRole).toString());
            }
        });

    m_watcher=new QFileSystemWatcher(this);
    connect(m_watcher,&QFileSystemWatcher::directoryChanged,this,&FilePane::reload);

    connect(&TagManager::instance(), &TagManager::fileTagChanged, this, [this](const QString &changedPath) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            auto *nameItem = m_model->item(row, 0);
            if (!nameItem) continue;
            if (nameItem->data(Qt::UserRole).toString() != changedPath) continue;
            int visIdx = 0;
            for (const auto &d : colDefs()) {
                if (!m_colVisible[d.id]) { continue; }
                if (d.id == FP_TAGS) {
                    auto *tagItem = m_model->item(row, visIdx);
                    if (tagItem) tagItem->setText(TagManager::instance().fileTag(changedPath));
                    break;
                }
                visIdx++;
            }
            break;
        }
    });

    setRootPath(QDir::homePath());
}

void FilePane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlayBar) {
        const int w = 8;
        const int hdrH = m_view->header()->height();
        m_overlayBar->setGeometry(m_view->width() - w, m_view->y() + hdrH,
                                  w, m_view->height() - hdrH);
    }
}

void FilePane::setFoldersFirst(bool on) {
    m_foldersFirst=on;
    static_cast<FolderFirstProxy*>(m_proxy)->foldersFirst=on;
    if(!m_currentPath.isEmpty()) populate(m_currentPath);
}

void FilePane::setNameFilter(const QString &pattern) {
    m_filter=pattern;
    m_proxy->setFilterWildcard(pattern);
    m_proxy->setFilterKeyColumn(0);
}

void FilePane::setRootUrl(const QUrl &url)
{
    m_kioMode = true;
    m_currentUrl = url;
    m_currentPath = url.toString();
    m_currentTagFilter.clear();
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    populateKio(url);
}

void FilePane::setRootPath(const QString &path) {
    // KIO-URLs erkennen (gdrive:/, smb://, sftp://, ftp://, mtp:/, remote:/)
    if (!path.startsWith("/") && !path.startsWith("~") && !path.isEmpty()) {
        const QUrl url(path);
        const QString scheme = url.scheme().toLower();
        static const QStringList kioSchemes = {
            "gdrive", "smb", "sftp", "ftp", "ftps", "mtp",
            "remote", "network", "bluetooth", "davs", "dav",
            "nfs", "fish", "webdav", "webdavs"
        };
        if (!scheme.isEmpty() && kioSchemes.contains(scheme)) {
            setRootUrl(url);
            return;
        }
    }
    // Normaler lokaler Pfad
    m_kioMode = false;
    m_currentUrl = QUrl();
    if(path.isEmpty()||!QFileInfo::exists(path)) return;
    m_currentTagFilter.clear();
    if(!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_currentPath=path;
    m_watcher->addPath(path);
    populate(path);
}

void FilePane::setColumnVisible(int colId, bool visible) {
    if (colId < 0 || colId >= FP_COUNT) return;
    if (m_colVisible[colId] == visible) return;
    m_colVisible[colId] = visible;
    int visCount=0;
    for(int i=0;i<FP_COUNT;i++) if(m_colVisible[i]) visCount++;
    m_model->setColumnCount(visCount);
    QStringList labels; labels<<"Name";
    for(const auto &d : colDefs())
        if(m_colVisible[d.id]&&d.id!=FP_NAME) labels<<d.label;
    m_model->setHorizontalHeaderLabels(labels);
    auto *hdr=m_view->header();
    int vi=0;
    for(const auto &d : colDefs()) {
        if(!m_colVisible[d.id]) continue;
        hdr->setSectionResizeMode(vi, d.id == FP_NAME ? QHeaderView::Stretch : QHeaderView::Interactive);
        if(d.id!=FP_NAME) hdr->resizeSection(vi, d.defaultWidth);
        vi++;
    }
    if(m_model->horizontalHeaderItem(0))
        m_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    if(!m_currentPath.isEmpty()) populate(m_currentPath);
}

void FilePane::setRowHeight(int height)
{
    if (!m_delegate) return;
    m_delegate->rowHeight = height;
    // Icon-Größe proportional zur Zeilenhöhe
    const int iconSize = qBound(12, height - 6, 48);
    m_view->setIconSize(QSize(iconSize, iconSize));
    m_iconView->setIconSize(QSize(iconSize * 2, iconSize * 2));
    // Schriftgröße anpassen
    m_delegate->fontSize = qBound(9, height / 3, 16);
    m_view->reset();
    QSettings().setValue("FilePane/rowHeight", height);
}

void FilePane::setViewMode(int mode) {
    // 0: Details, 1: Kompakt, 2: Liste, 3: Grid
    bool changed = false;
    QList<bool> newVis = m_colVisible;
    
    if (mode == 0) {
        m_stack->setCurrentWidget(m_view);
        for(int i=1; i<FP_COUNT; i++) newVis[i] = false;
        for(const auto &d : colDefs()) {
            if(d.defaultVisible) newVis[d.id] = true;
        }
    } else if (mode == 1) {
        m_stack->setCurrentWidget(m_iconView);
        m_iconView->setViewMode(QListView::ListMode);
        m_iconView->setAlternatingRowColors(true);
        m_iconView->setFlow(QListView::TopToBottom);
        m_iconView->setWrapping(false);
        m_iconView->setIconSize(QSize(48, 48));
        m_iconView->setGridSize(QSize()); // Let it adjust naturally
        m_iconView->setSpacing(0); // Set spacing to 0 so alternate colors are continuous
        for(int i=1; i<FP_COUNT; i++) newVis[i] = false;
    } else if (mode == 2) {
        m_stack->setCurrentWidget(m_view);
        for(int i=1; i<FP_COUNT; i++) newVis[i] = false;
        newVis[FP_GROESSE] = true;
        newVis[FP_DATUM] = true;
    } else if (mode == 3) {
        m_stack->setCurrentWidget(m_iconView);
        m_iconView->setViewMode(QListView::IconMode);
        m_iconView->setAlternatingRowColors(false);
        m_iconView->setFlow(QListView::LeftToRight);
        m_iconView->setWrapping(true);
        m_iconView->setResizeMode(QListView::Adjust);
        m_iconView->setIconSize(QSize(80, 80));
        m_iconView->setGridSize(QSize(110, 120));
        m_iconView->setSpacing(8);
        for(int i=1; i<FP_COUNT; i++) newVis[i] = false;
    }

    for (int i=1; i<FP_COUNT; i++) {
        if (m_colVisible[i] != newVis[i]) changed = true;
    }
    if (!changed) return;

    m_colVisible = newVis;

    int visCount=0;
    for(int i=0;i<FP_COUNT;i++) if(m_colVisible[i]) visCount++;
    m_model->setColumnCount(visCount);
    QStringList labels; labels<<"Name";
    for(const auto &d : colDefs())
        if(m_colVisible[d.id]&&d.id!=FP_NAME) labels<<d.label;
    m_model->setHorizontalHeaderLabels(labels);
    auto *hdr=m_view->header();
    int vi=0;
    for(const auto &d : colDefs()) {
        if(!m_colVisible[d.id]) continue;
        hdr->setSectionResizeMode(vi, d.id==FP_NAME ? QHeaderView::Stretch : QHeaderView::Fixed);
        if(d.id!=FP_NAME) hdr->resizeSection(vi, d.defaultWidth);
        vi++;
    }
    if(m_model->horizontalHeaderItem(0))
        m_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    if(!m_currentPath.isEmpty()) populate(m_currentPath);
}

void FilePane::reload() {
    if (!m_currentTagFilter.isEmpty())
        showTaggedFiles(m_currentTagFilter);
    else if (m_kioMode && m_currentUrl.isValid())
        populateKio(m_currentUrl);
    else if (!m_currentPath.isEmpty())
        populate(m_currentPath);
}

void FilePane::showTaggedFiles(const QString &tagName) {
    m_currentTagFilter = tagName;
    m_model->removeRows(0, m_model->rowCount());

    int row = 0;
    for (const QString &path : TagManager::instance().filesWithTag(tagName)) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        QList<QStandardItem*> items;
        buildRow(fi, items);
        m_model->appendRow(items);
        fetchMetaAsync(fi, row++);
    }
}

QString FilePane::currentPath() const { return m_currentPath; }

QList<QUrl> FilePane::selectedUrls() const
{
    QList<QUrl> urls;
    const auto indexes = m_view->selectionModel()->selectedRows();
    for (const auto &idx : indexes) {
        const auto srcIdx = m_proxy->mapToSource(idx);
        const QString p = m_model->item(srcIdx.row(), 0)
            ? m_model->item(srcIdx.row(), 0)->data(Qt::UserRole).toString() : QString();
        if (p.isEmpty()) continue;
        // KIO-URL oder lokaler Pfad
        if (m_kioMode || (!p.startsWith("/") && p.contains(":/")))
            urls << QUrl(p);
        else
            urls << QUrl::fromLocalFile(p);
    }
    return urls;
}
bool FilePane::hasFocus() const { return m_view->hasFocus(); }

void FilePane::onItemActivated(const QModelIndex &index) {
    QModelIndex src=m_proxy->mapToSource(index);
    if(!src.isValid()) return;
    auto *it=m_model->item(src.row(),0);
    if(!it) return;
    QString path=it->data(Qt::UserRole).toString();

    // KIO-URL (gdrive:/, smb://, sftp:// etc.)
    if (m_kioMode || (!path.startsWith("/") && path.contains("://"))) {
        const QUrl url(path);
        const QString scheme = url.scheme().toLower();
        static const QStringList kioSchemes = {
            "gdrive", "smb", "sftp", "ftp", "ftps", "mtp",
            "remote", "network", "bluetooth", "davs", "dav",
            "nfs", "fish", "webdav", "webdavs"
        };
        if (!scheme.isEmpty() && kioSchemes.contains(scheme)) {
            // Prüfen ob Verzeichnis via KIO StatJob
            auto *statJob = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
            connect(statJob, &KJob::result, this, [this, url, statJob]() {
                if (!statJob->error()) {
                    const KIO::UDSEntry entry = statJob->statResult();
                    if (entry.isDir()) {
                        setRootUrl(url);
                        emit fileActivated(url.toString());
                    } else {
                        QDesktopServices::openUrl(url);
                    }
                }
            });
            return;
        }
    }

    // Lokaler Pfad
    if(QFileInfo(path).isDir()){
        setRootPath(path);
        emit fileActivated(path);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

// ── Header-Kontextmenü (Spalten ein-/ausblenden) ──────────────────────────────
void FilePane::showHeaderMenu(const QPoint &pos) {
    QMenu menu;
    fp_applyMenuShadow(&menu);
    menu.setStyleSheet(
        TM().ssMenu() +
        "QMenu::item{padding:5px 20px 5px 8px;}"
        "QMenu::separator{background:rgba(236,239,244,120);height:1px;margin:3px 8px;}"
        "QMenu::indicator{width:14px;height:14px;}");

    // Gruppen-Submenüs
    QMap<QString,QMenu*> subMenus;
    QStringList groupOrder={"Bild","Audio","Video","Dokument","Weitere"};
    for(const QString &g : groupOrder) {
        auto *sub=menu.addMenu(g);
        sub->setStyleSheet(menu.styleSheet());
        subMenus[g]=sub;
    }
    menu.addSeparator();

    // Spalten ohne Gruppe direkt im Menü (außer Name der immer sichtbar ist)
    for(const auto &d : colDefs()) {
        if(d.id==FP_NAME) continue;
        QAction *act;
        if(d.group.isEmpty()) {
            act=menu.addAction(d.label);
        } else {
            act=subMenus[d.group]->addAction(d.label);
        }
        act->setCheckable(true);
        act->setChecked(m_colVisible[d.id]);
        connect(act,&QAction::toggled,this,[this,d](bool on){
            m_colVisible[d.id]=on;
            // In QSettings speichern
            QSettings s("SplitCommander","UI");
            s.beginGroup("columns");
            s.setValue(QString::number(d.id), on);
            s.endGroup();
            // Neuaufbau
            int visCount=0;
            for(int i=0;i<FP_COUNT;i++) if(m_colVisible[i]) visCount++;
            m_model->setColumnCount(visCount);
            QStringList labels; labels<<"Name";
            for(const auto &dd : colDefs())
                if(m_colVisible[dd.id]&&dd.id!=FP_NAME) labels<<dd.label;
            m_model->setHorizontalHeaderLabels(labels);
            auto *hdr=m_view->header();
            int vi=0;
            for(const auto &dd : colDefs()) {
                if(!m_colVisible[dd.id]) continue;
                hdr->setSectionResizeMode(vi, dd.id==FP_NAME ? QHeaderView::Stretch : QHeaderView::Fixed);
                if(dd.id!=FP_NAME) hdr->resizeSection(vi, dd.defaultWidth);
                vi++;
            }
            if(m_model->horizontalHeaderItem(0))
                m_model->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
            if(!m_currentPath.isEmpty()) populate(m_currentPath);
            emit columnsChanged(d.id, on);
        });
    }

    menu.exec(m_view->header()->mapToGlobal(pos));
}
void FilePane::openWithApp(const QString &desktopEntry, const QString &path) {
    KService::Ptr svc = KService::serviceByDesktopName(desktopEntry);
    if (!svc) return;
    QString exec = svc->exec();
    exec.replace("%f", "\"" + path + "\"");
    exec.replace("%F", "\"" + path + "\"");
    exec.replace("%u", "\"" + path + "\"");
    exec.replace("%U", "\"" + path + "\"");
    exec.replace("%i", "").replace("%c", svc->name()).replace("%k", "");
    QProcess::startDetached("sh", {"-c", exec.trimmed()});
}

void FilePane::showContextMenu(const QPoint &pos) {
    QModelIndex proxyIndex = m_view->indexAt(pos);
    QModelIndex index = m_proxy->mapToSource(proxyIndex);
    bool hasItem = index.isValid();
    QString path;
    if (hasItem) {
        auto *it = m_model->item(index.row(), 0);
        path = it ? it->data(Qt::UserRole).toString() : m_currentPath;
    } else {
        path = m_currentPath;
    }
    // KIO-Modus: QUrl direkt, keine lokalen QFileInfo
    if (m_kioMode || (!path.startsWith("/") && path.contains(":/"))) {
        QUrl kioUrl(path);
        QMenu menu(this);
        fp_applyMenuShadow(&menu);
        menu.setStyleSheet(menuStyle());
        if (hasItem) {
            menu.addAction(QIcon::fromTheme("document-open"), tr("Öffnen"), this,
                [this, path]() { setRootPath(path); });
            menu.addSeparator();
            menu.addAction(QIcon::fromTheme("edit-copy"), tr("Adresse kopieren"), this,
                [path]() { QGuiApplication::clipboard()->setText(path); });
        } else {
            menu.addAction(QIcon::fromTheme("utilities-terminal"), tr("Terminal hier öffnen"), this,
                [this]() { sc_openTerminal(m_currentPath); });
        }
        menu.exec(m_view->viewport()->mapToGlobal(pos));
        return;
    }
    QFileInfo info(path);
    bool isDir      = info.isDir();
    QString dirPath = info.dir().absolutePath(); // immer Elternverzeichnis, nicht das Item selbst
    QUrl    itemUrl = QUrl::fromLocalFile(path);

    QMenu menu(this);
    fp_applyMenuShadow(&menu);
    menu.setStyleSheet(menuStyle());

    // ── MIME-Daten für Öffnen-mit ─────────────────────────────────────────
    QMimeDatabase mdb;
    KService::List apps;
    bool isArchive = false;
    if (hasItem) {
        QMimeType mime = isDir ? mdb.mimeTypeForName("inode/directory")
                               : mdb.mimeTypeForFile(path);
        apps = KApplicationTrader::queryByMimeType(mime.name());
        const QString mt = mime.name();
        isArchive = mt.contains("zip") || mt.contains("tar") || mt.contains("rar")
                 || mt.contains("7z") || mt.contains("gzip") || mt.contains("bzip")
                 || mt.contains("xz") || mt.contains("archive") || mt.contains("compressed");
    }

    // ── 1. Öffnen mit ─────────────────────────────────────────────────────
    if (hasItem) {
        auto *openWithMenu = menu.addMenu(QIcon::fromTheme("document-open"), tr("Öffnen mit"));
        openWithMenu->setStyleSheet(menuStyle());
        // Standard-Aktion: direkt öffnen (erstes Item)
        openWithMenu->addAction(QIcon::fromTheme("document-open"), tr("Standard"), this,
            [path, isDir, this]() {
                if (isDir) setRootPath(path);
                else QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });
        openWithMenu->addSeparator();
        for (const KService::Ptr &svc : apps) {
            if (svc->noDisplay()) continue;
            QString desktop = svc->desktopEntryName();
            openWithMenu->addAction(QIcon::fromTheme(svc->icon()), svc->name(), this,
                [this, desktop, path]() { openWithApp(desktop, path); });
        }
        openWithMenu->addSeparator();
        openWithMenu->addAction(QIcon::fromTheme("application-x-executable"),
            tr("Andere Anwendung …"), this, [path]() {
                QProcess::startDetached("kioclient6",
                    {"openWith", QUrl::fromLocalFile(path).toString()});
            });
        menu.addSeparator();
    }

    // ── 2. Neu ────────────────────────────────────────────────────────────
    auto *newMenu = menu.addMenu(QIcon::fromTheme("folder-new"), tr("Neu"));
    newMenu->setStyleSheet(menuStyle());
    newMenu->addAction(QIcon::fromTheme("folder-new"), tr("Ordner …"), this,
        [this, dirPath]() {
            bool ok = true; QString name = fp_getText(this, tr("Neuer Ordner"), tr("Name:"), QString()); if (name.isNull()) ok = false;
            if (ok && !name.isEmpty()) QDir(dirPath).mkdir(name);
        });
    newMenu->addAction(QIcon::fromTheme("text-plain"), tr("Textdatei …"), this,
        [this, dirPath]() {
            bool ok = true; QString name = fp_getText(this, tr("Neue Textdatei"), tr("Name:"), tr("Neue Datei.txt")); if (name.isNull()) ok = false;
            if (ok && !name.isEmpty()) { QFile f(dirPath + "/" + name); if (f.open(QIODevice::WriteOnly)) f.close(); }
        });
    newMenu->addAction(QIcon::fromTheme("text-html"), tr("HTML-Datei …"), this,
        [this, dirPath]() {
            bool ok = true; QString name = fp_getText(this, tr("Neue HTML-Datei"), tr("Name:"), tr("index.html")); if (name.isNull()) ok = false;
            if (!ok || name.isEmpty()) return;
            QFile f(dirPath + "/" + name);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                f.write("<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\"><title></title></head>\n<body>\n\n</body>\n</html>\n");
        });
    newMenu->addAction(QIcon::fromTheme("document-new"), tr("Leere Datei …"), this,
        [this, dirPath]() {
            bool ok = true; QString name = fp_getText(this, tr("Leere Datei"), tr("Name:"), tr("Neue Datei")); if (name.isNull()) ok = false;
            if (ok && !name.isEmpty()) { QFile f(dirPath + "/" + name); if (f.open(QIODevice::WriteOnly)) f.close(); }
        });
    newMenu->addSeparator();
    newMenu->addAction(QIcon::fromTheme("inode-symlink"), tr("Verknüpfung zu Datei oder Ordner …"), this,
        [this, dirPath]() {
            QString target = QFileDialog::getExistingDirectory(this, tr("Ziel wählen"), dirPath);
            if (target.isEmpty())
                target = QFileDialog::getOpenFileName(this, tr("Ziel wählen"), dirPath);
            if (target.isEmpty()) return;
            bool ok = true; QString name = fp_getText(this, tr("Verknüpfungsname"), tr("Name:"), QFileInfo(target).fileName()); if (name.isNull()) ok = false;
            if (ok && !name.isEmpty()) QFile::link(target, dirPath + "/" + name);
        });

    // ── 3. Umbenennen ─────────────────────────────────────────────────────
    if (hasItem) {
        menu.addAction(QIcon::fromTheme("edit-rename"), tr("Umbenennen …"), this,
            [this, path, itemUrl, dirPath]() {
                bool ok;
                QString newName = fp_getText(this, tr("Umbenennen"), tr("Neuer Name:"), QFileInfo(path).fileName());
                if (ok && !newName.isEmpty() && newName != QFileInfo(path).fileName()) {
                    QUrl dest = QUrl::fromLocalFile(dirPath + "/" + newName);
                    auto *job = KIO::moveAs(itemUrl, dest, KIO::HideProgressInfo);
                    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
                }
            });
    }

    menu.addSeparator();

    // ── 4. In den Papierkorb / Löschen (Shift) ───────────────────────────
    if (hasItem) {
        auto *removeAct = new QAction(&menu);
        auto setTrash = [removeAct]() {
            removeAct->setText(QObject::tr("In den Papierkorb verschieben"));
            removeAct->setIcon(QIcon::fromTheme("edit-delete"));
        };
        auto setDelete = [removeAct]() {
            removeAct->setText(QObject::tr("Unwiderruflich löschen"));
            removeAct->setIcon(QIcon::fromTheme("edit-delete-shred"));
        };
        if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) setDelete();
        else setTrash();

        struct ShiftFilter : public QObject {
            std::function<void()> onPress, onRelease;
            ShiftFilter(QObject *p, std::function<void()> pr, std::function<void()> re)
                : QObject(p), onPress(pr), onRelease(re) {}
            bool eventFilter(QObject *, QEvent *e) override {
                if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
                    auto *ke = static_cast<QKeyEvent *>(e);
                    if (ke->key() == Qt::Key_Shift) {
                        if (e->type() == QEvent::KeyPress) onPress();
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
                shift ? KIO::AskUserActionInterface::Delete : KIO::AskUserActionInterface::Trash,
                KIO::AskUserActionInterface::DefaultConfirmation, this);
            job->start();
        });
        menu.addAction(removeAct);
        menu.addSeparator();
    }


    // ── 5. Senden an ──────────────────────────────────────────────────────
    if (hasItem) {
        auto *sendMenu = menu.addMenu(QIcon::fromTheme("document-send"), tr("Senden an"));
        sendMenu->setStyleSheet(menuStyle());

        sendMenu->addAction(QIcon::fromTheme("user-desktop"), tr("Desktop (Verknüpfung erstellen)"),
            this, [path]() {
                const QString desk = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                QFile::link(path, desk + "/" + QFileInfo(path).fileName());
            });

        sendMenu->addAction(QIcon::fromTheme("mail-send"), tr("E-Mail-Empfänger"),
            this, [path]() {
                QUrl mail(QString("mailto:?subject=%1&attachment=%2")
                    .arg(QFileInfo(path).fileName(),
                         QUrl::fromLocalFile(path).toString()));
                QDesktopServices::openUrl(mail);
            });

        sendMenu->addAction(QIcon::fromTheme("application-zip"), tr("ZIP-komprimierter Ordner"),
            this, [path, dirPath]() {
                QProcess::startDetached("ark", {"--batch", "--add-to",
                    dirPath + "/" + QFileInfo(path).fileName() + ".zip", path});
            });

        sendMenu->addAction(QIcon::fromTheme("bluetooth"), tr("Bluetooth-Gerät"),
            this, [path]() {
                QProcess::startDetached("bluedevil-sendfile",
                    {"-u", QUrl::fromLocalFile(path).toString()});
            });
    }

    // ── 5. Bearbeiten (Ausschneiden/Kopieren/Einfügen/Duplizieren) ───────
    {
        auto *editMenu = menu.addMenu(QIcon::fromTheme("edit-copy"), tr("Bearbeiten"));
        editMenu->setStyleSheet(menuStyle());

        if (hasItem) {
            editMenu->addAction(QIcon::fromTheme("edit-cut"), tr("Ausschneiden"), this,
                [itemUrl]() {
                    auto *mime = new QMimeData();
                    mime->setUrls({itemUrl});
                    mime->setData("x-kde-cut-selection", "1");
                    QGuiApplication::clipboard()->setMimeData(mime);
                });
            editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Kopieren"), this,
                [itemUrl]() {
                    auto *mime = new QMimeData();
                    mime->setUrls({itemUrl});
                    QGuiApplication::clipboard()->setMimeData(mime);
                });
        }

        const QMimeData *clip = QGuiApplication::clipboard()->mimeData();
        bool canPaste = clip && clip->hasUrls();
        auto *pasteAct = editMenu->addAction(QIcon::fromTheme("edit-paste"), tr("Einfügen"), this,
            [this, dirPath, clip]() {
                if (!clip || !clip->hasUrls()) return;
                bool isCut = clip->data("x-kde-cut-selection") == "1";
                QList<QUrl> urls = clip->urls();
                if (isCut)
                    KIO::move(urls, QUrl::fromLocalFile(dirPath), KIO::HideProgressInfo)
                        ->uiDelegate()->setAutoErrorHandlingEnabled(true);
                else
                    KIO::copy(urls, QUrl::fromLocalFile(dirPath), KIO::HideProgressInfo)
                        ->uiDelegate()->setAutoErrorHandlingEnabled(true);
            });
        pasteAct->setEnabled(canPaste);

        if (hasItem) {
            editMenu->addSeparator();
            editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Adresse kopieren"), this,
                [path]() { QGuiApplication::clipboard()->setText(path); });
            editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Hier duplizieren"), this,
                [this, path, dirPath, itemUrl]() {
                    QString baseName = QFileInfo(path).completeBaseName();
                    QString suffix   = QFileInfo(path).suffix();
                    QString copyName = suffix.isEmpty()
                        ? baseName + tr(" (Kopie)")
                        : baseName + tr(" (Kopie).") + suffix;
                    QUrl dest = QUrl::fromLocalFile(dirPath + "/" + copyName);
                    KIO::copy({itemUrl}, dest, KIO::HideProgressInfo)
                        ->uiDelegate()->setAutoErrorHandlingEnabled(true);
                });
        }
    }

    menu.addSeparator();

    // ── 6. Komprimieren / Entpacken ───────────────────────────────────────
    if (hasItem) {
        const QString baseName = QFileInfo(path).fileName();
        const QUrl dirUrl = QUrl::fromLocalFile(dirPath);
        auto *compressMenu = menu.addMenu(QIcon::fromTheme("archive-insert"), tr("Komprimieren"));
        compressMenu->setStyleSheet(menuStyle());

        const QString tgzName   = KFileUtils::suggestName(dirUrl, baseName + ".tar.gz");
        const QString zipName   = KFileUtils::suggestName(dirUrl, baseName + ".zip");
        const QString tgzTarget = dirPath + "/" + tgzName;
        const QString zipTarget = dirPath + "/" + zipName;

        compressMenu->addAction(tr("Komprimieren nach '%1'").arg(tgzName), this, [path, tgzTarget]() {
            QProcess::startDetached("ark", {"--batch", "--add-to", tgzTarget, path});
        });
        compressMenu->addAction(tr("Komprimieren nach '%1'").arg(zipName), this, [path, zipTarget]() {
            QProcess::startDetached("ark", {"--batch", "--add-to", zipTarget, path});
        });
        compressMenu->addAction(tr("Komprimieren nach ..."), this, [path]() {
            QProcess::startDetached("ark", {"--add", "--changetofirstpath", "--dialog", path});
        });

        if (isArchive) {
            auto *extractMenu = menu.addMenu(QIcon::fromTheme("archive-extract"), tr("Entpacken"));
            extractMenu->setStyleSheet(menuStyle());
            extractMenu->addAction(tr("Hierher entpacken"), this, [path]() {
                QProcess::startDetached("ark", {"--batch", "--autodestination", "--autosubfolder", path});
            });
            extractMenu->addAction(tr("Entpacken nach ..."), this, [path]() {
                QProcess::startDetached("ark", {"--extract", path});
            });
        }
    }

    // ── 7. Aktionen ───────────────────────────────────────────────────────
    auto *actMenu = menu.addMenu(QIcon::fromTheme("system-run"), tr("Aktionen"));
    actMenu->setStyleSheet(menuStyle());
    actMenu->addAction(QIcon::fromTheme("utilities-terminal"),
        tr("Terminal hier öffnen"), this, [dirPath]() { sc_openTerminal(dirPath); });
    if (hasItem) {
        actMenu->addSeparator();
        actMenu->addAction(QIcon::fromTheme("document-encrypt"),
            tr("Datei verschlüsseln …"), this, [path]() {
                QProcess::startDetached("kleopatra", {"--encrypt", path});
            });
        actMenu->addAction(QIcon::fromTheme("document-sign"),
            tr("Datei signieren & verschlüsseln …"), this, [path]() {
                QProcess::startDetached("kleopatra", {"--sign-encrypt", path});
            });
        actMenu->addAction(QIcon::fromTheme("document-sign"),
            tr("Datei signieren …"), this, [path]() {
                QProcess::startDetached("kleopatra", {"--sign", path});
            });
        actMenu->addSeparator();
        actMenu->addAction(QIcon::fromTheme("folder-new"),
            tr("In neuen Ordner verschieben …"), this, [this, itemUrl, dirPath]() {
                bool ok;
                QString folderName = fp_getText(this,
                    tr("In neuen Ordner verschieben"), tr("Ordnername:"),
                    QFileInfo(itemUrl.toLocalFile()).baseName());
                if (folderName.isEmpty()) return;
                QString dest = dirPath + "/" + folderName;
                QDir().mkdir(dest);
                KIO::move({itemUrl}, QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
            });
    }

    // ── 8. Tag ────────────────────────────────────────────────────────────
    if (hasItem) {
        auto *tagMenu = menu.addMenu(QIcon::fromTheme("tag"), tr("Tag"));
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
            tagMenu->addAction(QIcon::fromTheme("edit-clear"), tr("Tag entfernen"), this,
                [path]() { TagManager::instance().clearFileTag(path); });
        }
    }

    menu.addSeparator();

    // ── Eigenschaften ─────────────────────────────────────────────────────
    if (hasItem) {
        menu.addAction(QIcon::fromTheme("document-properties"), tr("Eigenschaften"), this,
            [this, path]() {
                KPropertiesDialog *dlg = new KPropertiesDialog(
                    QUrl::fromLocalFile(path), this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            });
    }

    menu.exec(m_view->mapToGlobal(pos));
}
