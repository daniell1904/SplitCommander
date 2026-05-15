#pragma once
#include <QString>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QMenu>

// --- Größenkonstanten ---
static constexpr int SC_MILLER_DRIVE_ROW_H  = 50;
static constexpr int SC_SIDEBAR_ROW_H       = 34;  // Standard Sidebar-Zeilenhöhe
static constexpr int SC_SIDEBAR_DRIVE_ROW_H = 50;
static constexpr int SC_SIDEBAR_NET_ROW_H   = 50;  // Netzwerk-Items in der Sidebar
static constexpr int SC_MILLER_HEADER_H     = 38;  // Miller-Spalten-Header
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

inline QString sc_fmtStorage(double gb) {
    if (gb >= 1000.0)
        return QString("%1 TB").arg(gb / 1024.0, 0, 'f', 1);
    return QString("%1 GB").arg((int)gb);
}


inline void mw_applyMenuShadow(QMenu *menu) {
    Q_UNUSED(menu)
    // QGraphicsDropShadowEffect auf QMenu zerstört Submenü-Positionierung
}
