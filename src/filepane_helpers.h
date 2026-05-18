// --- filepane_helpers.h -----------------------------------------------------
// Geteilte statische Helper für filepane.cpp / filepane_menus.cpp / filepaneproxy.cpp.
// ---------------------------------------------------------------------------
#pragma once

#include "thememanager.h"
#include <QFile>
#include <QFileDevice>
#include <QMenu>
#include <QString>

inline QString fp_menuStyle() {
  return TM().ssMenu() + "QMenu::separator{background:rgba(236,239,244,120);"
                         "height:1px;margin:4px 8px;}";
}

inline void fp_applyMenuShadow(QMenu *menu) {
  Q_UNUSED(menu)
  // QGraphicsDropShadowEffect auf QMenu zerstört Submenü-Positionierung
}

inline QString fp_fmtRwx(QFileDevice::Permissions p) {
  QString s;
  s += (p & QFile::ReadOwner)  ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteOwner) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeOwner)   ? QLatin1String("x") : QLatin1String("-");
  s += (p & QFile::ReadGroup)  ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteGroup) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeGroup)   ? QLatin1String("x") : QLatin1String("-");
  s += (p & QFile::ReadOther)  ? QLatin1String("r") : QLatin1String("-");
  s += (p & QFile::WriteOther) ? QLatin1String("w") : QLatin1String("-");
  s += (p & QFile::ExeOther)   ? QLatin1String("x") : QLatin1String("-");
  return s;
}
