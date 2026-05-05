#pragma once
#include <QString>
#include <QStringList>

// Gibt den konfigurierten Terminal-Binary-Namen zurück.
// Reihenfolge: SplitCommander-Einstellung → kdeglobals → gsettings → Fallback-Liste
QString sc_detectTerminal();

// Öffnet ein neues Terminal-Fenster im angegebenen Pfad.
void sc_openTerminal(const QString &path);

// Gibt die Liste aller installierten bekannten Terminals zurück.
QStringList sc_installedTerminals();
