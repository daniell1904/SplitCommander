#pragma once
#include <QString>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QMenu>

// --- Größenkonstanten ---
static constexpr int SC_MILLER_DRIVE_ROW_H  = 52;  // Laufwerk-Items im Miller
static constexpr int SC_SIDEBAR_DRIVE_ROW_H = 44;  // Laufwerk-Items in der Sidebar
static constexpr int SC_SIDEBAR_NET_ROW_H   = 44;  // Netzwerk-Items in der Sidebar
static constexpr int SC_MILLER_HEADER_H     = 40;  // Miller-Spalten-Header
static constexpr int SC_TOOLBAR_H           = 96;  // PaneToolbar Höhe
static constexpr int SC_DRIVE_REFRESH_MS    = 5000; // Drive-Timer Intervall

inline QString mw_normalizePath(QString s) {
    if (s.length() > 1 && s.endsWith('/'))
        s.chop(1);
    return s;
}

inline QString sc_rootVolumeName() {
    const QString name = QStorageInfo(QStringLiteral("/")).name();
    return name.isEmpty() ? QCoreApplication::translate("SplitCommander", "System") : name;
}

inline QString mw_fmtSize(qint64 sz) {
    if (sz < 1024)
        return QString("%1 B").arg(sz);
    if (sz < 1024 * 1024)
        return QString("%1 KB").arg(sz / 1024);
    if (sz < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(sz / (1024 * 1024));
    return QString("%1 GB").arg(sz / (1024LL * 1024 * 1024));
}

inline void mw_applyMenuShadow(QMenu *menu) {
    Q_UNUSED(menu)
    // QGraphicsDropShadowEffect auf QMenu zerstört Submenü-Positionierung
}
