#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QHash>
#include <QPair>
#include <QSet>

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
    void processStorageVolumeDevices(const QSet<QString> &shownUdis);
    void processSavedNetworkPlaces(QSet<QString> &shownPaths);
    void processMountedNetworkVolumes(QSet<QString> &shownPaths);

    QList<DriveInfo> m_localDrives;
    QList<DriveInfo> m_networkDrives;
    QHash<QString, QPair<double, double>> m_netFreeCache;
};
