#include "drivemanager.h"
#include "config.h"
#include "scglobal.h"

#include <QDir>
#include <QPointer>
#include <QStorageInfo>
#include <QUrl>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <KIO/FileSystemFreeSpaceJob>
#include <QTimer>

DriveManager* DriveManager::instance() {
    static DriveManager mgr;
    return &mgr;
}

DriveManager::DriveManager(QObject *parent) : QObject(parent) {
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, [this](const QString &) { refreshAll(); });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, [this](const QString &) { refreshAll(); });
    
    // Initial scan deferred slightly
    QTimer::singleShot(0, this, &DriveManager::refreshAll);
}

void DriveManager::refreshAll() {
    refreshLocal();
    refreshNetwork();
    emit drivesUpdated();
}

void DriveManager::refreshLocal() {
    m_localDrives.clear();
    QSet<QString> shownPaths;
    QSet<QString> shownUdis;

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        const auto *access = device.as<Solid::StorageAccess>();
        if (!access) continue;

        const bool mounted = access->isAccessible();
        const QString path = mounted ? access->filePath() : QString();

        if (mounted) {
            if (path.isEmpty() || shownPaths.contains(path)) continue;
            if (path.startsWith("/boot") || path.startsWith("/efi") ||
                path.startsWith("/snap") || path == "/home") continue;
            if (path.startsWith("/var/lib/docker") || path.startsWith("/var/lib/containers")) continue;
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
            else if (drv->isRemovable() || drv->isHotpluggable() || (mounted && path.startsWith("/run/media/")))
                iconName = "drive-removable-media";
            else
                iconName = "drive-harddisk";
        } else {
            iconName = device.icon().isEmpty()
                ? ((mounted && path.startsWith("/run/media/")) ? "drive-removable-media" : "drive-harddisk")
                : device.icon();
        }

        double totalG = 0;
        double freeG  = 0;
        if (mounted) {
            QStorageInfo info(path);
            if (info.isValid()) {
                totalG = info.bytesTotal() / 1073741824.0;
                freeG  = info.bytesFree()  / 1073741824.0;
            }
        }

        DriveInfo info;
        info.isNetwork = false;
        info.isMounted = mounted;
        info.path = mounted ? path : QString("solid:%1").arg(device.udi());
        info.name = name;
        info.iconName = iconName;
        info.udi = device.udi();
        info.total = totalG;
        info.free = freeG;
        m_localDrives.append(info);
    }

    const auto vdevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &device : vdevices) {
        if (shownUdis.contains(device.udi())) continue;
        const auto *vol = device.as<Solid::StorageVolume>();
        if (!vol || (vol->usage() != Solid::StorageVolume::FileSystem && vol->usage() != Solid::StorageVolume::Other)) continue;
        const QString fsType = vol->fsType().toLower();
        if (fsType == "iso9660" || fsType == "udf" || fsType == "swap" || fsType == "vfat" || fsType == "fat32") continue;
        const QString lbl = vol->label().toUpper();
        if (lbl.isEmpty() || lbl == "EFI" || lbl == "BOOT" || lbl == "EFI SYSTEM PARTITION" || lbl == "ESP" || lbl.startsWith("RECOVERY")) continue;
        const auto *acc = device.as<Solid::StorageAccess>();
        if (acc && acc->isAccessible()) continue;
        shownUdis.insert(device.udi());

        QString iconName = "drive-harddisk";
        if (const auto *drv = device.as<Solid::StorageDrive>()) {
            if (drv->driveType() == Solid::StorageDrive::CdromDrive)
                iconName = "drive-optical";
            else if (drv->isRemovable() || drv->isHotpluggable())
                iconName = "drive-removable-media";
        }

        QString name = vol->label();
        if (name.isEmpty()) name = device.udi().section('/', -1);

        DriveInfo info;
        info.isNetwork = false;
        info.isMounted = false;
        info.path = QString("solid:%1").arg(device.udi());
        info.name = name;
        info.iconName = iconName;
        info.udi = device.udi();
        info.total = 0;
        info.free = 0;
        m_localDrives.append(info);
    }
}

void DriveManager::refreshNetwork() {
    // Preserve existing sizes in cache before clearing
    for (const auto& drive : m_networkDrives) {
        if (drive.total > 0) {
            m_netFreeCache.insert(drive.path, {drive.total, drive.free});
        }
    }
    m_networkDrives.clear();

    QSet<QString> shownPaths;
    auto netSettings = Config::group("NetworkPlaces");
    QStringList savedPlaces = netSettings.readEntry("places", QStringList());

    for (const QString &p : savedPlaces) {
        if (p.isEmpty()) continue;
        const QString normalizedP = mw_normalizePath(p);
        if (shownPaths.contains(normalizedP)) continue;
        shownPaths.insert(normalizedP);

        const QUrl pUrl(p);
        const QString scheme = pUrl.scheme().toLower();
        const QString savedName = netSettings.readEntry("name_" + QString(p).replace("/","_").replace(":","_"),
            scheme == "gdrive" ? "Google Drive" : pUrl.fileName());

        if (savedName.isEmpty()) continue;

        const QString savedKey = QString(p).replace("/","_").replace(":","_");
        QString iconName = netSettings.readEntry("icon_" + savedKey, QString());
        if (iconName.isEmpty()) {
            iconName = scheme == "gdrive"      ? "folder-gdrive"
                     : scheme == "smb"        ? "folder-remote-smb"
                     : scheme == "sftp"       ? "network-connect"
                     : scheme == "mtp"        ? "multimedia-player"
                     : scheme == "bluetooth"  ? "bluetooth"
                                              : "network-server";
            netSettings.writeEntry("icon_" + savedKey, iconName);
            netSettings.config()->sync();
        }

        QString url = pUrl.toString();
        if ((scheme == "gdrive" || scheme == "mtp") && !url.endsWith("/"))
            url += "/";

        DriveInfo info;
        info.isNetwork = true;
        info.isMounted = true; // KIO drives are always "accessible" via their url
        info.path = url;
        info.name = savedName;
        info.iconName = iconName;
        info.scheme = scheme;
        info.total = netSettings.readEntry("total_" + savedKey, 0.0);
        info.free  = netSettings.readEntry("free_" + savedKey, 0.0);

        if (m_netFreeCache.contains(url)) {
            const auto &fs = m_netFreeCache.value(url);
            info.total = fs.first;
            info.free = fs.second;
        } else {
            const QUrl freeSpaceUrl = [&]() -> QUrl {
                const QUrl u(url);
                if (u.scheme() == QStringLiteral("gdrive") && u.path() == QStringLiteral("/")) {
                    return u;
                }
                return u;
            }();

            auto *freeJob = KIO::fileSystemFreeSpace(freeSpaceUrl);
            freeJob->setAutoDelete(true);
            const QString itemUrl = url;
            connect(freeJob, &KIO::FileSystemFreeSpaceJob::result, this,
                    [this, itemUrl, savedKey, freeJob](KJob *) {
                        if (freeJob->error()) return;
                        const double total = freeJob->size()          / 1073741824.0;
                        const double free  = freeJob->availableSize() / 1073741824.0;
                        if (total <= 0) return;

                        auto s = Config::group("NetworkPlaces");
                        s.writeEntry("total_" + savedKey, total);
                        s.writeEntry("free_" + savedKey, free);

                        // Update in cache and current list
                        m_netFreeCache.insert(itemUrl, {total, free});
                        bool updated = false;
                        for (int i = 0; i < m_networkDrives.size(); ++i) {
                            if (m_networkDrives[i].path == itemUrl) {
                                m_networkDrives[i].total = total;
                                m_networkDrives[i].free = free;
                                updated = true;
                                break;
                            }
                        }
                        if (updated) emit drivesUpdated();
                    });
        }
        m_networkDrives.append(info);
    }

    // Mounted Network Drives (FUSE, etc.)
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        const QString fs = storage.fileSystemType();
        if (fs != "cifs" && fs != "smb3" && fs != "nfs" && fs != "nfs4" &&
            fs != "sshfs" && fs != "fuse.sshfs" && fs != "davfs" &&
            fs != "fuse.davfs2" && !fs.startsWith("fuse.")) continue;
        if (fs == "fuse.portal" || fs == "fusectl") continue;

        const QString path = storage.rootPath();
        const QString normalizedPath = mw_normalizePath(path);
        if (shownPaths.contains(normalizedPath)) continue;
        shownPaths.insert(normalizedPath);

        QString name = storage.name().isEmpty() ? QUrl::fromLocalFile(path).fileName() : storage.name();
        if (name.isEmpty()) name = path;

        QString icon = (fs == "cifs" || fs == "smb3") ? "network-workgroup"
                     : (fs == "sshfs" || fs == "fuse.sshfs") ? "network-connect"
                     : "network-server";

        DriveInfo info;
        info.isNetwork = true;
        info.isMounted = true;
        info.path = path;
        info.name = name;
        info.iconName = icon;
        info.subtitle = QString(fs + " – " + path);
        info.scheme = fs;
        info.total = storage.bytesTotal() / 1073741824.0;
        info.free = storage.bytesFree() / 1073741824.0;
        m_networkDrives.append(info);
    }
}
