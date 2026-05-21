#pragma once
#include <QString>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QVariantList>
#include <QVariantMap>

// --- Größenkonstanten ---
static constexpr int SC_TOOLBAR_H           = 96;  // PaneToolbar Höhe

inline QString mw_normalizePath(QString s) {
    if (s.length() > 1 && s.endsWith('/'))
        s.chop(1);
    return s;
}

inline QString sc_rootVolumeName() {
    const QString name = QStorageInfo(QStringLiteral("/")).name();
    return name.isEmpty() ? QCoreApplication::translate("SplitCommander", "System") : name;
}

inline QString sc_fmtStorage(double gb) {
    if (gb >= 1000.0)
        return QString("%1 TB").arg(gb / 1024.0, 0, 'f', 1);
    return QString("%1 GB").arg((int)gb);
}

// Globale Systembenachrichtigung über D-Bus senden (keine externen Abhängigkeiten!)
inline void sc_notify(const QString &title, const QString &text, const QString &icon = QStringLiteral("splitcommander")) {
    QDBusInterface interface(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QDBusConnection::sessionBus()
    );

    if (interface.isValid()) {
        QVariantList args;
        args << QStringLiteral("SplitCommander") // App-Name
             << quint32(0)                     // Replaces-ID
             << icon                           // Icon-Name
             << title                          // Summary
             << text                           // Body
             << QStringList()                  // Actions
             << QVariantMap()                  // Hints
             << qint32(5000);                  // Timeout (ms)

        interface.callWithArgumentList(QDBus::NoBlock, QStringLiteral("Notify"), args);
    }
}



