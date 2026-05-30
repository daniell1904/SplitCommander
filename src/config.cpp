#include "config.h"
#include <KConfigGroup>
#include <QDir>
#include <QFileInfo>

static KConfigGroup generalGroup()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto group = config->group(QStringLiteral("General"));
    Q_ASSERT(group.isValid());
    return group;
}

static KConfigGroup appearanceGroup()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto group = config->group(QStringLiteral("Appearance"));
    Q_ASSERT(group.isValid());
    return group;
}

static KConfigGroup ageBadgeGroup()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto group = config->group(QStringLiteral("AgeBadge"));
    Q_ASSERT(group.isValid());
    return group;
}

static KConfigGroup adminGroup()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto group = config->group(QStringLiteral("Admin"));
    Q_ASSERT(group.isValid());
    return group;
}

bool Config::useSystemTheme()
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("useSystemTheme", false);
}

QString Config::selectedTheme()
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("theme", QStringLiteral("Nord"));
}

QColor Config::ageBadgeColor(int index)
{
    Q_ASSERT(index >= 0);
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);

    const int hues[6] = {0, 30, 80, 160, 220, 270};
    const int sat = g.readEntry("saturation", 220);
    const int lit = g.readEntry("lightness", 140);
    const int sMapped = 40 + (sat * (255 - 40) / 255);
    const int lMapped = 60 + (lit * (220 - 60) / 255);
    const int idx = qBound(0, index, 5);
    const int s_final = (idx == 5) ? sMapped / 2 : sMapped;
    return QColor::fromHsl(hues[idx], s_final, lMapped);
}

QString Config::dateFormat()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("dateFormat", QStringLiteral("yyyy-MM-dd HH:mm"));
}

bool Config::singleClickOpen()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("singleClick", false);
}

bool Config::showHiddenFiles()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("showHidden", false);
}

bool Config::showFileExtensions()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("showExtensions", true);
}

int Config::startupBehavior()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("startupBehavior", 1);
}

QString Config::startupPath()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("startupPath", QDir::homePath());
}

bool Config::showNewIndicator()
{
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("showNewIndicator", true);
}

int Config::ageBadgeSaturation()
{
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("saturation", 220);
}

int Config::ageBadgeLightness()
{
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("lightness", 140);
}

QString Config::lastLeftPath()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("lastLeftPath", QDir::homePath());
}

QString Config::lastRightPath()
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("lastRightPath", QDir::homePath());
}

void Config::setUseSystemTheme(bool b)
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("useSystemTheme", b);
    g.config()->sync();
}

void Config::setSelectedTheme(const QString &t)
{
    Q_ASSERT(!t.isEmpty());
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("theme", t);
    g.config()->sync();
}

void Config::setSingleClickOpen(bool b)
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("singleClick", b);
    g.config()->sync();
}

void Config::setShowHiddenFiles(bool b)
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("showHidden", b);
    g.config()->sync();
}

void Config::setShowFileExtensions(bool b)
{
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("showExtensions", b);
    g.config()->sync();
}

void Config::setStartupBehavior(int i)
{
    Q_ASSERT(i >= 0);
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("startupBehavior", i);
    g.config()->sync();
}

void Config::setStartupPath(const QString &p)
{
    Q_ASSERT(!p.isEmpty());
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("startupPath", p);
    g.config()->sync();
}

void Config::setShowNewIndicator(bool b)
{
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("showNewIndicator", b);
    g.config()->sync();
}

void Config::setAgeBadgeSaturation(int i)
{
    Q_ASSERT(i >= 0);
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("saturation", i);
    g.config()->sync();
}

void Config::setAgeBadgeLightness(int i)
{
    Q_ASSERT(i >= 0);
    auto g = ageBadgeGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("lightness", i);
    g.config()->sync();
}

void Config::setLastPaths(const QString &left, const QString &right)
{
    Q_ASSERT(!left.isEmpty());
    Q_ASSERT(!right.isEmpty());
    auto g = generalGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("lastLeftPath", left);
    g.writeEntry("lastRightPath", right);
    g.config()->sync();
}

bool Config::useThumbnails()
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("useThumbnails", true);
}

int Config::maxThumbnailSize()
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("maxThumbnailSize", 50);
}

void Config::setUseThumbnails(bool b)
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("useThumbnails", b);
    g.config()->sync();
}

void Config::setMaxThumbnailSize(int i)
{
    Q_ASSERT(i >= 0);
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("maxThumbnailSize", i);
    g.config()->sync();
}

QStringList Config::fileTypeColors()
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);

    QStringList defaults;
    defaults << QStringLiteral(".js:#f7df1e") << QStringLiteral(".cpp:#00599c") << QStringLiteral(".h:#00599c") 
             << QStringLiteral(".pdf:#ff0000") << QStringLiteral(".zip:#ffa500") << QStringLiteral(".tar:#ffa500") << QStringLiteral(".gz:#ffa500")
             << QStringLiteral(".md:#42b883") << QStringLiteral(".txt:#42b883")
             << QStringLiteral(".png:#bd93f9") << QStringLiteral(".jpg:#bd93f9") << QStringLiteral(".svg:#bd93f9")
             << QStringLiteral(".mp4:#ff79c6") << QStringLiteral(".mp3:#ff79c6");

    return g.readEntry("fileTypeColors", defaults);
}

void Config::setFileTypeColors(const QStringList &list)
{
    auto g = appearanceGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("fileTypeColors", list);
    g.config()->sync();
}

KConfigGroup Config::group(const QString &name)
{
    Q_ASSERT(!name.isEmpty());
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto grp = config->group(name);
    Q_ASSERT(grp.isValid());
    return grp;
}

bool Config::showDriveIp()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("showDriveIp", true);
}

bool Config::showMillerIp()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("showMillerIp", false);
}

QStringList Config::driveBlacklist()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("driveBlacklist", QStringList{QStringLiteral("/var/lib/docker"), QStringLiteral("/var/lib/containers")});
}

int Config::sidebarIconSize()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("sidebarIconSize", 22);
}

int Config::driveIconSize()
{
    return uiFontSize();
}

int Config::millerIconSize()
{
    return driveIconSize();
}

int Config::listIconSize()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("listIconSize", 16);
}

void Config::setShowDriveIp(bool b)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("showDriveIp", b);
    g.config()->sync();
}

void Config::setShowMillerIp(bool b)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("showMillerIp", b);
    g.config()->sync();
}

void Config::setDriveBlacklist(const QStringList &l)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("driveBlacklist", l);
    g.config()->sync();
}

void Config::setSidebarIconSize(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("sidebarIconSize", i);
    g.config()->sync();
}

void Config::setDriveIconSize(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("driveIconSize", i);
    g.config()->sync();
}

void Config::setMillerIconSize(int i)
{
    Q_UNUSED(i)
}

void Config::setListIconSize(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("listIconSize", i);
    g.config()->sync();
}

int Config::sidebarRowHeight()
{
    return uiFontSize() + uiSpacing() * 2 + 4;
}

int Config::sidebarDriveRowHeight()
{
    return uiFontSize() + 4 + uiSpacing() * 4 + 8;
}

int Config::sidebarNetRowHeight()
{
    return sidebarDriveRowHeight();
}

int Config::millerDriveRowHeight()
{
    return sidebarDriveRowHeight();
}

int Config::uiFontSize()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("uiFontSize", 14);
}

int Config::uiSpacing()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("uiSpacing", 2);
}

void Config::setUiFontSize(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("uiFontSize", i);
    g.config()->sync();
}

void Config::setUiSpacing(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("uiSpacing", i);
    g.config()->sync();
}

QString Config::uiFontFamily()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("uiFontFamily", QString());
}

void Config::setUiFontFamily(const QString &f)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("uiFontFamily", f);
    g.config()->sync();
}

QString Config::appLanguage()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("appLanguage", QString());
}

void Config::setAppLanguage(const QString &lang)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("appLanguage", lang);
    g.config()->sync();
}

int Config::millerHeaderHeight()
{
    return 38;
}

void Config::setSidebarRowHeight(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("sidebarRowHeight", i);
    g.config()->sync();
}

void Config::setSidebarDriveRowHeight(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("sidebarDriveRowHeight", i);
    g.config()->sync();
}

void Config::setSidebarNetRowHeight(int i)
{
    Q_UNUSED(i)
}

void Config::setMillerDriveRowHeight(int i)
{
    Q_UNUSED(i)
}

void Config::setMillerHeaderHeight(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("millerHeaderHeight", i);
    g.config()->sync();
}

QString Config::gitLocalDir()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitLocalDir", QString());
}

QString Config::gitRemoteUrl()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitRemoteUrl", QString());
}

QString Config::gitUsername()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitUsername", QString());
}

QList<Config::GitRepo> Config::gitRepos()
{
    QList<GitRepo> out;
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto g = config->group(QStringLiteral("GitRepos"));
    Q_ASSERT(g.isValid());

    const int count = g.readEntry("count", 0);
    for (int i = 0; i < count; ++i)
    {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        GitRepo r;
        r.name      = g.readEntry(prefix + QStringLiteral("name"), QString());
        r.localDir  = g.readEntry(prefix + QStringLiteral("localDir"), QString());
        r.remoteUrl = g.readEntry(prefix + QStringLiteral("remoteUrl"), QString());
        r.username  = g.readEntry(prefix + QStringLiteral("username"), QString());
        out << r;
    }
    if (out.isEmpty())
    {
        const QString lo = gitLocalDir();
        if (!lo.isEmpty())
        {
            GitRepo r;
            r.localDir  = lo;
            r.remoteUrl = gitRemoteUrl();
            r.username  = gitUsername();
            r.name      = QFileInfo(lo).fileName();
            out << r;
        }
    }
    return out;
}

void Config::setGitRepos(const QList<GitRepo> &repos)
{
    auto config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(config != nullptr);
    auto g = config->group(QStringLiteral("GitRepos"));
    Q_ASSERT(g.isValid());

    const int oldCount = g.readEntry("count", 0);
    for (int i = 0; i < oldCount; ++i)
    {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        g.deleteEntry(prefix + QStringLiteral("name"));
        g.deleteEntry(prefix + QStringLiteral("localDir"));
        g.deleteEntry(prefix + QStringLiteral("remoteUrl"));
        g.deleteEntry(prefix + QStringLiteral("username"));
    }
    g.writeEntry("count", repos.size());
    for (int i = 0; i < repos.size(); ++i)
    {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        g.writeEntry(prefix + QStringLiteral("name"),      repos[i].name);
        g.writeEntry(prefix + QStringLiteral("localDir"),  repos[i].localDir);
        g.writeEntry(prefix + QStringLiteral("remoteUrl"), repos[i].remoteUrl);
        g.writeEntry(prefix + QStringLiteral("username"),  repos[i].username);
    }
    g.config()->sync();
}

bool Config::gitShowSidebar()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitShowSidebar", true);
}

QString Config::gitRefreshMode()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitRefreshMode", QStringLiteral("onchange"));
}

int Config::gitRefreshIntervalMinutes()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitRefreshIntervalMinutes", 60);
}

void Config::setGitShowSidebar(bool b)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitShowSidebar", b);
    g.config()->sync();
}

void Config::setGitRefreshMode(const QString &m)
{
    Q_ASSERT(!m.isEmpty());
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitRefreshMode", m);
    g.config()->sync();
}

void Config::setGitRefreshIntervalMinutes(int i)
{
    Q_ASSERT(i >= 0);
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitRefreshIntervalMinutes", i);
    g.config()->sync();
}

QString Config::gitToken()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("gitToken", QString());
}

void Config::setGitLocalDir(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitLocalDir", s);
    g.config()->sync();
}

void Config::setGitRemoteUrl(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitRemoteUrl", s);
    g.config()->sync();
}

void Config::setGitUsername(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitUsername", s);
    g.config()->sync();
}

void Config::setGitToken(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("gitToken", s);
    g.config()->sync();
}

QString Config::paperlessUrl()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("paperlessUrl", QString());
}

QString Config::paperlessToken()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("paperlessToken", QString());
}

bool Config::paperlessSslIgnore()
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    return g.readEntry("paperlessSslIgnore", false);
}

void Config::setPaperlessUrl(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("paperlessUrl", s);
    g.config()->sync();
}

void Config::setPaperlessToken(const QString &s)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("paperlessToken", s);
    g.config()->sync();
}

void Config::setPaperlessSslIgnore(bool b)
{
    auto g = adminGroup();
    Q_ASSERT(g.isValid());
    Q_ASSERT(g.config() != nullptr);
    g.writeEntry("paperlessSslIgnore", b);
    g.config()->sync();
}
