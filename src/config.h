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
    static void setStartupPath(const QString &p);
    static void setShowNewIndicator(bool b);
    static void setAgeBadgeSaturation(int i);
    static void setAgeBadgeLightness(int i);
    static void setTerminalApp(const QString &t);
    static void setLastPaths(const QString &left, const QString &right);

    static bool useThumbnails();
    static int  maxThumbnailSize();
    static void setUseThumbnails(bool b);
    static void setMaxThumbnailSize(int i);

    static QStringList fileTypeColors();
    static void setFileTypeColors(const QStringList &list);

    // Erweiterte Einstellungen
    static bool showDriveIp();
    static bool showMillerIp();
    static QStringList driveBlacklist();
    static int sidebarIconSize();
    static int driveIconSize();
    static int millerIconSize();
    static int listIconSize();

    // Zeilenhöhen
    static int sidebarRowHeight();
    static int sidebarDriveRowHeight();
    static int sidebarNetRowHeight();
    static int millerDriveRowHeight();
    static int millerHeaderHeight();

    // Drive-Refresh
    static int driveRefreshMs();

    static void setShowDriveIp(bool b);
    static void setShowMillerIp(bool b);
    static void setDriveBlacklist(const QStringList &list);
    static void setSidebarIconSize(int i);
    static void setDriveIconSize(int i);
    static void setMillerIconSize(int i);
    static void setListIconSize(int i);

    static void setSidebarRowHeight(int i);
    static void setSidebarDriveRowHeight(int i);
    static void setSidebarNetRowHeight(int i);
    static void setMillerDriveRowHeight(int i);
    static void setMillerHeaderHeight(int i);
    static void setDriveRefreshMs(int i);

    static QString gitLocalDir();
    static QString gitRemoteUrl();
    static QString gitUsername();
    static QString gitToken();
    
    static void setGitLocalDir(const QString &s);
    static void setGitRemoteUrl(const QString &s);
    static void setGitUsername(const QString &s);
    static void setGitToken(const QString &s);

    static KConfigGroup group(const QString &name);

};

