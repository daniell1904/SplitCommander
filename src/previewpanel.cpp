#include "previewpanel.h"
#include "thememanager.h"
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QMimeDatabase>

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

void PreviewPanel::showFile(const QString &path)
{
    clear();
    QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    if (IMG_EXT.contains(ext) && !fi.isDir()) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();

        if (!img.isNull()) {
            m_storedPixmap = QPixmap::fromImage(img);
            // Bild wird in resizeEvent skaliert — hier nur Pixmap speichern
            // Falls das Label bereits eine Größe hat, sofort skalieren
            scalePixmap();

            QString depth;
            switch (img.format()) {
            case QImage::Format_Grayscale8:  depth = "Graustufen 8-bit";  break;
            case QImage::Format_Grayscale16: depth = "Graustufen 16-bit"; break;
            case QImage::Format_RGB32:
            case QImage::Format_RGB888:      depth = "RGB 24-bit";        break;
            case QImage::Format_ARGB32:
            case QImage::Format_RGBA8888:    depth = "RGBA 32-bit";       break;
            default: depth = QString("%1-bit").arg(img.depth());           break;
            }

            QString meta = "<table cellspacing='0' cellpadding='0'>";
            meta += row("Datei",    fi.fileName());
            meta += row("Typ",      fi.suffix().toUpper());
            meta += row("Größe",    fmtSize(fi.size()));
            meta += row("Aufl.",    QString("%1×%2").arg(img.width()).arg(img.height()));
            meta += row("Format",   QString(reader.format()).toUpper());
            meta += row("Tiefe",    depth);
            if (img.dotsPerMeterX() > 0) {
                meta += row("DPI", QString::number(qRound(img.dotsPerMeterX() * 0.0254)));
                double wcm = img.width()  / (img.dotsPerMeterX() / 100.0);
                double hcm = img.height() / (img.dotsPerMeterY() / 100.0);
                meta += row("Maße", QString("%1×%2 cm").arg(wcm,0,'f',1).arg(hcm,0,'f',1));
            }
            meta += row("Geändert", fi.lastModified().toString("dd.MM.yy HH:mm"));
            meta += "</table>";
            m_metaLabel->setText(meta);
            return;
        }
    }

    if (TEXT_EXT.contains(ext) && !fi.isDir()) {
        m_imgLabel->hide();
        m_textEdit->show();
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&f);
            QStringList lines;
            int i = 0;
            while (!stream.atEnd() && i < 100) { lines << stream.readLine(); ++i; }
            m_textEdit->setPlainText(lines.join("\n"));
        }
        QMimeDatabase mdb;
        const QString mime = mdb.mimeTypeForFile(path).comment();
        QString meta = "<table cellspacing='0' cellpadding='0'>";
        meta += row("Datei",    fi.fileName());
        meta += row("Typ",      mime.isEmpty() ? fi.suffix().toUpper() : mime);
        meta += row("Größe",    fmtSize(fi.size()));
        meta += row("Geändert", fi.lastModified().toString("dd.MM.yy HH:mm"));
        meta += row("Erstellt", fi.birthTime().toString("dd.MM.yy HH:mm"));
        meta += "</table>";
        m_metaLabel->setText(meta);
        return;
    }

    // Allgemein: Icon + Metadaten
    QMimeDatabase mdb;
    const QString mime = mdb.mimeTypeForFile(path).comment();
    const QIcon icon = QIcon::fromTheme(
        mdb.mimeTypeForFile(path).iconName(),
        QIcon::fromTheme(fi.isDir() ? "folder" : "text-x-generic"));
    if (!icon.isNull())
        m_imgLabel->setPixmap(icon.pixmap(80, 80));

    QString meta = "<table cellspacing='0' cellpadding='0'>";
    meta += row("Name",     fi.fileName());
    meta += row("Typ",      fi.isDir() ? "Ordner" : (mime.isEmpty() ? fi.suffix().toUpper() : mime));
    if (!fi.isDir())
        meta += row("Größe", fmtSize(fi.size()));
    meta += row("Geändert", fi.lastModified().toString("dd.MM.yy HH:mm"));
    meta += row("Erstellt", fi.birthTime().toString("dd.MM.yy HH:mm"));
    if (fi.isSymLink())
        meta += row("→", fi.symLinkTarget());
    meta += "</table>";
    m_metaLabel->setText(meta);
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
