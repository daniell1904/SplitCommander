#include "propertiesdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include <QDialogButtonBox>
#include <QStorageInfo>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QPainter>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QPixmap>
#include <QIcon>
#include <QDateTime>
#include <cmath>

static QString formatBytes(qint64 b) {
    if (b < 1024) return QString("%1 B").arg(b);
    if (b < 1024*1024) return QString("%1 KiB (%2)").arg(b/1024.0,0,'f',1).arg(b);
    if (b < 1024LL*1024*1024) return QString("%1 MiB (%2)").arg(b/1048576.0,0,'f',1).arg(b);
    return QString("%1 GiB (%2)").arg(b/1073741824.0,0,'f',1).arg(b);
}

// Donut-Chart Pixmap
static QPixmap donutChart(qint64 used, qint64 total, int size = 120) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    int margin = 8;
    QRectF rect(margin, margin, size-2*margin, size-2*margin);
    double pct = total > 0 ? (double)used / total : 0;
    int usedAngle = qRound(pct * 360 * 16);
    int freeAngle = 360*16 - usedAngle;
    // Belegt: blau
    p.setBrush(QColor("#5e81ac"));
    p.setPen(Qt::NoPen);
    p.drawPie(rect, 90*16, -usedAngle);
    // Frei: dunkelgrau
    p.setBrush(QColor("#3b4252"));
    p.drawPie(rect, 90*16 - usedAngle, -freeAngle);
    // Loch
    p.setBrush(QColor("#1e2330"));
    p.drawEllipse(rect.adjusted(20,20,-20,-20));
    // Prozent Text
    p.setPen(QColor("#ccd4e8"));
    QFont f; f.setPixelSize(16); f.setBold(true);
    p.setFont(f);
    p.drawText(rect.adjusted(20,20,-20,-20), Qt::AlignCenter,
               QString("%1%").arg(qRound(pct*100)));
    return px;
}

PropertiesDialog::PropertiesDialog(const QString &path, QWidget *parent)
    : QDialog(parent)
{
    QFileInfo fi(path);
    QStorageInfo si(path);
    setWindowTitle(tr("Eigenschaften — ") + fi.fileName());
    setMinimumWidth(440);
    setStyleSheet(
        "QDialog { background:#1e2330; color:#ccd4e8; }"
        "QTabWidget::pane { border:1px solid #2c3245; background:#1e2330; }"
        "QTabBar::tab { background:#23283a; color:#8a93a8; padding:6px 16px; border:1px solid #2c3245; }"
        "QTabBar::tab:selected { background:#1e2330; color:#ccd4e8; border-bottom:none; }"
        "QLabel { color:#ccd4e8; }"
        "QLabel#key { color:#8a93a8; }"
        "QFrame#sep { background:#2c3245; }"
    );

    auto *mainL = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    // ── Tab 1: Allgemein ─────────────────────────
    auto *genWidget = new QWidget();
    auto *genL = new QVBoxLayout(genWidget);
    genL->setSpacing(12);

    // Icon + Name
    auto *topRow = new QHBoxLayout();
    auto *iconLbl = new QLabel();
    QIcon icon = path.startsWith("/run/media/") || path.startsWith("/media/")
        ? QIcon::fromTheme("drive-removable-media")
        : QIcon::fromTheme("drive-harddisk");
    iconLbl->setPixmap(icon.pixmap(48,48));
    topRow->addWidget(iconLbl);
    auto *nameLbl = new QLabel("<b>" + fi.fileName() + "</b>");
    nameLbl->setStyleSheet("font-size:16px; color:#ccd4e8;");
    topRow->addWidget(nameLbl, 1);
    genL->addLayout(topRow);

    auto *sep1 = new QFrame(); sep1->setObjectName("sep");
    sep1->setFrameShape(QFrame::HLine); sep1->setFixedHeight(1);
    genL->addWidget(sep1);

    // Grid mit Infos
    auto *grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(6);
    int row = 0;
    auto addRow = [&](const QString &key, const QString &val) {
        auto *k = new QLabel(key + ":"); k->setObjectName("key");
        auto *v = new QLabel(val); v->setWordWrap(true);
        grid->addWidget(k, row, 0, Qt::AlignTop | Qt::AlignRight);
        grid->addWidget(v, row, 1);
        ++row;
    };

    addRow(tr("Typ"), tr("Laufwerk/Ordner"));
    addRow(tr("Adresse"), path);

    // Block-Device via findmnt
    QProcess proc;
    proc.start("findmnt", {"-n", "-o", "SOURCE", path});
    proc.waitForFinished(1000);
    QString source = proc.readAllStandardOutput().trimmed();
    if (!source.isEmpty()) addRow(tr("Eingehängt von"), source);

    if (si.isValid()) {
        addRow(tr("Dateisystem"), QString(si.fileSystemType()));
        addRow(tr("Einhängepunkt"), si.rootPath());

        auto *sep2 = new QFrame(); sep2->setObjectName("sep");
        sep2->setFrameShape(QFrame::HLine); sep2->setFixedHeight(1);
        grid->addWidget(sep2, row++, 0, 1, 2);

        qint64 total = si.bytesTotal();
        qint64 free  = si.bytesFree();
        qint64 used  = total - free;
        addRow(tr("Gesamt"),  formatBytes(total));
        addRow(tr("Belegt"),  formatBytes(used));
        addRow(tr("Frei"),    formatBytes(free));

        // Donut + Legende
        auto *chartRow = new QHBoxLayout();
        auto *donut = new QLabel();
        donut->setPixmap(donutChart(used, total));
        chartRow->addWidget(donut);
        auto *legend = new QVBoxLayout();
        auto mkDot = [](const QString &color, const QString &text) {
            auto *h = new QHBoxLayout();
            auto *dot = new QLabel();
            dot->setFixedSize(12,12);
            dot->setStyleSheet(QString("background:%1;border-radius:2px;").arg(color));
            h->addWidget(dot);
            h->addWidget(new QLabel(text));
            h->addStretch();
            return h;
        };
        int pct = total > 0 ? qRound((double)used/total*100) : 0;
        legend->addLayout(mkDot("#5e81ac",
            tr("Belegt: %1 GB (%2%)").arg(used/1073741824.0,0,'f',1).arg(pct)));
        legend->addLayout(mkDot("#3b4252",
            tr("Frei: %1 GB").arg(free/1073741824.0,0,'f',1)));
        legend->addStretch();
        chartRow->addLayout(legend, 1);
        grid->addLayout(chartRow, row++, 0, 1, 2);
    }

    genL->addLayout(grid);
    genL->addStretch();
    tabs->addTab(genWidget, tr("Allgemein"));

    // ── Tab 2: Berechtigungen ─────────────────────
    auto *permWidget = new QWidget();
    auto *permVL = new QVBoxLayout(permWidget);
    permVL->setSpacing(12);

    // Zugangsberechtigungen
    auto *zugTitle = new QLabel(tr("<b>Zugangsberechtigungen</b>"));
    zugTitle->setAlignment(Qt::AlignCenter);
    permVL->addWidget(zugTitle);

    QFile::Permissions perms = fi.permissions();
    auto permToText = [](bool r, bool w, bool x) -> QString {
        if (r && w && x) return QObject::tr("Anzeige, Änderung & Ausführung möglich");
        if (r && w)      return QObject::tr("Anzeige & Änderung des Inhalts möglich");
        if (r && x)      return QObject::tr("Anzeige & Ausführung möglich");
        if (r)           return QObject::tr("Nur Anzeige möglich");
        if (w)           return QObject::tr("Nur Änderung möglich");
        return QObject::tr("Kein Zugriff");
    };

    auto *zugGrid = new QGridLayout();
    zugGrid->setColumnStretch(1,1);
    zugGrid->setHorizontalSpacing(16);
    zugGrid->setVerticalSpacing(8);

    QStringList permOptions = {
        tr("Kein Zugriff"),
        tr("Nur Anzeige möglich"),
        tr("Anzeige & Ausführung möglich"),
        tr("Anzeige & Änderung des Inhalts möglich"),
        tr("Anzeige, Änderung & Ausführung möglich"),
    };
    auto permToIndex = [](bool r, bool w, bool x) -> int {
        if (r && w && x) return 4;
        if (r && w)      return 3;
        if (r && x)      return 2;
        if (r)           return 1;
        return 0;
    };
    QString comboStyle =
        "QComboBox { background:#23283a; border:1px solid #2c3245; border-radius:4px;"
        " padding:4px 8px; color:#ccd4e8; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:#23283a; color:#ccd4e8; border:1px solid #2c3245; }";

    QComboBox *ownerCombo = new QComboBox();
    QComboBox *groupCombo = new QComboBox();
    QComboBox *otherCombo = new QComboBox();
    for (auto *cb : {ownerCombo, groupCombo, otherCombo}) {
        cb->addItems(permOptions);
        cb->setStyleSheet(comboStyle);
    }
    ownerCombo->setCurrentIndex(permToIndex(
        perms & QFile::ReadOwner, perms & QFile::WriteOwner, perms & QFile::ExeOwner));
    groupCombo->setCurrentIndex(permToIndex(
        perms & QFile::ReadGroup, perms & QFile::WriteGroup, perms & QFile::ExeGroup));
    otherCombo->setCurrentIndex(permToIndex(
        perms & QFile::ReadOther, perms & QFile::WriteOther, perms & QFile::ExeOther));

    auto addDropRow = [&](const QString &label, QComboBox *combo, int row) {
        auto *lbl = new QLabel(label + ":"); lbl->setObjectName("key");
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        zugGrid->addWidget(lbl,   row, 0);
        zugGrid->addWidget(combo, row, 1);
    };
    addDropRow(tr("Eigentümer"), ownerCombo, 0);
    addDropRow(tr("Gruppe"),     groupCombo, 1);
    addDropRow(tr("Sonstige"),   otherCombo, 2);

    permVL->addLayout(zugGrid);

    // Erweiterte Berechtigungen Button
    auto *advBtn = new QPushButton(tr("Erweiterte Berechtigungen"));
    advBtn->setStyleSheet(
        "QPushButton { background:#23283a; color:#ccd4e8; border:1px solid #2c3245;"
        " border-radius:4px; padding:4px 12px; }"
        "QPushButton:hover { background:#3b4252; }");
    connect(advBtn, &QPushButton::clicked, this, [this, path, ownerCombo, groupCombo, otherCombo]() {
        // chmod Maske berechnen
        auto idxToOctal = [](int idx) -> int {
            switch(idx) {
            case 1: return 4; // r--
            case 2: return 5; // r-x
            case 3: return 6; // rw-
            case 4: return 7; // rwx
            default: return 0;
            }
        };
        int mode = idxToOctal(ownerCombo->currentIndex()) * 64
                 + idxToOctal(groupCombo->currentIndex()) * 8
                 + idxToOctal(otherCombo->currentIndex());
        QString octal = QString("%1").arg(mode, 3, 8, QChar('0'));
        QProcess::startDetached("chmod", {octal, path});
    });
    permVL->addWidget(advBtn, 0, Qt::AlignCenter);

    // Erweiterte Berechtigungen (Sticky-Bit)
    auto *stickyRow = new QHBoxLayout();
    auto *stickyChk = new QLabel();
    stickyChk->setFixedSize(14,14);
    bool sticky = (perms & QFile::ExeOwner) && fi.isDir();
    stickyChk->setStyleSheet(sticky
        ? "background:#5e81ac; border:1px solid #4c72a0; border-radius:2px;"
        : "background:#23283a; border:1px solid #2c3245; border-radius:2px;");
    stickyRow->addWidget(stickyChk);
    stickyRow->addWidget(new QLabel(tr("Nur der Eigentümer kann Inhalte löschen oder umbenennen")));
    stickyRow->addStretch();
    permVL->addLayout(stickyRow);

    // Trennlinie
    auto *permSep = new QFrame(); permSep->setObjectName("sep");
    permSep->setFrameShape(QFrame::HLine); permSep->setFixedHeight(1);
    permVL->addWidget(permSep);

    // Eigentümer-Bereich
    auto *ownTitle = new QLabel(tr("<b>Eigentümer</b>"));
    ownTitle->setAlignment(Qt::AlignCenter);
    permVL->addWidget(ownTitle);

    auto *ownGrid = new QGridLayout();
    ownGrid->setColumnStretch(1,1);
    ownGrid->setHorizontalSpacing(16);
    ownGrid->setVerticalSpacing(6);

    auto addOwnRow = [&](const QString &label, const QString &val, int row) {
        auto *lbl = new QLabel(label + ":"); lbl->setObjectName("key");
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *vl = new QLabel(val);
        ownGrid->addWidget(lbl, row, 0);
        ownGrid->addWidget(vl,  row, 1);
    };
    addOwnRow(tr("Benutzer"), fi.owner(),  0);
    addOwnRow(tr("Gruppe"),   fi.group(), 1);
    permVL->addLayout(ownGrid);

    permVL->addStretch();
    tabs->addTab(permWidget, tr("Berechtigungen"));

    // ── Tab 3: Details ─────────────────────────────
    auto *detWidget = new QWidget();
    auto *detL = new QGridLayout(detWidget);
    detL->setSpacing(8);
    detL->setColumnStretch(1,1);
    int dr = 0;
    auto addDet = [&](const QString &k, const QString &v) {
        auto *kl = new QLabel(k+":"); kl->setObjectName("key");
        auto *vl = new QLabel(v); vl->setWordWrap(true);
        detL->addWidget(kl, dr, 0, Qt::AlignRight);
        detL->addWidget(vl, dr++, 1);
    };
    addDet(tr("Erstellt"),       fi.birthTime().toString("dddd, d. MMMM yyyy HH:mm:ss"));
    addDet(tr("Geändert"),       fi.lastModified().toString("dddd, d. MMMM yyyy HH:mm:ss"));
    addDet(tr("Letzter Zugriff"),fi.lastRead().toString("dddd, d. MMMM yyyy HH:mm:ss"));
    if (si.isValid()) {
        addDet(tr("Blockgröße"), QString("%1 B").arg(si.blockSize()));
    }
    auto *detSpacer = new QWidget(); detSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    detL->addWidget(detSpacer, dr, 0, 1, 2);
    tabs->addTab(detWidget, tr("Details"));

    mainL->addWidget(tabs);
    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->setStyleSheet("QPushButton { background:#3b4252; color:#ccd4e8; border:1px solid #2c3245; padding:4px 16px; border-radius:4px; }"
                          "QPushButton:hover { background:#4c566a; }");
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    mainL->addWidget(btnBox);
}
