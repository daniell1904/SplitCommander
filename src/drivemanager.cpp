// Kernkomponente für das Drive-Management
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

DriveManager* DriveManager::instance()
{
    static DriveManager mgr;
    return &mgr;
}

QList<DriveInfo> DriveManager::localDrives() const
{
    return m_localDrives;
}

QList<DriveInfo> DriveManager::networkDrives() const
{
    return m_networkDrives;
}

DriveManager::DriveManager(QObject *parent)
    : QObject(parent)
{
    auto *notifier = Solid::DeviceNotifier::instance();
    Q_ASSERT(notifier != nullptr);

    connect(notifier, &Solid::DeviceNotifier::deviceAdded,
            this, [this](const QString &) { refreshAll(); });
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved,
            this, [this](const QString &) { refreshAll(); });
    
    QTimer::singleShot(0, this, &DriveManager::refreshAll);
}

void DriveManager::refreshAll()
{
    refreshLocal();
    refreshNetwork();
    emit drivesUpdated();
}

void DriveManager::refreshLocal()
{
    m_localDrives.clear();
    QSet<QString> shownPaths;
    QSet<QString> shownUdis;

    processStorageAccessDevices(shownPaths, shownUdis);
    processStorageVolumeDevices(shownUdis);
}

void DriveManager::refreshNetwork()
{
    for (const auto &drive : m_networkDrives)
    {
        if (drive.total > 0.0)
        {
            m_netFreeCache.insert(drive.path, {drive.total, drive.free});
        }
    }
    m_networkDrives.clear();

    QSet<QString> shownPaths;
    processSavedNetworkPlaces(shownPaths);
    processMountedNetworkVolumes(shownPaths);
}

void DriveManager::processStorageAccessDevices(QSet<QString> &shownPaths, QSet<QString> &shownUdis)
{
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices)
    {
        processStorageAccessDeviceItem(device, shownPaths, shownUdis);
    }
}

void DriveManager::processStorageAccessDeviceItem(const Solid::Device &device, QSet<QString> &shownPaths, QSet<QString> &shownUdis)
{
    const auto *access = device.as<Solid::StorageAccess>();
    if (access == nullptr) { return; }

    const bool mounted = access->isAccessible();
    const QString path = mounted ? access->filePath() : QString();

    if (mounted && shownPaths.contains(path)) { return; }
    if (isStorageAccessDeviceBlacklisted(device, mounted, path)) { return; }

    if (mounted)
    {
        shownPaths.insert(path);
    }
    shownUdis.insert(device.udi());

    const auto *vol = device.as<Solid::StorageVolume>();
    QString name = determineStorageDeviceName(device, mounted, path, vol);
    QString iconName = determineStorageDeviceIcon(device, mounted, path);

    double totalG = 0.0;
    double freeG  = 0.0;
    if (mounted)
    {
        QStorageInfo info(path);
        if (info.isValid())
        {
            totalG = info.bytesTotal() / 1073741824.0;
            freeG  = info.bytesFree()  / 1073741824.0;
        }
    }

    DriveInfo info;
    info.isNetwork = false;
    info.isMounted = mounted;
    info.path = mounted ? path : QStringLiteral("solid:%1").arg(device.udi());
    info.name = name;
    info.iconName = iconName;
    info.udi = device.udi();
    info.total = totalG;
    info.free = freeG;
    m_localDrives.append(info);
}

bool DriveManager::isStorageAccessDeviceBlacklisted(const Solid::Device &device, bool mounted, const QString &path)
{
    if (mounted)
    {
        if (path.isEmpty()) { return true; }
        if (path.startsWith(QStringLiteral("/boot")) || path.startsWith(QStringLiteral("/efi")) ||
            path.startsWith(QStringLiteral("/snap")) || path == QStringLiteral("/home"))
        {
            return true;
        }
        const QStringList blacklist = Config::driveBlacklist();
        for (const QString &bl : blacklist)
        {
            if (!bl.isEmpty() && path.startsWith(bl))
            {
                return true;
            }
        }
    }

    const auto *vol = device.as<Solid::StorageVolume>();
    if (vol != nullptr)
    {
        const QString lbl = vol->label().toUpper();
        const QString fsType = vol->fsType().toLower();
        if (lbl == QStringLiteral("BOOT") || lbl == QStringLiteral("EFI") || 
            lbl == QStringLiteral("EFI SYSTEM PARTITION") || lbl == QStringLiteral("ESP"))
        {
            return true;
        }
        if (fsType == QStringLiteral("iso9660") || fsType == QStringLiteral("udf"))
        {
            return true;
        }
    }
    return false;
}

QString DriveManager::determineStorageDeviceName(const Solid::Device &device, bool mounted, const QString &path, const Solid::StorageVolume *vol)
{
    QString name = (mounted && path == QStringLiteral("/")) ? sc_rootVolumeName() : device.description();
    if (name.isEmpty() && vol != nullptr)
    {
        name = vol->label();
    }
    if (name.isEmpty())
    {
        name = device.udi().section(QLatin1Char('/'), -1);
    }
    return name;
}

QString DriveManager::determineStorageDeviceIcon(const Solid::Device &device, bool mounted, const QString &path)
{
    if (const auto *drv = device.as<Solid::StorageDrive>())
    {
        if (drv->driveType() == Solid::StorageDrive::CdromDrive)
        {
            return QStringLiteral("drive-optical");
        }
        if (drv->isRemovable() || drv->isHotpluggable() || (mounted && path.startsWith(QStringLiteral("/run/media/"))))
        {
            return QStringLiteral("drive-removable-media");
        }
        return QStringLiteral("drive-harddisk");
    }
    return device.icon().isEmpty()
        ? ((mounted && path.startsWith(QStringLiteral("/run/media/"))) ? QStringLiteral("drive-removable-media") : QStringLiteral("drive-harddisk"))
        : device.icon();
}

void DriveManager::processStorageVolumeDevices(const QSet<QString> &shownUdis)
{
    const auto vdevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &device : vdevices)
    {
        if (shownUdis.contains(device.udi())) { continue; }
        const auto *vol = device.as<Solid::StorageVolume>();
        if (vol == nullptr || (vol->usage() != Solid::StorageVolume::FileSystem && vol->usage() != Solid::StorageVolume::Other))
        {
            continue;
        }
        const QString fsType = vol->fsType().toLower();
        if (fsType == QStringLiteral("iso9660") || fsType == QStringLiteral("udf") || 
            fsType == QStringLiteral("swap") || fsType == QStringLiteral("vfat") || fsType == QStringLiteral("fat32"))
        {
            continue;
        }
        const QString lbl = vol->label().toUpper();
        if (lbl.isEmpty() || lbl == QStringLiteral("EFI") || lbl == QStringLiteral("BOOT") || 
            lbl == QStringLiteral("EFI SYSTEM PARTITION") || lbl == QStringLiteral("ESP") || lbl.startsWith(QStringLiteral("RECOVERY")))
        {
            continue;
        }
        const auto *acc = device.as<Solid::StorageAccess>();
        if (acc != nullptr && acc->isAccessible()) { continue; }

        QString iconName = QStringLiteral("drive-harddisk");
        if (const auto *drv = device.as<Solid::StorageDrive>())
        {
            if (drv->driveType() == Solid::StorageDrive::CdromDrive)
            {
                iconName = QStringLiteral("drive-optical");
            }
            else if (drv->isRemovable() || drv->isHotpluggable())
            {
                iconName = QStringLiteral("drive-removable-media");
            }
        }

        QString name = vol->label();
        if (name.isEmpty())
        {
            name = device.udi().section(QLatin1Char('/'), -1);
        }

        DriveInfo info;
        info.isNetwork = false;
        info.isMounted = false;
        info.path = QStringLiteral("solid:%1").arg(device.udi());
        info.name = name;
        info.iconName = iconName;
        info.udi = device.udi();
        info.total = 0.0;
        info.free = 0.0;
        m_localDrives.append(info);
    }
}

void DriveManager::processSavedNetworkPlaces(QSet<QString> &shownPaths)
{
    auto netSettings = Config::group(QStringLiteral("NetworkPlaces"));
    const QStringList savedPlaces = netSettings.readEntry(QStringLiteral("places"), QStringList());

    for (const QString &p : savedPlaces)
    {
        processSavedNetworkPlaceItem(p, shownPaths, netSettings);
    }
}

void DriveManager::processSavedNetworkPlaceItem(const QString &p, QSet<QString> &shownPaths, KConfigGroup &netSettings)
{
    if (p.isEmpty()) { return; }
    const QString normalizedP = mw_normalizePath(p);
    if (shownPaths.contains(normalizedP)) { return; }
    shownPaths.insert(normalizedP);

    const QUrl pUrl(p);
    const QString scheme = pUrl.scheme().toLower();
    QString savedName;
    QString iconName;
    determineNetworkPlaceNameAndIcon(p, pUrl, scheme, netSettings, savedName, iconName);

    if (savedName.isEmpty()) { return; }

    QString url = pUrl.toString();
    if ((scheme == QStringLiteral("gdrive") || scheme == QStringLiteral("mtp")) && !url.endsWith(QLatin1Char('/')))
    {
        url += QLatin1Char('/');
    }

    DriveInfo info;
    info.isNetwork = true;
    info.isMounted = true;
    info.path = url;
    info.name = savedName;
    info.iconName = iconName;
    info.scheme = scheme;

    const QString savedKey = QString(p).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
    setupNetworkPlaceFreeSpace(pUrl, url, savedKey, info, netSettings);

    m_networkDrives.append(info);
}

void DriveManager::determineNetworkPlaceNameAndIcon(const QString &p, const QUrl &pUrl, const QString &scheme, KConfigGroup &netSettings, QString &savedName, QString &iconName)
{
    const QString savedKey = QString(p).replace(QLatin1Char('/'), QLatin1Char('_')).replace(QLatin1Char(':'), QLatin1Char('_'));
    savedName = netSettings.readEntry("name_" + savedKey,
        scheme == QStringLiteral("gdrive") ? QStringLiteral("Google Drive") : pUrl.fileName());

    iconName = netSettings.readEntry("icon_" + savedKey, QString());
    if (iconName.isEmpty())
    {
        iconName = scheme == QStringLiteral("gdrive")      ? QStringLiteral("folder-gdrive")
                 : scheme == QStringLiteral("smb")        ? QStringLiteral("folder-remote-smb")
                 : scheme == QStringLiteral("sftp")       ? QStringLiteral("network-connect")
                 : scheme == QStringLiteral("mtp")        ? QStringLiteral("multimedia-player")
                 : scheme == QStringLiteral("bluetooth")  ? QStringLiteral("bluetooth")
                                                          : QStringLiteral("network-server");
        netSettings.writeEntry("icon_" + savedKey, iconName);
        netSettings.config()->sync();
    }
}

void DriveManager::setupNetworkPlaceFreeSpace(const QUrl &pUrl, const QString &url, const QString &savedKey, DriveInfo &info, KConfigGroup &netSettings)
{
    info.total = netSettings.readEntry("total_" + savedKey, 0.0);
    info.free  = netSettings.readEntry("free_" + savedKey, 0.0);

    if (m_netFreeCache.contains(url))
    {
        const auto &fs = m_netFreeCache.value(url);
        info.total = fs.first;
        info.free = fs.second;
    }
    else
    {
        auto *freeJob = KIO::fileSystemFreeSpace(pUrl);
        Q_ASSERT(freeJob != nullptr);
        freeJob->setAutoDelete(true);
        const QString itemUrl = url;
        connect(freeJob, &KIO::FileSystemFreeSpaceJob::result, this,
                [this, itemUrl, savedKey, freeJob](KJob *)
                {
                    if (freeJob->error() != 0) { return; }
                    const double total = freeJob->size()          / 1073741824.0;
                    const double free  = freeJob->availableSize() / 1073741824.0;
                    if (total <= 0.0) { return; }

                    auto s = Config::group(QStringLiteral("NetworkPlaces"));
                    s.writeEntry("total_" + savedKey, total);
                    s.writeEntry("free_" + savedKey, free);

                    m_netFreeCache.insert(itemUrl, {total, free});
                    bool updated = false;
                    for (int i = 0; i < m_networkDrives.size(); ++i)
                    {
                        if (m_networkDrives[i].path == itemUrl)
                        {
                            m_networkDrives[i].total = total;
                            m_networkDrives[i].free = free;
                            updated = true;
                            break;
                        }
                    }
                    if (updated)
                    {
                        emit drivesUpdated();
                    }
                });
    }
}

void DriveManager::processMountedNetworkVolumes(QSet<QString> &shownPaths)
{
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes())
    {
        if (!storage.isValid() || !storage.isReady())
        {
            continue;
        }
        const QString fs = storage.fileSystemType();
        if (fs != QStringLiteral("cifs") && fs != QStringLiteral("smb3") && 
            fs != QStringLiteral("nfs") && fs != QStringLiteral("nfs4") &&
            fs != QStringLiteral("sshfs") && fs != QStringLiteral("fuse.sshfs") && 
            fs != QStringLiteral("davfs") && fs != QStringLiteral("fuse.davfs2") && 
            !fs.startsWith(QStringLiteral("fuse.")))
        {
            continue;
        }
        if (fs == QStringLiteral("fuse.portal") || fs == QStringLiteral("fusectl"))
        {
            continue;
        }

        const QString path = storage.rootPath();
        const QString normalizedPath = mw_normalizePath(path);
        if (shownPaths.contains(normalizedPath))
        {
            continue;
        }
        shownPaths.insert(normalizedPath);

        QString name = storage.name().isEmpty() ? QUrl::fromLocalFile(path).fileName() : storage.name();
        if (name.isEmpty())
        {
            name = path;
        }

        QString icon = (fs == QStringLiteral("cifs") || fs == QStringLiteral("smb3")) ? QStringLiteral("network-workgroup")
                     : (fs == QStringLiteral("sshfs") || fs == QStringLiteral("fuse.sshfs")) ? QStringLiteral("network-connect")
                     : QStringLiteral("network-server");

        DriveInfo info;
        info.isNetwork = true;
        info.isMounted = true;
        info.path = path;
        info.name = name;
        info.iconName = icon;
        info.subtitle = fs + QStringLiteral(" – ") + path;
        info.scheme = fs;
        info.total = storage.bytesTotal() / 1073741824.0;
        info.free = storage.bytesFree() / 1073741824.0;
        m_networkDrives.append(info);
    }
}
