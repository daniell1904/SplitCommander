#include "filemanager1.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>

FileManager1::FileManager1(QObject *parent)
    : QObject(parent)
{
    auto sessionBus = QDBusConnection::sessionBus();
    Q_ASSERT(sessionBus.isConnected());

    const bool regObj = sessionBus.registerObject(
        QStringLiteral("/org/freedesktop/FileManager1"),
        this,
        QDBusConnection::ExportScriptableContents | QDBusConnection::ExportAdaptors);
    Q_ASSERT(regObj);

    QDBusConnectionInterface *iface = sessionBus.interface();
    if (iface != nullptr)
    {
        iface->registerService(
            QStringLiteral("org.freedesktop.FileManager1"),
            QDBusConnectionInterface::QueueService);
    }
}

void FileManager1::ShowFolders(const QStringList &uriList, const QString &startupId)
{
    Q_UNUSED(startupId)
    Q_ASSERT(!uriList.isEmpty());
    Q_ASSERT(uriList.first().contains(QStringLiteral("://")));

    if (!uriList.isEmpty())
    {
        emit showFoldersRequested(uriList);
    }
}

void FileManager1::ShowItems(const QStringList &uriList, const QString &startupId)
{
    Q_UNUSED(startupId)
    Q_ASSERT(!uriList.isEmpty());
    Q_ASSERT(uriList.first().contains(QStringLiteral("://")));

    if (!uriList.isEmpty())
    {
        emit showItemsRequested(uriList);
    }
}

void FileManager1::ShowItemProperties(const QStringList &uriList, const QString &startupId)
{
    Q_UNUSED(uriList)
    Q_UNUSED(startupId)
    Q_ASSERT(startupId.isEmpty() || !startupId.isEmpty());
    Q_ASSERT(uriList.isEmpty() || !uriList.isEmpty());
}

#include "moc_filemanager1.cpp"
