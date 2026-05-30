#include "mountiso.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QEventLoop>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QTimer>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/GenericInterface>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <fcntl.h>
#include <unistd.h>

namespace MountIso
{

static const QStringList s_mountableMimes = {
    QStringLiteral("application/vnd.efi.iso"),
    QStringLiteral("application/vnd.efi.img"),
    QStringLiteral("application/x-cd-image"),
    QStringLiteral("application/x-raw-disk-image"),
    QStringLiteral("application/octet-stream")
};

bool isMountable(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("iso") || suffix == QStringLiteral("img"))
    {
        return true;
    }
    const QString mime = QMimeDatabase().mimeTypeForFile(filePath).name();
    return s_mountableMimes.contains(mime);
}

[[nodiscard]] static Solid::Device deviceFromBackingFile(const QString &backingFile)
{
    const auto devices = Solid::Device::listFromQuery(QStringLiteral("[ IS StorageVolume AND IS GenericInterface ]"));
    for (const Solid::Device &d : devices)
    {
        auto *gi = d.as<Solid::GenericInterface>();
        if (gi && gi->property(QStringLiteral("BackingFile")).toString() == backingFile)
        {
            return d;
        }
    }
    return Solid::Device();
}

bool isMounted(const QString &filePath)
{
    return deviceFromBackingFile(filePath).isValid();
}

void mount(const QString &filePath, QWidget *parent)
{
    const int fd = open(filePath.toLocal8Bit().data(), O_RDONLY);
    if (fd == -1)
    {
        QMessageBox::warning(parent, QObject::tr("ISO einbinden"), QObject::tr("Konnte Datei nicht öffnen: %1").arg(filePath));
        return;
    }
    auto qtFd = QDBusUnixFileDescriptor(fd);
    close(fd);

    QMap<QString, QVariant> opts;
    QDBusInterface mgr(QStringLiteral("org.freedesktop.UDisks2"),
                       QStringLiteral("/org/freedesktop/UDisks2/Manager"),
                       QStringLiteral("org.freedesktop.UDisks2.Manager"),
                       QDBusConnection::systemBus());
    QDBusReply<QDBusObjectPath> reply = mgr.call(QStringLiteral("LoopSetup"), QVariant::fromValue(qtFd), opts);
    if (!reply.isValid())
    {
        QMessageBox::warning(parent, QObject::tr("ISO einbinden"), QObject::tr("Fehler: %1").arg(reply.error().message()));
        return;
    }

    // Warten bis Solid die neue Device erkennt
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto *notifier = Solid::DeviceNotifier::instance();
    Q_ASSERT(notifier != nullptr);
    QObject::connect(notifier, &Solid::DeviceNotifier::deviceAdded, &loop, &QEventLoop::quit);

    Solid::Device device;
    for (int i = 0; i < 4; ++i)
    {
        timer.start(5000);
        loop.exec();
        device = Solid::Device(reply.value().path());
        if (device.is<Solid::StorageVolume>())
        {
            break;
        }
    }
    if (!device.is<Solid::StorageVolume>())
    {
        return;
    }

    const QString uuid = device.as<Solid::StorageVolume>()->uuid();
    const auto accessDevs = Solid::Device::listFromQuery(QStringLiteral("[ StorageVolume.uuid == '%1' AND IS StorageAccess ]").arg(uuid));
    for (auto d : accessDevs)
    {
        auto *sa = d.as<Solid::StorageAccess>();
        if (sa != nullptr)
        {
            sa->setup();
        }
    }
}

void unmount(const QString &filePath, QWidget *parent)
{
    Solid::Device device = deviceFromBackingFile(filePath);
    if (!device.isValid())
    {
        QMessageBox::warning(parent, QObject::tr("ISO aushängen"), QObject::tr("Gerät nicht gefunden."));
        return;
    }

    // Erst StorageAccess teardown
    auto *gi = device.as<Solid::GenericInterface>();
    const QString uuid = gi ? gi->property(QStringLiteral("IdUUID")).toString().toLower() : QString();
    if (!uuid.isEmpty())
    {
        const auto devs = Solid::Device::listFromQuery(QStringLiteral("[ StorageVolume.uuid == '%1' AND IS StorageAccess ]").arg(uuid));
        for (auto d : devs)
        {
            auto *sa = d.as<Solid::StorageAccess>();
            if (sa != nullptr && sa->isAccessible())
            {
                sa->teardown();
            }
        }
    }

    // Dann Loop löschen
    QMap<QString, QVariant> opts;
    QDBusInterface loop(QStringLiteral("org.freedesktop.UDisks2"),
                        device.udi(),
                        QStringLiteral("org.freedesktop.UDisks2.Loop"),
                        QDBusConnection::systemBus());
    loop.call(QStringLiteral("Delete"), opts);
}

} // namespace MountIso
