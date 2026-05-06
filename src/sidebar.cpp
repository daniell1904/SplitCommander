// ─────────────────────────────────────────────────────────────────────────────
// sidebar.cpp — SplitCommander Sidebar
// ─────────────────────────────────────────────────────────────────────────────

#include "sidebar.h"
#include "propertiesdialog.h"
#include <KPropertiesDialog>
#include <QUrl>
#include "settingsdialog.h"
#include "thememanager.h"
#include "terminalutils.h"
#include <QTimer>

#include <KIO/CopyJob>
#include <KIO/ListJob>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QButtonGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <QApplication>

// ─────────────────────────────────────────────────────────────────────────────
// Hilfsfunktionen (file-scope)
// ─────────────────────────────────────────────────────────────────────────────

static void sc_applyMenuShadow(QMenu *menu)
{
    if (!menu) return;
    auto *shadow = new QGraphicsDropShadowEffect(menu);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 140));
    menu->setGraphicsEffect(shadow);
}

// Kontextmenü-Einträge für Favoriten/Gruppen-Einträge
static void sc_buildPlaceMenu(QMenu &menu, const QString &path, QWidget *parent,
    std::function<void()> removeAction,
    std::function<void(const QString &, const QString &)> editAction)
{
    if (removeAction)
        QObject::connect(
            menu.addAction(QIcon::fromTheme("list-remove"), QObject::tr("Aus Gruppe entfernen")),
            &QAction::triggered, removeAction);

    // Bearbeiten-Untermenü (wie Detailliste)
    auto *editMenu = menu.addMenu(QIcon::fromTheme("document-edit"), QObject::tr("Bearbeiten"));
    editMenu->setStyleSheet(menu.styleSheet());
    editMenu->addAction(QIcon::fromTheme("edit-cut"), QObject::tr("Ausschneiden"),
        [path]() {
            auto *mime = new QMimeData();
            mime->setUrls({QUrl::fromLocalFile(path)});
            mime->setData("x-special/gnome-copied-files", QByteArray("cut\n") + QUrl::fromLocalFile(path).toEncoded());
            QGuiApplication::clipboard()->setMimeData(mime);
        });
    editMenu->addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Kopieren"),
        [path]() {
            auto *mime = new QMimeData();
            mime->setUrls({QUrl::fromLocalFile(path)});
            QGuiApplication::clipboard()->setMimeData(mime);
        });
    editMenu->addAction(QIcon::fromTheme("edit-paste"), QObject::tr("Einfügen"),
        [path]() {
            const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
            if (!mime->hasUrls()) return;
            for (const QUrl &u : mime->urls()) {
                const QString src = u.toLocalFile();
                if (src.isEmpty()) continue;
                QFile::copy(src, path + "/" + QFileInfo(src).fileName());
            }
        });
    editMenu->addSeparator();
    editMenu->addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Adresse kopieren"),
        [path]() { QGuiApplication::clipboard()->setText(path); });
    editMenu->addAction(QIcon::fromTheme("edit-copy-path"), QObject::tr("Hier duplizieren"),
        [path]() {
            const QString newPath = path + " (Kopie)";
            QFile::copy(path, newPath);
        });

    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("utilities-terminal"), QObject::tr("In Terminal öffnen"),
        [path]() { sc_openTerminal(path); });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("edit-copy"), QObject::tr("Pfad kopieren"),
        [path]() { QGuiApplication::clipboard()->setText(path); });
    menu.addAction(QIcon::fromTheme("emblem-symbolic-link"), QObject::tr("Verknüpfung erstellen"),
        [path]() {
            QString d = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QFile f(d + "/" + QFileInfo(path).fileName() + ".desktop");
            if (f.open(QIODevice::WriteOnly)) {
                QTextStream s(&f);
                s << "[Desktop Entry]\nType=Link\nName=" << QFileInfo(path).fileName()
                  << "\nURL=" << path << "\nIcon=folder\n";
            }
        });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme("document-properties"), QObject::tr("Eigenschaften"),
        [path, parent]() {
            auto *d = new KPropertiesDialog(QUrl::fromLocalFile(path), nullptr);
            d->setAttribute(Qt::WA_DeleteOnClose);
            d->show();
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Konstanten
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int SC_SIDEBAR_ROW_H   = 34;
static constexpr int SC_MAX_VISIBLE     = 7;

// ─────────────────────────────────────────────────────────────────────────────
// DriveDelegate
// ─────────────────────────────────────────────────────────────────────────────
void DriveDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);

    if (opt.state & QStyle::State_Selected)
        p->fillRect(opt.rect, QColor(TM().colors().bgSelect));
    else if (opt.state & QStyle::State_MouseOver)
        p->fillRect(opt.rect, QColor(TM().colors().bgHover));

    const QRect    r       = opt.rect;
    const QIcon    icon    = idx.data(Qt::DecorationRole).value<QIcon>();
    const QString  name    = idx.data(Qt::DisplayRole).toString();
    const QString  path    = idx.data(Qt::UserRole).toString();
    const int      iconSz  = 16;
    const int      iconX   = r.left() + 8;
    const int      iconY   = r.top() + (r.height() - iconSz) / 2;
    const int      textX   = r.left() + 32;
    const int      textW   = r.width() - 40;

    icon.paint(p, iconX, iconY, iconSz, iconSz);
    p->setFont(QFont("sans-serif", 9));

    if (m_showBars && path.startsWith("/")) {
        QStorageInfo storage(path);
        if (storage.isValid() && storage.bytesTotal() > 0) {
            const double used  = (storage.bytesTotal() - storage.bytesFree()) / 1073741824.0;
            const double total = storage.bytesTotal() / 1073741824.0;
            const double pct   = used / total;

            QFontMetrics fm(p->font());
            const QString usedStr  = QString("%1").arg((int)used);
            const QString restStr  = QString(" / %1 GB").arg((int)total);
            const int     usedW    = fm.horizontalAdvance(usedStr);
            const int     restW    = fm.horizontalAdvance(restStr);
            const int     sizeW    = usedW + restW;
            const int     nameW    = textW - sizeW - 6;
            const int     sizeX    = r.right() - sizeW - 6;
            const int     lineH    = r.height() / 2;
            const int     barY     = r.top() + lineH + (r.height() - lineH) / 2 - 1;

            p->setPen(QColor(TM().colors().textPrimary));
            p->drawText(textX, r.top(), nameW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(name, Qt::ElideRight, nameW));

            p->setPen(QColor(TM().colors().textLight));
            p->drawText(sizeX, r.top(), usedW, lineH, Qt::AlignLeft | Qt::AlignVCenter, usedStr);
            p->setPen(QColor(TM().colors().textAccent));
            p->drawText(sizeX + usedW, r.top(), restW, lineH, Qt::AlignLeft | Qt::AlignVCenter, restStr);

            p->setBrush(QColor(TM().colors().splitter)); p->setPen(Qt::NoPen);
            p->drawRoundedRect(textX, barY, textW, 3, 1, 1);
            p->setBrush(QColor(TM().colors().accentHover));
            p->drawRoundedRect(textX, barY, (int)(textW * pct), 3, 1, 1);
        } else {
            p->setPen(QColor(TM().colors().textPrimary));
            p->drawText(textX, r.top(), textW, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
        }
    } else {
        // Nicht eingehängt: Name gedämpft + Eject-Symbol rechts
        const bool unmounted = path.startsWith("solid:");
        p->setPen(QColor(unmounted ? TM().colors().textMuted : TM().colors().textPrimary));
        if (unmounted) {
            // Name linksbündig, Eject-Icon rechts
            const QIcon ejectIcon = QIcon::fromTheme("media-eject");
            const int   eSz  = 12;
            const int   eX   = r.right() - eSz - 6;
            const int   eY   = r.top() + (r.height() - eSz) / 2;
            p->drawText(textX, r.top(), textW - eSz - 10, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
            ejectIcon.paint(p, eX, eY, eSz, eSz, Qt::AlignCenter, QIcon::Disabled);
        } else {
            p->drawText(textX, r.top(), textW, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
        }
    }
    p->restore();

    // 1px Trennlinie am unteren Rand
    p->save();
    p->setPen(QPen(QColor(TM().colors().border), 1));
    p->drawLine(opt.rect.left(), opt.rect.bottom(), opt.rect.right(), opt.rect.bottom());
    p->restore();
}

QSize DriveDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &idx) const
{
    const QString path = idx.data(Qt::UserRole).toString();
    // Eingehängt (Pfad) oder ausgehängt (solid:): beide bekommen 44px wenn showBars
    return QSize(200, m_showBars ? 44 : SC_SIDEBAR_ROW_H);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::adjustListHeight
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::adjustListHeight(QListWidget *list)
{
    if (!list) return;
    const int n = list->count();

    if (n == 0) {
        list->setFixedHeight(0);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else if (n <= SC_MAX_VISIBLE) {
        list->setFixedHeight(n * SC_SIDEBAR_ROW_H + (n - 1) * 2);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        list->setFixedHeight(SC_MAX_VISIBLE * SC_SIDEBAR_ROW_H + (SC_MAX_VISIBLE - 1) * 2);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    list->updateGeometry();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::Sidebar — Konstruktor
// ─────────────────────────────────────────────────────────────────────────────
Sidebar::Sidebar(QWidget *parent) : QWidget(parent)
{
    setStyleSheet(QString("background-color:%1; border:none;").arg(TM().colors().bgMain));

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    buildLogo(outerLay);
    buildDrivesSection(outerLay);
    buildGroupsSection(outerLay);
    buildNewGroupFixedSection(outerLay);
    buildTagsSection(outerLay);

    setupTags();
    loadUserPlaces();
    updateDrives();
    loadGDriveAccountsAsync();
    connectDriveList();
    setupDriveContextMenu();
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, [this](const QString &) { updateDrives(); emit drivesChanged(); });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, [this](const QString &) { updateDrives(); emit drivesChanged(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildLogo
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildLogo(QVBoxLayout *parent)
{
    auto *wrapper = new QWidget(this);
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *lay = new QHBoxLayout(wrapper);
    lay->setContentsMargins(12, 6, 12, 4);
    lay->setSpacing(8);

    // ── Icon ──
    auto *iconLabel = new QLabel();
    QPixmap pix;

    // 1. System-Theme (funktioniert nach sc-install)
    QIcon themeIcon = QIcon::fromTheme("splitcommander");
    if (!themeIcon.isNull()) {
        pix = themeIcon.pixmap(32, 32);
    }

    // 2. Fallback: Pfad relativ zum Binary (Dev-Build)
    if (pix.isNull()) {
        QString iconPath = QCoreApplication::applicationDirPath() + "/../src/splitcommander_64.png";
        if (!QFile::exists(iconPath))
            iconPath = QCoreApplication::applicationDirPath() + "/splitcommander_64.png";
        pix = QPixmap(iconPath);
    }

    // 3. Letzter Fallback: generisches Icon
    if (pix.isNull())
        pix = QIcon::fromTheme("system-file-manager").pixmap(32, 32);

    iconLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(32, 32);
    iconLabel->setStyleSheet("background:transparent; border:none;");
    lay->addWidget(iconLabel);

    // ── Text ──
    auto *nameLbl = new QLabel(
        "<span style='font-weight:200;color:#ccd4e8;font-size:12px;'>Split</span>"
        "<span style='font-weight:600;color:#88c0d0;font-size:12px;'>Commander</span>"
        "<span style='color:#4c566a;font-size:9px;'> | Dateimanager</span>");
    nameLbl->setStyleSheet("background:transparent; border:none;");
    lay->addWidget(nameLbl);
    lay->addStretch();

    parent->addWidget(wrapper);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildDrivesSection
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildDrivesSection(QVBoxLayout *parent)
{
    auto *wrapper = new QWidget(this);
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(10, 6, 6, 6);
    wLay->setSpacing(0);

    auto *box = new QWidget(wrapper);
    box->setObjectName("outerBox");
    box->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(box);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // Header
    auto *header = new QWidget();
    header->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(0);
    QSettings driveBoxSettings("SplitCommander", "UI");
    const QString driveBoxLabel = driveBoxSettings.value("driveBoxLabel", tr("LAUFWERKE")).toString();
    auto *lbl = new QLabel(driveBoxLabel);
    lbl->setStyleSheet(QString("font-size:12px;font-weight:normal;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    auto *menuBtn = new QPushButton();
    menuBtn->setIcon(QIcon::fromTheme("application-menu")); // KDE Standard für "Drei Punkte"
    if (menuBtn->icon().isNull()) menuBtn->setIcon(QIcon::fromTheme("view-more-symbolic"));
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; padding: 2px; }"
        "QPushButton:hover { background: #3b4252; border-radius: 4px; }"
    );
    hLay->addWidget(menuBtn);
    vbox->addWidget(header);

    // Drive list
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_driveList = new QListWidget();
    m_driveList->setFrameShape(QFrame::NoFrame);
    m_driveList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setStyleSheet(TM().ssListWidget());
    m_driveList->setItemDelegate(new DriveDelegate(this));
    listLay->addWidget(m_driveList);
    m_netBox = new QWidget();
    m_netBox->setVisible(false);
    auto *netWLay = new QVBoxLayout(m_netBox);
    netWLay->setContentsMargins(6, 0, 6, 4);
    m_netBox->setStyleSheet("margin-top:0px; padding-top:0px;");
    netWLay->setSpacing(0);

    m_netList = new QListWidget();
    m_netList->setFrameShape(QFrame::NoFrame);
    m_netList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setStyleSheet(TM().ssListWidget());
    m_netList->setItemDelegate(new DriveDelegate(false, this));
    netWLay->addWidget(m_netList);

    // Netzwerk-Box in vbox einfügen (nach listCont, vor Toggle)

    vbox->addWidget(listCont);
    vbox->addWidget(m_netBox);

    // Toggle
    auto *toggleBtn = new QPushButton();
    toggleBtn->setIcon(QIcon::fromTheme("go-up"));
    toggleBtn->setIconSize(QSize(10, 10));
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(12);
    toggleBtn->setStyleSheet(QString("QPushButton{background:transparent !important;font-size:7px;border:none;color:%1;}").arg(TM().colors().textMuted));
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);
    connect(toggleBtn, &QPushButton::toggled, this, [this, listCont, toggleBtn](bool on) {
        listCont->setVisible(!on);
        if (m_netBox) m_netBox->setVisible(!on && !m_gdriveAccounts.isEmpty());
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    // ── Netzwerk-Untersektion innerhalb der Geräte-Box ──────────────────

    wLay->addWidget(box);
    parent->addWidget(wrapper);

    connect(m_netList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty()) emit driveClicked(p);
    });

    // Rechtsklick Netzwerk-Liste
    m_netList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_netList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_netList->itemAt(pos);
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        const QString name = item->text();

        QMenu menu(this);
        sc_applyMenuShadow(&menu);
        menu.setStyleSheet(TM().ssMenu());
        menu.addAction(QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
            [this, path]() { emit driveClicked(path); });
        menu.addAction(QIcon::fromTheme("folder-open"), tr("In rechter Pane öffnen"), this,
            [this, path]() { emit driveClickedRight(path); });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("bookmark-new"), tr("Zu Laufwerken hinzufügen"), this,
            [this, path, name]() {
                QSettings s("SplitCommander", "NetworkPlaces");
                QStringList saved = s.value("places").toStringList();
                if (!saved.contains(path)) {
                    saved << path;
                    s.setValue("places", saved);
                    s.setValue("name_" + QString(path).replace("/","_"), name);
                    s.sync();
                }
                updateDrives();
                emit drivesChanged();
            });
        // Netzwerklaufwerk trennen
        menu.addAction(QIcon::fromTheme("media-eject"), tr("Trennen"), this,
            [this, path]() {
                auto *proc = new QProcess(this);
                connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, proc](int, QProcess::ExitStatus) {
                    updateDrives(); emit drivesChanged(); proc->deleteLater();
                });
                proc->start("umount", {path});
            });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"), this,
            [path]() { QGuiApplication::clipboard()->setText(path); });
        menu.exec(m_netList->mapToGlobal(pos));
    });

    // Drives-Menü
    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl]() {
        auto *m = new QMenu(this);
        sc_applyMenuShadow(m);
        m->setStyleSheet(TM().ssMenu());
        m->addAction(QIcon::fromTheme("edit-rename"), tr("Box umbenennen …"), this, [this, lbl]() {
            bool ok;
            QString name = QInputDialog::getText(this, tr("Box umbenennen"),
                tr("Name:"), QLineEdit::Normal, lbl->text(), &ok);
            if (ok && !name.isEmpty()) {
                lbl->setText(name);
                QSettings s("SplitCommander", "UI");
                s.setValue("driveBoxLabel", name);
                s.sync();
            }
        });
        m->addSeparator();
        m->addAction(QIcon::fromTheme("view-refresh"), tr("Alles aktualisieren"),
                     this, [this]() { updateDrives(); })->setShortcut(Qt::Key_F5);
        m->addSeparator();
        m->addAction(QIcon::fromTheme("network-connect"), tr("Netzwerklaufwerk verbinden"),
                     this, []() { QProcess::startDetached("knetattach", {}); });
        m->addAction(QIcon::fromTheme("folder-gdrive"), tr("Google Drive verbinden"),
                     this, []() { QProcess::startDetached("kcmshell6", {"kcm_kaccounts"}); });
        m->addSeparator();
        m->addAction(QIcon::fromTheme("multimedia-player"), tr("Telefon durchsuchen (MTP)"),
                     this, [this]() { emit driveClicked("mtp:/"); });
        m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildGroupsSection
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildGroupsSection — NUR NOCH DER SCROLLBEREICH
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildGroupsSection(QVBoxLayout *parent)
{
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QString("QScrollArea{border:none;background:%1;}").arg(TM().colors().bgMain));
    m_scrollArea->verticalScrollBar()->hide();
    m_scrollArea->horizontalScrollBar()->hide();

    auto *scrollWidget = new QWidget();
    scrollWidget->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    m_scrollArea->setWidget(scrollWidget);

    m_contentLayout = new QVBoxLayout(scrollWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    loadCustomGroups();

    m_contentLayout->addStretch(1);
    parent->addWidget(m_scrollArea, 1);

    // Overlay-Scrollbar
    m_overlayBar = new QScrollBar(Qt::Vertical, this);
    m_overlayBar->setStyleSheet(
        "QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:none;}"
        "QScrollBar::handle:vertical{background:rgba(136,192,208,60);border-radius:4px;min-height:20px;margin:1px;}"
        "QScrollBar::handle:vertical:hover{background:rgba(136,192,208,140);}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
    m_overlayBar->hide();
    m_overlayBar->raise();

    auto *native = m_scrollArea->verticalScrollBar();
    connect(native, &QScrollBar::rangeChanged, m_overlayBar, &QScrollBar::setRange);
    connect(native, &QScrollBar::valueChanged, m_overlayBar, &QScrollBar::setValue);
    connect(m_overlayBar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildNewGroupFixedSection — DER FESTE BUTTON UNTER DEN TAGS
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildNewGroupFixedSection(QVBoxLayout *parent)
{
    auto *ngWrapper = new QWidget(this);
    ngWrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *ngWLay = new QVBoxLayout(ngWrapper);
    ngWLay->setContentsMargins(10, 6, 6, 6);
    ngWLay->setSpacing(0);

    m_newGroupBox = new QWidget(ngWrapper);
    m_newGroupBox->setObjectName("ngBox");
    m_newGroupBox->setStyleSheet(TM().ssBox());

    auto *ngLay = new QVBoxLayout(m_newGroupBox);
    ngLay->setContentsMargins(6, 6, 6, 6);
    ngLay->setSpacing(0);

    auto *ngBtn = new QPushButton(tr("+ Neue Gruppe"));
    ngBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ngBtn->setStyleSheet(QString(
        "QPushButton { background:%1; border:none; color:%2; font-size:11px;"
        " padding:8px; text-align:center; border-radius:4px; }"
        "QPushButton:hover { background:%3; color:%4; }")
        .arg(TM().colors().bgList, TM().colors().textPrimary,
             TM().colors().bgHover, TM().colors().textLight));

    ngLay->addWidget(ngBtn);
    ngWLay->addWidget(m_newGroupBox);

    // Wichtig: In das Hauptlayout der Sidebar (fest stehend)
    parent->addWidget(ngWrapper);

    connect(ngBtn, &QPushButton::clicked, this, [this]() { onNewGroupDialog(); });
}
// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildTagsSection
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildTagsSection(QVBoxLayout *parent)
{
    m_tagsWrap = new QWidget(this);
    m_tagsWrap->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(m_tagsWrap);
    wLay->setContentsMargins(10, 6, 6, 6);
    wLay->setSpacing(0);

    m_tagsBox = new QWidget(m_tagsWrap);
    m_tagsBox->setObjectName("tagsBox");
    m_tagsBox->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(m_tagsBox);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // Header
    auto *header = new QWidget();
    header->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);
    auto *lbl = new QLabel(tr("TAGS"));
    lbl->setStyleSheet(QString("font-size:12px;font-weight:normal;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);
    auto *addBtn = new QPushButton();
    addBtn->setIcon(QIcon::fromTheme("list-add")); // KDE Standard für "+"
    if (addBtn->icon().isNull()) addBtn->setIcon(QIcon::fromTheme("add-subtitle-symbolic"));
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QPushButton { background:transparent; border:none; padding:2px; }"
        "QPushButton:hover { background:%1; border-radius:4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(header);

    // Tag list
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);
    listLay->setContentsMargins(6, 0, 6, 4);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_tagList = new QListWidget();
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setStyleSheet(TM().ssListWidget());
    listLay->addWidget(m_tagList);
    vbox->addWidget(listCont);

    // Toggle
    auto *toggleBtn = new QPushButton();
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16);
    toggleBtn->setIcon(QIcon::fromTheme("go-up"));
    toggleBtn->setIconSize(QSize(10, 10));
    // Entferne 'color:%1', da Icons über das Theme gesteuert werden
    toggleBtn->setStyleSheet("QPushButton{background:transparent !important; border:none;}");
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    connect(toggleBtn, &QPushButton::toggled, this, [listCont, toggleBtn](bool on) {
        listCont->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Neuer Tag"), tr("Tag-Name:"),
                                             QLineEdit::Normal, "", &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        QColor col = QColorDialog::getColor(QColor(TM().colors().textAccent), this, tr("Farbe wählen"));
        if (!col.isValid()) return;
        addTagItem(name.trimmed(), col.name());
        saveTags();
    });

    wLay->addWidget(m_tagsBox);
    parent->addWidget(m_tagsWrap);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::buildFooter
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::buildFooter(QVBoxLayout *parent)
{
    auto *footer = new QWidget(this);
    footer->setStyleSheet("background:transparent; border:none;");
    auto *lay = new QHBoxLayout(footer);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);

    const QString btnSS = TM().ssFooterBtn();

    auto makeBtn = [&](const QString &icon, const QString &tip) -> QToolButton * {
        auto *b = new QToolButton();
        b->setIcon(QIcon::fromTheme(icon));
        b->setIconSize(QSize(20, 20));
        b->setFixedSize(34, 34);
        b->setToolTip(tip);
        b->setStyleSheet(btnSS);
        return b;
    };

    auto *settingsBtn = makeBtn("preferences-system",    tr("Einstellungen"));
    auto *infoBtn     = makeBtn("dialog-information",    tr("Über"));
    auto *searchBtn   = makeBtn("system-search",         tr("Suchen"));
    auto *printBtn    = makeBtn("document-print",        tr("Drucken"));
    auto *chatBtn     = makeBtn("mail-message-new",      tr("Nachricht"));

    lay->addStretch();
    lay->addWidget(settingsBtn);
    lay->addStretch();
    lay->addWidget(infoBtn);
    lay->addStretch();
    lay->addWidget(searchBtn);
    lay->addStretch();
    lay->addWidget(printBtn);
    lay->addStretch();
    lay->addWidget(chatBtn);
    lay->addStretch();

    parent->addWidget(footer);

    // Einstellungen-Button
    connect(settingsBtn, &QToolButton::clicked, this, [this]() {
        auto *dlg = new SettingsDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &SettingsDialog::shortcutsChanged,   this, [this]() { emit settingsChanged(); });
        connect(dlg, &SettingsDialog::hiddenFilesChanged, this, [this](bool show) { emit hiddenFilesChanged(show); });
        connect(dlg, &SettingsDialog::singleClickChanged, this, [this]() { emit settingsChanged(); });
        dlg->exec();
        // QTimer::singleShot(0) stellt sicher dass deleteLater() für dlg
        // bereits ausgeführt wurde (s_instance == nullptr) bevor settingsChanged
        // den viewport-Repaint triggert. Ohne den Timer würde ageBadgeColor()
        // noch s_instance->m_ageColors lesen — die zwar korrekt sind, aber
        // das Timing ist nicht garantiert auf allen Qt-Versionen.
        QTimer::singleShot(0, this, [this]() { emit settingsChanged(); });
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::connectDriveList
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::connectDriveList()
{
    connect(m_driveList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        const QString p = it->data(Qt::UserRole).toString();
        if (p == "kcm_kaccounts")
            QProcess::startDetached("kcmshell6", {"kcm_kaccounts"});
        else
            emit driveClicked(p);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::loadGDriveAccountsAsync
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::loadGDriveAccountsAsync()
{
    auto *job = KIO::listDir(QUrl("gdrive:/"), KIO::HideProgressInfo);
    bool *found = new bool(false);
    connect(job, &KIO::ListJob::entries, this,
        [this, found](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &e : entries) {
            const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name.isEmpty() || name == "." || name == ".." || name == "new-account") continue;
            if (!e.isDir()) continue;
            *found = true;
            if (!m_gdriveAccounts.contains(name)) m_gdriveAccounts << name;
        }
    });
    connect(job, &KJob::result, this, [this, found](KJob *j) {
        if (j->error() && j->error() != KJob::KilledJobError)
            qDebug() << "gdrive:" << j->errorString();
        if (*found) updateDrives();
        delete found;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::resizeEvent
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlayBar && m_scrollArea) {
        const int w = 8;
        const int x = m_scrollArea->x() + m_scrollArea->width() - w;
        const int y = m_scrollArea->y();
        const int h = m_scrollArea->height();
        m_overlayBar->setGeometry(x, y, w, h);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::loadUserPlaces
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::loadUserPlaces()
{
    const QString xbelPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, "user-places.xbel");
    if (xbelPath.isEmpty()) return;

    QFile f(xbelPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QXmlStreamReader xml(&f);
    QString href, title;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("bookmark")) {
                href = xml.attributes().value("href").toString();
                title.clear();
            } else if (xml.name() == QLatin1String("title")) {
                title = xml.readElementText();
            }
        } else if (xml.isEndElement() && xml.name() == QLatin1String("bookmark")) {
            if (href.isEmpty()) continue;
            const QUrl url(href);
            const QString scheme = url.scheme().toLower();

            // gdrive ausschliesslich via KIO::listDir

            // Andere Netzwerkplätze persistent speichern
            static const QStringList netSchemes = {
                "smb","sftp","ftp","ftps","davs","dav","nfs","fish","webdav","webdavs"
            };
            if (netSchemes.contains(scheme)) {
                QSettings s("SplitCommander", "NetworkPlaces");
                QStringList saved = s.value("places").toStringList();
                if (!saved.contains(href)) {
                    saved << href;
                    s.setValue("places", saved);
                    const QString n = title.isEmpty() ? url.host() : title;
                    s.setValue("name_" + QString(href).replace("/","_"), n);
                    s.sync();
                }
            }
        }
    }
}

// Sidebar::updateDrives
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::updateDrives()
{
    // Reentrancy-Guard
    static bool s_updating = false;
    if (s_updating) return;
    s_updating = true;

    m_driveList->clear();
    QSet<QString> shownPaths;

    // ── Gemountete Volumes via Solid ──
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        const auto *access = device.as<Solid::StorageAccess>();
        if (!access) continue;

        const bool mounted = access->isAccessible();
        const QString path = mounted ? access->filePath() : QString();

        if (mounted) {
            if (path.isEmpty() || shownPaths.contains(path)) continue;
            if (path.startsWith("/boot") || path.startsWith("/efi")
                || path.startsWith("/snap") || path == "/home") continue;
        }

        const auto *vol = device.as<Solid::StorageVolume>();
        if (vol) {
            const QString lbl    = vol->label().toUpper();
            const QString fsType = vol->fsType().toLower();
            if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" || lbl == "ESP") continue;
            if (fsType == "iso9660" || fsType == "udf") continue;
        }

        if (mounted) shownPaths.insert(path);

        QString name = (mounted && path == "/") ? "Fedora" : device.description();
        if (name.isEmpty() && vol) name = vol->label();
        if (name.isEmpty()) name = device.udi().section('/', -1);

        QString iconName;
        if (const auto *drv = device.as<Solid::StorageDrive>()) {
            if (drv->driveType() == Solid::StorageDrive::CdromDrive)
                iconName = "drive-optical";
            else if (drv->isRemovable() || drv->isHotpluggable()
                     || (mounted && path.startsWith("/run/media/")))
                iconName = "drive-removable-media";
            else
                iconName = "drive-harddisk";
        } else {
            iconName = device.icon().isEmpty()
                ? ((mounted && path.startsWith("/run/media/")) ? "drive-removable-media" : "drive-harddisk")
                : device.icon();
        }

        QString freeStr;
        if (mounted) {
            QStorageInfo info(path);
            if (info.isValid())
                freeStr = QString("%1 GB frei / %2 GB")
                    .arg(info.bytesFree() / 1073741824.0, 0, 'f', 0)
                    .arg(info.bytesTotal() / 1073741824.0, 0, 'f', 0);
        }

        auto *it = new QListWidgetItem(QIcon::fromTheme(iconName), name, m_driveList);
        it->setData(Qt::UserRole,     mounted ? path : QString("solid:") + device.udi());
        it->setData(Qt::UserRole + 1, freeStr);
        it->setData(Qt::UserRole + 2, device.udi());
        if (!mounted) it->setForeground(QColor(TM().colors().textMuted));
    }

    // ── Nicht gemountete Block-Geräte ──
    const auto vdevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &device : vdevices) {
        const auto *vol = device.as<Solid::StorageVolume>();
        if (!vol) continue;
        if (vol->usage() != Solid::StorageVolume::FileSystem
            && vol->usage() != Solid::StorageVolume::Other) continue;

        const QString label  = vol->label();
        const QString lbl    = label.toUpper();
        const QString fsType = vol->fsType().toLower();
        if (label.isEmpty()) continue;
        if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" || lbl == "ESP") continue;
        if (fsType == "iso9660" || fsType == "udf") continue;

        const auto *access = device.as<Solid::StorageAccess>();
        if (access && access->isAccessible()) continue;

        QString iconName = "drive-harddisk";
        if (const auto *drv = device.as<Solid::StorageDrive>()) {
            if (drv->driveType() == Solid::StorageDrive::CdromDrive) iconName = "drive-optical";
            else if (drv->isRemovable() || drv->isHotpluggable())    iconName = "drive-removable-media";
        } else if (!device.icon().isEmpty()) {
            iconName = device.icon();
        }

        auto *it = new QListWidgetItem(QIcon::fromTheme(iconName), label, m_driveList);
        it->setData(Qt::UserRole,     QString("solid:") + device.udi());
        it->setData(Qt::UserRole + 1, QString("Nicht eingehängt – klicken zum Einhängen"));
    }

    // ── Google Drive → wird in Netzwerk-Sektion angezeigt (siehe unten) ──

    // ── Netzwerklaufwerke in m_netList ──
    if (m_netList) m_netList->clear();
    bool hasNet = false;

    // Gespeicherte Netzwerkplätze (persistent)
    QSettings netSettings("SplitCommander", "NetworkPlaces");
    // gdrive-Einträge aus NetworkPlaces entfernen (werden via KIO geladen)
    QStringList savedPlaces = netSettings.value("places").toStringList();
    savedPlaces.removeIf([](const QString &p) { return p.startsWith("gdrive:"); });
    for (const QString &p : savedPlaces) {
        if (shownPaths.contains(p)) continue;
        shownPaths.insert(p);
        hasNet = true;
        const QString savedName = netSettings.value("name_" + QString(p).replace("/","_"), QDir(p).dirName()).toString();
        // Icon je nach Protokoll
        const QString scheme = QUrl(p).scheme().toLower();
        QString savedIcon = scheme == "smb" ? "network-workgroup"
                          : scheme == "sftp" || scheme == "ssh" ? "network-connect"
                          : "network-server";
        if (m_netList) {
            auto *it = new QListWidgetItem(QIcon::fromTheme(savedIcon), savedName, m_netList);
            it->setData(Qt::UserRole, p);
            it->setData(Qt::UserRole + 1, p);
            it->setSizeHint(QSize(0, 36));
        }
    }

    // Aktiv gemountete Netzwerklaufwerke
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        const QString fs = storage.fileSystemType();
        if (fs != "cifs" && fs != "smb3" && fs != "nfs" && fs != "nfs4"
            && fs != "sshfs" && fs != "fuse.sshfs" && fs != "davfs"
            && fs != "fuse.davfs2" && !fs.startsWith("fuse.")) continue;

        const QString path = storage.rootPath();
        if (shownPaths.contains(path)) continue;
        shownPaths.insert(path);
        hasNet = true;

        QString name = storage.name().isEmpty() ? QDir(path).dirName() : storage.name();
        if (name.isEmpty()) name = path;

        QString icon = (fs == "cifs" || fs == "smb3") ? "network-workgroup"
                     : (fs == "sshfs" || fs == "fuse.sshfs") ? "network-connect"
                     : "network-server";

        if (m_netList) {
            auto *it = new QListWidgetItem(QIcon::fromTheme(icon), name, m_netList);
            it->setData(Qt::UserRole,     path);
            it->setData(Qt::UserRole + 1, fs + " – " + path);
            it->setSizeHint(QSize(0, 36));
        }
    }

    // gdrive-Accounts in Netzwerk-Sektion
    for (const QString &acc : m_gdriveAccounts) {
        const QString p = "gdrive:/" + acc;
        if (shownPaths.contains(p)) continue;
        shownPaths.insert(p);
        hasNet = true;
        if (m_netList) {
            QIcon gIcon = QIcon::fromTheme("folder-gdrive");
            if (gIcon.isNull()) gIcon = QIcon::fromTheme("folder-remote");
            if (gIcon.isNull()) gIcon = QIcon::fromTheme("folder");
            auto *it = new QListWidgetItem(gIcon, acc, m_netList);
            it->setData(Qt::DecorationRole, gIcon);
            it->setData(Qt::UserRole,     p);
            it->setData(Qt::UserRole + 1, "Google Drive");
            it->setSizeHint(QSize(0, 36));
        }
    }

    // Netzwerk-Sektion ein-/ausblenden
    if (m_netBox) {
        m_netBox->setVisible(hasNet);
        if (hasNet) {
            if (m_netList) {
                int netH = 0;
                for (int i = 0; i < m_netList->count(); ++i)
                    netH += m_netList->sizeHintForRow(i);
                if (netH > 0) m_netList->setFixedHeight(netH);
            }
            m_netBox->adjustSize();
            if (m_netBox->parentWidget())
                m_netBox->parentWidget()->adjustSize();
        }
    }

    // ── Höhe anpassen ──
    int totalH = 0;
    for (int i = 0; i < m_driveList->count(); ++i)
        totalH += m_driveList->sizeHintForRow(i);
    int maxH = 0;
    for (int i = 0; i < qMin(m_driveList->count(), SC_MAX_VISIBLE); ++i)
        maxH += m_driveList->sizeHintForRow(i);

    if (m_driveList->count() <= SC_MAX_VISIBLE) {
        m_driveList->setFixedHeight(qMax(1, totalH));
        m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        m_driveList->setFixedHeight(qMax(1, maxH));
        m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    m_driveList->updateGeometry();

    // ── Hot-Plug (einmalig verbinden) ──

    s_updating = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::setupDriveContextMenu
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::setupDriveContextMenu()
{
    m_driveList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_driveList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_driveList->itemAt(pos);
        if (!item) return;

        const QString path     = item->data(Qt::UserRole).toString();
        const QString name     = item->text();
        const QString udi      = item->data(Qt::UserRole + 2).toString();
        const bool    isSolid  = path.startsWith("solid:");
        const bool    isGdrive = path.startsWith("gdrive:/");
        // Gemountetes Solid-Laufwerk: hat UDI aber kein solid:-Prefix
        const bool    isMountedSolid = !udi.isEmpty() && !isSolid && !isGdrive;

        QMenu menu(this);
        sc_applyMenuShadow(&menu);
        menu.setStyleSheet(TM().ssMenu());

        auto *openInMenu = menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Pane"),  this, [this, path]() { emit driveClicked(path); });
        openInMenu->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });
        menu.addAction(QIcon::fromTheme("folder-open"), tr("Öffnen"), this, [this, path]() { emit driveClicked(path); });
        menu.addSeparator();

        // Gemountetes Solid-Laufwerk: Aushängen via UDI
        if (isMountedSolid) {
            menu.addAction(QIcon::fromTheme("media-eject"), tr("Aushängen"), this, [this, udi]() {
                Solid::Device dev(udi);
                auto *acc = dev.as<Solid::StorageAccess>();
                if (!acc) return;
                emit driveClicked(QDir::homePath());
                connect(acc, &Solid::StorageAccess::teardownDone, this,
                        [this](Solid::ErrorType, QVariant, const QString &) {
                    updateDrives(); emit drivesChanged();
                }, Qt::SingleShotConnection);
                acc->teardown();
            });
        }

        // Nicht-gemountetes normales Laufwerk: umount
        if (!isSolid && !isMountedSolid && path != "/" && !path.isEmpty() && !isGdrive) {
            menu.addAction(QIcon::fromTheme("media-eject"), tr("Auswerfen"), this, [this, path]() {
                auto *proc = new QProcess(this);
                connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, proc](int, QProcess::ExitStatus) {
                    updateDrives(); emit drivesChanged(); proc->deleteLater();
                });
                proc->start("umount", {path});
            });
        }

        if (isSolid) {
            Solid::Device dev(path.mid(6));
            auto *acc    = dev.as<Solid::StorageAccess>();
            bool mounted = acc && acc->isAccessible();
            if (mounted) {
                menu.addAction(QIcon::fromTheme("media-eject"), tr("Aushängen"), this, [this, acc]() {
                    emit driveClicked(QDir::homePath());
                    connect(acc, &Solid::StorageAccess::teardownDone, this,
                            [this](Solid::ErrorType, QVariant, const QString &) {
                        updateDrives(); emit drivesChanged();
                    }, Qt::SingleShotConnection);
                    acc->teardown();
                });
            } else {
                menu.addAction(QIcon::fromTheme("drive-harddisk"), tr("Einhängen"),
                               this, [this, path]() { emit driveClicked(path); });
            }
        }

        menu.addSeparator();
        auto *copyMenu = menu.addMenu(QIcon::fromTheme("edit-copy"), tr("Kopieren"));
        copyMenu->setStyleSheet(TM().ssMenu());
        copyMenu->addAction(tr("Pfad kopieren"), this, [path]() { QGuiApplication::clipboard()->setText(path); });
        copyMenu->addAction(tr("Name kopieren"), this, [name]() { QGuiApplication::clipboard()->setText(name); });

        if (!isSolid && !isGdrive) {
            menu.addAction(QIcon::fromTheme("emblem-symbolic-link"), tr("Verknüpfung erstellen"),
                           this, [path]() {
                QString desktop  = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                QFile f(desktop + "/" + QDir(path).dirName() + ".desktop");
                if (f.open(QIODevice::WriteOnly)) {
                    QTextStream s(&f);
                    s << "[Desktop Entry]\nType=Link\nName=" << QDir(path).dirName()
                      << "\nURL=" << path << "\nIcon=folder\n";
                }
            });
            menu.addSeparator();
            menu.addAction(QIcon::fromTheme("document-properties"), tr("Eigenschaften"),
                           this, [this, path]() {
                auto *dlg = new KPropertiesDialog(QUrl::fromLocalFile(path), nullptr);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            });
        }

        menu.exec(m_driveList->mapToGlobal(pos));
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::onNewGroupDialog
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::onNewGroupDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Neue Gruppe"));
    dlg.setMinimumWidth(360);
    dlg.setStyleSheet(TM().ssDialog());

    auto *vl       = new QVBoxLayout(&dlg);
    vl->setSpacing(10);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->addWidget(new QLabel(tr("Gruppenname:")));
    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Mein Ordner..."));
    vl->addWidget(nameEdit);

    vl->addWidget(new QLabel(tr("Inhalt:")));
    auto *btnGrp   = new QButtonGroup(&dlg);
    auto *emptyBtn = new QPushButton(tr("Leere Gruppe"));
    auto *homeBtn  = new QPushButton(tr("Home-Favoriten"));
    for (auto *b : {emptyBtn, homeBtn}) { b->setCheckable(true); b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); }
    emptyBtn->setChecked(true);
    btnGrp->addButton(emptyBtn, 0);
    btnGrp->addButton(homeBtn,  1);
    auto *optRow = new QHBoxLayout(); optRow->setSpacing(6);
    optRow->addWidget(emptyBtn); optRow->addWidget(homeBtn);
    vl->addLayout(optRow);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(btns);

    nameEdit->setFocus();
    if (dlg.exec() != QDialog::Accepted) return;

    const QString grpName = nameEdit->text().trimmed();
    if (grpName.isEmpty()) return;

    QSettings s("SplitCommander", "CustomGroups");
    QStringList groups = s.value("groups").toStringList();
    if (!groups.contains(grpName)) {
        groups << grpName;
        s.setValue("groups", groups);
        s.sync();
    }

    QListWidget *list = createGroupWidget(grpName, nullptr);

    if (btnGrp->checkedId() == 1) {
        const QStringList xdgPaths = {
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        };
        for (const QString &path : xdgPaths)
            if (!path.isEmpty() && QDir(path).exists())
                addToGroup(grpName, list, path);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::createGroupWidget
// ─────────────────────────────────────────────────────────────────────────────
QListWidget *Sidebar::createGroupWidget(const QString &name, QWidget *beforeWidget)
{
    // Äußere Box
    auto *outerBox = new QWidget();
    outerBox->setObjectName("groupBox");
    outerBox->setStyleSheet(TM().ssBox());
    outerBox->setProperty("groupName", name);

    QSettings pinSettings("SplitCommander", "CustomGroups");
    const bool pinned = pinSettings.value("pinned_" + name, false).toBool();
    outerBox->setProperty("pinned", pinned);

    auto *vbox = new QVBoxLayout(outerBox);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // Header (einfaches Widget ohne Drag-Funktion)
    auto *headerRow = new QWidget(outerBox);
    headerRow->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(headerRow);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);

    auto *lbl = new QLabel(name.toUpper());
    lbl->setStyleSheet(QString("font-size:12px;font-weight:normal;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    // Menü-Button (KDE Style)
    auto *menuBtn = new QPushButton();
    menuBtn->setIcon(QIcon::fromTheme("application-menu"));
    if (menuBtn->icon().isNull()) menuBtn->setIcon(QIcon::fromTheme("view-more-symbolic"));
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; padding: 2px; }"
        "QPushButton:hover { background: #3b4252; border-radius: 4px; }"
    );
    hLay->addWidget(menuBtn);

    // Plus-Button (KDE Style)
    auto *addBtn = new QPushButton();
    addBtn->setIcon(QIcon::fromTheme("list-add"));
    if (addBtn->icon().isNull()) addBtn->setIcon(QIcon::fromTheme("add-subtitle-symbolic"));
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QPushButton { background:transparent; border:none; padding:2px; }"
        "QPushButton:hover { background:%1; border-radius:4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(headerRow);

    // ── Liste in einen Container einbetten (für die Margins) ──
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);

    // Hier setzen wir die Abstände: 6px links/rechts, passend zur Geräte-Box
    listLay->setContentsMargins(6, 0, 6, 4);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *list = new QListWidget();
    list->setFrameShape(QFrame::NoFrame);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSpacing(0);
    list->setStyleSheet(TM().ssListWidget());
    list->setItemDelegate(new DriveDelegate(false, this));
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Liste zum Container-Layout hinzufügen
    listLay->addWidget(list);

    adjustListHeight(list);
    vbox->addWidget(listCont);

    // Toggle für Orte (Favoriten)
    auto *toggleBtn = new QPushButton(); // KEIN "▲" im Konstruktor
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16); // Etwas höher für bessere Klickbarkeit
    toggleBtn->setIcon(QIcon::fromTheme("go-up"));
    toggleBtn->setIconSize(QSize(10, 10));

    // font-size:7px entfernen, da Icons nicht auf Schriftgröße reagieren
    toggleBtn->setStyleSheet("QPushButton{background:transparent !important; border:none;}");

    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    connect(toggleBtn, &QPushButton::toggled, this, [listCont, toggleBtn](bool on) {
        listCont->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    // Wrapper mit dunklem Hintergrund (wie GERÄTE-Box)
    auto *wrapper = new QWidget();
    wrapper->setObjectName("groupWrapper");
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(10, 6, 6, 6);
    wLay->setSpacing(0);
    wLay->addWidget(outerBox);

    // In Layout einfügen: Wenn kein spezielles Ziel, dann immer auf Index 0 (ganz oben)
    int insertIdx = beforeWidget ? m_contentLayout->indexOf(beforeWidget) : 0;
    m_contentLayout->insertWidget(insertIdx, wrapper);

    // Signals
    auto sharedName = std::make_shared<QString>(name);

    connect(toggleBtn, &QPushButton::toggled, this, [list, toggleBtn](bool on) {
        list->setVisible(!on);
        // Wechselt das Icon je nach Zustand
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    connect(addBtn, &QPushButton::clicked, this, [this, list, sharedName]() {
        QString activePath;
        emit requestActivePath(&activePath);
        addToGroup(*sharedName, list, activePath);
    });

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit driveClicked(it->data(Qt::UserRole).toString());
    });

    connect(list, &QListWidget::customContextMenuRequested, this,
            [this, list, sharedName](const QPoint &pos) {
        auto *item = list->itemAt(pos);
        if (!item) return;
        showPlaceContextMenu(item, list, pos, *sharedName);
    });

    connect(menuBtn, &QPushButton::clicked, this,
            [this, menuBtn, lbl, sharedName, outerBox, wrapper]() {
        auto *m = new QMenu(this);
        m->setAttribute(Qt::WA_DeleteOnClose);
        sc_applyMenuShadow(m);
        m->setStyleSheet(TM().ssMenu());

        // Umbenennen
        connect(m->addAction(QIcon::fromTheme("edit-rename"), tr("Gruppe umbenennen")),
                &QAction::triggered, this, [this, lbl, sharedName]() {
            bool ok;
            QString newName = QInputDialog::getText(this, tr("Gruppe umbenennen"), tr("Neuer Name:"),
                                                    QLineEdit::Normal, *sharedName, &ok);
            if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == *sharedName) return;
            const QString oldName = *sharedName;
            *sharedName = newName.trimmed();
            lbl->setText(sharedName->toUpper());

            QSettings gs("SplitCommander", "CustomGroups");
            QStringList grps = gs.value("groups").toStringList();
            int i = grps.indexOf(oldName);
            if (i != -1) { grps[i] = *sharedName; gs.setValue("groups", grps); }

            QVariantList items;
            int cnt = gs.beginReadArray("group_" + oldName);
            for (int j = 0; j < cnt; ++j) {
                gs.setArrayIndex(j);
                items << QVariantMap{{"path", gs.value("path")}, {"name", gs.value("name")}};
            }
            gs.endArray();
            gs.remove("group_" + oldName);
            gs.beginWriteArray("group_" + *sharedName);
            for (int j = 0; j < items.size(); ++j) {
                gs.setArrayIndex(j);
                auto map = items[j].toMap();
                gs.setValue("path", map["path"]); gs.setValue("name", map["name"]);
            }
            gs.endArray(); gs.sync();
        });

        m->addSeparator();

        // Pin/Unpin
        const bool isPinned = outerBox->property("pinned").toBool();
        connect(m->addAction(QIcon::fromTheme(isPinned ? "window-unpin" : "window-pin"),
                             isPinned ? tr("Lösen") : tr("An Position verankern")),
                &QAction::triggered, this, [this, outerBox, sharedName]() {
            const bool nowPinned = !outerBox->property("pinned").toBool();
            outerBox->setProperty("pinned", nowPinned);
            QSettings gs("SplitCommander", "CustomGroups");
            gs.setValue("pinned_" + *sharedName, nowPinned); gs.sync();

            auto *vb = qobject_cast<QVBoxLayout *>(outerBox->layout());
            if (vb && vb->count() > 0) {
                auto *oldHeader = vb->itemAt(0)->widget();
                if (oldHeader) {
                    QList<QWidget *> children;
                    if (auto *hLay2 = oldHeader->layout())
                        for (int i = 0; i < hLay2->count(); ++i)
                            if (auto *w = hLay2->itemAt(i)->widget()) { children << w; w->setParent(nullptr); }
                    vb->removeWidget(oldHeader); oldHeader->deleteLater();
                    auto *newHeader = nowPinned
                        ? static_cast<QWidget *>(new QWidget(outerBox))
                        : static_cast<QWidget *>(new GroupDragHandle(outerBox, outerBox));
                    newHeader->setStyleSheet("background:transparent; border:none;");
                    auto *nh = new QHBoxLayout(newHeader);
                    nh->setContentsMargins(12, 10, 8, 6); nh->setSpacing(4);
                    for (auto *w : children) nh->addWidget(w);
                    vb->insertWidget(0, newHeader);
                }
            }
        });

        m->addSeparator();
        auto *upAct   = m->addAction(QIcon::fromTheme("go-up"),       tr("Nach oben"));
        auto *downAct = m->addAction(QIcon::fromTheme("go-down"),     tr("Nach unten"));
        if (isPinned) { upAct->setEnabled(false); downAct->setEnabled(false); }
        m->addSeparator();
        auto *delAct = m->addAction(QIcon::fromTheme("edit-delete"), tr("Gruppe löschen"));

        connect(upAct, &QAction::triggered, this, [this, wrapper]() {
            int idx = m_contentLayout->indexOf(wrapper);
            if (idx > 0) { m_contentLayout->removeWidget(wrapper); m_contentLayout->insertWidget(idx - 1, wrapper); saveGroupOrder(); }
        });
        connect(downAct, &QAction::triggered, this, [this, wrapper]() {
            int idx = m_contentLayout->indexOf(wrapper);
            if (idx >= 0 && idx < m_contentLayout->count() - 2) { m_contentLayout->removeWidget(wrapper); m_contentLayout->insertWidget(idx + 1, wrapper); saveGroupOrder(); }
        });
        connect(delAct, &QAction::triggered, this, [this, wrapper, sharedName]() {
            m_contentLayout->removeWidget(wrapper); wrapper->deleteLater();
            QSettings gs("SplitCommander", "CustomGroups");
            gs.remove("group_" + *sharedName); gs.sync();
            saveGroupOrder();
        });

        m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    return list;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::saveGroupOrder
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::saveGroupOrder()
{
    QStringList order;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *w = m_contentLayout->itemAt(i) ? m_contentLayout->itemAt(i)->widget() : nullptr;
        if (!w || w->objectName() != "groupWrapper") continue;
        auto *ob = w->findChild<QWidget *>("groupBox");
        if (ob) order << ob->property("groupName").toString();
    }
    QSettings gs("SplitCommander", "CustomGroups");
    gs.setValue("groups", order);
    gs.sync();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::loadCustomGroups
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::loadCustomGroups()
{
    QSettings s("SplitCommander", "CustomGroups");
    const QStringList groups = s.value("groups").toStringList();
    for (const QString &groupName : groups) {
        QListWidget *list = createGroupWidget(groupName, m_newGroupBox);
        if (groupName == "Favoriten") m_favList = list;

        int cnt = s.beginReadArray("group_" + groupName);
        for (int i = 0; i < cnt; ++i) {
            s.setArrayIndex(i);
            const QString path     = s.value("path").toString();
            const QString itemName = s.value("name").toString();
            if (path.isEmpty()) continue;
            QFileIconProvider ip;
            QIcon ico = ip.icon(QFileInfo(path));
            if (ico.isNull()) ico = QIcon::fromTheme("folder");
            auto *it = new QListWidgetItem(ico, itemName, list);
            it->setData(Qt::UserRole, path);
        }
        s.endArray();
        adjustListHeight(list);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::addToGroup
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::addToGroup(const QString &groupName, QListWidget *list, const QString &path)
{
    if (path.isEmpty() || !list) return;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == path) return;

    QString name = QDir(path).dirName();
    if (name.isEmpty()) name = path;
    QFileIconProvider ip;
    QIcon ico = ip.icon(QFileInfo(path));
    if (ico.isNull()) ico = QIcon::fromTheme("folder");
    auto *it = new QListWidgetItem(ico, name, list);
    it->setData(Qt::UserRole, path);
    adjustListHeight(list);

    QSettings gs("SplitCommander", "CustomGroups");
    gs.remove("group_" + groupName);
    gs.beginWriteArray("group_" + groupName);
    for (int i = 0; i < list->count(); ++i) {
        gs.setArrayIndex(i);
        gs.setValue("path", list->item(i)->data(Qt::UserRole).toString());
        gs.setValue("name", list->item(i)->text());
    }
    gs.endArray();
    gs.sync();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::addPlace
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::addPlace(const QString &path)
{
    if (path.isEmpty() || !m_favList) return;
    addToGroup("Favoriten", m_favList, path);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::showPlaceContextMenu
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::showPlaceContextMenu(QListWidgetItem *item, QListWidget *list,
                                   const QPoint &pos, const QString &groupName)
{
    const QString path = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    sc_applyMenuShadow(&menu);
    menu.setStyleSheet(TM().ssMenu());

    auto *openIn = menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
    openIn->setStyleSheet(TM().ssMenu());
    openIn->addAction(tr("Linke Pane"),  this, [this, path]() { emit driveClicked(path); });
    openIn->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });

    auto saveList = [this, list, groupName]() {
        if (!groupName.isEmpty()) {
            QSettings gs("SplitCommander", "CustomGroups");
            gs.remove("group_" + groupName);
            gs.beginWriteArray("group_" + groupName);
            for (int i = 0; i < list->count(); ++i) {
                gs.setArrayIndex(i);
                gs.setValue("path", list->item(i)->data(Qt::UserRole).toString());
                gs.setValue("name", list->item(i)->text());
            }
            gs.endArray(); gs.sync();
        }
    };

    sc_buildPlaceMenu(menu, path, this,
        [this, list, item, saveList]() {
            delete list->takeItem(list->row(item));
            adjustListHeight(list);
            saveList();
        },
        [item, saveList](const QString &newName, const QString &newPath) {
            item->setText(newName);
            item->setData(Qt::UserRole, newPath);
            saveList();
        });

    menu.exec(list->mapToGlobal(pos));
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar::setupTags / addTagItem / saveTags
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::addTagItem(const QString &name, const QString &color, const QString &fontFamily)
{
    QPixmap pix(10, 10);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(color)); p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 10, 10);

    auto *it = new QListWidgetItem(QIcon(pix), name, m_tagList);
    it->setData(Qt::UserRole,     color);
    it->setData(Qt::UserRole + 1, fontFamily);
    if (!fontFamily.isEmpty()) it->setFont(QFont(fontFamily));
}

void Sidebar::setupTags()
{
    m_tagList->clear();

    QSettings s("SplitCommander", "Tags");
    int cnt = s.beginReadArray("tags");
    if (cnt == 0) {
        s.endArray();
        s.beginWriteArray("tags");
        s.setArrayIndex(0); s.setValue("name", "Wichtig"); s.setValue("color", "#bf616a");
        s.setArrayIndex(1); s.setValue("name", "Arbeit");  s.setValue("color", TM().colors().accentHover);
        s.endArray(); s.sync();
        cnt = 2;
        s.beginReadArray("tags");
    }
    for (int i = 0; i < cnt; ++i) {
        s.setArrayIndex(i);
        addTagItem(s.value("name").toString(), s.value("color").toString(), s.value("font").toString());
    }
    s.endArray();

    m_tagList->setFixedHeight(m_tagList->count() * 32);
    if (m_tagsBox)  m_tagsBox->updateGeometry();
    if (m_tagsWrap) m_tagsWrap->updateGeometry();

    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit tagClicked(it->text());
    });

    m_tagList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tagList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_tagList->itemAt(pos);
        if (!item) return;
        QMenu menu(this);
        sc_applyMenuShadow(&menu);
        menu.setStyleSheet(TM().ssMenu());

        menu.addAction(QIcon::fromTheme("color-picker"), tr("Farbe ändern …"), this,
            [this, item]() {
                QColor col = QColorDialog::getColor(
                    QColor(item->data(Qt::UserRole).toString()), this, tr("Farbe wählen"));
                if (!col.isValid()) return;
                item->setData(Qt::UserRole, col.name());
                QPixmap pix(10, 10); pix.fill(Qt::transparent);
                QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(col); p.setPen(Qt::NoPen); p.drawEllipse(0, 0, 10, 10);
                item->setIcon(QIcon(pix));
                saveTags();
            });
        menu.addAction(QIcon::fromTheme("edit-rename"), tr("Umbenennen …"), this,
            [this, item]() {
                bool ok;
                QString name = QInputDialog::getText(this, tr("Tag umbenennen"),
                    tr("Name:"), QLineEdit::Normal, item->text(), &ok);
                if (ok && !name.isEmpty()) { item->setText(name); saveTags(); }
            });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("edit-delete"), tr("Löschen"), this,
            [this, item]() {
                delete m_tagList->takeItem(m_tagList->row(item));
                m_tagList->setFixedHeight(m_tagList->count() * 32);
                if (m_tagsBox)  m_tagsBox->updateGeometry();
                if (m_tagsWrap) m_tagsWrap->updateGeometry();
                saveTags();
            });

        menu.exec(m_tagList->mapToGlobal(pos));
    });
}

void Sidebar::saveTags()
{
    QSettings s("SplitCommander", "Tags");
    s.beginWriteArray("tags");
    for (int i = 0; i < m_tagList->count(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name",  m_tagList->item(i)->text());
        s.setValue("color", m_tagList->item(i)->data(Qt::UserRole).toString());
        s.setValue("font",  m_tagList->item(i)->data(Qt::UserRole + 1).toString());
    }
    s.endArray();
    s.sync();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar — leere Stubs (Interface-Kompatibilität)
// ─────────────────────────────────────────────────────────────────────────────
void Sidebar::setupPlaces()  {}
void Sidebar::setupRemotes() {}
void Sidebar::savePlaces(QListWidget **list)
{
    if (!list || !*list) return;
    QSettings gs("SplitCommander", "CustomGroups");
    gs.remove("group_Favoriten");
    gs.beginWriteArray("group_Favoriten");
    int idx = 0;
    for (int i = 0; i < (*list)->count(); ++i) {
        auto *item = (*list)->item(i);
        if (item->data(Qt::UserRole + 1).toBool()) continue;
        gs.setArrayIndex(idx++);
        gs.setValue("path", item->data(Qt::UserRole).toString());
        gs.setValue("name", item->text());
    }
    gs.endArray();
    adjustListHeight(*list);
    gs.sync();
}

// ─────────────────────────────────────────────────────────────────────────────
// GroupDragHandle — Implementierung
// ─────────────────────────────────────────────────────────────────────────────
GroupDragHandle::GroupDragHandle(QWidget *outerBox, QWidget *parent)
    : QWidget(parent), m_outerBox(outerBox)
{
    setMouseTracking(true);
    outerBox->setMouseTracking(true);
    outerBox->installEventFilter(this);
}

bool GroupDragHandle::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj != m_outerBox) return false;

    if (ev->type() == QEvent::MouseButtonPress) {
        auto *e = static_cast<QMouseEvent *>(ev);
        if (e->button() == Qt::LeftButton
            && e->position().y() <= 36
            && e->position().x() < m_outerBox->width() - 60) {
            m_dragging     = true;
            m_startY       = e->globalPosition().toPoint().y();
            m_dragIndex    = layoutIndex();
            m_currentIndex = m_dragIndex;
            auto *eff = new QGraphicsOpacityEffect(m_outerBox);
            eff->setOpacity(0.5);
            m_outerBox->setGraphicsEffect(eff);
            m_outerBox->setCursor(Qt::ClosedHandCursor);
            showIndicator(m_dragIndex);
        }
    } else if (ev->type() == QEvent::MouseMove) {
        auto *e = static_cast<QMouseEvent *>(ev);
        if (!m_dragging) {
            m_outerBox->setCursor(
                (e->position().y() <= 36 && e->position().x() < m_outerBox->width() - 60)
                ? Qt::OpenHandCursor : Qt::ArrowCursor);
            return false;
        }
        auto *l = parentLayout();
        if (!l) return false;
        int dy       = e->globalPosition().toPoint().y() - m_startY;
        int boxH     = qMax(m_outerBox->height(), 50);
        int newIndex = qBound(1, m_dragIndex + (int)(dy / (boxH * 0.6)), l->count() - 2);
        if (newIndex != m_currentIndex) {
            m_currentIndex = newIndex;
            showIndicator(m_currentIndex);
        }
    } else if (ev->type() == QEvent::MouseButtonRelease) {
        if (m_dragging) {
            m_dragging = false;
            m_outerBox->setGraphicsEffect(nullptr);
            m_outerBox->setCursor(Qt::ArrowCursor);
            auto *l = parentLayout();
            if (l && m_currentIndex != m_dragIndex) {
                l->removeWidget(m_outerBox);
                l->insertWidget(m_currentIndex, m_outerBox);
            }
            hideIndicator();
        }
    }
    return false;
}

void GroupDragHandle::showIndicator(int index)
{
    auto *l = parentLayout();
    if (!l) return;
    if (!m_indicator) {
        m_indicator = new QWidget(m_outerBox->parentWidget());
        m_indicator->setStyleSheet("background:#5e81ac;");
        m_indicator->setFixedHeight(2);
        m_indicator->raise();
    }
    for (int i = 0; i < l->count(); ++i) {
        auto *item = l->itemAt(i);
        if (item && item->widget() && l->indexOf(item->widget()) == index) {
            auto *ref = item->widget();
            m_indicator->setGeometry(ref->x(), ref->y() - 1, ref->width(), 2);
            m_indicator->show();
            return;
        }
    }
}

void GroupDragHandle::hideIndicator()
{
    if (m_indicator) m_indicator->hide();
}

QVBoxLayout *GroupDragHandle::parentLayout() const
{
    if (!m_outerBox || !m_outerBox->parentWidget()) return nullptr;
    return qobject_cast<QVBoxLayout *>(m_outerBox->parentWidget()->layout());
}

int GroupDragHandle::layoutIndex() const
{
    auto *l = parentLayout();
    return l ? l->indexOf(m_outerBox) : -1;
}
