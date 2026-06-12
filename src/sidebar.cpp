#include "sidebar.h"
#include "addnetworkdialog.h"
#include "config.h"
#include "dialogutils.h"
#include "drivedelegate.h"
#include "drivemanager.h"
#include "hoverfader.h"
#include "scglobal.h"
#include "thememanager.h"
#ifdef SC_PLUGIN_GIT
#include "plugins/git/gitstatusmanager.h"
#endif

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
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
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <KDialogJobUiDelegate>
#include <KDirLister>
#include <KDirWatch>
#include <KFile>
#include <KIO/CopyJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/ListJob>
#include <KIO/StoredTransferJob>
#include <KIconDialog>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>
#include <KUrlRequester>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

static QString sc_getText(QWidget *parent, const QString &title, const QString &label, const QString &defaultText = QString())
{
    QDialog dlg(parent);
    dlg.setAttribute(Qt::WA_StyledBackground, true);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    auto *lay = new QVBoxLayout(&dlg);
    Q_ASSERT(lay != nullptr);
    lay->setSpacing(4);
    lay->setContentsMargins(12, 10, 12, 10);
    auto *lbl = new QLabel(label, &dlg);
    Q_ASSERT(lbl != nullptr);
    auto *edit = new QLineEdit(defaultText, &dlg);
    Q_ASSERT(edit != nullptr);
    auto *btnRow = new QHBoxLayout();
    Q_ASSERT(btnRow != nullptr);
    auto *ok = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok")), QCoreApplication::translate("SplitCommander", "OK"), &dlg);
    auto *can = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QCoreApplication::translate("SplitCommander", "Abbrechen"), &dlg);
    Q_ASSERT(ok != nullptr && can != nullptr);
    ok->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    btnRow->addWidget(can);
    lay->addWidget(lbl);
    lay->addWidget(edit);
    lay->addLayout(btnRow);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(can, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.adjustSize();
    if (parent != nullptr)
    {
        QPoint center = parent->mapToGlobal(parent->rect().center());
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    if (dlg.exec() != QDialog::Accepted)
    {
        return {};
    }
    return edit->text();
}

static QPushButton* sc_buildEditPlaceIconBtn(QWidget *parent, QString &currentIcon, const QString &icon)
{
    auto *iconBtn = new QPushButton(parent);
    Q_ASSERT(iconBtn != nullptr);
    currentIcon = icon.isEmpty() ? QStringLiteral("folder") : icon;
    iconBtn->setIcon(QIcon::fromTheme(currentIcon));
    iconBtn->setFixedSize(64, 64);
    iconBtn->setIconSize(QSize(32, 32));
    iconBtn->setCursor(Qt::PointingHandCursor);
    iconBtn->setStyleSheet(QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 8px; }"
                                         "QPushButton:hover { background: %3; }")
                              .arg(TM().colors().bgBox, TM().colors().border, TM().colors().bgHover));
    return iconBtn;
}

static void sc_buildEditPlaceForm(QFormLayout *form, const QString &name, const QString &path, QLineEdit *&nameEdit, KUrlRequester *&urlReq, QWidget *parent)
{
    nameEdit = new QLineEdit(name, parent);
    Q_ASSERT(nameEdit != nullptr);
    form->addRow(QObject::tr("Name:"), nameEdit);

    urlReq = new KUrlRequester(QUrl::fromUserInput(path), parent);
    Q_ASSERT(urlReq != nullptr);
    urlReq->setMode(KFile::Directory | KFile::File | KFile::LocalOnly);
    form->addRow(QObject::tr("Adresse:"), urlReq);
}

static void sc_connectEditPlaceIconBtn(QPushButton *iconBtn, QDialog *dlg, QString &currentIcon)
{
    QObject::connect(iconBtn, &QPushButton::clicked, [iconBtn, dlg, &currentIcon]()
    {
        KIconDialog iconDlg(dlg);
        iconDlg.setSelectedIcon(currentIcon);
        QString newIcon = iconDlg.openDialog();
        if (!newIcon.isEmpty())
        {
            currentIcon = newIcon;
            iconBtn->setIcon(QIcon::fromTheme(newIcon));
        }
    });
}

static bool sc_editPlaceDialog(QWidget *parent, QString &name, QString &path, QString &icon)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Eintrag bearbeiten"));
    dlg.setFixedWidth(500);

    auto *mainVl = new QVBoxLayout(&dlg);
    Q_ASSERT(mainVl != nullptr);
    mainVl->setSizeConstraint(QLayout::SetFixedSize);
    mainVl->setContentsMargins(12, 12, 12, 12);
    mainVl->setSpacing(12);

    auto *topHl = new QHBoxLayout();
    Q_ASSERT(topHl != nullptr);
    topHl->setSpacing(12);

    QString currentIcon;
    auto *iconBtn = sc_buildEditPlaceIconBtn(&dlg, currentIcon, icon);
    topHl->addWidget(iconBtn, 0, Qt::AlignTop);

    auto *form = new QFormLayout();
    Q_ASSERT(form != nullptr);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    QLineEdit *nameEdit = nullptr;
    KUrlRequester *urlReq = nullptr;
    sc_buildEditPlaceForm(form, name, path, nameEdit, urlReq, &dlg);
    
    topHl->addLayout(form, 1);
    mainVl->addLayout(topHl);

    sc_connectEditPlaceIconBtn(iconBtn, &dlg, currentIcon);

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    Q_ASSERT(bbox != nullptr);
    mainVl->addWidget(bbox);

    QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
    {
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
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-entry")), QObject::tr("Bearbeiten..."), [editAction, path, name, icon]()
    {
        QString n = name;
        QString p = path;
        QString i = icon;
        if (sc_editPlaceDialog(nullptr, n, p, i))
        {
            editAction(n, p, i);
        }
    });

    menu.addSeparator();

    menu.addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), QObject::tr("In Terminal öffnen"), [path]()
    {
        auto *job = new KTerminalLauncherJob(QString());
        Q_ASSERT(job != nullptr);
        job->setWorkingDirectory(path);
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
        job->start();
    });
    
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QObject::tr("Pfad kopieren"), [path]()
    {
        QGuiApplication::clipboard()->setText(path);
    });

    if (removeAction)
    {
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), QObject::tr("Aus Gruppe entfernen"), removeAction);
    }

    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QObject::tr("Eigenschaften"), [path]()
    {
        KPropertiesDialog::showDialog(QUrl::fromUserInput(path));
    });
}

void Sidebar::adjustListHeight(QListWidget *list)
{
    if (list == nullptr)
    {
        return;
    }
    const int n = list->count();

    if (n == 0)
    {
        list->setFixedHeight(0);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    else
    {
        list->setFixedHeight(n * Config::sidebarRowHeight());
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    list->updateGeometry();
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("background-color:%1; border:none;").arg(TM().colors().bgMain));

    auto *outerLay = new QVBoxLayout(this);
    Q_ASSERT(outerLay != nullptr);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    buildLogo(outerLay);
    buildDrivesSection(outerLay);
    buildGroupsSection(outerLay);
    buildNewGroupFixedSection(outerLay);
    buildTagsSection(outerLay);

    setupTags();
    loadUserPlaces();
    DriveManager::instance()->refreshAll();
    connectDriveList();
    setupDriveContextMenu();
    
    connect(DriveManager::instance(), &DriveManager::drivesUpdated, this, [this]()
    {
        updateDrives();
        emit drivesChanged();
    });

#ifdef SC_PLUGIN_GIT
    connect(&GitStatusManager::instance(), &GitStatusManager::statusUpdated, this, [this](const QString &)
    {
        refreshGitSection();
    });
#endif

    m_trashLister = new KDirLister(this);
    Q_ASSERT(m_trashLister != nullptr);
    connect(m_trashLister, &KDirLister::completed, this, &Sidebar::onTrashChanged);
    connect(m_trashLister, &KDirLister::itemsAdded, this, &Sidebar::onTrashChanged);
    connect(m_trashLister, &KDirLister::itemsDeleted, this, &Sidebar::onTrashChanged);
    m_trashLister->openUrl(QUrl(QStringLiteral("trash:/")), KDirLister::Keep);
}

void Sidebar::setupLogoIcon(QHBoxLayout *lay)
{
    Q_ASSERT(lay != nullptr);
    auto *iconLabel = new QLabel();
    Q_ASSERT(iconLabel != nullptr);
    QPixmap pix;

    QIcon themeIcon = QIcon::fromTheme(QStringLiteral("splitcommander"));
    if (!themeIcon.isNull())
    {
        pix = themeIcon.pixmap(32, 32);
    }

    if (pix.isNull())
    {
        QString iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../src/splitcommander_64.png");
        if (!QFile::exists(iconPath))
        {
            iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/splitcommander_64.png");
        }
        pix = QPixmap(iconPath);
    }

    if (pix.isNull())
    {
        pix = QIcon::fromTheme(QStringLiteral("system-file-manager")).pixmap(32, 32);
    }

    iconLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(32, 32);
    iconLabel->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    lay->addWidget(iconLabel);
}

void Sidebar::buildLogo(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    auto *wrapper = new QWidget(this);
    Q_ASSERT(wrapper != nullptr);
    wrapper->setStyleSheet(TM().ssSidebar());
    auto *lay = new QHBoxLayout(wrapper);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(12, 6, 12, 4);
    lay->setSpacing(8);

    setupLogoIcon(lay);

    auto *nameLbl = new QLabel(
        QStringLiteral("<span style='font-weight:200;color:#ccd4e8;font-size:12px;'>Split</span>"
                       "<span style='font-weight:600;color:#88c0d0;font-size:12px;'>Commander</span>"
                       "<span style='color:#4c566a;font-size:9px;'> | Dateimanager</span>"));
    Q_ASSERT(nameLbl != nullptr);
    nameLbl->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    lay->addWidget(nameLbl);
    lay->addStretch();

    auto *layoutBtn = new QToolButton();
    Q_ASSERT(layoutBtn != nullptr);
    layoutBtn->setFixedSize(32, 32);
    layoutBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    layoutBtn->setIconSize(QSize(32, 32));
    layoutBtn->setToolTip(tr("Layout wählen"));
    layoutBtn->setStyleSheet(TM().ssToolBtn());
    lay->addWidget(layoutBtn);

    connect(layoutBtn, &QToolButton::clicked, this, [this, layoutBtn]()
    {
        showLayoutMenu(layoutBtn);
    });

    parent->addWidget(wrapper);
}

static QPushButton* sc_buildLayoutModeButton(const QString &label, const QString &sub, const QString &icon, int mode, int current)
{
    auto *btn = new QPushButton();
    Q_ASSERT(btn != nullptr);
    btn->setCheckable(true);
    btn->setChecked(mode == current);
    btn->setFixedSize(72, 68);

    auto *vl = new QVBoxLayout(btn);
    Q_ASSERT(vl != nullptr);
    vl->setContentsMargins(4, 6, 4, 4);
    vl->setSpacing(3);
    
    auto *ic = new QLabel();
    Q_ASSERT(ic != nullptr);
    ic->setPixmap(QIcon::fromTheme(icon).pixmap(24, 24));
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    
    auto *lb1 = new QLabel(label);
    Q_ASSERT(lb1 != nullptr);
    lb1->setAlignment(Qt::AlignCenter);
    lb1->setStyleSheet(QStringLiteral("background:transparent;border:none;font-weight:bold;font-size:10px;"));
    
    auto *lb2 = new QLabel(sub);
    Q_ASSERT(lb2 != nullptr);
    lb2->setAlignment(Qt::AlignCenter);
    lb2->setStyleSheet(QStringLiteral("background:transparent;border:none;color:%1;font-size:9px;").arg(TM().colors().textMuted));
    
    vl->addWidget(ic);
    vl->addWidget(lb1);
    vl->addWidget(lb2);
    
    return btn;
}

void Sidebar::setupLayoutMenuModes(QButtonGroup *grp, QDialog *popup, int current)
{
    Q_ASSERT(grp != nullptr && popup != nullptr);

    struct ModeEntry
    {
        QString label, sub, icon;
        int mode;
    };
    const QList<ModeEntry> modes = {
        {tr("Klassisch"), tr("Einzeln"), QStringLiteral("view-list-details"), 0},
        {tr("Standard"), tr("Dual"), QStringLiteral("view-split-left-right"), 1},
        {tr("Spalten"), tr("Dual"), QStringLiteral("view-split-top-bottom"), 2},
    };

    auto *lay2 = popup->layout();
    Q_ASSERT(lay2 != nullptr);

    for (const auto &entry : modes)
    {
        auto *btn = sc_buildLayoutModeButton(entry.label, entry.sub, entry.icon, entry.mode, current);
        grp->addButton(btn, entry.mode);
        lay2->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, popup, mode = entry.mode]()
        {
            auto ss = Config::group("UI");
            ss.writeEntry(QStringLiteral("layoutMode"), mode);
            ss.config()->sync();
            emit layoutChangeRequested(mode);
            popup->close();
        });
    }
}

void Sidebar::showLayoutMenu(QWidget *anchor)
{
    QWidget *posAnchor = (anchor != nullptr) ? anchor : this;
    auto *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    Q_ASSERT(popup != nullptr);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    const auto &c = TM().colors();
    popup->setStyleSheet(
        TM().ssDialog() +
        QStringLiteral("QPushButton { background:%1; border:1px solid %2; color:%3;"
                       " border-radius:4px; padding:8px; font-size:10px; }"
                       "QPushButton:hover { background:%4; border-color:%5; }"
                       "QPushButton:checked { background:%4; border:2px solid %5; color:%6; }")
        .arg(c.bgInput, c.borderAlt, c.textPrimary, c.bgHover, c.accent, c.textAccent));

    auto *lay2 = new QHBoxLayout(popup);
    Q_ASSERT(lay2 != nullptr);
    lay2->setContentsMargins(8, 8, 8, 8);
    lay2->setSpacing(6);

    auto *grp = new QButtonGroup(popup);
    Q_ASSERT(grp != nullptr);
    auto s = Config::group("UI");
    int current = s.readEntry("layoutMode", 1);

    setupLayoutMenuModes(grp, popup, current);

    QPoint targetPos = posAnchor->mapToGlobal(QPoint(0, posAnchor->height() + 2));
    if (posAnchor->mapToGlobal(QPoint(0, 0)).y() > posAnchor->window()->height() - 150)
    {
        targetPos = posAnchor->mapToGlobal(QPoint(0, -86));
    }
    popup->move(targetPos);
    popup->exec();
}

void Sidebar::setupDrivesList(QVBoxLayout *listLay)
{
    Q_ASSERT(listLay != nullptr);
    m_driveList = new QListWidget();
    Q_ASSERT(m_driveList != nullptr);
    m_driveList->setSelectionMode(QAbstractItemView::NoSelection);
    m_driveList->setFrameShape(QFrame::NoFrame);
    m_driveList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->setIconSize(QSize(Config::driveIconSize(), Config::driveIconSize()));
    m_driveList->setStyleSheet(TM().ssListWidget());
    
    auto *driveDel = new DriveDelegate(true, this);
    Q_ASSERT(driveDel != nullptr);
    driveDel->setHoverFader(new HoverFader(m_driveList, driveDel));
    m_driveList->setItemDelegate(driveDel);
    listLay->addWidget(m_driveList);
}

void Sidebar::setupNetList(QVBoxLayout *netWLay)
{
    Q_ASSERT(netWLay != nullptr);
    m_netList = new QListWidget();
    Q_ASSERT(m_netList != nullptr);
    m_netList->setSelectionMode(QAbstractItemView::NoSelection);
    m_netList->setFrameShape(QFrame::NoFrame);
    m_netList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_netList->setIconSize(QSize(Config::driveIconSize(), Config::driveIconSize()));
    m_netList->setStyleSheet(TM().ssListWidget());
    
    auto *netDel = new DriveDelegate(true, this);
    Q_ASSERT(netDel != nullptr);
    netDel->setHoverFader(new HoverFader(m_netList, netDel));
    m_netList->setItemDelegate(netDel);
    netWLay->addWidget(m_netList);
}

void Sidebar::buildDrivesHeader(QVBoxLayout *vbox, QLabel *&lbl, QPushButton *&menuBtn)
{
    auto *header = new QWidget();
    Q_ASSERT(header != nullptr);
    header->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *hLay = new QHBoxLayout(header);
    Q_ASSERT(hLay != nullptr);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(0);

    auto driveBoxSettings = Config::group("UI");
    QString driveBoxLabel = driveBoxSettings.readEntry("driveBoxLabel", QStringLiteral("Laufwerke"));
    if (driveBoxLabel == QStringLiteral("Laufwerke"))
    {
        driveBoxLabel = tr("Laufwerke");
    }
    lbl = new QLabel(driveBoxLabel);
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QStringLiteral("font-size:14px;font-weight:normal;background:transparent;color:%1;")
                          .arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    menuBtn = new QPushButton();
    Q_ASSERT(menuBtn != nullptr);
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    if (menuBtn->icon().isNull())
    {
        menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    }
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(QStringLiteral("QPushButton { background: transparent; border: none; padding: 2px; }"
                                         "QPushButton:hover { background: %1; border-radius: 4px; }")
                              .arg(TM().colors().bgHover));
    hLay->addWidget(menuBtn);
    vbox->addWidget(header);
}

void Sidebar::buildDrivesLists(QVBoxLayout *vbox, QWidget *&listCont)
{
    listCont = new QWidget();
    Q_ASSERT(listCont != nullptr);
    listCont->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *listLay = new QVBoxLayout(listCont);
    Q_ASSERT(listLay != nullptr);
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

    setupDrivesList(listLay);

    m_netBox = new QWidget();
    Q_ASSERT(m_netBox != nullptr);
    m_netBox->setVisible(false);
    auto *netWLay = new QVBoxLayout(m_netBox);
    Q_ASSERT(netWLay != nullptr);
    netWLay->setContentsMargins(6, 0, 6, 4);
    m_netBox->setStyleSheet(QStringLiteral("margin-top:0px; padding-top:0px;"));
    netWLay->setSpacing(0);

    setupNetList(netWLay);

    vbox->addWidget(listCont);
    vbox->addWidget(m_netBox);
}

void Sidebar::buildDrivesToggle(QVBoxLayout *vbox, QWidget *listCont)
{
    auto *toggleBtn = new QPushButton();
    Q_ASSERT(toggleBtn != nullptr);
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    toggleBtn->setIconSize(QSize(10, 10));
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(12);
    toggleBtn->setStyleSheet(QStringLiteral("QPushButton{background:transparent !important;font-size:7px;border:none;color:%1;}")
                                .arg(TM().colors().textMuted));
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    connect(toggleBtn, &QPushButton::toggled, this, [this, listCont, toggleBtn](bool on)
    {
        listCont->setVisible(!on);
        if (m_netBox != nullptr)
        {
            m_netBox->setVisible(!on);
        }
        toggleBtn->setIcon(QIcon::fromTheme(on ? QStringLiteral("go-down") : QStringLiteral("go-up")));
    });
}

void Sidebar::buildDrivesSection(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    auto *wrapper = new QWidget(this);
    Q_ASSERT(wrapper != nullptr);
    wrapper->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    Q_ASSERT(wLay != nullptr);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);

    auto *box = new QWidget(wrapper);
    Q_ASSERT(box != nullptr);
    box->setObjectName(QStringLiteral("outerBox"));
    box->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(box);
    Q_ASSERT(vbox != nullptr);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    QLabel *lbl = nullptr;
    QPushButton *menuBtn = nullptr;
    buildDrivesHeader(vbox, lbl, menuBtn);

    QWidget *listCont = nullptr;
    buildDrivesLists(vbox, listCont);

    buildDrivesToggle(vbox, listCont);

    wLay->addWidget(box);
    parent->addWidget(wrapper);

    connect(m_netList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it)
    {
        Q_ASSERT(it != nullptr);
        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty()) emit driveClicked(p);
    });

    m_netList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_netList, &QListWidget::customContextMenuRequested, this, &Sidebar::showNetListContextMenu);
    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl]()
    {
        showDrivesMenu(menuBtn, lbl);
    });
}

void Sidebar::addNetworkPlace(const QString &path, const QString &name)
{
    auto s = Config::group("NetworkPlaces");
    QStringList saved = s.readEntry("places", QStringList());
    const QString npath = mw_normalizePath(path);
    if (!saved.contains(npath))
    {
        saved << npath;
        s.writeEntry("places", saved);
        s.writeEntry("name_" + QString(npath).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')), name);
        s.config()->sync();
        saveToUserPlaces(npath, name);
    }
    DriveManager::instance()->refreshAll();
    emit drivesChanged();
}

void Sidebar::removeNetworkPlace(const QString &path)
{
    auto s = Config::group("NetworkPlaces");
    QStringList saved = s.readEntry("places", QStringList());
    const QString npath = mw_normalizePath(path);
    saved.removeAll(npath);
    
    QString otherVersion = npath.endsWith(QLatin1Char('/')) ? npath.left(npath.length() - 1) : npath + QLatin1Char('/');
    if (otherVersion != QStringLiteral("/"))
    {
        saved.removeAll(otherVersion);
    }

    s.writeEntry("places", saved);
    const QString key1 = QString(npath).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
    const QString key2 = QString(otherVersion).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
    s.deleteEntry("name_" + key1);
    s.deleteEntry("name_" + key2);
    s.deleteEntry("icon_" + key1);
    s.deleteEntry("icon_" + key2);
    s.config()->sync();

    const QString xbelPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");
    QFile f(xbelPath);
    if (f.open(QIODevice::ReadOnly))
    {
        QByteArray data = f.readAll();
        f.close();
        QUrl checkUrl(npath);
        checkUrl.setUserInfo(QString());
        QUrl checkUrl2(otherVersion);
        checkUrl2.setUserInfo(QString());
        for (const QUrl &u : {QUrl(npath), QUrl(otherVersion), checkUrl, checkUrl2})
        {
            const QString tag = QStringLiteral("href=\"%1\"").arg(u.toString());
            int start = data.indexOf(tag.toUtf8());
            if (start < 0) continue;
            int bStart = data.lastIndexOf("<bookmark", start);
            int bEnd = data.indexOf("</bookmark>", start);
            if (bStart >= 0 && bEnd >= 0)
            {
                data.remove(bStart, bEnd - bStart + 11);
            }
        }
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            f.write(data);
        }
    }

    DriveManager::instance()->refreshAll();
    emit drivesChanged();
    emit removeFromPlacesRequested(path);
}

void Sidebar::buildNetListContextMenuOpenActions(QMenu &menu, const QString &path)
{
    menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this, [this, path]() { emit driveClicked(path); });
    auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
    Q_ASSERT(openInMenu != nullptr);
    openInMenu->setStyleSheet(TM().ssMenu());
    openInMenu->addAction(tr("Linke Pane"), this, [this, path]() { emit driveClickedLeft(path); });
    openInMenu->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });
    menu.addSeparator();
}

void Sidebar::buildNetListContextMenuActionPlaces(QMenu &menu, const QString &path)
{
    const bool isKioPlace = !path.startsWith(QStringLiteral("/")) && !path.startsWith(QStringLiteral("solid:")) && path.contains(QStringLiteral(":/"));
    if (isKioPlace)
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Aus Laufwerken entfernen"), this, [this, path]()
        {
            removeNetworkPlace(path);
        });
    }
    else
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Trennen"), this, [this, path]()
        {
            emit unmountRequested(path);
        });
    }
}

bool Sidebar::isNetworkPlaceAlreadySaved(const QString &path)
{
    auto netCheck = Config::group("NetworkPlaces");
    const QStringList netPlaces = netCheck.readEntry("places", QStringList());
    const QString npath = mw_normalizePath(path);
    return std::any_of(netPlaces.begin(), netPlaces.end(), [&npath](const QString &p) {
        return mw_normalizePath(p) == npath;
    });
}

void Sidebar::showNetListContextMenu(const QPoint &pos)
{
    Q_ASSERT(m_netList != nullptr);
    auto *item = m_netList->itemAt(pos);
    if (item == nullptr) return;
    const QString path = item->data(Qt::UserRole).toString();
    const QString name = item->text();

    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());
    buildNetListContextMenuOpenActions(menu, path);

    if (isNetworkPlaceAlreadySaved(path))
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"), this, [this, path, name]()
        {
            bool ok;
            const QString newName = DialogUtils::getText(this, tr("Umbenennen"), tr("Anzeigename:"), name, &ok);
            if (ok && !newName.trimmed().isEmpty()) renameNetworkPlace(path, newName.trimmed());
        });
        menu.addSeparator();
    }
    else
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), tr("Zu Laufwerken hinzufügen"), this, [this, path, name]()
        {
            addNetworkPlace(path, name);
        });
    }

    buildNetListContextMenuActionPlaces(menu, path);
    
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Pfad kopieren"), this, [path]()
    {
        QGuiApplication::clipboard()->setText(path);
    });
    
    menu.exec(m_netList->mapToGlobal(pos));
}

void Sidebar::handleDrivesMenuAction(const QString &actionName, QPushButton *menuBtn, QLabel *lbl)
{
    Q_ASSERT(menuBtn != nullptr);
    Q_ASSERT(lbl != nullptr);

    if (actionName == QStringLiteral("rename"))
    {
        bool ok;
        QString name = sc_getText(this, tr("Box umbenennen"), tr("Name:"), lbl->text());
        ok = !name.isNull();
        if (ok && !name.isEmpty())
        {
            lbl->setText(name);
            auto s = Config::group("UI");
            s.writeEntry(QStringLiteral("driveBoxLabel"), name);
            s.config()->sync();
        }
    }
    else if (actionName == QStringLiteral("add_smb"))
    {
        auto *dlg = new AddNetworkDialog(this);
        Q_ASSERT(dlg != nullptr);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &QDialog::accepted, this, [this, dlg]()
        {
            const QString url = dlg->url();
            const QString name = dlg->name();
            const QString icon = dlg->iconName();
            if (url.isEmpty())
            {
                return;
            }
            auto s = Config::group("NetworkPlaces");
            QStringList saved = s.readEntry("places", QStringList());
            const QString key = QString(url).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
            if (!saved.contains(url))
            {
                saved << url;
                s.writeEntry("places", saved);
                s.writeEntry("name_" + key, name);
                s.writeEntry("icon_" + key, icon);
                s.config()->sync();
                saveToUserPlaces(url, name);
            }
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
        });
        dlg->open();
    }
}

void Sidebar::showDrivesMenu(QPushButton *menuBtn, QLabel *lbl)
{
    Q_ASSERT(menuBtn != nullptr);
    Q_ASSERT(lbl != nullptr);

    auto *m = new QMenu(this);
    Q_ASSERT(m != nullptr);
    m->setStyleSheet(TM().ssMenu());
    
    m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Box umbenennen …"), this, [this, menuBtn, lbl]()
    {
        handleDrivesMenuAction(QStringLiteral("rename"), menuBtn, lbl);
    });
    
    m->addSeparator();
    
    m->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Alles aktualisieren"), this, []()
    {
        DriveManager::instance()->refreshAll();
    })->setShortcut(Qt::Key_F5);
    
    m->addSeparator();
    
    m->addAction(QIcon::fromTheme(QStringLiteral("network-connect")), tr("Netzwerklaufwerk verbinden"), this, [this]()
    {
        emit driveClicked(QStringLiteral("remote:/"));
    });
    
    m->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), tr("SMB Laufwerke verbinden"), this, [this, menuBtn, lbl]()
    {
        handleDrivesMenuAction(QStringLiteral("add_smb"), menuBtn, lbl);
    });
    
    m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
}

void Sidebar::buildGroupsSection(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    m_scrollArea = new QScrollArea(this);
    Q_ASSERT(m_scrollArea != nullptr);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QStringLiteral("QScrollArea{border:none;background:%1;}").arg(TM().colors().bgMain));
    m_scrollArea->verticalScrollBar()->hide();
    m_scrollArea->horizontalScrollBar()->hide();

    auto *scrollWidget = new QWidget();
    Q_ASSERT(scrollWidget != nullptr);
    scrollWidget->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    m_scrollArea->setWidget(scrollWidget);

    m_contentLayout = new QVBoxLayout(scrollWidget);
    Q_ASSERT(m_contentLayout != nullptr);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    loadCustomGroups();

    m_contentLayout->addStretch(1);
    parent->addWidget(m_scrollArea, 1);

    m_overlayBar = new QScrollBar(Qt::Vertical, this);
    Q_ASSERT(m_overlayBar != nullptr);
    m_overlayBar->setStyleSheet(
        QStringLiteral("QScrollBar:vertical{background:transparent;width:8px;margin:0px;border:none;}"
                       "QScrollBar::handle:vertical{background:%1;border-radius:4px;min-height:20px;margin:1px;}"
                       "QScrollBar::handle:vertical:hover{background:%2;}"
                       "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
                       "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}")
        .arg(TM().colors().separator, TM().colors().accent));
    m_overlayBar->hide();
    m_overlayBar->raise();

    auto *native = m_scrollArea->verticalScrollBar();
    Q_ASSERT(native != nullptr);
    connect(native, &QScrollBar::rangeChanged, m_overlayBar, &QScrollBar::setRange);
    connect(native, &QScrollBar::valueChanged, m_overlayBar, &QScrollBar::setValue);
    connect(m_overlayBar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);
}

void Sidebar::buildNewGroupFixedSection(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    auto *ngWrapper = new QWidget(this);
    Q_ASSERT(ngWrapper != nullptr);
    ngWrapper->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    auto *ngWLay = new QVBoxLayout(ngWrapper);
    Q_ASSERT(ngWLay != nullptr);
    ngWLay->setContentsMargins(10, 2, 6, 2);
    ngWLay->setSpacing(0);

    m_newGroupBox = new QWidget(ngWrapper);
    Q_ASSERT(m_newGroupBox != nullptr);
    m_newGroupBox->setObjectName(QStringLiteral("ngBox"));
    m_newGroupBox->setStyleSheet(TM().ssBox());

    auto *ngLay = new QVBoxLayout(m_newGroupBox);
    Q_ASSERT(ngLay != nullptr);
    ngLay->setContentsMargins(6, 6, 6, 6);
    ngLay->setSpacing(0);

    auto *ngBtn = new QPushButton(tr("+ Neue Gruppe"));
    Q_ASSERT(ngBtn != nullptr);
    ngBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ngBtn->setStyleSheet(
        QStringLiteral("QPushButton { background:%1; border:none; color:%2; font-size:11px; padding:8px; text-align:center; border-radius:4px; }"
                       "QPushButton:hover { background:%3; color:%4; }")
        .arg(TM().colors().bgList, TM().colors().textPrimary, TM().colors().bgHover, TM().colors().textLight));

    ngLay->addWidget(ngBtn);
    ngWLay->addWidget(m_newGroupBox);

    parent->addWidget(ngWrapper);

    connect(ngBtn, &QPushButton::clicked, this, &Sidebar::onNewGroupDialog);
}

#ifdef SC_PLUGIN_GIT
static int sc_updateGitRepoColors(QTreeWidgetItem *item, QTreeWidgetItem *prnt, const ThemeManager &tm)
{
    int worst = item->data(0, Qt::UserRole + 1).toInt();
    for (int i = 0; i < item->childCount(); ++i)
    {
        int childWorst = sc_updateGitRepoColors(item->child(i), prnt, tm);
        if (childWorst > worst)
        {
            worst = childWorst;
        }
    }
    item->setData(0, Qt::UserRole + 1, worst);

    if (item != prnt)
    {
        QColor c;
        switch ((GitFileStatus)worst)
        {
        case GitFileStatus::LocalChange:
            c = QColor(QStringLiteral("#ff2a2a"));
            break;
        case GitFileStatus::RemoteAhead:
            c = QColor(QStringLiteral("#ffc107"));
            break;
        default:
            c = QColor(QStringLiteral("#1fbf3a"));
            break;
        }

        const QString path = item->data(0, Qt::UserRole).toString();
        const bool isDir = QFileInfo(path).isDir() || item->childCount() > 0;
        QIcon baseIcon = isDir ? QIcon::fromTheme(QStringLiteral("folder")) : QIcon::fromTheme(QStringLiteral("text-x-generic"));

        const int iconSz = qMax(8, Config::sidebarIconSize() * 2 / 3);
        const int dotSz = qMax(6, iconSz / 2);
        const int gap = 4;
        const int totalW = dotSz + gap + iconSz;
        QPixmap pix(totalW, iconSz);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, (iconSz - dotSz) / 2, dotSz, dotSz);
        QPixmap basePix = baseIcon.pixmap(iconSz, iconSz);
        p.drawPixmap(dotSz + gap, 0, basePix);
        p.end();

        item->setIcon(0, QIcon(pix));
    }
    return worst;
}

static int sc_addRepoFiles(QTreeWidgetItem *parent, const QString &dirPath, const QString &repoRoot, int depth = 0)
{
    Q_UNUSED(dirPath)
    Q_UNUSED(depth)

    const QStringList files = GitStatusManager::instance().trackedFiles(repoRoot);

    for (const QString &relPath : files)
    {
        if (relPath.isEmpty())
        {
            continue;
        }
        GitFileStatus status = GitStatusManager::instance().statusFor(repoRoot, relPath);
        if (status == GitFileStatus::Unchanged)
        {
            continue;
        }

        const QStringList parts = relPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QTreeWidgetItem *current = parent;
        QString currentFullPath = repoRoot;

        for (int i = 0; i < parts.size(); ++i)
        {
            const QString &part = parts.at(i);
            currentFullPath = QDir(currentFullPath).filePath(part);

            QTreeWidgetItem *child = nullptr;
            for (int j = 0; j < current->childCount(); ++j)
            {
                if (current->child(j)->text(0) == part)
                {
                    child = current->child(j);
                    break;
                }
            }

            if (child == nullptr)
            {
                child = new QTreeWidgetItem(current);
                Q_ASSERT(child != nullptr);
                child->setText(0, part);
                child->setData(0, Qt::UserRole, currentFullPath);
                child->setForeground(0, QBrush(QColor(TM().colors().textPrimary)));
            }

            int currentStatus = child->data(0, Qt::UserRole + 1).toInt();
            if ((int)status > currentStatus)
            {
                child->setData(0, Qt::UserRole + 1, (int)status);
            }

            current = child;
        }
    }

    return sc_updateGitRepoColors(parent, parent, TM());
}

void Sidebar::adjustGitTreeHeight(QTreeWidget *tree)
{
    int totalH = 0;
    const int rowH = tree->sizeHintForRow(0);
    const int defaultRow = rowH > 0 ? rowH : (Config::sidebarIconSize() + 6);
    std::function<int(QTreeWidgetItem *)> count = [&](QTreeWidgetItem *it) -> int
    {
        int n = 1;
        if (it->isExpanded())
        {
            for (int i = 0; i < it->childCount(); ++i)
            {
                n += count(it->child(i));
            }
        }
        return n;
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        totalH += count(tree->topLevelItem(i)) * defaultRow;
    }
    totalH += 8;
    tree->setMinimumHeight(qMax(defaultRow + 8, totalH));
    tree->setMaximumHeight(qMax(defaultRow + 8, totalH));
}

void Sidebar::refreshGitSection()
{
    const auto trees = findChildren<QTreeWidget *>(QStringLiteral("gitTree"));
    for (auto *tree : trees)
    {
        tree->clear();
        const auto repos = Config::gitRepos();
        for (const auto &r : repos)
        {
            if (r.localDir.isEmpty() || !QDir(r.localDir).exists()) continue;

            auto *root = new QTreeWidgetItem(tree);
            Q_ASSERT(root != nullptr);
            root->setText(0, r.name.isEmpty() ? QFileInfo(r.localDir).fileName() : r.name);
            root->setData(0, Qt::UserRole, r.localDir);
            QFont f = root->font(0);
            f.setBold(true);
            root->setFont(0, f);

            int worst = sc_addRepoFiles(root, r.localDir, r.localDir);
            QColor rc;
            switch ((GitFileStatus)worst)
            {
            case GitFileStatus::LocalChange:
                rc = QColor(QStringLiteral("#ff2a2a"));
                break;
            case GitFileStatus::RemoteAhead:
                rc = QColor(QStringLiteral("#ffc107"));
                break;
            default:
                rc = QColor(QStringLiteral("#1fbf3a"));
                break;
            }
            
            QIcon vcsIcon = QIcon::fromTheme(QStringLiteral("vcs-git"), QIcon::fromTheme(QStringLiteral("folder-git")));
            const int iconSz = Config::sidebarIconSize();
            const int dotSz = 10;
            const int gap = 4;
            QPixmap rpix(dotSz + gap + iconSz, iconSz);
            rpix.fill(Qt::transparent);
            QPainter rp(&rpix);
            rp.setRenderHint(QPainter::Antialiasing);
            rp.setBrush(rc);
            rp.setPen(Qt::NoPen);
            rp.drawEllipse(0, (iconSz - dotSz) / 2, dotSz, dotSz);
            rp.drawPixmap(dotSz + gap, 0, vcsIcon.pixmap(iconSz, iconSz));
            rp.end();
            root->setIcon(0, QIcon(rpix));
            root->setForeground(0, QBrush(QColor(TM().colors().textPrimary)));
        }
        
        adjustGitTreeHeight(tree);
    }
}
#endif

void Sidebar::buildTagsHeader(QVBoxLayout *vbox, QPushButton *&addBtn)
{
    auto *header = new QWidget();
    Q_ASSERT(header != nullptr);
    header->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *hLay = new QHBoxLayout(header);
    Q_ASSERT(hLay != nullptr);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);
    
    auto *lbl = new QLabel(tr("Tags"));
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QStringLiteral("font-size:14px;font-weight:normal;background:transparent;color:%1;")
                          .arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);
    
    addBtn = new QPushButton();
    Q_ASSERT(addBtn != nullptr);
    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    if (addBtn->icon().isNull())
    {
        addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
    }
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QStringLiteral("QPushButton { background:transparent; border:none; padding:2px; }"
                                         "QPushButton:hover { background:%1; border-radius:4px; }")
                             .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(header);
}

void Sidebar::buildTagsList(QVBoxLayout *vbox, QWidget *&listCont)
{
    listCont = new QWidget();
    Q_ASSERT(listCont != nullptr);
    listCont->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *listLay = new QVBoxLayout(listCont);
    Q_ASSERT(listLay != nullptr);
    listLay->setContentsMargins(6, 0, 6, 4);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
    
    m_tagList = new QListWidget();
    Q_ASSERT(m_tagList != nullptr);
    m_tagList->setSelectionMode(QAbstractItemView::NoSelection);
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setIconSize(QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
    m_tagList->setStyleSheet(TM().ssListWidget());
    listLay->addWidget(m_tagList);
    vbox->addWidget(listCont);
}

void Sidebar::connectTagsAddButton(QPushButton *addBtn)
{
    connect(addBtn, &QPushButton::clicked, this, [this]()
    {
        bool ok;
        QString name = sc_getText(this, tr("Neuer Tag"), tr("Tag-Name:"));
        ok = !name.isNull();
        if (!ok || name.trimmed().isEmpty())
        {
            return;
        }
        QColorDialog dlg(QColor(TM().colors().textAccent), this);
        dlg.setWindowTitle(tr("Farbe wählen"));
        dlg.setOptions(QColorDialog::DontUseNativeDialog);
        dlg.setStyleSheet(TM().ssDialog());
        if (dlg.exec() != QDialog::Accepted)
        {
            return;
        }
        QColor col = dlg.currentColor();
        if (!col.isValid())
        {
            return;
        }
        addTagItem(name.trimmed(), col.name());
        adjustListHeight(m_tagList);
        if (m_tagsBox != nullptr)
        {
            m_tagsBox->updateGeometry();
        }
        if (m_tagsWrap != nullptr)
        {
            m_tagsWrap->updateGeometry();
        }
        saveTags();
    });
}

void Sidebar::buildTagsSection(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    m_tagsWrap = new QWidget(this);
    Q_ASSERT(m_tagsWrap != nullptr);
    m_tagsWrap->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(m_tagsWrap);
    Q_ASSERT(wLay != nullptr);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);

    m_tagsBox = new QWidget(m_tagsWrap);
    Q_ASSERT(m_tagsBox != nullptr);
    m_tagsBox->setObjectName(QStringLiteral("tagsBox"));
    m_tagsBox->setStyleSheet(TM().ssBox());
    auto *vbox = new QVBoxLayout(m_tagsBox);
    Q_ASSERT(vbox != nullptr);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
    
    QPushButton *addBtn = nullptr;
    buildTagsHeader(vbox, addBtn);
    
    QWidget *listCont = nullptr;
    buildTagsList(vbox, listCont);
    
    auto *toggleBtn = new QPushButton();
    Q_ASSERT(toggleBtn != nullptr);
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16);
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    toggleBtn->setIconSize(QSize(10, 10));
    toggleBtn->setStyleSheet(QStringLiteral("QPushButton{background:transparent !important; border:none;}"));
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    connect(toggleBtn, &QPushButton::toggled, this, [listCont, toggleBtn](bool on)
    {
        listCont->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? QStringLiteral("go-down") : QStringLiteral("go-up")));
    });

    connectTagsAddButton(addBtn);

    wLay->addWidget(m_tagsBox);
    parent->addWidget(m_tagsWrap);
}

void Sidebar::buildFooter(QVBoxLayout *parent)
{
    Q_ASSERT(parent != nullptr);
    auto *footer = new QWidget(this);
    Q_ASSERT(footer != nullptr);
    footer->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *lay = new QHBoxLayout(footer);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);

    const QString btnSS = TM().ssFooterBtn();

    auto makeBtn = [&](const QString &icon, const QString &tip) -> QToolButton *
    {
        auto *b = new QToolButton();
        Q_ASSERT(b != nullptr);
        b->setIcon(QIcon::fromTheme(icon));
        b->setIconSize(QSize(20, 20));
        b->setFixedSize(34, 34);
        b->setToolTip(tip);
        b->setStyleSheet(btnSS);
        return b;
    };

    auto *infoBtn = makeBtn(QStringLiteral("dialog-information"), tr("Über"));
    auto *searchBtn = makeBtn(QStringLiteral("system-search"), tr("Suchen"));
    auto *printBtn = makeBtn(QStringLiteral("document-print"), tr("Drucken"));
    auto *chatBtn = makeBtn(QStringLiteral("mail-message-new"), tr("Nachricht"));

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

void Sidebar::connectDriveList()
{
    Q_ASSERT(m_driveList != nullptr);
    connect(m_driveList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it)
    {
        Q_ASSERT(it != nullptr);
        const QString p = it->data(Qt::UserRole).toString();
        if (p == QStringLiteral("kcm_kaccounts"))
        {
            QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_kaccounts")});
        }
        else
        {
            emit driveClicked(p);
        }
    });
}

void Sidebar::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlayBar != nullptr && m_scrollArea != nullptr)
    {
        const int w = 8;
        const int x = m_scrollArea->x() + m_scrollArea->width() - w;
        const int y = m_scrollArea->y();
        const int h = m_scrollArea->height();
        m_overlayBar->setGeometry(x, y, w, h);
    }
}

void Sidebar::processUserPlaceXmlBookmark(const QString &href, const QString &title)
{
    if (href.isEmpty())
    {
        return;
    }
    const QUrl url(href);
    const QString scheme = url.scheme().toLower();

    static const QStringList netSchemes = {
        QStringLiteral("smb"), QStringLiteral("sftp"), QStringLiteral("ftp"), QStringLiteral("ftps"),
        QStringLiteral("davs"), QStringLiteral("dav"), QStringLiteral("nfs"), QStringLiteral("fish"),
        QStringLiteral("webdav"), QStringLiteral("webdavs")
    };
    
    if (netSchemes.contains(scheme))
    {
        auto s = Config::group("NetworkPlaces");
        QStringList saved = s.readEntry("places", QStringList());

        QUrl checkUrl(href);
        checkUrl.setUserInfo(QString());
        bool alreadyIn = false;
        for (const QString &p : saved)
        {
            QUrl pu(p);
            pu.setUserInfo(QString());
            if (pu == checkUrl)
            {
                alreadyIn = true;
                break;
            }
        }

        if (!alreadyIn)
        {
            saved << href;
            s.writeEntry("places", saved);
            const QString key = QString(href).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
            const QString n = title.isEmpty() ? url.host() : title;
            s.writeEntry("name_" + key, n);
            s.config()->sync();
        }
    }
}

void Sidebar::loadUserPlaces()
{
    const QString xbelPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("user-places.xbel"));
    if (xbelPath.isEmpty())
    {
        return;
    }

    static KDirWatch *s_placesWatcher = nullptr;
    if (s_placesWatcher == nullptr)
    {
        s_placesWatcher = new KDirWatch(this);
        Q_ASSERT(s_placesWatcher != nullptr);
        s_placesWatcher->addFile(xbelPath);
        connect(s_placesWatcher, &KDirWatch::dirty, this, [this]()
        {
            loadUserPlaces();
            DriveManager::instance()->refreshAll();
        });
    }

    QFile f(xbelPath);
    if (!f.open(QIODevice::ReadOnly))
    {
        return;
    }

    QXmlStreamReader xml(&f);
    QString href, title;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            if (xml.name() == QLatin1String("bookmark"))
            {
                href = xml.attributes().value(QStringLiteral("href")).toString();
                title.clear();
            }
            else if (xml.name() == QLatin1String("title"))
            {
                title = xml.readElementText();
            }
        }
        else if (xml.isEndElement() && xml.name() == QLatin1String("bookmark"))
        {
            processUserPlaceXmlBookmark(href, title);
        }
    }
}

void Sidebar::renameInCustomGroups(const QString &path, const QString &newName)
{
    auto gs = Config::group("CustomGroups");
    const QStringList groups = gs.readEntry("groups", QStringList());
    for (const QString &grp : groups)
    {
        KConfigGroup g(gs.config(), gs.name() + "/group_" + grp);
        int cnt = g.readEntry("size", 0);
        QList<QPair<QString, QString>> items;
        bool changed = false;
        for (int i = 1; i <= cnt; ++i)
        {
            KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
            QString p = itemG.readEntry("path", QString());
            QString n = itemG.readEntry("name", QString());
            if (p == path)
            {
                n = newName;
                changed = true;
            }
            items << qMakePair(p, n);
        }
        if (changed)
        {
            KConfigGroup(gs.config(), gs.name() + "/group_" + grp).deleteGroup();
            KConfigGroup gNew(gs.config(), gs.name() + "/group_" + grp);
            gNew.writeEntry("size", items.size());
            for (int i = 0; i < items.size(); ++i)
            {
                KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));
                itemG.writeEntry("path", items[i].first);
                itemG.writeEntry("name", items[i].second);
            }
        }
    }
    gs.config()->sync();
}

void Sidebar::renameNetworkPlace(const QString &path, const QString &newName)
{
    auto s = Config::group("NetworkPlaces");
    const QString npath = mw_normalizePath(path);
    s.writeEntry("name_" + QString(npath).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')), newName);
    s.config()->sync();

    renameInCustomGroups(path, newName);

    auto updateList = [&](QListWidget *list)
    {
        if (list == nullptr)
        {
            return;
        }
        for (int i = 0; i < list->count(); ++i)
        {
            if (list->item(i)->data(Qt::UserRole).toString() == path)
            {
                list->item(i)->setText(newName);
            }
        }
    };
    updateList(m_driveList);
    updateList(m_netList);
    for (QListWidget *list : findChildren<QListWidget *>())
    {
        updateList(list);
    }

    DriveManager::instance()->refreshAll();
    emit drivesChanged();
}

void Sidebar::applyIconSizes()
{
    if (m_driveList != nullptr)
    {
        m_driveList->setIconSize(QSize(Config::driveIconSize(), Config::driveIconSize()));
    }
    if (m_netList != nullptr)
    {
        m_netList->setIconSize(QSize(Config::driveIconSize(), Config::driveIconSize()));
    }
    if (m_tagList != nullptr)
    {
        m_tagList->setIconSize(QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
    }
    for (auto *list : findChildren<QListWidget *>())
    {
        if (list == m_driveList || list == m_netList || list == m_tagList)
        {
            continue;
        }
        list->setIconSize(QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
    }
}

void Sidebar::populateLocalDrivesList(DriveManager *dm)
{
    for (const auto &info : dm->localDrives())
    {
        QString freeStr;
        if (info.isMounted && info.total > 0)
        {
            freeStr = QStringLiteral("%1 frei / %2").arg(sc_fmtStorage(info.free), sc_fmtStorage(info.total));
        }

        auto *it = new QListWidgetItem(QIcon::fromTheme(info.iconName), info.name, m_driveList);
        Q_ASSERT(it != nullptr);
        it->setData(Qt::UserRole, info.path);
        it->setData(Qt::UserRole + 1, freeStr);
        it->setData(Qt::UserRole + 2, info.udi);
        it->setData(Qt::UserRole + 10, info.total);
        it->setData(Qt::UserRole + 11, info.free);
        if (!info.isMounted)
        {
            it->setForeground(QColor(TM().colors().textMuted));
            it->setSizeHint(QSize(0, Config::sidebarDriveRowHeight()));
        }
    }
}

void Sidebar::populateNetworkDrivesList(DriveManager *dm, bool &hasNet)
{
    if (m_netList == nullptr)
    {
        return;
    }
    for (const auto &info : dm->networkDrives())
    {
        hasNet = true;
        auto *it = new QListWidgetItem(QIcon::fromTheme(info.iconName), info.name, m_netList);
        Q_ASSERT(it != nullptr);
        it->setData(Qt::UserRole, info.path);
        it->setData(Qt::UserRole + 1, info.subtitle.isEmpty() ? info.path : info.subtitle);
        it->setSizeHint(QSize(0, info.subtitle.isEmpty() ? Config::sidebarDriveRowHeight() : Config::sidebarNetRowHeight() - 8));
        it->setData(Qt::UserRole + 10, info.total);
        it->setData(Qt::UserRole + 11, info.free);
    }
}

void Sidebar::updateDrives()
{
    static bool s_updating = false;
    if (s_updating)
    {
        return;
    }
    s_updating = true;

    Q_ASSERT(m_driveList != nullptr);
    m_driveList->clear();
    if (m_netList != nullptr)
    {
        m_netList->clear();
    }
    bool hasNet = false;

    auto *dm = DriveManager::instance();
    Q_ASSERT(dm != nullptr);

    populateLocalDrivesList(dm);
    populateNetworkDrivesList(dm, hasNet);

    if (m_netBox != nullptr)
    {
        m_netBox->setVisible(hasNet);
        if (hasNet && m_netList != nullptr)
        {
            int netH = 0;
            for (int i = 0; i < m_netList->count(); ++i)
            {
                netH += m_netList->sizeHintForRow(i);
            }
            if (netH > 0 && m_netList->height() != netH)
            {
                m_netList->setFixedHeight(netH);
                m_netBox->adjustSize();
                if (m_netBox->parentWidget() != nullptr)
                {
                    m_netBox->parentWidget()->adjustSize();
                }
            }
        }
    }

    int totalH = 0;
    for (int i = 0; i < m_driveList->count(); ++i)
    {
        totalH += m_driveList->sizeHintForRow(i);
    }
    if (m_driveList->height() != qMax(1, totalH))
    {
        m_driveList->setFixedHeight(qMax(1, totalH));
    }
    m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveList->updateGeometry();
    s_updating = false;
}

void Sidebar::setupDriveContextMenu()
{
    Q_ASSERT(m_driveList != nullptr);
    m_driveList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_driveList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos)
    {
        auto *item = m_driveList->itemAt(pos);
        if (item != nullptr)
        {
            showDriveContextMenu(item, pos);
        }
    });
}

void Sidebar::showDriveContextMenuSolid(QMenu &menu, const QString &path, const QString &udi)
{
    const bool isSolid = path.startsWith(QStringLiteral("solid:"));
    if (isSolid)
    {
        Solid::Device dev(path.mid(6));
        auto *acc = dev.as<Solid::StorageAccess>();
        bool mounted = (acc != nullptr) && acc->isAccessible();
        const QString solidUdi = dev.udi();
        if (mounted)
        {
            menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Aushängen"), this, [this, solidUdi]()
            {
                emit teardownRequested(solidUdi);
            });
        }
        else
        {
            menu.addAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), tr("Einhängen"), this, [this, solidUdi]()
            {
                Solid::Device dev(solidUdi);
                auto *acc = dev.as<Solid::StorageAccess>();
                if (acc == nullptr)
                {
                    return;
                }
                connect(acc, &Solid::StorageAccess::setupDone, this, [this](Solid::ErrorType, QVariant, const QString &)
                {
                    DriveManager::instance()->refreshAll();
                    emit drivesChanged();
                }, Qt::SingleShotConnection);
                acc->setup();
            });
        }
    }
    else
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Aushängen"), this, [this, udi]()
        {
            emit teardownRequested(udi);
        });
    }
}

void Sidebar::showDriveContextMenuNonSolid(QMenu &menu, const QString &path)
{
    menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Auswerfen"), this, [this, path]()
    {
        const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
        QString foundUdi;
        for (const Solid::Device &d : devices)
        {
            const auto *a = d.as<Solid::StorageAccess>();
            if ((a != nullptr) && a->filePath() == path)
            {
                foundUdi = d.udi();
                break;
            }
        }
        if (foundUdi.isEmpty())
        {
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
            return;
        }
        auto *device = new Solid::Device(foundUdi);
        Q_ASSERT(device != nullptr);
        auto *acc = device->as<Solid::StorageAccess>();
        if (acc == nullptr)
        {
            delete device;
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
            return;
        }
        connect(acc, &Solid::StorageAccess::teardownDone, this, [this, device](Solid::ErrorType, QVariant, const QString &)
        {
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
            delete device;
        }, Qt::SingleShotConnection);
        acc->teardown();
    });
}

void Sidebar::showDriveContextMenuPinned(QMenu &menu, const QString &path, const QString &name)
{
    auto netCheck = Config::group("NetworkPlaces");
    const QStringList netPlaces = netCheck.readEntry("places", QStringList());
    if (netPlaces.contains(path))
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"), this, [this, path, name]()
        {
            QDialog dlg(this);
            dlg.setWindowTitle(tr("Umbenennen"));
            dlg.setStyleSheet(TM().ssDialog());
            auto *vl = new QVBoxLayout(&dlg);
            Q_ASSERT(vl != nullptr);
            vl->addWidget(new QLabel(tr("Anzeigename:")));
            auto *edit = new QLineEdit(name, &dlg);
            Q_ASSERT(edit != nullptr);
            vl->addWidget(edit);
            auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            Q_ASSERT(box != nullptr);
            vl->addWidget(box);
            connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            if (dlg.exec() == QDialog::Accepted && !edit->text().trimmed().isEmpty())
            {
                renameNetworkPlace(path, edit->text().trimmed());
            }
        });
        
        menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Aus Laufwerken entfernen"), this, [this, path]()
        {
            auto s = Config::group("NetworkPlaces");
            QStringList saved = s.readEntry("places", QStringList());
            const QString npath = mw_normalizePath(path);
            saved.removeAll(npath);
            QString otherVersion = npath.endsWith(QLatin1Char('/')) ? npath.left(npath.length() - 1) : npath + QLatin1Char('/');
            if (otherVersion != QStringLiteral("/"))
            {
                saved.removeAll(otherVersion);
            }

            s.writeEntry("places", saved);
            s.deleteEntry("name_" + QString(npath).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')));
            s.deleteEntry("name_" + QString(otherVersion).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')));
            s.config()->sync();
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
        });
        menu.addSeparator();
    }
}

void Sidebar::showDriveContextMenuShortcut(QMenu &menu, const QString &path, const QString &name)
{
    Q_UNUSED(name)
    menu.addAction(QIcon::fromTheme(QStringLiteral("emblem-symbolic-link")), tr("Verknüpfung erstellen"), this, [this, path]()
    {
        const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        const QUrl url = QUrl::fromUserInput(path);
        const QString dirName = url.isLocalFile() ? QDir(url.toLocalFile()).dirName() : url.fileName();
        const QByteArray content = QStringLiteral("[Desktop Entry]\nType=Link\nName=%1\nURL=%2\nIcon=folder\n")
                                      .arg(dirName, path)
                                      .toUtf8();
        const QUrl dest = QUrl::fromLocalFile(desktop + QLatin1Char('/') + dirName + QStringLiteral(".desktop"));
        auto *job = KIO::storedPut(content, dest, -1, KIO::Overwrite);
        Q_ASSERT(job != nullptr);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), tr("Eigenschaften"), this, [path]()
    {
        auto *dlg = new KPropertiesDialog(QUrl::fromLocalFile(path), nullptr);
        Q_ASSERT(dlg != nullptr);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
}

void Sidebar::showDriveContextMenu(QListWidgetItem *item, const QPoint &pos)
{
    Q_ASSERT(item != nullptr);
    const QString path = item->data(Qt::UserRole).toString();
    const QString name = item->text();
    const QString udi = item->data(Qt::UserRole + 2).toString();
    const bool isSolid = path.startsWith(QStringLiteral("solid:"));
    const bool isGdrive = path.startsWith(QStringLiteral("gdrive:/"));
    const bool isMountedSolid = !udi.isEmpty() && !isSolid && !isGdrive;

    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());

    menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this, [this, path]()
    {
        emit driveClicked(path);
    });
    auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
    Q_ASSERT(openInMenu != nullptr);
    openInMenu->setStyleSheet(TM().ssMenu());
    openInMenu->addAction(tr("Linke Pane"), this, [this, path]() { emit driveClickedLeft(path); });
    openInMenu->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });
    menu.addSeparator();

    if (isMountedSolid || isSolid)
    {
        showDriveContextMenuSolid(menu, path, isMountedSolid ? udi : path);
    }

    if (!isSolid && !isMountedSolid && path != QStringLiteral("/") && !path.isEmpty() && !isGdrive)
    {
        showDriveContextMenuNonSolid(menu, path);
    }

    showDriveContextMenuPinned(menu, path, name);

    auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Kopieren"));
    Q_ASSERT(copyMenu != nullptr);
    copyMenu->setStyleSheet(TM().ssMenu());
    copyMenu->addAction(tr("Pfad kopieren"), this, [path]() { QGuiApplication::clipboard()->setText(path); });
    copyMenu->addAction(tr("Name kopieren"), this, [name]() { QGuiApplication::clipboard()->setText(name); });

    if (!isSolid && !isGdrive)
    {
        menu.addSeparator();
        showDriveContextMenuShortcut(menu, path, name);
    }

    Q_ASSERT(m_driveList != nullptr);
    menu.exec(m_driveList->mapToGlobal(pos));
}

void Sidebar::showPlaceContextMenu(QListWidgetItem *item, QListWidget *list, const QPoint &pos, const QString &groupName)
{
    Q_ASSERT(item != nullptr && list != nullptr);
    const QString path = item->data(Qt::UserRole).toString();
    const QString name = item->text();
    const QString icon = item->data(Qt::UserRole + 2).toString();

    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());

    auto *openIn = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
    Q_ASSERT(openIn != nullptr);
    openIn->setStyleSheet(TM().ssMenu());
    openIn->addAction(tr("Linke Pane"), this, [this, path]() { emit driveClicked(path); });
    openIn->addAction(tr("Rechte Pane"), this, [this, path]() { emit driveClickedRight(path); });

    auto saveList = [list, groupName]()
    {
        if (!groupName.isEmpty())
        {
            auto gs = Config::group("CustomGroups");
            KConfigGroup(gs.config(), gs.name() + "/group_" + groupName).deleteGroup();
            KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
            gNew.writeEntry(QStringLiteral("size"), list->count());

            for (int i = 0; i < list->count(); ++i)
            {
                KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));
                itemG.writeEntry(QStringLiteral("path"), list->item(i)->data(Qt::UserRole).toString());
                itemG.writeEntry(QStringLiteral("name"), list->item(i)->text());
                itemG.writeEntry(QStringLiteral("icon"), list->item(i)->data(Qt::UserRole + 2).toString());
            }
            gs.config()->sync();
        }
    };

    sc_buildPlaceMenu(menu, path, name, icon, [list, item, saveList]()
    {
        delete list->takeItem(list->row(item));
        adjustListHeight(list);
        saveList();
    }, [item, saveList](const QString &newName, const QString &newPath, const QString &newIcon)
    {
        item->setText(newName);
        item->setData(Qt::UserRole, newPath);
        item->setData(Qt::UserRole + 2, newIcon);
        if (!newIcon.isEmpty())
        {
            item->setIcon(QIcon::fromTheme(newIcon));
        }
        else
        {
            item->setIcon(QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(newPath))));
        }
        saveList();
    });

    menu.exec(list->mapToGlobal(pos));
}

void Sidebar::onTrashChanged()
{
    if (m_trashLister == nullptr)
    {
        return;
    }
    const bool isEmpty = m_trashLister->items().isEmpty();
    const QString iconName = isEmpty ? QStringLiteral("user-trash") : QStringLiteral("user-trash-full");
    QIcon trashIcon = QIcon::fromTheme(iconName);
    if (trashIcon.isNull())
    {
        trashIcon = QIcon::fromTheme(isEmpty ? QStringLiteral("trash-empty") : QStringLiteral("trash-full"));
    }

    QList<QListWidget *> lists;
    if (m_driveList != nullptr)
    {
        lists << m_driveList;
    }
    if (m_favList != nullptr)
    {
        lists << m_favList;
    }
    if (m_netList != nullptr)
    {
        lists << m_netList;
    }

    if (m_contentLayout != nullptr)
    {
        for (int i = 0; i < m_contentLayout->count(); ++i)
        {
            if (auto *box = m_contentLayout->itemAt(i)->widget())
            {
                if (auto *lw = box->findChild<QListWidget *>())
                {
                    if (!lists.contains(lw))
                    {
                        lists << lw;
                    }
                }
            }
        }
    }

    for (auto *list : lists)
    {
        for (int i = 0; i < list->count(); ++i)
        {
            auto *item = list->item(i);
            Q_ASSERT(item != nullptr);
            const QString path = item->data(Qt::UserRole).toString();
            if (QUrl(path).scheme() == QStringLiteral("trash"))
            {
                item->setIcon(trashIcon);
            }
        }
    }
}

void Sidebar::saveToUserPlaces(const QString &url, const QString &name)
{
    const QString xbelPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");

    QFile f(xbelPath);
    QByteArray existing;
    if (f.open(QIODevice::ReadOnly))
    {
        existing = f.readAll();
        f.close();
    }

    if (existing.contains(url.toUtf8()))
    {
        return;
    }

    const QString entry = QStringLiteral("  <bookmark href=\"%1\">\n"
                                         "    <title>%2</title>\n"
                                         "  </bookmark>\n")
                             .arg(url, name);

    if (existing.contains("</xbel>"))
    {
        existing.replace("</xbel>", (entry + QStringLiteral("</xbel>")).toUtf8());
    }
    else if (existing.isEmpty())
    {
        existing = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                  "<!DOCTYPE xbel>\n"
                                  "<xbel xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\""
                                  " xmlns:kdepriv=\"http://www.kde.org/kdepriv\" version=\"1.0\">\n"
                                  "%1"
                                  "</xbel>\n")
                      .arg(entry)
                      .toUtf8();
    }
    else
    {
        return;
    }

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        f.write(existing);
    }
}
