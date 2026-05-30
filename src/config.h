#pragma once

#include <QString>
#include <QStringList>
#include <QColor>
#include <QList>
#include <KSharedConfig>
#include <KConfigGroup>

class Config
{
public:
    [[nodiscard]] static bool useSystemTheme();
    [[nodiscard]] static QString selectedTheme();
    [[nodiscard]] static QColor ageBadgeColor(int index);
    [[nodiscard]] static QString dateFormat();
    [[nodiscard]] static bool singleClickOpen();
    [[nodiscard]] static bool showHiddenFiles();
    [[nodiscard]] static bool showFileExtensions();
    [[nodiscard]] static int startupBehavior();
    [[nodiscard]] static QString startupPath();
    [[nodiscard]] static bool showNewIndicator();
    [[nodiscard]] static int ageBadgeSaturation();
    [[nodiscard]] static int ageBadgeLightness();

    [[nodiscard]] static QString lastLeftPath();
    [[nodiscard]] static QString lastRightPath();

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
    static void setLastPaths(const QString &left, const QString &right);

    [[nodiscard]] static bool useThumbnails();
    [[nodiscard]] static int maxThumbnailSize();
    static void setUseThumbnails(bool b);
    static void setMaxThumbnailSize(int i);

    [[nodiscard]] static QStringList fileTypeColors();
    static void setFileTypeColors(const QStringList &list);

    [[nodiscard]] static bool showDriveIp();
    [[nodiscard]] static bool showMillerIp();
    [[nodiscard]] static QStringList driveBlacklist();
    
    [[nodiscard]] static int sidebarIconSize();
    [[nodiscard]] static int driveIconSize();
    [[nodiscard]] static int millerIconSize();
    [[nodiscard]] static int listIconSize();

    [[nodiscard]] static int sidebarRowHeight();
    [[nodiscard]] static int sidebarDriveRowHeight();
    [[nodiscard]] static int sidebarNetRowHeight();
    [[nodiscard]] static int millerDriveRowHeight();
    [[nodiscard]] static int millerHeaderHeight();
    
    [[nodiscard]] static int uiFontSize();
    [[nodiscard]] static QString uiFontFamily();
    [[nodiscard]] static QString appLanguage();
    [[nodiscard]] static int uiSpacing();
    
    static void setUiFontSize(int i);
    static void setUiSpacing(int i);
    static void setUiFontFamily(const QString &f);
    static void setAppLanguage(const QString &lang);

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

    [[nodiscard]] static QString gitLocalDir();
    [[nodiscard]] static QString gitRemoteUrl();
    [[nodiscard]] static QString gitUsername();

    struct GitRepo
    {
        QString name;
        QString localDir;
        QString remoteUrl;
        QString username;
    };
    
    [[nodiscard]] static QList<GitRepo> gitRepos();
    static void setGitRepos(const QList<GitRepo> &repos);

    [[nodiscard]] static bool gitShowSidebar();
    [[nodiscard]] static QString gitRefreshMode();
    [[nodiscard]] static int gitRefreshIntervalMinutes();
    static void setGitShowSidebar(bool b);
    static void setGitRefreshMode(const QString &m);
    static void setGitRefreshIntervalMinutes(int i);
    [[nodiscard]] static QString gitToken();

    static void setGitLocalDir(const QString &s);
    static void setGitRemoteUrl(const QString &s);
    static void setGitUsername(const QString &s);
    static void setGitToken(const QString &s);

    [[nodiscard]] static QString paperlessUrl();
    [[nodiscard]] static QString paperlessToken();
    [[nodiscard]] static bool paperlessSslIgnore();
    static void setPaperlessUrl(const QString &s);
    static void setPaperlessToken(const QString &s);
    static void setPaperlessSslIgnore(bool b);

    [[nodiscard]] static KConfigGroup group(const QString &name);
};
