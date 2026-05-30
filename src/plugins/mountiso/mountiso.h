#pragma once

#include <QString>
#include <QWidget>

namespace MountIso
{
    // Gibt true zurück wenn die Datei eine ISO/IMG ist
    [[nodiscard]] bool isMountable(const QString &filePath);

    // Gibt true zurück wenn die ISO bereits gemountet ist
    [[nodiscard]] bool isMounted(const QString &filePath);

    // ISO als Loop-Device einbinden
    void mount(const QString &filePath, QWidget *parent);

    // Loop-Device aushängen
    void unmount(const QString &filePath, QWidget *parent);
}
