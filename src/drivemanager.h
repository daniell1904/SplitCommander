#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QHash>
#include <QPair>

struct DriveInfo {
    bool isNetwork;
    bool isMounted;
    QString path;       // Pfad, URL oder "solid:udi"
    QString name;       // Anzeigename
    QString iconName;   // Icon
    QString udi;        // Solid UDI (falls zutreffend)
    QString subtitle;   // Zusatzinfo (z.B. Dateisystem oder URL-Scheme)
    QString scheme;     // Netzwerk-Scheme (gdrive, smb, ftp etc)
    double total;       // Gesamt in GB
    double free;        // Frei in GB
};

class DriveManager : public QObject {
    Q_OBJECT
public:
    static DriveManager* instance();

    QList<DriveInfo> localDrives() const { return m_localDrives; }
    QList<DriveInfo> networkDrives() const { return m_networkDrives; }

public slots:
    void refreshAll();
    void refreshLocal();
    void refreshNetwork();

signals:
    void drivesUpdated();

private:
    explicit DriveManager(QObject *parent = nullptr);
    ~DriveManager() override = default;

    QList<DriveInfo> m_localDrives;
    QList<DriveInfo> m_networkDrives;
    QHash<QString, QPair<double,double>> m_netFreeCache;
};
