#include "terminalutils.h"
#include "config.h"
#include <QProcess>


#include <QStandardPaths>

#include <QFileInfo>

static const QStringList knownTerminals()
{
    return { "konsole", "ptyxis", "gnome-terminal", "kitty", "alacritty", "xterm", "tilix", "wezterm", "foot" };
}

QStringList sc_installedTerminals()
{
    QStringList result;
    for (const QString &t : knownTerminals())
        if (!QStandardPaths::findExecutable(t).isEmpty())
            result << t;
    return result;
}

// Sucht Terminal-Binary – zuerst QStandardPaths, dann bekannte Präfixe direkt
static QString sc_findBinary(const QString &name)
{
    // 1. Qt-Standardsuche (nutzt PATH des Prozesses)
    const QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty())
        return found;

    // 2. Direkt in bekannten Verzeichnissen suchen
    for (const char *dir : {"/usr/bin", "/usr/local/bin", "/bin"}) {
        const QString full = QLatin1String(dir) + QStringLiteral("/") + name;
        if (QFileInfo::exists(full))
            return full;
    }
    return {};
}

static QString sc_findInstalledTerminal()
{
    for (const QString &t : knownTerminals()) {
        const QString full = sc_findBinary(t);
        if (!full.isEmpty())
            return full;
    }
    return {};
}

QString sc_detectTerminal()
{
    // 1. SplitCommander-eigene Einstellung
    const QString userChoice = Config::terminalApp().trimmed();
    if (!userChoice.isEmpty())
        return userChoice;


    // 2. KDE-Systemeinstellung – nur wenn Binary existiert
    QProcess p;
    p.start("kreadconfig6", {"--group", "General", "--key", "TerminalApplication"});
    p.waitForFinished(500);
    const QString kdeterm = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (!kdeterm.isEmpty()) {
        const QString full = kdeterm.startsWith("/") ? kdeterm : sc_findBinary(kdeterm);
        if (!full.isEmpty() && QFileInfo::exists(full))
            return full;
    }

    // 3. Fallback: erstes installiertes bekanntes Terminal
    return sc_findInstalledTerminal();
}

void sc_openTerminal(const QString &path)
{
    QString term = sc_detectTerminal();
    if (term.isEmpty()) {
        qWarning() << "sc_openTerminal: kein Terminal gefunden";
        return;
    }

    if (!term.startsWith("/")) {
        const QString full = sc_findBinary(term);
        if (!full.isEmpty())
            term = full;
    }

    const QString bin = term.section('/', -1);
    bool ok = false;

    if (bin.contains("konsole"))
        ok = QProcess::startDetached(term, {"--nofork", "--separate", "--workdir", path});
    else if (bin.contains("ptyxis") || bin.contains("gnome-terminal") || bin.contains("tilix"))
        ok = QProcess::startDetached(term, {"--new-window", "--working-directory", path});
    else if (bin.contains("kitty") || bin.contains("alacritty") || bin.contains("wezterm"))
        ok = QProcess::startDetached(term, {"--directory", path});
    else if (bin.contains("foot"))
        ok = QProcess::startDetached(term, {"--working-directory", path});
    else
        ok = QProcess::startDetached(term, {path});

    if (!ok)
        qWarning() << "sc_openTerminal: startDetached fehlgeschlagen für" << term;
}
