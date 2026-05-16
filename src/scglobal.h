#pragma once
#include <QString>
#include <QStorageInfo>
#include <QCoreApplication>

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


