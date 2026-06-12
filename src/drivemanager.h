#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QHash>
#include <QPair>
#include <QSet>
#include <Solid/Device>
#include <Solid/StorageVolume>

struct DriveInfo
{
    bool isNetwork = false;
    bool isMounted = false;
    QString path;
    QString name;
    QString iconName;
    QString udi;
    QString subtitle;
    QString scheme;
    double total = 0.0;
    double free = 0.0;
};

class DriveManager : public QObject
{
    Q_OBJECT
public:
    [[nodiscard]] static DriveManager* instance();

    [[nodiscard]] QList<DriveInfo> localDrives() const;
    [[nodiscard]] QList<DriveInfo> networkDrives() const;

public slots:
    void refreshAll();
    void refreshLocal();
    void refreshNetwork();

signals:
    void drivesUpdated();

private:
    explicit DriveManager(QObject *parent = nullptr);
    ~DriveManager() override = default;

    void processStorageAccessDevices(QSet<QString> &shownPaths, QSet<QString> &shownUdis);
    void processStorageAccessDeviceItem(const Solid::Device &device, QSet<QString> &shownPaths, QSet<QString> &shownUdis);
    bool isStorageAccessDeviceBlacklisted(const Solid::Device &device, bool mounted, const QString &path);
    QString determineStorageDeviceName(const Solid::Device &device, bool mounted, const QString &path, const Solid::StorageVolume *vol);
    QString determineStorageDeviceIcon(const Solid::Device &device, bool mounted, const QString &path);
    void processStorageVolumeDevices(const QSet<QString> &shownUdis);
    void processSavedNetworkPlaces(QSet<QString> &shownPaths);
    void processSavedNetworkPlaceItem(const QString &p, QSet<QString> &shownPaths, class KConfigGroup &netSettings);
    void determineNetworkPlaceNameAndIcon(const QString &p, const QUrl &pUrl, const QString &scheme, class KConfigGroup &netSettings, QString &savedName, QString &iconName);
    void setupNetworkPlaceFreeSpace(const QUrl &pUrl, const QString &url, const QString &savedKey, DriveInfo &info, class KConfigGroup &netSettings);
    void processMountedNetworkVolumes(QSet<QString> &shownPaths);

    QList<DriveInfo> m_localDrives;
    QList<DriveInfo> m_networkDrives;
    QHash<QString, QPair<double, double>> m_netFreeCache;
};
