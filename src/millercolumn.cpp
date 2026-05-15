#include "millercolumn.h"

#include <QPropertyAnimation>
#include <QEasingCurve>
#include "config.h"
#include "dialogutils.h"
#include "drophandler.h"
#include "panewidgets.h"
#include "thememanager.h"
#include "scglobal.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <KDirLister>
#include <KFileItem>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <QScrollBar>

// --- MillerWidgetItem für Ordner-Sortierung ---
class MillerWidgetItem : public QListWidgetItem {
public:
  MillerWidgetItem(const QIcon &icon, const QString &text, bool isDir,
                   QListWidget *parent = nullptr)
      : QListWidgetItem(icon, text, parent), m_isDir(isDir) {}

  bool operator<(const QListWidgetItem &other) const override {
    const auto *otherM = dynamic_cast<const MillerWidgetItem *>(&other);
    if (otherM) {
      if (m_isDir != otherM->m_isDir)
        return m_isDir; // Ordner vor Dateien
    }
    return text().localeAwareCompare(other.text()) < 0;
  }

private:
  bool m_isDir;
};

// --- MillerColumn ---
MillerColumn::MillerColumn(QWidget *parent) : QWidget(parent) {
  setMinimumWidth(120);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // Lister für normale Verzeichnisse
  m_lister = new KDirLister(this);
  connect(m_lister, &KDirLister::newItems, this,
          [this](const KFileItemList &items) {
            if (m_path == "__drives__") {
              return;
            }
            for (const KFileItem &item : items) {
              auto *it = new MillerWidgetItem(
                  QIcon::fromTheme(item.iconName(),
                                   QIcon::fromTheme(QStringLiteral("folder"))),
                  item.name(), item.isDir());
              it->setData(Qt::UserRole, item.url().toString());
              it->setData(Qt::UserRole + 3, item.isDir());
              m_list->addItem(it);
            }
          });
  connect(m_lister, &KDirLister::completed, this,
          [this]() { m_list->sortItems(); });

  m_header = new QPushButton();
  m_header->setObjectName("MillerHeader");
  m_header->setFlat(true);
  m_header->setFixedHeight(SC_MILLER_HEADER_H);
  m_header->setIconSize(QSize(22, 22));
  m_header->setStyleSheet(
      QString(
          "QPushButton#MillerHeader { background:%1; border:none; "
          "border-right:1px solid %2; color:%3; "
          "font-family:'Segoe UI Semilight', 'Roboto Light', sans-serif; "
          "font-weight:300; font-size:22px; padding:4px 12px; text-align:left; "
          "border-top-right-radius:8px; border-top-left-radius:0px; "
          "border-bottom-right-radius:0px; border-bottom-left-radius:0px; } "
          "QPushButton#MillerHeader:hover { background:%4; color:%5; "
          "border-top-right-radius:8px; }")
          .arg(TM().colors().bgBox, TM().colors().separator,
               TM().colors().textAccent, TM().colors().bgHover,
               TM().colors().textPrimary));
  auto *hlay = new QHBoxLayout();
  hlay->setContentsMargins(0, 0, 0, 0);
  hlay->setSpacing(0);
  hlay->addWidget(m_header, 1);
  hlay->addStretch(1);
  lay->addLayout(hlay);
  connect(m_header, &QPushButton::clicked, this, [this]() {
    emit headerClicked(m_path == "__drives__" ? QString() : m_path);
  });

  m_list = new QListWidget();
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

  auto resolver = [this](const QModelIndex &idx) -> QUrl {
    QUrl destUrl = QUrl::fromUserInput(m_path);
    if (idx.isValid()) {
      QListWidgetItem *it = m_list->item(idx.row());
      if (it) {
        QString itemPath = it->data(Qt::UserRole).toString();
        if (!itemPath.isEmpty() && !itemPath.startsWith("solid:")) {
          destUrl = QUrl::fromUserInput(itemPath);
        }
      }
    }
    return destUrl;
  };
  m_list->viewport()->installEventFilter(
      new DropHandler(m_list, resolver, m_list));

  lay->addWidget(m_list);


  connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
    emit activated(this);
    const QString p = it->data(Qt::UserRole).toString();
    if (p.isEmpty())
      return;
    if (p.startsWith("solid:")) {
      // Nicht eingehängtes Laufwerk – einhängen
      const QString udi = it->data(Qt::UserRole + 1).toString();
      if (!udi.isEmpty())
        emit setupRequested(udi);
    } else {
      emit entryClicked(p, this);
    }
  });

  connect(m_list, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            emit activated(this);
            QListWidgetItem *it = m_list->itemAt(pos);
            if (!it)
              return;

            const QString itemPath = it->data(Qt::UserRole).toString();
            QMenu menu(this);
            menu.setStyleSheet(TM().ssMenu());

    if (m_path == QLatin1String("__drives__")) {
      // --- Laufwerk-Menü ---
      const QString udi = it->data(Qt::UserRole + 1).toString();
      mw_applyMenuShadow(&menu);

      // Öffnen + Öffnen in für alle navigierbaren Einträge
      if (!itemPath.isEmpty() && !itemPath.startsWith("solid:")) {
        menu.addAction(
            QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
            [this, itemPath]() { emit entryClicked(itemPath, this); });
        auto *openInMenu =
            menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Ansicht"), this, [this, itemPath]() {
          emit openInLeft(itemPath);
        });
        openInMenu->addAction(tr("Rechte Ansicht"), this, [this, itemPath]() {
          emit openInRight(itemPath);
        });
        menu.addSeparator();
      }

      {
        auto netCheck = Config::group("NetworkPlaces");
        const QStringList netPlaces =
            netCheck.readEntry("places", QStringList());
        const QString npath = mw_normalizePath(itemPath);
        bool alreadyIn = false;
        for (const QString &p : netPlaces) {
          if (mw_normalizePath(p) == npath) {
            alreadyIn = true;
            break;
          }
        }

        if (alreadyIn) {
          menu.addAction(
              QIcon::fromTheme("edit-rename"), tr("Umbenennen"), this,
              [this, itemPath, it]() {
                bool ok;
                const QString newName =
                    DialogUtils::getText(this, tr("Umbenennen"),
                                         tr("Anzeigename:"), it->text(), &ok);
                if (!ok || newName.trimmed().isEmpty())
                  return;

                auto s = Config::group("NetworkPlaces");

                s.writeEntry(
                    "name_" +
                        QString(itemPath).replace("/", "_").replace(":", "_"),
                    newName.trimmed());
                s.config()->sync();
                populateDrives(); // Neuladen der Spalte
              });
          menu.addAction(
              QIcon::fromTheme("list-remove"), tr("Aus Laufwerken entfernen"),
              this, [this, itemPath]() {
                auto s = Config::group("NetworkPlaces");
                QStringList saved = s.readEntry("places", QStringList());
                const QString npath = mw_normalizePath(itemPath);
                saved.removeAll(npath);
                QString otherVersion = npath.endsWith('/')
                                           ? npath.left(npath.length() - 1)
                                           : npath + "/";
                if (otherVersion != "/")
                  saved.removeAll(otherVersion);

                s.writeEntry("places", saved);
                s.deleteEntry(
                    "name_" +
                    QString(npath).replace("/", "_").replace(":", "_"));
                s.deleteEntry(
                    "name_" +
                    QString(otherVersion).replace("/", "_").replace(":", "_"));
                s.config()->sync();
                emit removeFromPlacesRequested(itemPath);
              });
        }
        menu.addSeparator();
      }

      auto *copyMenu =
          menu.addMenu(QIcon::fromTheme("edit-copy"), tr("Kopieren"));
      copyMenu->setStyleSheet(TM().ssMenu());
      copyMenu->addAction(tr("Pfad kopieren"), [itemPath]() {
        QGuiApplication::clipboard()->setText(itemPath);
      });
      copyMenu->addAction(tr("Name kopieren"), [it]() {
        QGuiApplication::clipboard()->setText(it->text());
      });
      menu.addSeparator();

      // Solid: Aushängen/Einhängen
      if (!udi.isEmpty()) {
        Solid::Device dev(udi);
        const auto *acc = dev.as<Solid::StorageAccess>();
        if (acc) {
          if (acc->isAccessible())
            menu.addAction(QIcon::fromTheme("media-eject"), tr("Aushängen"),
                           this,
                           [this, udi]() { emit teardownRequested(udi); });
          else
            menu.addAction(QIcon::fromTheme("drive-harddisk"), tr("Einhängen"),
                           this, [this, udi]() { emit setupRequested(udi); });
          menu.addSeparator();
        }
      }

      menu.addAction(
          QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"), this,
          [itemPath]() { QGuiApplication::clipboard()->setText(itemPath); });
    } else {
      // --- Ordner-Menü ---
      mw_applyMenuShadow(&menu);
      menu.addAction(QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
                     [this, itemPath]() { emit entryClicked(itemPath, this); });

      auto *openInMenu =
          menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
      openInMenu->setStyleSheet(TM().ssMenu());
      openInMenu->addAction(tr("Linke Ansicht"), this,
                            [this, itemPath]() { emit openInLeft(itemPath); });
      openInMenu->addAction(tr("Rechte Ansicht"), this,
                            [this, itemPath]() { emit openInRight(itemPath); });

      menu.addSeparator();
      menu.addAction(QIcon::fromTheme("utilities-terminal"),
                     tr("Im Terminal öffnen"), this,
                     [this, itemPath]() {
                         auto *job = new KTerminalLauncherJob(QString());
                         job->setWorkingDirectory(itemPath);
                         job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
                         job->start();
                     });
      menu.addSeparator();
      menu.addAction(
          QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"), this,
          [itemPath]() { QGuiApplication::clipboard()->setText(itemPath); });
      menu.addSeparator();
      menu.addAction(
          QIcon::fromTheme("document-properties"), tr("Eigenschaften"), this,
          [this, itemPath]() { emit propertiesRequested(itemPath); });
    }

    menu.exec(m_list->mapToGlobal(pos));
      });
}

void MillerColumn::populateDrives() {
  m_path = "__drives__";
  m_lister->stop();
  m_header->setText(tr("Dieser PC"));
  m_header->setIcon(QIcon::fromTheme("computer"));

  QHash<QString, QPair<double,double>> freeSpaceCache;
  for (int i = 0; i < m_list->count(); ++i) {
    QListWidgetItem *it = m_list->item(i);
    const double total = it->data(Qt::UserRole + 10).toDouble();
    if (total > 0)
      freeSpaceCache.insert(it->data(Qt::UserRole).toString(), {total, it->data(Qt::UserRole + 11).toDouble()});
  }

  m_list->clear();
  m_list->setStyleSheet(TM().ssColDrives());
  m_list->setIconSize(QSize(22, 22));
  m_list->setItemDelegate(new MillerItemDelegate(m_list));
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->verticalScrollBar()->hide();

  QSet<QString> shownUdis;
  QSet<QString> shownPaths;

  // --- Eingehängte Laufwerke ---
  for (const Solid::Device &dev :
       Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess)) {
    const auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      continue;

    // Bereits gezeigtes UDI überspringen
    if (shownUdis.contains(dev.udi()))
      continue;

    const bool mounted = acc->isAccessible();
    const QString p = acc->filePath();

    // Systempartitionen überspringen
    if (mounted) {
      if (p.isEmpty())
        continue;
      if (p.startsWith("/boot") || p.startsWith("/efi") ||
          p.startsWith("/snap"))
        continue;
    }

    // Nicht-gemountete Geräte: nur anzeigen wenn echtes Filesystem-Volume
    if (!mounted) {
      const auto *vol = dev.as<Solid::StorageVolume>();
      if (!vol)
        continue;
      if (vol->usage() != Solid::StorageVolume::FileSystem &&
          vol->usage() != Solid::StorageVolume::Other)
        continue;
      const QString lbl = vol->label().toUpper();
      const QString fs = vol->fsType().toLower();
      if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" ||
          lbl == "ESP")
        continue;
      if (fs == "iso9660" || fs == "udf")
        continue;
    }

    shownUdis.insert(dev.udi());
    if (mounted)
      shownPaths.insert(mw_normalizePath(p));

    QString driveName =
        mounted && (p == "/") ? sc_rootVolumeName() : dev.description();
    if (driveName.isEmpty())
      driveName = mounted ? QDir(p).dirName() : dev.udi().section('/', -1);
    if (driveName.isEmpty())
      continue;

    QString iconName = dev.icon().isEmpty() ? "drive-harddisk" : dev.icon();
    if (const auto *drv = dev.as<Solid::StorageDrive>()) {
      if (drv->driveType() == Solid::StorageDrive::CdromDrive)
        iconName = "drive-optical";
      else if (drv->isRemovable() || drv->isHotpluggable() ||
               (mounted && p.startsWith("/run/media/")))
        iconName = dev.icon().isEmpty() ? "drive-removable-media" : dev.icon();
    }

    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, driveName);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
    it->setData(Qt::UserRole, mounted ? p : QString("solid:") + dev.udi());
    it->setData(Qt::UserRole + 1, dev.udi());
    it->setSizeHint(QSize(0, 50));
    it->setData(Qt::UserRole + 3, true); // Laufwerke sind immer navigierbar
    it->setData(Qt::UserRole + 12, true); // Drive-Root-Item Flag

    if (mounted && !p.isEmpty()) {
      QStorageInfo info(p);
      if (info.isValid()) {
        it->setData(Qt::UserRole + 10, info.bytesTotal() / 1073741824.0);
        it->setData(Qt::UserRole + 11, info.bytesFree()  / 1073741824.0);
      }
    }

    if (!mounted) {
      it->setForeground(QColor(TM().colors().textMuted));
    }
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
    // Entfernt: if (label.isEmpty()) continue;
    if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" || lbl == "ESP") continue;
    if (fsType == "iso9660" || fsType == "udf") continue;

    const auto *access = device.as<Solid::StorageAccess>();
    if (access && access->isAccessible()) continue;

    QString name = label;
    if (name.isEmpty()) name = device.udi().section('/', -1);
    if (name.isEmpty()) continue;  // Fallback, falls UDI leer

    QString iconName = "drive-harddisk";
    if (const auto *drv = device.as<Solid::StorageDrive>()) {
      if (drv->driveType() == Solid::StorageDrive::CdromDrive) iconName = "drive-optical";
      else if (drv->isRemovable() || drv->isHotpluggable())    iconName = "drive-removable-media";
    } else if (!device.icon().isEmpty()) {
      iconName = device.icon();
    }

    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, name);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
    it->setData(Qt::UserRole, QString(QStringLiteral("solid:") + device.udi()));
    it->setData(Qt::UserRole + 1, device.udi());
    it->setSizeHint(QSize(0, SC_MILLER_DRIVE_ROW_H));
    it->setData(Qt::UserRole + 3, true); // Laufwerke sind immer navigierbar
    it->setData(Qt::UserRole + 12, true); // Drive-Root-Item Flag
    it->setForeground(QColor(TM().colors().textMuted));
  }

  // --- Netzwerklaufwerke (FUSE-gemountet) ---
  for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
    if (!storage.isValid() || !storage.isReady())
      continue;
    const QString fs = storage.fileSystemType();
    if (fs != "cifs" && fs != "smb3" && fs != "nfs" && fs != "nfs4" &&
        fs != "sshfs" && fs != "fuse.sshfs" && fs != "davfs" &&
        fs != "fuse.davfs2" && !fs.startsWith("fuse."))
      continue;
    if (fs == "fuse.portal" || fs == "fusectl")
      continue;
    const QString path = storage.rootPath();
    const QString normalizedPath = mw_normalizePath(path);
    if (shownPaths.contains(normalizedPath))
      continue;
    shownPaths.insert(normalizedPath);
    QString name = storage.name().isEmpty()
                       ? QUrl::fromLocalFile(path).fileName()
                       : storage.name();
    if (name.isEmpty())
      name = path;
    QString icon = (fs == "cifs" || fs == "smb3")          ? "network-workgroup"
                   : (fs == "sshfs" || fs == "fuse.sshfs") ? "network-connect"
                                                           : "network-server";
    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, name);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(icon));
    it->setData(Qt::UserRole, path);
    it->setData(Qt::UserRole + 1, fs);
    it->setSizeHint(QSize(0, SC_MILLER_DRIVE_ROW_H));
    it->setData(Qt::UserRole + 3, true); // Laufwerke sind immer navigierbar
    it->setData(Qt::UserRole + 12, true); // Drive-Root-Item Flag
  }

  // --- Gespeicherte Netzwerkplätze (NetworkPlaces) ---
  {
    auto netSettings = Config::group("NetworkPlaces");
    const QStringList savedPlaces =
        netSettings.readEntry("places", QStringList());
    for (const QString &p : savedPlaces) {
      if (p.isEmpty())
        continue;
      const QString normalizedP = mw_normalizePath(p);
      if (shownPaths.contains(normalizedP))
        continue;
      shownPaths.insert(normalizedP);
      const QUrl pUrl(p);
      const QString scheme = pUrl.scheme().toLower();
      const QString savedName = netSettings.readEntry(
          "name_" + QString(p).replace("/", "_").replace(":", "_"),
          scheme == "gdrive" ? "Google Drive" : pUrl.fileName());

      if (savedName.isEmpty())
        continue;

      const QString savedKey = QString(p).replace("/", "_").replace(":", "_");
      QString iconName = netSettings.readEntry("icon_" + savedKey, QString());
      if (iconName.isEmpty()) {
        iconName = scheme == "gdrive"      ? "folder-gdrive"
                 : scheme == "smb"        ? "folder-remote-smb"
                 : scheme == "sftp"       ? "network-connect"
                 : scheme == "mtp"        ? "multimedia-player"
                 : scheme == "bluetooth"  ? "bluetooth"
                                          : "network-server";
        // Einmalig speichern
        netSettings.writeEntry("icon_" + savedKey, iconName);
        netSettings.config()->sync();
      }
      auto *it = new QListWidgetItem(m_list);
      it->setData(Qt::DisplayRole, savedName);
      it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
      QString url = pUrl.toString();
      if ((url.startsWith("gdrive:") || url.startsWith("mtp:")) &&
          !url.endsWith("/"))
        url += "/";
      it->setData(Qt::UserRole, url);
      it->setData(Qt::UserRole + 1, scheme);
      it->setSizeHint(QSize(0, SC_MILLER_DRIVE_ROW_H));
    it->setData(Qt::UserRole + 3, true); // Laufwerke sind immer navigierbar
    it->setData(Qt::UserRole + 12, true); // Drive-Root-Item Flag

      // Gespeicherten Cache auslesen
      const double cachedTotal = netSettings.readEntry("total_" + savedKey, 0.0);
      const double cachedFree  = netSettings.readEntry("free_" + savedKey, 0.0);
      if (cachedTotal > 0) {
        it->setData(Qt::UserRole + 10, cachedTotal);
        it->setData(Qt::UserRole + 11, cachedFree);
      }

      // Cache wiederverwenden oder neuen Job starten
      if (freeSpaceCache.contains(url)) {
        const auto &fs = freeSpaceCache.value(url);
        it->setData(Qt::UserRole + 10, fs.first);
        it->setData(Qt::UserRole + 11, fs.second);
      } else {
        auto *freeJob = KIO::fileSystemFreeSpace(QUrl(url));
        freeJob->setAutoDelete(true);
        const QString itemUrl = url;
        QPointer<QListWidget> listPtr = m_list;
        connect(freeJob, &KIO::FileSystemFreeSpaceJob::result, m_list,
                [listPtr, itemUrl, savedKey, freeJob](KJob *) {
                  if (freeJob->error() || !listPtr) return;
                  const double total = freeJob->size()          / 1073741824.0;
                  const double free  = freeJob->availableSize() / 1073741824.0;
                  if (total <= 0) return;

                  auto s = Config::group("NetworkPlaces");
                  s.writeEntry("total_" + savedKey, total);
                  s.writeEntry("free_" + savedKey, free);

                  for (int i = 0; i < listPtr->count(); ++i) {
                    QListWidgetItem *it = listPtr->item(i);
                    if (it && it->data(Qt::UserRole).toString() == itemUrl) {
                      it->setData(Qt::UserRole + 10, total);
                      it->setData(Qt::UserRole + 11, free);
                      break;
                    }
                  }
                });
      }
    }
  }
}

void MillerColumn::populateDir(const QString &path) {
  m_path = path;
  QUrl url(path);
  if (url.scheme().isEmpty() && !path.isEmpty())
    url = QUrl::fromUserInput(path);

  QString name;
  const QString local = url.toLocalFile();
  if (path == "__drives__")
    name = tr("Dieser PC");
  else if (local == "/" || (url.path() == "/" && url.scheme().isEmpty()))
    name = sc_rootVolumeName();
  else
    name = url.fileName().isEmpty() ? (local.isEmpty() ? path : local)
                                    : url.fileName();

  m_header->setText(name);
  if (path == "__drives__")
    m_header->setIcon(QIcon::fromTheme("computer"));
  else if (url.scheme() == "gdrive")
    m_header->setIcon(QIcon::fromTheme("folder-gdrive"));
  else if (url.scheme() == "mtp")
    m_header->setIcon(QIcon::fromTheme("multimedia-player"));
  else
    m_header->setIcon(QIcon::fromTheme("folder"));

  m_list->clear();
  m_list->setStyleSheet(TM().ssColInactive());
  m_list->setIconSize(QSize(22, 22));
  m_list->setItemDelegate(new MillerItemDelegate(m_list));

  if (path != "__drives__") {
    m_lister->stop();
    m_lister->openUrl(url, KDirLister::NoFlags);
  }
}

void MillerColumn::refreshStyle() {
  setActive(m_list->styleSheet() != TM().ssColInactive());
}

void MillerColumn::setActive(bool active) {
  m_list->setStyleSheet(
      m_path == "__drives__"
          ? TM().ssColDrives()
          : (active ? TM().ssColActive() : TM().ssColInactive()));
}

// --- MillerArea ---
static constexpr int FULL_COLS = 3;

MillerArea::MillerArea(QWidget *parent) : QWidget(parent) {
  setStyleSheet(TM().ssPane() + "border-top:none;");
  auto *outerLay = new QVBoxLayout(this);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->setSpacing(0);

  m_rowWidget = new QWidget();
  m_rowWidget->setStyleSheet(TM().ssPane());
  m_rowLayout = new QHBoxLayout(m_rowWidget);
  m_rowLayout->setContentsMargins(0, 0, 0, 0);
  m_rowLayout->setSpacing(0);

  m_stripDivider = new QFrame();
  m_stripDivider->setFrameShape(QFrame::VLine);
  m_stripDivider->setStyleSheet(
      QString("background:%1;color:%1;").arg(TM().colors().colActive));
  m_stripDivider->setFixedWidth(2);
  m_stripDivider->setFrameShadow(QFrame::Sunken);
  m_stripDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  m_stripDivider->setVisible(false);
  m_rowLayout->addWidget(m_stripDivider);

  m_colContainer = new QWidget();
  m_colContainer->setStyleSheet(TM().ssPane());
  m_colLayout = new QHBoxLayout(m_colContainer);
  m_colLayout->setContentsMargins(0, 0, 0, 0);
  m_colLayout->setSpacing(0);
  m_rowLayout->addWidget(m_colContainer, 1);

  outerLay->addWidget(m_rowWidget, 1);
}

void MillerArea::updateVisibleColumns() {
  const int n = m_cols.size();
  const int stripCount = qMax(0, n - FULL_COLS);

  // 1. Alte Strips bereinigen
  for (auto *s : m_strips) {
    s->hide();
    m_rowLayout->removeWidget(s);
    s->deleteLater();
  }
  m_strips.clear();

  // 2. Neue Strips für ausgeblendete Spalten
  for (int i = 0; i < stripCount; ++i) {
    QUrl u(m_cols[i]->path());
    QString label = (i == 0) ? QStringLiteral("This PC") : u.fileName();
    if (label.isEmpty())
      label = u.path();
    if (label.isEmpty())
      label = m_cols[i]->path();

    auto *strip = new MillerStrip(label, m_rowWidget);
    m_rowLayout->insertWidget(i, strip);
    m_strips.append(strip);

    connect(strip, &MillerStrip::clicked, this, [this, i]() {
      emit focusRequested();
      QString targetPath = m_cols[i]->path();
      while (m_cols.size() > i + 1) {
        trimAfter(m_cols[i]);
      }
      updateVisibleColumns();
      emit headerClicked(targetPath);
    });
  }

  // Alte Trenner entfernen und neu aufbauen
  for (auto *sep : m_colSeparators) {
    sep->hide();
    m_colLayout->removeWidget(sep);
    sep->deleteLater();
  }
  m_colSeparators.clear();

  m_stripDivider->setVisible(stripCount > 0);

  // Spalten ein-/ausblenden und Trenner neu aufbauen
  for (int i = 0; i < n; ++i) {
    const bool vis = (i >= n - FULL_COLS);
    m_cols[i]->setVisible(vis);
    m_colLayout->setStretchFactor(m_cols[i], vis ? 1 : 0);

    if (vis && i > stripCount) {
      QFrame *sep = new QFrame(m_colContainer);
      sep->setFixedWidth(1);
      sep->setStyleSheet(
          QString("background:%1;border:none;").arg(TM().colors().separator));

      int layoutIdx = m_colLayout->indexOf(m_cols[i]);
      m_colLayout->insertWidget(layoutIdx, sep);
      m_colSeparators.append(sep);
    }
  }
}

void MillerArea::refreshDrives() {
  if (m_cols.isEmpty())
    return;
  // Nur den Inhalt der ersten Spalte aktualisieren, ohne die Navigation zu
  // unterbrechen
  m_cols[0]->populateDrives();
}

void MillerArea::init() {
  auto *col = new MillerColumn();
  col->populateDrives();
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);
  m_activeCol = col;

  connect(col, &MillerColumn::entryClicked, this,
          [this, col](const QString &path, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;
            QString p = path;
            if ((p.startsWith("gdrive:") || p.startsWith("mtp:")) &&
                !p.endsWith("/"))
              p += "/";

            // isDir aus dem Item lesen (gespeichert in UserRole+3)
            bool itemIsDir = false;
            for (int i = 0; i < col->list()->count(); ++i) {
              if (col->list()->item(i)->data(Qt::UserRole).toString() == path) {
                itemIsDir = col->list()->item(i)->data(Qt::UserRole + 3).toBool();
                break;
              }
            }
            const QUrl u = QUrl::fromUserInput(p);
            const QString sch = u.scheme();
            const bool isKioDir = (sch == "gdrive" || sch == "mtp" ||
                sch == "smb" || sch == "sftp" || sch == "ftp" || sch == "remote");

            if (isKioDir || itemIsDir) {
              appendColumn(p);
              emit pathChanged(p);
            } else {
              auto *job = new KIO::OpenUrlJob(u);
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(
                  KJobUiDelegate::AutoHandlingEnabled, nullptr));
              job->start();
            }
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);

  connect(col, &MillerColumn::teardownRequested, this,
          &MillerArea::teardownRequested);
  connect(col, &MillerColumn::setupRequested, this, [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;
    connect(
        acc, &Solid::StorageAccess::setupDone, this,
        [this](Solid::ErrorType, QVariant, const QString &) {
          refreshDrives();
          emit drivesChanged(); // Sidebar aktualisieren
        },
        Qt::SingleShotConnection);
    acc->setup();
  });
  connect(col, &MillerColumn::removeFromPlacesRequested, this,
          &MillerArea::removeFromPlacesRequested);
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
}

void MillerArea::refresh() {
  for (auto *col : m_cols) {
    if (col->path() == "__drives__")
      col->populateDrives();
    else
      col->populateDir(col->path());
  }
}

void MillerArea::appendColumn(const QString &path) {
  auto *col = new MillerColumn();
  col->populateDir(path);
  col->setActive(true);
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);

  connect(col, &MillerColumn::entryClicked, this,
          [this, col](const QString &p2, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;

            QUrl u2 = QUrl::fromUserInput(p2);
            const QString sch2 = u2.scheme();
            const bool isKioDir =
                (sch2 == "gdrive" || sch2 == "mtp" || sch2 == "smb" ||
                 sch2 == "sftp" || sch2 == "ftp" || sch2 == "remote");

            // isDir aus UserRole+3 lesen
            bool itemIsDir = false;
            for (int i = 0; i < col->list()->count(); ++i) {
              if (col->list()->item(i)->data(Qt::UserRole).toString() == p2) {
                itemIsDir = col->list()->item(i)->data(Qt::UserRole + 3).toBool();
                break;
              }
            }

            if (isKioDir || itemIsDir) {
              QString nav = p2;
              if (isKioDir && !nav.endsWith("/"))
                nav += "/";
              appendColumn(nav);
              emit pathChanged(p2);
            } else {
              auto *job = new KIO::OpenUrlJob(u2);
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(
                  KJobUiDelegate::AutoHandlingEnabled, nullptr));
              job->start();
            }
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
  updateVisibleColumns();

  // Slide-in Animation für die neue Spalte
  col->setMaximumWidth(0);
  auto *anim = new QPropertyAnimation(col, "maximumWidth");
  anim->setDuration(400);
  anim->setStartValue(0);
  anim->setEndValue(2000); // Erlaubt dem Layout, die Spalte normal zu füllen
  anim->setEasingCurve(QEasingCurve::OutQuad);
  connect(anim, &QPropertyAnimation::finished, col, [col]() {
      col->setMaximumWidth(16777215); // Zurück auf Standard (QWIDGETSIZE_MAX)
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MillerArea::trimAfter(MillerColumn *col) {
  const int idx = m_cols.indexOf(col);
  if (idx < 0)
    return;
  while (m_cols.size() > idx + 1) {
    auto *last = m_cols.takeLast();
    last->hide();
    m_colLayout->removeWidget(last);
    last->deleteLater();

    // Zugehörigen Trenner ebenfalls entfernen
    if (!m_colSeparators.isEmpty()) {
      auto *sep = m_colSeparators.takeLast();
      sep->hide();
      m_colLayout->removeWidget(sep);
      sep->deleteLater();
    }
  }
  updateVisibleColumns();
}

void MillerArea::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); }

QString MillerArea::activePath() const {
  return m_activeCol ? m_activeCol->path() : QString();
}

QList<QUrl> MillerArea::selectedUrls() const {
  return {}; // Löschen in Miller-Spalten deaktiviert
}

void MillerArea::navigateTo(const QString &path, bool clearForward) {
  (void)clearForward;
  if (path.isEmpty())
    return;

  if (path == "__drives__") {
    if (!m_cols.isEmpty()) {
      trimAfter(m_cols[0]);
      m_cols[0]->populateDrives();
      m_cols[0]->list()->clearSelection();
      m_cols[0]->setActive(true);
      m_activeCol = m_cols[0];
    }
    return;
  }

  QUrl startUrl(path);
  if (startUrl.scheme().isEmpty() && !path.isEmpty())
    startUrl = QUrl::fromUserInput(path);
  if (startUrl.isLocalFile() && !QFileInfo::exists(startUrl.toLocalFile()))
    return;

  QString drivePath;
  if (!m_cols.isEmpty()) {
    QListWidget *driveList = m_cols[0]->list();
    const QString normPath =
        startUrl.isLocalFile() ? startUrl.toLocalFile() : startUrl.toString();
    const QString targetNorm = mw_normalizePath(normPath);
    for (int i = 0; i < driveList->count(); ++i) {
      QString dp = driveList->item(i)->data(Qt::UserRole).toString();
      QString normDp = dp;
      if (normDp.startsWith("file://"))
        normDp = QUrl(normDp).toLocalFile();
      const QString dpNorm = mw_normalizePath(normDp);

      if (!dpNorm.isEmpty() &&
          (targetNorm == dpNorm || targetNorm.startsWith(dpNorm + "/"))) {
        if (dpNorm.length() >= mw_normalizePath(drivePath).length()) {
          drivePath = dp;
          driveList->setCurrentRow(i);
          m_cols[0]->setActive(true);
        }
      }
    }
    // Fallback für KIO-Protokolle falls Discovery noch nicht fertig
    if (drivePath.isEmpty()) {
      if (startUrl.scheme() == "gdrive")
        drivePath = "gdrive:/";
      else if (startUrl.scheme() == "mtp")
        drivePath = "mtp:/";
      else if (startUrl.scheme() == "remote")
        drivePath = "remote:/";
    }
    trimAfter(m_cols[0]);
  }

  QStringList segments;
  QUrl cur = startUrl;
  while (cur.isValid()) {
    const QString curStr = cur.toString();
    segments.prepend(curStr);
    if (!drivePath.isEmpty() &&
        mw_normalizePath(curStr) == mw_normalizePath(drivePath))
      break; // Stop bei Laufwerks-Wurzel

    QUrl up = cur.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    if (up == cur ||
        (up.path().isEmpty() && up.host().isEmpty() && up.scheme() != "file"))
      break;
    cur = up;
  }

  const QString targetDir = startUrl.toString();

  // Segmente überspringen die VOR dem drivePath liegen
  int startIdx = 0;
  if (!drivePath.isEmpty()) {
    for (int i = 0; i < segments.size(); ++i) {
      if (mw_normalizePath(segments[i]) == mw_normalizePath(drivePath)) {
        startIdx = i;
        break;
      }
    }
  }

  for (int i = startIdx + 1; i < segments.size(); ++i) {
    const QString seg = segments[i - 1];
    appendColumn(seg);
    if (!m_cols.isEmpty()) {
      MillerColumn *col = m_cols.last();
      const QString next = segments[i];
      for (int r = 0; r < col->list()->count(); ++r) {
        if (col->list()->item(r)->data(Qt::UserRole).toString() == next) {
          col->list()->setCurrentRow(r);
          break;
        }
      }
    }
  }

  if (m_cols.isEmpty() || m_cols.last()->path() != targetDir)
    appendColumn(targetDir);

  updateVisibleColumns();
}

void MillerArea::setFocused(bool f) {
  m_focused = f;
  setStyleSheet(TM().ssPane());
}

