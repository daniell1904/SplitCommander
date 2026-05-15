#pragma once
#include <QObject>
#include <QStringList>

// Implementiert org.freedesktop.FileManager1 — wie Dolphin dbusinterface.h
// Ermöglicht "In Ordner anzeigen" aus Browsern
class FileManager1 : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")
public:
    explicit FileManager1(QObject *parent = nullptr);

    Q_SCRIPTABLE void ShowFolders(const QStringList &uriList, const QString &startupId);
    Q_SCRIPTABLE void ShowItems(const QStringList &uriList, const QString &startupId);
    Q_SCRIPTABLE void ShowItemProperties(const QStringList &uriList, const QString &startupId);

signals:
    void showFoldersRequested(const QStringList &uriList);
    void showItemsRequested(const QStringList &uriList);
};
