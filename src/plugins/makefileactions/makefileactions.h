#pragma once

#include <QStringList>
#include <QString>
#include <QWidget>

namespace MakefileActions
{
    // Gibt true zurück wenn im Verzeichnis ein Makefile liegt
    [[nodiscard]] bool hasMakefile(const QString &dirPath);

    // Gibt alle verfügbaren Targets zurück
    [[nodiscard]] QStringList listTargets(const QString &dirPath);

    // Führt ein Make-Target aus (öffnet Terminal)
    void runTarget(const QString &dirPath, const QString &target, QWidget *parent);
}
