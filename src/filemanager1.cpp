#include "filemanager1.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>

FileManager1::FileManager1(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/org/freedesktop/FileManager1"),
        this,
        QDBusConnection::ExportScriptableContents | QDBusConnection::ExportAdaptors);

    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (iface) {
        iface->registerService(
            QStringLiteral("org.freedesktop.FileManager1"),
            QDBusConnectionInterface::QueueService);
    }
}

void FileManager1::ShowFolders(const QStringList &uriList, const QString & /*startupId*/) {
    if (!uriList.isEmpty())
        emit showFoldersRequested(uriList);
}

void FileManager1::ShowItems(const QStringList &uriList, const QString & /*startupId*/) {
    if (!uriList.isEmpty())
        emit showItemsRequested(uriList);
}

void FileManager1::ShowItemProperties(const QStringList & /*uriList*/,
                                       const QString & /*startupId*/) {}

#include "moc_filemanager1.cpp"
