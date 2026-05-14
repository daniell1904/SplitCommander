#pragma once

#include <QString>
#include <QColor>
#include <KSharedConfig>
#include <KConfigGroup>

class Config {
public:
    static bool    useSystemTheme();
    static QString selectedTheme();
    static QColor  ageBadgeColor(int index);
    static QString dateFormat();
    static bool    singleClickOpen();
    static bool    confirmDelete();
    static bool    showHiddenFiles();
    static bool    showFileExtensions();
    static int     startupBehavior();
    static QString startupPath();
    static bool    showNewIndicator();
    static int     ageBadgeSaturation();
    static int     ageBadgeLightness();

    static QString terminalApp();
    static QString lastLeftPath();
    static QString lastRightPath();


    static void setUseSystemTheme(bool b);

    static void setSelectedTheme(const QString &t);
    static void setSingleClickOpen(bool b);
    static void setShowHiddenFiles(bool b);
    static void setShowFileExtensions(bool b);
    static void setStartupBehavior(int i);
    static void setShowNewIndicator(bool b);
    static void setAgeBadgeSaturation(int i);
    static void setAgeBadgeLightness(int i);
    static void setTerminalApp(const QString &t);
    static void setLastPaths(const QString &left, const QString &right);

    static KConfigGroup group(const QString &name);

};

