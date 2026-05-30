#include "millercolumn.h"
#include "drivemanager.h"
#include "hoverfader.h"
#include "listtopshadow.h"
#include "config.h"
#include "dialogutils.h"
#include "drophandler.h"
#include "panecomponents.h"
#include "scglobal.h"
#include "thememanager.h"
#include <KDialogJobUiDelegate>
#include <KDirLister>
#include <KFileItem>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KTerminalLauncherJob>
#include <QClipboard>
#include <QDir>
#include <QEasingCurve>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QStorageInfo>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <QVBoxLayout>

class MillerWidgetItem : public QListWidgetItem
{
public:
    MillerWidgetItem(const QIcon &icon, const QString &text, bool isDir, QListWidget *parent = nullptr)
        : QListWidgetItem(icon, text, parent)
        , m_isDir(isDir)
    {
    }

    bool operator<(const QListWidgetItem &other) const override
    {
        const auto *otherM = dynamic_cast<const MillerWidgetItem *>(&other);
        if (otherM != nullptr)
        {
            if (m_isDir != otherM->m_isDir)
            {
                return m_isDir;
            }
        }
        return text().localeAwareCompare(other.text()) < 0;
    }

private:
    bool m_isDir;
};

MillerColumn::MillerColumn(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(120);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *lay = new QVBoxLayout(this);
    Q_ASSERT(lay != nullptr);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_lister = new KDirLister(this);
    Q_ASSERT(m_lister != nullptr);
    connect(m_lister, &KDirLister::newItems, this, [this](const KFileItemList &items)
    {
        if (m_path == QStringLiteral("__drives__"))
        {
            return;
        }
        for (const KFileItem &item : items)
        {
            auto *it = new MillerWidgetItem(
                QIcon::fromTheme(item.iconName(), QIcon::fromTheme(QStringLiteral("folder"))),
                item.name(), item.isDir());
            Q_ASSERT(it != nullptr);
            it->setData(Qt::UserRole, item.url().toString());
            it->setData(Qt::UserRole + 3, item.isDir());
            Q_ASSERT(m_list != nullptr);
            m_list->addItem(it);
        }
    });

    connect(m_lister, &KDirLister::completed, this, [this]()
    {
        Q_ASSERT(m_list != nullptr);
        m_list->sortItems();
    });

    auto *headerWidget = new QWidget();
    Q_ASSERT(headerWidget != nullptr);
    headerWidget->setFixedHeight(Config::millerHeaderHeight());
    headerWidget->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgList));
    m_headerLay = new QHBoxLayout(headerWidget);
    Q_ASSERT(m_headerLay != nullptr);
    m_headerLay->setContentsMargins(0, 0, 0, 0);
    m_headerLay->setSpacing(0);

    m_colLabel = new QPushButton();
    Q_ASSERT(m_colLabel != nullptr);
    m_colLabel->setFlat(true);
    m_colLabel->setFixedHeight(Config::millerHeaderHeight());
    m_colLabel->setStyleSheet(
        QStringLiteral("QPushButton { background:transparent; color:%1; font-size:18px;"
                       " padding:0px 0px 0px 8px; margin:0; border:none; "
                       "border-radius:0; text-align:left; }"
                       "QPushButton:hover { background:%2; border-radius:0; }")
        .arg(TM().colors().textAccent, TM().colors().bgHover));
    
    connect(m_colLabel, &QPushButton::clicked, this, [this]()
    {
        if (m_active)
        {
            emit editPathRequested();
        }
        else
        {
            emit headerClicked(m_path.isEmpty() ? QStringLiteral("__drives__") : m_path);
        }
    });
    
    m_headerLay->addWidget(m_colLabel, 1);
    lay->addWidget(headerWidget);

    m_list = new QListWidget();
    Q_ASSERT(m_list != nullptr);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->verticalScrollBar()->hide();
    m_list->setStyleSheet(TM().ssColInactive());
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setDragEnabled(true);
    m_list->setAcceptDrops(true);
    m_list->setDropIndicatorShown(true);
    m_list->setDragDropMode(QAbstractItemView::DragDrop);

    auto resolver = [this](const QModelIndex &idx) -> QUrl
    {
        QUrl destUrl = QUrl::fromUserInput(m_path);
        if (idx.isValid())
        {
            Q_ASSERT(m_list != nullptr);
            QListWidgetItem *it = m_list->item(idx.row());
            if (it != nullptr)
            {
                QString itemPath = it->data(Qt::UserRole).toString();
                if (!itemPath.isEmpty() && !itemPath.startsWith(QStringLiteral("solid:")))
                {
                    destUrl = QUrl::fromUserInput(itemPath);
                }
            }
        }
        return destUrl;
    };
    
    Q_ASSERT(m_list->viewport() != nullptr);
    m_list->viewport()->installEventFilter(new DropHandler(m_list, resolver, m_list));

    auto *millerDel = new MillerItemDelegate(m_list);
    Q_ASSERT(millerDel != nullptr);
    millerDel->setHoverFader(new HoverFader(m_list, millerDel));
    m_list->setItemDelegate(millerDel);

    lay->addWidget(m_list);

    auto *topShadow = new ListTopShadow(m_list->viewport());
    Q_ASSERT(topShadow != nullptr);
    topShadow->setGeometry(0, 0, m_list->viewport()->width(), 18);
    m_list->viewport()->installEventFilter(topShadow);
    
    connect(m_list->horizontalScrollBar(), &QScrollBar::rangeChanged, topShadow, [topShadow, this]()
    {
        Q_ASSERT(m_list->viewport() != nullptr);
        topShadow->setGeometry(0, 0, m_list->viewport()->width(), 18);
        topShadow->raise();
    });

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it)
    {
        Q_ASSERT(it != nullptr);
        emit activated(this);
        const QString p = it->data(Qt::UserRole).toString();
        if (p.isEmpty())
        {
            return;
        }
        if (p.startsWith(QStringLiteral("solid:")))
        {
            const QString udi = it->data(Qt::UserRole + 1).toString();
            if (!udi.isEmpty())
            {
                emit setupRequested(udi);
            }
        }
        else
        {
            emit entryClicked(p, this);
        }
    });

    connect(m_list, &QListWidget::customContextMenuRequested, this, &MillerColumn::showContextMenu);
}

const QString& MillerColumn::path() const
{
    return m_path;
}

QListWidget* MillerColumn::list()
{
    Q_ASSERT(m_list != nullptr);
    return m_list;
}

void MillerColumn::addHeaderWidget(QWidget *w)
{
    Q_ASSERT(m_headerLay != nullptr);
    if (w != nullptr)
    {
        m_headerLay->insertWidget(0, w);
    }
}

void MillerColumn::populateDrives()
{
    m_path = QStringLiteral("__drives__");
    Q_ASSERT(m_lister != nullptr);
    m_lister->stop();
    if (m_colLabel != nullptr)
    {
        m_colLabel->setText(tr("Dieser PC"));
    }

    Q_ASSERT(m_list != nullptr);
    m_list->clear();
    m_list->setStyleSheet(TM().ssColDrives());
    m_list->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->verticalScrollBar()->hide();

    auto *dm = DriveManager::instance();
    Q_ASSERT(dm != nullptr);

    for (const auto &info : dm->localDrives())
    {
        auto *it = new QListWidgetItem(m_list);
        Q_ASSERT(it != nullptr);
        it->setData(Qt::DisplayRole, info.name);
        it->setData(Qt::DecorationRole, QIcon::fromTheme(info.iconName));
        it->setData(Qt::UserRole, info.path);
        it->setData(Qt::UserRole + 1, info.udi);
        it->setSizeHint(QSize(0, Config::millerDriveRowHeight()));
        it->setData(Qt::UserRole + 3, true);
        it->setData(Qt::UserRole + 12, true);
        it->setData(Qt::UserRole + 10, info.total);
        it->setData(Qt::UserRole + 11, info.free);

        if (!info.isMounted)
        {
            it->setForeground(QColor(TM().colors().textMuted));
        }
    }

    for (const auto &info : dm->networkDrives())
    {
        auto *it = new QListWidgetItem(m_list);
        Q_ASSERT(it != nullptr);
        it->setData(Qt::DisplayRole, info.name);
        it->setData(Qt::DecorationRole, QIcon::fromTheme(info.iconName));
        it->setData(Qt::UserRole, info.path);
        it->setData(Qt::UserRole + 1, info.scheme);
        it->setSizeHint(QSize(0, Config::millerDriveRowHeight()));
        it->setData(Qt::UserRole + 3, true);
        it->setData(Qt::UserRole + 12, true);
        it->setData(Qt::UserRole + 10, info.total);
        it->setData(Qt::UserRole + 11, info.free);
    }
}

void MillerColumn::populateDir(const QString &path)
{
    m_path = path;
    QUrl url(path);
    if (url.scheme().isEmpty() && !path.isEmpty())
    {
        url = QUrl::fromUserInput(path);
    }

    QString name;
    const QString local = url.toLocalFile();
    if (path == QStringLiteral("__drives__"))
    {
        name = tr("Dieser PC");
    }
    else if (local == QStringLiteral("/") || (url.path() == QStringLiteral("/") && url.scheme().isEmpty()))
    {
        name = sc_rootVolumeName();
    }
    else if (url.scheme() == QStringLiteral("gdrive") && url.fileName().isEmpty())
    {
        name = tr("Google Drive");
    }
    else
    {
        name = url.fileName().isEmpty() ? (local.isEmpty() ? path : local) : url.fileName();
    }

    if (m_colLabel != nullptr)
    {
        m_colLabel->setText(name);
    }

    Q_ASSERT(m_list != nullptr);
    m_list->clear();
    m_list->setStyleSheet(TM().ssColInactive());
    m_list->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
    m_list->setItemDelegate(new MillerItemDelegate(m_list));

    if (path != QStringLiteral("__drives__"))
    {
        Q_ASSERT(m_lister != nullptr);
        m_lister->stop();
        m_lister->openUrl(url, KDirLister::NoFlags);
    }
}

void MillerColumn::refreshStyle()
{
    Q_ASSERT(m_list != nullptr);
    setActive(m_list->styleSheet() != TM().ssColInactive());
    const int sz = Config::millerIconSize();
    m_list->setIconSize(QSize(sz, sz));
    m_list->viewport()->update();
}

void MillerColumn::setActive(bool active)
{
    m_active = active;
    Q_ASSERT(m_list != nullptr);
    m_list->setStyleSheet(
        m_path == QStringLiteral("__drives__")
            ? TM().ssColDrives()
            : (active ? TM().ssColActive() : TM().ssColInactive()));
}

void MillerColumn::showContextMenu(const QPoint &pos)
{
    emit activated(this);
    Q_ASSERT(m_list != nullptr);
    QListWidgetItem *it = m_list->itemAt(pos);
    if (it == nullptr)
    {
        return;
    }

    const QString itemPath = it->data(Qt::UserRole).toString();
    if (m_path == QStringLiteral("__drives__"))
    {
        handleDrivesContextMenu(it, itemPath, pos);
    }
    else
    {
        handleDirContextMenu(itemPath, pos);
    }
}

void MillerColumn::handleDrivesContextMenu(QListWidgetItem *it, const QString &itemPath, const QPoint &pos)
{
    Q_ASSERT(it != nullptr);
    Q_ASSERT(m_list != nullptr);

    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());
    const QString udi = it->data(Qt::UserRole + 1).toString();

    if (!itemPath.isEmpty() && !itemPath.startsWith(QStringLiteral("solid:")))
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this, [this, itemPath]()
        {
            emit entryClicked(itemPath, this);
        });
        
        auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
        Q_ASSERT(openInMenu != nullptr);
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Ansicht"), this, [this, itemPath]() { emit openInLeft(itemPath); });
        openInMenu->addAction(tr("Rechte Ansicht"), this, [this, itemPath]() { emit openInRight(itemPath); });
        menu.addSeparator();
    }

    auto netCheck = Config::group("NetworkPlaces");
    const QStringList netPlaces = netCheck.readEntry("places", QStringList());
    const QString npath = mw_normalizePath(itemPath);
    bool alreadyIn = false;
    for (const QString &p : netPlaces)
    {
        if (mw_normalizePath(p) == npath)
        {
            alreadyIn = true;
            break;
        }
    }

    if (alreadyIn)
    {
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"), this, [this, itemPath, it]()
        {
            bool ok;
            const QString newName = DialogUtils::getText(this, tr("Umbenennen"), tr("Anzeigename:"), it->text(), &ok);
            if (!ok || newName.trimmed().isEmpty())
            {
                return;
            }

            auto s = Config::group("NetworkPlaces");
            s.writeEntry("name_" + QString(itemPath).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_')), newName.trimmed());
            s.config()->sync();
            
            auto *dm = DriveManager::instance();
            Q_ASSERT(dm != nullptr);
            dm->refreshAll();
        });

        menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Aus Laufwerken entfernen"), this, [this, itemPath]()
        {
            auto s = Config::group("NetworkPlaces");
            QStringList saved = s.readEntry("places", QStringList());
            const QString npath = mw_normalizePath(itemPath);
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
            emit removeFromPlacesRequested(itemPath);
        });
        menu.addSeparator();
    }

    auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Kopieren"));
    Q_ASSERT(copyMenu != nullptr);
    copyMenu->setStyleSheet(TM().ssMenu());
    copyMenu->addAction(tr("Pfad kopieren"), [itemPath]() { QGuiApplication::clipboard()->setText(itemPath); });
    copyMenu->addAction(tr("Name kopieren"), [it]() { QGuiApplication::clipboard()->setText(it->text()); });
    menu.addSeparator();

    if (!udi.isEmpty())
    {
        Solid::Device dev(udi);
        const auto *acc = dev.as<Solid::StorageAccess>();
        if (acc != nullptr)
        {
            if (acc->isAccessible())
            {
                menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")), tr("Aushängen"), this, [this, udi]() { emit teardownRequested(udi); });
            }
            else
            {
                menu.addAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), tr("Einhängen"), this, [this, udi]() { emit setupRequested(udi); });
            }
            menu.addSeparator();
        }
    }

    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Pfad kopieren"), this, [itemPath]()
    {
        QGuiApplication::clipboard()->setText(itemPath);
    });

    menu.exec(m_list->mapToGlobal(pos));
}

void MillerColumn::handleDirContextMenu(const QString &itemPath, const QPoint &pos)
{
    Q_ASSERT(m_list != nullptr);

    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());
    
    menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"), this, [this, itemPath]()
    {
        emit entryClicked(itemPath, this);
    });

    auto *openInMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
    Q_ASSERT(openInMenu != nullptr);
    openInMenu->setStyleSheet(TM().ssMenu());
    openInMenu->addAction(tr("Linke Ansicht"), this, [this, itemPath]() { emit openInLeft(itemPath); });
    openInMenu->addAction(tr("Rechte Ansicht"), this, [this, itemPath]() { emit openInRight(itemPath); });
    menu.addSeparator();

    menu.addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), tr("Im Terminal öffnen"), this, [this, itemPath]()
    {
        auto *job = new KTerminalLauncherJob(QString());
        Q_ASSERT(job != nullptr);
        job->setWorkingDirectory(itemPath);
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    
    menu.addSeparator();
    
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Pfad kopieren"), this, [itemPath]()
    {
        QGuiApplication::clipboard()->setText(itemPath);
    });
    
    menu.addSeparator();
    
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), tr("Eigenschaften"), this, [this, itemPath]()
    {
        emit propertiesRequested(itemPath);
    });

    menu.exec(m_list->mapToGlobal(pos));
}
