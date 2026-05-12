
// --- sidebar.cpp — SplitCommander Sidebar ---

#include "sidebar.h"

// 1. Qt Core / UI
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
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
#include <QScrollBar>
#include <QStackedLayout>

#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QXmlStreamReader>

// 2. KDE / KIO / Solid
#include <KDirLister>
#include <KIO/CopyJob>
#include <KIO/ListJob>
#include <KPropertiesDialog>
#include <KIconDialog>
#include <KUrlRequester>
#include <KFile>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

// 3. SplitCommander
#include "dialogutils.h"
#include "config.h"
#include "terminalutils.h"
#include "tagmanager.h"



#include "thememanager.h"
// Themed input dialog - styled like QMenu
static QString sc_getText(QWidget *parent, const QString &title, const QString &label,
                          const QString &defaultText = QString())
{
    QDialog dlg(parent);
    dlg.setAttribute(Qt::WA_StyledBackground, true);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    auto *lay = new QVBoxLayout(&dlg);
    lay->setSpacing(4);
    lay->setContentsMargins(12, 10, 12, 10);
    auto *lbl = new QLabel(label, &dlg);
    auto *edit = new QLineEdit(defaultText, &dlg);
    auto *btnRow = new QHBoxLayout();
    auto *ok  = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok")),     QCoreApplication::translate("SplitCommander", "OK"),         &dlg);
    auto *can = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QCoreApplication::translate("SplitCommander", "Abbrechen"),  &dlg);
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

// --- Hilfsfunktionen (file-scope) ---

static QString sc_rootVolumeName()
{
    const QString name = QStorageInfo(QStringLiteral("/")).name();
    return name.isEmpty() ? QObject::tr("System") : name;
}

static void sc_applyMenuShadow(QMenu *menu)
{
    if (!menu) return;
    auto *shadow = new QGraphicsDropShadowEffect(menu);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 140));
    menu->setGraphicsEffect(shadow);
}

static QString sc_normalizePath(QString s)
{
    if (s.length() > 1 && s.endsWith('/'))
        s.chop(1);
    return s;
}

// --- KDE-Style Edit Dialog ---
static bool sc_editPlaceDialog(QWidget *parent, QString &name, QString &path, QString &icon)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Eintrag bearbeiten"));
    dlg.setFixedWidth(500);

    auto *mainVl = new QVBoxLayout(&dlg);
    mainVl->setSizeConstraint(QLayout::SetFixedSize);
    mainVl->setContentsMargins(12, 12, 12, 12);
    mainVl->setSpacing(12);

    auto *topHl = new QHBoxLayout();
    topHl->setSpacing(12);

    // Icon Button (Oben links)
    auto *iconBtn = new QPushButton(&dlg);
    QString currentIcon = icon.isEmpty() ? QStringLiteral("folder") : icon;
    iconBtn->setIcon(QIcon::fromTheme(currentIcon));
    iconBtn->setFixedSize(64, 64);
    iconBtn->setIconSize(QSize(32, 32));
    iconBtn->setCursor(Qt::PointingHandCursor);
    iconBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; }"
                           "QPushButton:hover { background: rgba(255,255,255,0.08); }");
    topHl->addWidget(iconBtn, 0, Qt::AlignTop);

    // Formular (Rechts)
    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *nameEdit = new QLineEdit(name, &dlg);
    form->addRow(QObject::tr("Name:"), nameEdit);

    auto *urlReq = new KUrlRequester(QUrl::fromUserInput(path), &dlg);
    urlReq->setMode(KFile::Directory | KFile::File | KFile::LocalOnly);
    form->addRow(QObject::tr("Adresse:"), urlReq);

    topHl->addLayout(form, 1);
    mainVl->addLayout(topHl);

    QObject::connect(iconBtn, &QPushButton::clicked, [&]() {
        KIconDialog iconDlg(&dlg);
        iconDlg.setSelectedIcon(currentIcon);
        QString newIcon = iconDlg.openDialog();
        if (!newIcon.isEmpty()) {
            currentIcon = newIcon;
            iconBtn->setIcon(QIcon::fromTheme(newIcon));
        }
    });

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    mainVl->addWidget(bbox);

    QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        name = nameEdit->text().trimmed();
        path = urlReq->url().toLocalFile();
        if (path.isEmpty()) path = urlReq->text();
        icon = currentIcon;
        return true;
    }
    return false;
}

static void sc_buildPlaceMenu(QMenu &menu, const QString &path, const QString &name, const QString &icon,
                       std::function<void()> removeAction,
                       std::function<void(const QString &, const QString &, const QString &)> editAction)
{
    // 1. Bearbeiten (Direkt-Aktion)
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-entry")), QObject::tr("Bearbeiten..."), [editAction, path, name, icon]() {
        QString n = name;
        QString p = path;
        QString i = icon;
        if (sc_editPlaceDialog(nullptr, n, p, i)) {
            editAction(n, p, i);
        }
    });

    menu.addSeparator();

    // 2. System-Aktionen
    menu.addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), QObject::tr("In Terminal öffnen"), [path]() {
        sc_openTerminal(path);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QObject::tr("Pfad kopieren"), [path]() {
        QGuiApplication::clipboard()->setText(path);
    });

    // 3. Entfernen
    if (removeAction) {
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), QObject::tr("Aus Gruppe entfernen"), removeAction);
    }

    // 4. Eigenschaften (Ganz unten)
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QObject::tr("Eigenschaften"), [path]() {
        KPropertiesDialog::showDialog(QUrl::fromUserInput(path));
    });
}

// --- Konstanten ---
static constexpr int SC_SIDEBAR_ROW_H   = 34;
static constexpr int SC_MAX_VISIBLE     = 8;

// --- DriveDelegate ---
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
        const double total = idx.data(Qt::UserRole + 10).toDouble();
        const double free  = idx.data(Qt::UserRole + 11).toDouble();

        if (total > 0) {
            const double used  = total - free;
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
            const QIcon ejectIcon = QIcon::fromTheme(QStringLiteral("media-eject"));
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

// --- Sidebar::adjustListHeight ---
void Sidebar::adjustListHeight(QListWidget *list)
{
    if (!list) return;
    const int n = list->count();

    if (n == 0) {
        list->setFixedHeight(0);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else if (n <= SC_MAX_VISIBLE) {
        list->setFixedHeight(n * SC_SIDEBAR_ROW_H);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        list->setFixedHeight(SC_MAX_VISIBLE * SC_SIDEBAR_ROW_H);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    list->updateGeometry();
}

// --- Sidebar::Sidebar — Konstruktor ---
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
    connectDriveList();
    setupDriveContextMenu();
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, [this](const QString &) { updateDrives(); emit drivesChanged(); });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, [this](const QString &) { updateDrives(); emit drivesChanged(); });

    m_trashLister = new KDirLister(this);
    connect(m_trashLister, &KDirLister::completed, this, &Sidebar::onTrashChanged);
    connect(m_trashLister, &KDirLister::itemsAdded, this, &Sidebar::onTrashChanged);
    connect(m_trashLister, &KDirLister::itemsDeleted, this, &Sidebar::onTrashChanged);
    m_trashLister->openUrl(QUrl(QStringLiteral("trash:/")), KDirLister::Keep);
}

// --- Sidebar::buildLogo ---
void Sidebar::buildLogo(QVBoxLayout *parent)
{
    auto *wrapper = new QWidget(this);
    wrapper->setStyleSheet(TM().ssSidebar());
    auto *lay = new QHBoxLayout(wrapper);
    lay->setContentsMargins(12, 6, 12, 4);
    lay->setSpacing(8);

    // --- Icon ---
    auto *iconLabel = new QLabel();
    QPixmap pix;

    // 1. System-Theme (funktioniert nach sc-install)
    QIcon themeIcon = QIcon::fromTheme(QStringLiteral("splitcommander"));
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
        pix = QIcon::fromTheme(QStringLiteral("system-file-manager")).pixmap(32, 32);

    iconLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(32, 32);
    iconLabel->setStyleSheet("background:transparent; border:none;");
    lay->addWidget(iconLabel);

    // --- Text ---
    auto *nameLbl = new QLabel(
        "<span style='font-weight:200;color:#ccd4e8;font-size:12px;'>Split</span>"
        "<span style='font-weight:600;color:#88c0d0;font-size:12px;'>Commander</span>"
        "<span style='color:#4c566a;font-size:9px;'> | Dateimanager</span>");
    nameLbl->setStyleSheet("background:transparent; border:none;");
    lay->addWidget(nameLbl);
    lay->addStretch();

    parent->addWidget(wrapper);
}

// --- Sidebar::buildDrivesSection ---
void Sidebar::buildDrivesSection(QVBoxLayout *parent)
{
    auto *wrapper = new QWidget(this);
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);

    auto *box = new QWidget(wrapper);
    box->setObjectName(QStringLiteral("outerBox"));
    box->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(box);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto *header = new QWidget();
    header->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(0);
    auto driveBoxSettings = Config::group("UI");
    const QString driveBoxLabel = driveBoxSettings.readEntry("driveBoxLabel", tr("LAUFWERKE"));
    auto *lbl = new QLabel(driveBoxLabel);

    lbl->setStyleSheet(QString("font-size:12px;font-weight:normal;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    auto *menuBtn = new QPushButton();
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu"))); // KDE Standard für "Drei Punkte"
    if (menuBtn->icon().isNull()) menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; padding: 2px; }"
        "QPushButton:hover { background: #3b4252; border-radius: 4px; }"
    );
    hLay->addWidget(menuBtn);
    vbox->addWidget(header);
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_driveList = new QListWidget();
    m_driveList->setSelectionMode(QAbstractItemView::NoSelection);
    m_driveList->setFrameShape(QFrame::NoFrame);
    m_driveList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setStyleSheet(TM().ssListWidget());
    m_driveList->setItemDelegate(new DriveDelegate(true, this));
    listLay->addWidget(m_driveList);
    m_netBox = new QWidget();
    m_netBox->setVisible(false);
    auto *netWLay = new QVBoxLayout(m_netBox);
    netWLay->setContentsMargins(6, 0, 6, 4);
    m_netBox->setStyleSheet("margin-top:0px; padding-top:0px;");
    netWLay->setSpacing(0);

    m_netList = new QListWidget();
    m_netList->setSelectionMode(QAbstractItemView::NoSelection);
    m_netList->setFrameShape(QFrame::NoFrame);
    m_netList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setStyleSheet(TM().ssListWidget());
    m_netList->setItemDelegate(new DriveDelegate(false, this));
    netWLay->addWidget(m_netList);

    // Netzwerk-Box in vbox einfügen (nach listCont, vor Toggle)

    vbox->addWidget(listCont);
    vbox->addWidget(m_netBox);
    auto *toggleBtn = new QPushButton();
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    toggleBtn->setIconSize(QSize(10, 10));
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(12);
    toggleBtn->setStyleSheet(QString("QPushButton{background:transparent !important;font-size:7px;border:none;color:%1;}").arg(TM().colors().textMuted));
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);
    connect(toggleBtn, &QPushButton::toggled, this, [this, listCont, toggleBtn](bool on) {
        listCont->setVisible(!on);
        if (m_netBox) m_netBox->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    // --- Netzwerk-Untersektion innerhalb der Geräte-Box ---

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
        const QString accountId = item->data(Qt::UserRole + 2).toString(); // nur bei gdrive gesetzt

        QMenu menu(this);
        sc_applyMenuShadow(&menu);
        menu.setStyleSheet(TM().ssMenu());
        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this,
            [this, path]() { emit driveClicked(path); });
        auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Pane"),  this, [this, path]() { emit driveClickedLeft(path); });
        openInMenu->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });
        menu.addSeparator();

        // Umbenennen — für alle NetworkPlaces-Einträge
        {
            auto netCheck = Config::group("NetworkPlaces");
            const QStringList netPlaces = netCheck.readEntry("places", QStringList());
            const QString npath = sc_normalizePath(path);
            bool alreadyIn = false;
            for (const QString &p : netPlaces) {
                if (sc_normalizePath(p) == npath) { alreadyIn = true; break; }
            }

            if (alreadyIn) {
                menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"), this,
                    [this, path, name]() {
                        bool ok;
                        const QString newName = DialogUtils::getText(this, tr("Umbenennen"), tr("Anzeigename:"), name, &ok);
                        if (ok && !newName.trimmed().isEmpty()) {
                            renameNetworkPlace(path, newName.trimmed());
                        }
                    });
                menu.addSeparator();
            }
        }


        // Zu Laufwerken hinzufügen — nur wenn noch nicht drin
        {
            auto netCheck = Config::group("NetworkPlaces");
            const QStringList netPlaces = netCheck.readEntry("places", QStringList());
            const QString npath = sc_normalizePath(path);
            bool alreadyIn = false;
            for (const QString &p : netPlaces) {
                if (sc_normalizePath(p) == npath) { alreadyIn = true; break; }
            }

            if (!alreadyIn) {
                menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), tr("Zu Laufwerken hinzufügen"), this,
                    [this, path, name]() {
                        auto s = Config::group("NetworkPlaces");
                        QStringList saved = s.readEntry("places", QStringList());
                        const QString npath = sc_normalizePath(path);
                        if (!saved.contains(npath)) {
                            saved << npath;
                            s.writeEntry("places", saved);
                            s.writeEntry("name_" + QString(npath).replace("/","_").replace(":","_"), name);
                            s.config()->sync();
                        }
                        updateDrives();
                        emit drivesChanged();
                    });
            }
        }

        // Netzwerklaufwerk trennen (gemountet) oder aus Liste entfernen (KIO)
        const bool isKioPlace = !path.startsWith("/") && path.contains(QStringLiteral(":/"));
        if (isKioPlace) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Aus Laufwerken entfernen"), this,
                [this, path]() {
                    auto s = Config::group("NetworkPlaces");
                    QStringList saved = s.readEntry("places", QStringList());
                    const QString npath = sc_normalizePath(path);
                    saved.removeAll(npath);
                    QString otherVersion = npath.endsWith('/') ? npath.left(npath.length()-1) : npath + "/";
                    if (otherVersion != "/") saved.removeAll(otherVersion);

                    s.writeEntry("places", saved);
                    s.deleteEntry("name_" + QString(npath).replace("/","_").replace(":","_"));
                    s.deleteEntry("name_" + QString(otherVersion).replace("/","_").replace(":","_"));
                    s.config()->sync();
                    updateDrives();
                    emit drivesChanged();
                    emit removeFromPlacesRequested(path);
                });
        } else {

            menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Trennen"), this,
                [this, path]() {
                    emit unmountRequested(path);
                });
        }
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Pfad kopieren"), this,
            [path]() { QGuiApplication::clipboard()->setText(path); });
        menu.exec(m_netList->mapToGlobal(pos));
    });

    // Drives-Menü
    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl]() {
        auto *m = new QMenu(this);
        sc_applyMenuShadow(m);
        m->setStyleSheet(TM().ssMenu());
        m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Box umbenennen …"), this, [this, lbl]() {
            bool ok;
            QString name = sc_getText(this, tr("Box umbenennen"), tr("Name:"), lbl->text());
            ok = !name.isNull();
            if (ok && !name.isEmpty()) {
                lbl->setText(name);
                auto s = Config::group("UI");
                s.writeEntry("driveBoxLabel", name);
                s.config()->sync();
            }

        });
        m->addSeparator();
        m->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Alles aktualisieren"),
                     this, [this]() { updateDrives(); })->setShortcut(Qt::Key_F5);
        m->addSeparator();
        m->addAction(QIcon::fromTheme(QStringLiteral("network-connect")), tr("Netzwerklaufwerk verbinden"),
                     this, [this]() { emit driveClicked("remote:/"); });
        m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });
}

// --- Sidebar::buildGroupsSection ---
// --- Sidebar::buildGroupsSection — NUR NOCH DER SCROLLBEREICH ---
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

// --- Sidebar::buildNewGroupFixedSection — DER FESTE BUTTON UNTER DEN TAGS ---
void Sidebar::buildNewGroupFixedSection(QVBoxLayout *parent)
{
    auto *ngWrapper = new QWidget(this);
    ngWrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *ngWLay = new QVBoxLayout(ngWrapper);
    ngWLay->setContentsMargins(10, 2, 6, 2);
    ngWLay->setSpacing(0);

    m_newGroupBox = new QWidget(ngWrapper);
    m_newGroupBox->setObjectName(QStringLiteral("ngBox"));
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
// --- Sidebar::buildTagsSection ---
void Sidebar::buildTagsSection(QVBoxLayout *parent)
{
    m_tagsWrap = new QWidget(this);
    m_tagsWrap->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(m_tagsWrap);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);

    m_tagsBox = new QWidget(m_tagsWrap);
    m_tagsBox->setObjectName(QStringLiteral("tagsBox"));
    m_tagsBox->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(m_tagsBox);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto *header = new QWidget();
    header->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);
    auto *lbl = new QLabel(tr("TAGS"));
    lbl->setStyleSheet(QString("font-size:12px;font-weight:normal;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);
    auto *addBtn = new QPushButton();
    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add"))); // KDE Standard für "+"
    if (addBtn->icon().isNull()) addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QPushButton { background:transparent; border:none; padding:2px; }"
        "QPushButton:hover { background:%1; border-radius:4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(header);
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);
    listLay->setContentsMargins(6, 0, 6, 4);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_tagList = new QListWidget();
    m_tagList->setSelectionMode(QAbstractItemView::NoSelection);
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setStyleSheet(TM().ssListWidget());
    listLay->addWidget(m_tagList);
    vbox->addWidget(listCont);
    auto *toggleBtn = new QPushButton();
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16);
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
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
        QString name = sc_getText(this, tr("Neuer Tag"), tr("Tag-Name:"));
        ok = !name.isNull();
        if (!ok || name.trimmed().isEmpty()) return;
        QColorDialog dlg(QColor(TM().colors().textAccent), this);
        dlg.setWindowTitle(tr("Farbe wählen"));
        dlg.setOptions(QColorDialog::DontUseNativeDialog);
        dlg.setStyleSheet(TM().ssDialog());
        if (dlg.exec() != QDialog::Accepted) return;
        QColor col = dlg.currentColor();
        if (!col.isValid()) return;
        addTagItem(name.trimmed(), col.name());
        adjustListHeight(m_tagList);
        if (m_tagsBox)  m_tagsBox->updateGeometry();
        if (m_tagsWrap) m_tagsWrap->updateGeometry();
        saveTags();
    });

    wLay->addWidget(m_tagsBox);
    parent->addWidget(m_tagsWrap);
}

// --- Sidebar::buildFooter ---
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

    auto *infoBtn     = makeBtn("dialog-information",    tr("Über"));
    auto *searchBtn   = makeBtn("system-search",         tr("Suchen"));
    auto *printBtn    = makeBtn("document-print",        tr("Drucken"));
    auto *chatBtn     = makeBtn("mail-message-new",      tr("Nachricht"));

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

}

// --- Sidebar::connectDriveList ---
void Sidebar::connectDriveList()
{
    connect(m_driveList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        const QString p = it->data(Qt::UserRole).toString();
        if (p == "kcm_kaccounts") {
            QProcess::startDetached("kcmshell6", {"kcm_kaccounts"});
        }
        else
            emit driveClicked(p);
    });
}

// ---  ---
// --- Sidebar::resizeEvent ---
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

// --- Sidebar::loadUserPlaces ---
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
                href = xml.attributes().value(QStringLiteral("href")).toString();
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
                auto s = Config::group("NetworkPlaces");
                QStringList saved = s.readEntry("places", QStringList());
                if (!saved.contains(href)) {
                    saved << href;
                    s.writeEntry("places", saved);
                    const QString n = title.isEmpty() ? url.host() : title;
                    s.writeEntry("name_" + QString(href).replace("/","_"), n);
                    s.config()->sync();
                }
            }

        }
    }
}

// Sidebar::updateDrives
// ---  ---
void Sidebar::renameNetworkPlace(const QString &path, const QString &newName)
{
    // NetworkPlaces aktualisieren
    auto s = Config::group("NetworkPlaces");
    const QString npath = sc_normalizePath(path);
    s.writeEntry("name_" + QString(npath).replace("/","_").replace(":","_"), newName);
    s.config()->sync();

    // CustomGroups Settings aktualisieren
    auto gs = Config::group("CustomGroups");
    const QStringList groups = gs.readEntry("groups", QStringList());
    for (const QString &grp : groups) {
        KConfigGroup g(gs.config(), gs.name() + "/group_" + grp);
        int cnt = g.readEntry("size", 0);
        QList<QPair<QString,QString>> items;
        bool changed = false;
        for (int i = 1; i <= cnt; ++i) {
            KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
            QString p = itemG.readEntry("path", QString());
            QString n = itemG.readEntry("name", QString());
            if (p == path) { n = newName; changed = true; }
            items << qMakePair(p, n);
        }
        if (changed) {
            KConfigGroup(gs.config(), gs.name() + "/group_" + grp).deleteGroup();
            KConfigGroup gNew(gs.config(), gs.name() + "/group_" + grp);
            gNew.writeEntry("size", items.size());
            for (int i = 0; i < items.size(); ++i) {
                KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));
                itemG.writeEntry("path", items[i].first);
                itemG.writeEntry("name", items[i].second);
            }
        }

    }
    gs.config()->sync();


    // Alle QListWidgets in der Sidebar direkt aktualisieren (kein Neu-Erstellen)
    auto updateList = [&](QListWidget *list) {
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == path)
                list->item(i)->setText(newName);
        }
    };
    updateList(m_driveList);
    updateList(m_netList);
    // Custom group lists
    for (QListWidget *list : findChildren<QListWidget*>())
        updateList(list);

    updateDrives();
    emit drivesChanged();
}

void Sidebar::updateDrives()
{
    // Reentrancy-Guard
    static bool s_updating = false;
    if (s_updating) return;
    s_updating = true;

    m_driveList->clear();
    QSet<QString> shownPaths;
    QSet<QString> shownUdis;  // verhindert Duplikate zwischen beiden Schleifen

    // --- Gemountete Volumes via Solid ---
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
        shownUdis.insert(device.udi());

        QString name = (mounted && path == "/") ? sc_rootVolumeName() : device.description();
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
        double totalG = 0;
        double freeG  = 0;
        if (mounted) {
            QStorageInfo info(path);
            if (info.isValid()) {
                totalG = info.bytesTotal() / 1073741824.0;
                freeG  = info.bytesFree()  / 1073741824.0;
                freeStr = QString("%1 GB frei / %2 GB")
                    .arg(freeG, 0, 'f', 0)
                    .arg(totalG, 0, 'f', 0);
            }
        }

        auto *it = new QListWidgetItem(QIcon::fromTheme(iconName), name, m_driveList);
        it->setData(Qt::UserRole,      mounted ? path : QString("solid:") + device.udi());
        it->setData(Qt::UserRole + 1,  freeStr);
        it->setData(Qt::UserRole + 2,  device.udi());
        it->setData(Qt::UserRole + 10, totalG);
        it->setData(Qt::UserRole + 11, freeG);
        if (!mounted) it->setForeground(QColor(TM().colors().textMuted));
    }

    // --- Nicht gemountete Block-Geräte ---
    const auto vdevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &device : vdevices) {
        // Bereits in Schleife 1 gezeigt — überspringen
        if (shownUdis.contains(device.udi())) continue;

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
        it->setData(Qt::UserRole,     QString(QStringLiteral("solid:") + device.udi()));
        it->setData(Qt::UserRole + 1, QString("Nicht eingehängt – klicken zum Einhängen"));
    }

    // --- Google Drive → wird in Netzwerk-Sektion angezeigt (siehe unten) ---

    // --- Netzwerklaufwerke in m_netList ---
    if (m_netList) m_netList->clear();
    bool hasNet = false;

    // Gespeicherte Netzwerkplätze (persistent)
    auto netSettings = Config::group("NetworkPlaces");
    QStringList savedPlaces = netSettings.readEntry("places", QStringList());
    for (const QString &p : savedPlaces) {
        if (shownPaths.contains(p)) continue;
        shownPaths.insert(p);
        hasNet = true;
        const QString scheme = QUrl::fromUserInput(p).scheme().toLower();
        const QString savedName = netSettings.readEntry("name_" + QString(p).replace("/","_").replace(":","_"),
            scheme == "gdrive" ? "Google Drive" : QDir(p).dirName());

        // Icon je nach Protokoll
        QString savedIcon = scheme == "gdrive"    ? "folder-gdrive"
                          : scheme == "smb"       ? "network-workgroup"
                          : scheme == "sftp" || scheme == "ssh" ? "network-connect"
                          : scheme == "mtp"       ? "multimedia-player"
                          : scheme == "bluetooth" ? "bluetooth"
                          : scheme == "afc"       ? "phone"
                          : "network-server";
        QString url = p;
        if ((scheme == "gdrive" || scheme == "mtp") && !url.endsWith("/"))
            url += "/";

        if (m_netList) {
            auto *it = new QListWidgetItem(QIcon::fromTheme(savedIcon), savedName, m_netList);
            it->setData(Qt::UserRole, url);
            it->setData(Qt::UserRole + 1, url);
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

        QString name = storage.name().isEmpty() ? QUrl::fromLocalFile(path).fileName() : storage.name();
        if (name.isEmpty()) name = path;

        QString icon = (fs == "cifs" || fs == "smb3") ? "network-workgroup"
                     : (fs == "sshfs" || fs == "fuse.sshfs") ? "network-connect"
                     : "network-server";

        if (m_netList) {
            auto *it = new QListWidgetItem(QIcon::fromTheme(icon), name, m_netList);
            it->setData(Qt::UserRole,     path);
            it->setData(Qt::UserRole + 1, QString(fs + " – " + path));
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

    // --- Höhe anpassen ---
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

    // --- Hot-Plug (einmalig verbinden) ---

    s_updating = false;
}

// --- Sidebar::setupDriveContextMenu ---
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

        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this, [this, path]() { emit driveClicked(path); });
        auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Pane"),  this, [this, path]() { emit driveClickedLeft(path); });
        openInMenu->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });
        menu.addSeparator();

        // Gemountetes Solid-Laufwerk: Aushängen via MainWindow koordinieren
        if (isMountedSolid) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Aushängen"), this, [this, udi]() {
                emit teardownRequested(udi);
            });
        }

        // Nicht-gemountetes normales Laufwerk: umount
        if (!isSolid && !isMountedSolid && path != "/" && !path.isEmpty() && !isGdrive) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Auswerfen"), this, [this, path]() {
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
            const QString solidUdi = dev.udi();
            if (mounted) {
                menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Aushängen"), this, [this, solidUdi]() {
                    emit teardownRequested(solidUdi);
                });
            } else {
                menu.addAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), tr("Einhängen"),
                               this, [this, path]() { emit driveClicked(path); });
            }
        }

        menu.addSeparator();

        // Für manuell gepinnte Netzwerkeinträge: umbenennen + aus Liste entfernen
        {
            auto netCheck = Config::group("NetworkPlaces");
            const QStringList netPlaces = netCheck.readEntry("places", QStringList());
            if (netPlaces.contains(path)) {
                menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"), this,
                    [this, path, name]() {
                        QDialog dlg(this);
                        dlg.setWindowTitle(tr("Umbenennen"));
                        dlg.setStyleSheet(TM().ssDialog());
                        auto *vl = new QVBoxLayout(&dlg);
                        vl->addWidget(new QLabel(tr("Anzeigename:")));
                        auto *edit = new QLineEdit(name, &dlg);
                        vl->addWidget(edit);
                        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                        vl->addWidget(box);
                        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                        if (dlg.exec() == QDialog::Accepted && !edit->text().trimmed().isEmpty()) {
                            renameNetworkPlace(path, edit->text().trimmed());
                        }
                    });
                menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Aus Laufwerken entfernen"), this,
                    [this, path]() {
                        auto s = Config::group("NetworkPlaces");
                        QStringList saved = s.readEntry("places", QStringList());
                        const QString npath = sc_normalizePath(path);
                        saved.removeAll(npath);
                        QString otherVersion = npath.endsWith('/') ? npath.left(npath.length()-1) : npath + "/";
                        if (otherVersion != "/") saved.removeAll(otherVersion);

                        s.writeEntry("places", saved);
                        s.deleteEntry("name_" + QString(npath).replace("/","_").replace(":","_"));
                        s.deleteEntry("name_" + QString(otherVersion).replace("/","_").replace(":","_"));
                        s.config()->sync();
                        updateDrives();
                        emit drivesChanged();
                    });
                menu.addSeparator();
            }
        }


        auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Kopieren"));
        copyMenu->setStyleSheet(TM().ssMenu());
        copyMenu->addAction(tr("Pfad kopieren"), this, [path]() { QGuiApplication::clipboard()->setText(path); });
        copyMenu->addAction(tr("Name kopieren"), this, [name]() { QGuiApplication::clipboard()->setText(name); });

        if (!isSolid && !isGdrive) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("emblem-symbolic-link")), tr("Verknüpfung erstellen"),
                           this, [path]() {
                QString desktop  = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                const QUrl url = QUrl::fromUserInput(path);
                const QString dirName = url.isLocalFile() ? QDir(url.toLocalFile()).dirName() : url.fileName();
                QFile f(desktop + "/" + dirName + ".desktop");
                if (f.open(QIODevice::WriteOnly)) {
                    QTextStream s(&f);
                    s << "[Desktop Entry]\nType=Link\nName=" << dirName
                      << "\nURL=" << path << "\nIcon=folder\n";
                }
            });
            menu.addSeparator();
            menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), tr("Eigenschaften"),
                           this, [path]() {
                auto *dlg = new KPropertiesDialog(QUrl::fromLocalFile(path), nullptr);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            });
        }

        menu.exec(m_driveList->mapToGlobal(pos));
    });
}

// --- Sidebar::onNewGroupDialog ---
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

    auto s = Config::group("CustomGroups");
    QStringList groups = s.readEntry("groups", QStringList());
    if (!groups.contains(grpName)) {
        groups << grpName;
        s.writeEntry("groups", groups);
        s.config()->sync();
    }


    QListWidget *list = createGroupWidget(grpName, nullptr);

    if (btnGrp->checkedId() == 1) {
        const QStringList xdgPaths = {
            QDir::homePath(),
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
            QStringLiteral("trash:/"),
        };
        for (const QString &path : xdgPaths) {
            const QUrl url(path);
            const bool isKio = !url.scheme().isEmpty() && url.scheme() != "file";
            if (!path.isEmpty() && (isKio || QDir(path).exists()))
                addToGroup(grpName, list, path);
        }
    }
}

// --- Sidebar::createGroupWidget ---
QListWidget *Sidebar::createGroupWidget(const QString &name, QWidget *beforeWidget)
{
    // Äußere Box
    auto *outerBox = new QWidget();
    outerBox->setObjectName(QStringLiteral("groupBox"));
    outerBox->setStyleSheet(TM().ssBox());
    outerBox->setProperty("groupName", name);

    {
        auto pinSettings = Config::group("CustomGroups");
        outerBox->setProperty("pinned", pinSettings.readEntry("pinned_" + name, false));
    }


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
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    if (menuBtn->icon().isNull()) menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; padding: 2px; }"
        "QPushButton:hover { background: #3b4252; border-radius: 4px; }"
    );
    hLay->addWidget(menuBtn);

    // Plus-Button (KDE Style)
    auto *addBtn = new QPushButton();
    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    if (addBtn->icon().isNull()) addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QPushButton { background:transparent; border:none; padding:2px; }"
        "QPushButton:hover { background:%1; border-radius:4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(headerRow);

    // --- Liste in einen Container einbetten (für die Margins) ---
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);

    // Hier werden die Abstände gesetzt: 6px links/rechts, passend zur Geräte-Box
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSpacing(0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *list = new QListWidget();
    list->setSelectionMode(QAbstractItemView::NoSelection);
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
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
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
    wrapper->setObjectName(QStringLiteral("groupWrapper"));
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);
    wLay->addWidget(outerBox);

    // In Layout einfügen: Wenn kein spezielles Ziel, dann immer auf Index 0 (ganz oben)
    int insertIdx = beforeWidget ? m_contentLayout->indexOf(beforeWidget) : 0;
    m_contentLayout->insertWidget(insertIdx, wrapper);

    // Signals
    auto sharedName = std::make_shared<QString>(name);

    connect(toggleBtn, &QPushButton::toggled, list, [list, toggleBtn](bool on) {
        list->setVisible(!on);
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
        connect(m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Gruppe umbenennen")),
                &QAction::triggered, this, [this, lbl, sharedName]() {
            bool ok;
            QString newName = sc_getText(this, tr("Gruppe umbenennen"), tr("Neuer Name:"), *sharedName);
            ok = !newName.isNull();
            if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == *sharedName) return;
            const QString oldName = *sharedName;
            *sharedName = newName.trimmed();
            lbl->setText(sharedName->toUpper());

            auto gs = Config::group("CustomGroups");
            QStringList grps = gs.readEntry("groups", QStringList());
            int idx = grps.indexOf(oldName);
            if (idx != -1) { grps[idx] = *sharedName; gs.writeEntry("groups", grps); }
 
            KConfigGroup oldGrp(gs.config(), gs.name() + "/group_" + oldName);
            KConfigGroup newGrp(gs.config(), gs.name() + "/group_" + *sharedName);
            int cnt = oldGrp.readEntry("size", 0);
            newGrp.writeEntry("size", cnt);
            for (int j = 1; j <= cnt; ++j) {
                KConfigGroup oldItem(oldGrp.config(), oldGrp.name() + "/" + QString::number(j));
                KConfigGroup newItem(newGrp.config(), newGrp.name() + "/" + QString::number(j));
                newItem.writeEntry("path", oldItem.readEntry("path", QString()));
                newItem.writeEntry("name", oldItem.readEntry("name", QString()));
            }
            oldGrp.deleteGroup();
            gs.writeEntry("pinned_" + *sharedName, gs.readEntry("pinned_" + oldName, false));
            gs.deleteEntry("pinned_" + oldName);
            gs.config()->sync();

        });

        m->addSeparator();

        // Pin/Unpin
        const bool isPinned = outerBox->property("pinned").toBool();
        connect(m->addAction(QIcon::fromTheme(isPinned ? "window-unpin" : "window-pin"),
                             isPinned ? tr("Lösen") : tr("An Position verankern")),
                &QAction::triggered, this, [outerBox, sharedName]() {

            const bool nowPinned = !outerBox->property("pinned").toBool();
            outerBox->setProperty("pinned", nowPinned);
            auto gs = Config::group("CustomGroups");
            gs.writeEntry("pinned_" + *sharedName, nowPinned);
            gs.config()->sync();
        });

        m->addSeparator();

        auto *upAct   = m->addAction(QIcon::fromTheme(QStringLiteral("go-up")),   tr("Nach oben"));
        auto *downAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-down")), tr("Nach unten"));
        if (isPinned) { upAct->setEnabled(false); downAct->setEnabled(false); }

        m->addSeparator();
        auto *delAct = m->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Gruppe löschen"));

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
            auto gs = Config::group("CustomGroups");
            KConfigGroup(gs.config(), gs.name() + "/group_" + *sharedName).deleteGroup();
            gs.config()->sync();
            saveGroupOrder();
        });


        m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    return list;
}

// --- Sidebar::saveGroupOrder ---
void Sidebar::saveGroupOrder()
{
    QStringList order;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *w = m_contentLayout->itemAt(i) ? m_contentLayout->itemAt(i)->widget() : nullptr;
        if (!w || w->objectName() != "groupWrapper") continue;
        auto *ob = w->findChild<QWidget *>("groupBox");
        if (ob) order << ob->property("groupName").toString();
    }
    auto gs = Config::group("CustomGroups");
    gs.writeEntry("groups", order);
    gs.config()->sync();
}

void Sidebar::loadCustomGroups()
{
    auto s = Config::group("CustomGroups");
    const QStringList groups = s.readEntry("groups", QStringList());
    for (const QString &groupName : groups) {
        QListWidget *list = createGroupWidget(groupName, m_newGroupBox);
        if (groupName == "Favoriten") m_favList = list;

        KConfigGroup g(s.config(), s.name() + "/group_" + groupName);
        int cnt = g.readEntry("size", 0);
        for (int i = 1; i <= cnt; ++i) {
            KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
            const QString path      = itemG.readEntry("path", QString());
            const QString itemName  = itemG.readEntry("name", QString());
            const QString customIco = itemG.readEntry("icon", QString());
            if (path.isEmpty()) continue;

            QIcon ico;
            if (!customIco.isEmpty()) {
                ico = QIcon::fromTheme(customIco);
            } else if (!path.startsWith("/")) {
                const QString scheme = QUrl::fromUserInput(path).scheme().toLower();
                ico = QIcon::fromTheme(
                    scheme == "gdrive"    ? "folder-gdrive"     :
                    scheme == "smb"       ? "network-workgroup"  :
                    scheme == "sftp" || scheme == "ssh" ? "network-connect" :
                    scheme == "mtp"       ? "multimedia-player"  :
                    scheme == "bluetooth" ? "bluetooth"          :
                    "network-server");
            } else {
                QFileIconProvider ip;
                ico = ip.icon(QFileInfo(path));
            }
            if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("folder"));
            auto *it = new QListWidgetItem(ico, itemName, list);
            it->setData(Qt::UserRole, path);
            it->setData(Qt::UserRole + 2, customIco);
        }
        adjustListHeight(list);
    }
}


// --- Sidebar::addToGroup ---
void Sidebar::addToGroup(const QString &groupName, QListWidget *list, const QString &path)
{
    if (path.isEmpty() || !list) return;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == path) return;

    QUrl url(path);
    QString name = url.isLocalFile() ? QDir(path).dirName() : url.fileName();
    if (name.isEmpty()) name = path;

    // Spezielle Namen für bekannte KIO-Pfade
    const QString scheme = url.scheme().toLower();
    if (scheme == "trash")             name = tr("Papierkorb");
    else if (scheme == "recentdocuments") name = tr("Zuletzt verwendet");
    else if (scheme == "remote")       name = tr("Netzwerk");
    else if (path == QDir::homePath()) name = tr("Persönlicher Ordner");

    // Icon immer aus System-Theme
    QIcon ico;
    if (scheme == "trash") {
        ico = QIcon::fromTheme("user-trash");
        if (ico.isNull()) ico = QIcon::fromTheme("user-trash-full");
        if (ico.isNull()) ico = QIcon::fromTheme("trash-empty");
        if (ico.isNull()) ico = QIcon::fromTheme("trash");
    } else if (scheme == "recentdocuments") {
        ico = QIcon::fromTheme("document-open-recent");
    } else if (!path.startsWith("/")) {
        ico = QIcon::fromTheme(
            scheme == "gdrive"    ? "folder-gdrive"      :
            scheme == "smb"       ? "network-workgroup"   :
            scheme == "sftp" || scheme == "ssh" ? "network-connect" :
            scheme == "mtp"       ? "multimedia-player"   :
            scheme == "bluetooth" ? "bluetooth"           :
            scheme == "afc"       ? "phone"               :
            "network-server");
    } else {
        QFileIconProvider ip;
        ico = ip.icon(QFileInfo(path));
    }
    if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("folder"));

    auto *it = new QListWidgetItem(ico, name, list);
    it->setData(Qt::UserRole, path);
    adjustListHeight(list);

    auto gs = Config::group("CustomGroups");
    KConfigGroup(gs.config(), gs.name() + "/group_" + groupName).deleteGroup();
    KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
    gNew.writeEntry("size", list->count());

    for (int i = 0; i < list->count(); ++i) {
        KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));

        itemG.writeEntry("path", list->item(i)->data(Qt::UserRole).toString());
        itemG.writeEntry("name", list->item(i)->text());
    }
    gs.config()->sync();

}

// --- Sidebar::addPlace ---
void Sidebar::addPlace(const QString &path)
{
    if (path.isEmpty() || !m_favList) return;
    addToGroup("Favoriten", m_favList, path);
}

QStringList Sidebar::groupNames() const
{
    QStringList names;
    if (!m_contentLayout) return names;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *item = m_contentLayout->itemAt(i);
        if (!item) continue;
        auto *wrapper = item->widget();
        if (!wrapper) continue;
        auto *box = wrapper->findChild<QWidget*>("groupBox");
        if (box) {
            QString name = box->property("groupName").toString();
            if (!name.isEmpty()) names << name;
        }
    }
    return names;
}

void Sidebar::addPathToGroup(const QString &groupName, const QString &path)
{
    if (!m_contentLayout) return;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *item = m_contentLayout->itemAt(i);
        if (!item) continue;
        auto *wrapper = item->widget();
        if (!wrapper) continue;
        auto *box = wrapper->findChild<QWidget*>("groupBox");
        if (box && box->property("groupName").toString() == groupName) {
            auto *list = box->findChild<QListWidget*>();
            if (list) {
                addToGroup(groupName, list, path);
                return;
            }
        }
    }
}

void Sidebar::addNetworkPlace(const QString &path, const QString &name)
{
    if (path.isEmpty()) return;
    auto s = Config::group("NetworkPlaces");
    QStringList saved = s.readEntry("places", QStringList());
    const QString npath = sc_normalizePath(path);
    if (!saved.contains(npath)) {
        saved << npath;
        s.writeEntry("places", saved);
        s.writeEntry("name_" + QString(npath).replace("/","_").replace(":","_"), name);
        s.config()->sync();
    }
    updateDrives();
    emit drivesChanged();
}

// --- Sidebar::showPlaceContextMenu ---
void Sidebar::showPlaceContextMenu(QListWidgetItem *item, QListWidget *list,
                                   const QPoint &pos, const QString &groupName)
{
    const QString path = item->data(Qt::UserRole).toString();
    const QString name = item->text();
    const QString icon = item->data(Qt::UserRole + 2).toString();

    QMenu menu(this);
    sc_applyMenuShadow(&menu);
    menu.setStyleSheet(TM().ssMenu());

    auto *openIn = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
    openIn->setStyleSheet(TM().ssMenu());
    openIn->addAction(tr("Linke Pane"),  this, [this, path]() { emit driveClicked(path); });
    openIn->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });

    auto saveList = [list, groupName]() {
        if (!groupName.isEmpty()) {
            auto gs = Config::group("CustomGroups");
            KConfigGroup(gs.config(), gs.name() + "/group_" + groupName).deleteGroup();
            KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
            gNew.writeEntry("size", list->count());

            for (int i = 0; i < list->count(); ++i) {
                KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));
                itemG.writeEntry("path", list->item(i)->data(Qt::UserRole).toString());
                itemG.writeEntry("name", list->item(i)->text());
                itemG.writeEntry("icon", list->item(i)->data(Qt::UserRole + 2).toString());
            }
            gs.config()->sync();
        }
    };

    sc_buildPlaceMenu(menu, path, name, icon,
        [list, item, saveList]() {
            delete list->takeItem(list->row(item));
            adjustListHeight(list);
            saveList();
        },
        [item, saveList](const QString &newName, const QString &newPath, const QString &newIcon) {
            item->setText(newName);
            item->setData(Qt::UserRole, newPath);
            item->setData(Qt::UserRole + 2, newIcon);
            
            // Icon aktualisieren
            if (!newIcon.isEmpty()) {
                item->setIcon(QIcon::fromTheme(newIcon));
            } else {
                // Fallback auf Standard-Icon falls manuelles Icon gelöscht wurde
                QFileIconProvider ip;
                item->setIcon(ip.icon(QFileInfo(newPath)));
            }
            saveList();
        });

    menu.exec(list->mapToGlobal(pos));
}

// --- Sidebar::setupTags / addTagItem / saveTags ---
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
    for (const auto &t : TagManager::instance().tags()) {
        addTagItem(t.first, t.second, QString());
    }

 
    adjustListHeight(m_tagList);
    if (m_tagsBox)  m_tagsBox->updateGeometry();
    if (m_tagsWrap) m_tagsWrap->updateGeometry();
 
    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit tagClicked(it->text());
        QTimer::singleShot(150, m_tagList, [this]() {
            m_tagList->clearSelection();
        });
    });



    m_tagList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tagList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_tagList->itemAt(pos);
        if (!item) return;
        QMenu menu(this);
        sc_applyMenuShadow(&menu);
        menu.setStyleSheet(TM().ssMenu());

        menu.addAction(QIcon::fromTheme(QStringLiteral("color-picker")), tr("Farbe ändern …"), this,
            [this, item]() {
                QColorDialog dlg(QColor(item->data(Qt::UserRole).toString()), this);
                dlg.setWindowTitle(tr("Farbe wählen"));
                dlg.setOptions(QColorDialog::DontUseNativeDialog);
                dlg.setStyleSheet(TM().ssDialog());
                if (dlg.exec() != QDialog::Accepted) return;
                QColor col = dlg.currentColor();
                if (!col.isValid()) return;
                item->setData(Qt::UserRole, col.name());
                QPixmap pix(10, 10); pix.fill(Qt::transparent);
                QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(col); p.setPen(Qt::NoPen); p.drawEllipse(0, 0, 10, 10);
                item->setIcon(QIcon(pix));
                saveTags();
            });
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen …"), this,
            [this, item]() {
                bool ok;
                QString name = sc_getText(this, tr("Tag umbenennen"), tr("Name:"), item->text());
                ok = !name.isNull();
                if (ok && !name.isEmpty()) { item->setText(name); saveTags(); }
            });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Löschen"), this,
            [this, item]() {
                delete m_tagList->takeItem(m_tagList->row(item));
                adjustListHeight(m_tagList);
                if (m_tagsBox)  m_tagsBox->updateGeometry();
                if (m_tagsWrap) m_tagsWrap->updateGeometry();
                saveTags();
            });

        menu.exec(m_tagList->mapToGlobal(pos));
    });
}

void Sidebar::saveTags()
{
    auto s = Config::group("Tags");
    KConfigGroup(s.config(), s.name() + "/tags").deleteGroup();
    KConfigGroup tagsG(s.config(), s.name() + "/tags");
    tagsG.writeEntry("size", m_tagList->count());
    for (int i = 0; i < m_tagList->count(); ++i) {
        KConfigGroup tG(tagsG.config(), tagsG.name() + "/" + QString::number(i + 1));
        tG.writeEntry("name",  m_tagList->item(i)->text());
        tG.writeEntry("color", m_tagList->item(i)->data(Qt::UserRole).toString());
        tG.writeEntry("font",  m_tagList->item(i)->data(Qt::UserRole + 1).toString());
    }
    s.config()->sync();
}



// --- Sidebar — leere Stubs (Interface-Kompatibilität) ---
void Sidebar::setupPlaces()  {}
void Sidebar::setupRemotes() {}
void Sidebar::savePlaces(QListWidget **list)
{
    if (!list || !*list) return;
    auto gs = Config::group("CustomGroups");
    KConfigGroup g(gs.config(), gs.name() + "/group_Favoriten");
    g.deleteGroup();
    int idx = 1;
    for (int i = 0; i < (*list)->count(); ++i) {
        auto *item = (*list)->item(i);
        if (item->data(Qt::UserRole + 1).toBool()) continue;
        KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(idx++));
        itemG.writeEntry("path", item->data(Qt::UserRole).toString());
        itemG.writeEntry("name", item->text());
    }
    g.writeEntry("size", idx - 1);
    adjustListHeight(*list);
    gs.config()->sync();
}


// --- GroupDragHandle — Implementierung ---
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

void Sidebar::onTrashChanged() {
    if (!m_trashLister) return;
    const bool isEmpty = m_trashLister->items().isEmpty();
    const QString iconName = isEmpty ? QStringLiteral("user-trash") : QStringLiteral("user-trash-full");
    QIcon trashIcon = QIcon::fromTheme(iconName);
    if (trashIcon.isNull()) trashIcon = QIcon::fromTheme(isEmpty ? QStringLiteral("trash-empty") : QStringLiteral("trash-full"));

    // Alle QListWidgets in der Sidebar durchlaufen
    QList<QListWidget*> lists;
    if (m_driveList) lists << m_driveList;
    if (m_favList) lists << m_favList;
    if (m_netList) lists << m_netList;
    
    // Benutzerdefinierte Gruppen finden
    if (m_contentLayout) {
        for (int i = 0; i < m_contentLayout->count(); ++i) {
            if (auto *box = m_contentLayout->itemAt(i)->widget()) {
                if (auto *lw = box->findChild<QListWidget*>()) {
                    if (!lists.contains(lw)) lists << lw;
                }
            }
        }
    }

    for (auto *list : lists) {
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            const QString path = item->data(Qt::UserRole).toString();
            if (QUrl(path).scheme() == QStringLiteral("trash")) {
                item->setIcon(trashIcon);
            }
        }
    }
}
