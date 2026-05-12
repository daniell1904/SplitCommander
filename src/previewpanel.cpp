#include "previewpanel.h"
#include "thememanager.h"
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QMimeDatabase>
#include <QtConcurrent>
#include <QDateTime>
#include <QPainter>
#include <QUrl>
#include <KFileItem>

static const QStringList IMG_EXT  = {"jpg","jpeg","png","gif","bmp","webp","tiff","tif","ico","ppm","pgm"};
static const QStringList TEXT_EXT = {"txt","md","cpp","c","h","py","sh","bash","json",
                                     "yaml","yml","toml","conf","ini","log","xml","html",
                                     "css","js","ts","cmake","rs","go","java","php","rb"};

static QString fmtSize(qint64 s) {
    if (s < 1024)             return QString("%1 B").arg(s);
    if (s < 1024*1024)        return QString("%1 KB").arg(s/1024.0,0,'f',1);
    if (s < 1024LL*1024*1024) return QString("%1 MB").arg(s/1048576.0,0,'f',1);
    return QString("%1 GB").arg(s/1073741824.0,0,'f',2);
}

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background:transparent;");

    auto *mainL = new QHBoxLayout(this);
    mainL->setContentsMargins(8, 6, 8, 6);
    mainL->setSpacing(10);

    m_imgLabel = new QLabel(this);
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet(
        QString("background:%1;border-radius:5px;border:1px solid %2;")
        .arg(TM().colors().bgDeep, TM().colors().borderAlt));
    m_imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imgLabel->setMinimumWidth(160);
    m_imgLabel->setScaledContents(false);
    mainL->addWidget(m_imgLabel, 3);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_textEdit->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;font-family:monospace;"
                "font-size:9px;border:1px solid %3;border-radius:5px;}")
        .arg(TM().colors().bgDeep, TM().colors().textAccent, TM().colors().borderAlt));
    m_textEdit->hide();
    mainL->addWidget(m_textEdit, 3);

    auto *infoWidget = new QWidget(this);
    infoWidget->setFixedWidth(190);
    infoWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    infoWidget->setStyleSheet(
        QString("background:%1;border-radius:5px;border:1px solid %2;")
        .arg(TM().colors().bgDeep, TM().colors().borderAlt));

    auto *infoL = new QVBoxLayout(infoWidget);
    infoL->setContentsMargins(8, 8, 8, 8);
    infoL->setSpacing(0);

    m_metaLabel = new QLabel(infoWidget);
    m_metaLabel->setWordWrap(true);
    m_metaLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_metaLabel->setTextFormat(Qt::RichText);
    m_metaLabel->setStyleSheet(
        QString("color:%1;font-size:10px;background:transparent;border:none;")
        .arg(TM().colors().textPrimary));
    m_metaLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    infoL->addWidget(m_metaLabel);
    infoL->addStretch();

    mainL->addWidget(infoWidget, 0);

    connect(&m_watcher, &QFutureWatcher<PreviewResult>::finished, this, &PreviewPanel::onPreviewReady);
}

void PreviewPanel::clear()
{
    m_imgLabel->clear();
    m_imgLabel->show();
    m_storedPixmap = QPixmap();
    m_metaLabel->clear();
    m_textEdit->clear();
    m_textEdit->hide();
}

static QString row(const QString &key, const QString &val)
{
    return QString(
        "<tr>"
        "<td style='color:%3;padding:2px 8px 2px 0;white-space:nowrap;vertical-align:top'>%1</td>"
        "<td style='color:%4;padding:2px 0;vertical-align:top'>%2</td>"
        "</tr>")
        .arg(key, val.toHtmlEscaped(),
             TM().colors().textMuted, TM().colors().textPrimary);
}

// --- loadPreviewData --- (Statische Hilfsfunktion für Hintergrund-Loading)
static PreviewResult loadPreviewData(const QString &path) {
    PreviewResult res;
    res.path = path;
    const QUrl url(path);
    const bool isLocal = url.isLocalFile() || url.scheme().isEmpty();

    if (isLocal) {
        QFileInfo fi(url.isLocalFile() ? url.toLocalFile() : path);
        if (!fi.exists()) return res;
        const QString ext = fi.suffix().toLower();

        // --- Bild-Handling ---
        if (IMG_EXT.contains(ext) && !fi.isDir()) {
            QImageReader reader(url.isLocalFile() ? url.toLocalFile() : path);
            reader.setAutoTransform(true);
            res.image = reader.read();

            if (!res.image.isNull()) {
                QString depth;
                switch (res.image.format()) {
                    case QImage::Format_Grayscale8:  depth = QCoreApplication::translate("SplitCommander", "Graustufen 8-bit");  break;
                    case QImage::Format_Grayscale16: depth = QCoreApplication::translate("SplitCommander", "Graustufen 16-bit"); break;
                    case QImage::Format_RGB32:
                    case QImage::Format_RGB888:      depth = QCoreApplication::translate("SplitCommander", "RGB 24-bit");        break;
                    case QImage::Format_ARGB32:
                    case QImage::Format_RGBA8888:    depth = QCoreApplication::translate("SplitCommander", "RGBA 32-bit");       break;
                    default: depth = QCoreApplication::translate("SplitCommander", "%1-bit").arg(res.image.depth());   break;
                }

                res.meta = "<table cellspacing='0' cellpadding='0'>";
                res.meta += row(QCoreApplication::translate("SplitCommander", "Datei"),    fi.fileName());
                res.meta += row(QCoreApplication::translate("SplitCommander", "Typ"),      fi.suffix().toUpper());
                res.meta += row(QCoreApplication::translate("SplitCommander", "Größe"),    fmtSize(fi.size()));
                res.meta += row(QCoreApplication::translate("SplitCommander", "Aufl."),    QString("%1×%2").arg(res.image.width()).arg(res.image.height()));
                res.meta += row(QCoreApplication::translate("SplitCommander", "Format"),   QString(reader.format()).toUpper());
                res.meta += row(QCoreApplication::translate("SplitCommander", "Tiefe"),    depth);
                if (res.image.dotsPerMeterX() > 0) {
                    res.meta += row(QCoreApplication::translate("SplitCommander", "DPI"), QString::number(qRound(res.image.dotsPerMeterX() * 0.0254)));
                    double wcm = res.image.width()  / (res.image.dotsPerMeterX() / 100.0);
                    double hcm = res.image.height() / (res.image.dotsPerMeterY() / 100.0);
                    res.meta += row(QCoreApplication::translate("SplitCommander", "Maße"), QString("%1×%2 cm").arg(wcm,0,'f',1).arg(hcm,0,'f',1));
                }
                res.meta += row(QCoreApplication::translate("SplitCommander", "Geändert"), fi.lastModified().toString("dd.MM.yy HH:mm"));
                res.meta += "</table>";
                return res;
            }
        }

        // --- Text-Handling ---
        if (TEXT_EXT.contains(ext) && !fi.isDir()) {
            res.isText = true;
            QFile f(url.isLocalFile() ? url.toLocalFile() : path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream stream(&f);
                QStringList lines;
                int i = 0;
                while (!stream.atEnd() && i < 100) { lines << stream.readLine(); ++i; }
                res.text = lines.join("\n");
            }
            QMimeDatabase mdb;
            const QString mime = mdb.mimeTypeForFile(url.isLocalFile() ? url.toLocalFile() : path).comment();
            res.meta = "<table cellspacing='0' cellpadding='0'>";
            res.meta += row(QCoreApplication::translate("SplitCommander", "Datei"),    fi.fileName());
            res.meta += row(QCoreApplication::translate("SplitCommander", "Typ"),      mime.isEmpty() ? fi.suffix().toUpper() : mime);
            res.meta += row(QCoreApplication::translate("SplitCommander", "Größe"),    fmtSize(fi.size()));
            res.meta += row(QCoreApplication::translate("SplitCommander", "Geändert"), fi.lastModified().toString("dd.MM.yy HH:mm"));
            res.meta += row(QCoreApplication::translate("SplitCommander", "Erstellt"), fi.birthTime().toString("dd.MM.yy HH:mm"));
            res.meta += "</table>";
            return res;
        }

        // --- Allgemein (Lokal) ---
        QMimeDatabase mdb;
        const QString mime = mdb.mimeTypeForFile(url.isLocalFile() ? url.toLocalFile() : path).comment();
        res.meta = "<table cellspacing='0' cellpadding='0'>";
        res.meta += row(QCoreApplication::translate("SplitCommander", "Name"),     fi.fileName());
        res.meta += row(QCoreApplication::translate("SplitCommander", "Typ"),      fi.isDir() ? QCoreApplication::translate("SplitCommander", "Ordner") : (mime.isEmpty() ? fi.suffix().toUpper() : mime));
        if (!fi.isDir())
            res.meta += row(QCoreApplication::translate("SplitCommander", "Größe"), fmtSize(fi.size()));
        res.meta += row(QCoreApplication::translate("SplitCommander", "Geändert"), fi.lastModified().toString("dd.MM.yy HH:mm"));
        res.meta += row(QCoreApplication::translate("SplitCommander", "Erstellt"), fi.birthTime().toString("dd.MM.yy HH:mm"));
        if (fi.isSymLink())
            res.meta += row("→", fi.symLinkTarget());
        res.meta += "</table>";

    } else {
        // --- KIO / Remote Handling ---
        KFileItem item(url);
        QMimeDatabase mdb;
        const QString mime = mdb.mimeTypeForUrl(url).comment();
        
        res.meta = "<table cellspacing='0' cellpadding='0'>";
        res.meta += row(QCoreApplication::translate("SplitCommander", "Name"),     url.fileName().isEmpty() ? path : url.fileName());
        res.meta += row(QCoreApplication::translate("SplitCommander", "Typ"),      mime.isEmpty() ? url.scheme().toUpper() : mime);
        res.meta += row(QCoreApplication::translate("SplitCommander", "Protokoll"), url.scheme());
        res.meta += "</table>";
    }
    
    return res;
}


void PreviewPanel::showFile(const QString &path)
{
    // Aktuellen Ladevorgang ggf. ignorieren (wird durch neuen ersetzt)
    if (m_watcher.isRunning()) {
        // Wir können einen laufenden Thread nicht einfach killen, 
        // aber wir starten einen neuen. Der Watcher wird am Ende onPreviewReady()
        // für das neuste Ergebnis aufrufen.
    }
    
    m_watcher.setFuture(QtConcurrent::run(loadPreviewData, path));
}

void PreviewPanel::onPreviewReady()
{
    PreviewResult res = m_watcher.result();
    
    // UI aktualisieren
    clear();
    
    if (!res.image.isNull()) {
        m_storedPixmap = QPixmap::fromImage(res.image);
        scalePixmap();
        m_metaLabel->setText(res.meta);
        return;
    }

    if (res.isText) {
        m_imgLabel->hide();
        m_textEdit->show();
        m_textEdit->setPlainText(res.text);
        m_metaLabel->setText(res.meta);
        return;
    }

    // Standard-Icon Fall
    QMimeDatabase mdb;
    QFileInfo fi(res.path);
    const QIcon icon = QIcon::fromTheme(
        mdb.mimeTypeForFile(res.path).iconName(),
        QIcon::fromTheme(fi.isDir() ? "folder" : "text-x-generic"));
    if (!icon.isNull())
        m_imgLabel->setPixmap(icon.pixmap(80, 80));
    
    m_metaLabel->setText(res.meta);
}

void PreviewPanel::scalePixmap()
{
    if (m_storedPixmap.isNull() || !m_imgLabel->isVisible()) return;
    const int w = qMax(m_imgLabel->width(),  160);
    const int h = qMax(m_imgLabel->height(), 60);
    m_imgLabel->setPixmap(
        m_storedPixmap.scaled(QSize(w, h), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PreviewPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scalePixmap();
}
