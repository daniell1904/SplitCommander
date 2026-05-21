#include "config.h"
#include <KConfigGroup>
#include <QDir>
#include <QFileInfo>

static KConfigGroup generalGroup() {
    return KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("General"));
}

static KConfigGroup appearanceGroup() {
    return KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("Appearance"));
}

static KConfigGroup ageBadgeGroup() {
    return KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("AgeBadge"));
}

bool Config::useSystemTheme() {
    return appearanceGroup().readEntry("useSystemTheme", false);
}

QString Config::selectedTheme() {
    return appearanceGroup().readEntry("theme", QStringLiteral("Nord"));
}

QColor Config::ageBadgeColor(int index) {
    const int hues[6] = {0, 30, 80, 160, 220, 270};
    auto group = ageBadgeGroup();
    int sat = group.readEntry("saturation", 220);
    int lit = group.readEntry("lightness", 140);
    int sMapped = 40 + (sat * (255 - 40) / 255);
    int lMapped = 60 + (lit * (220 - 60) / 255);
    int idx = qBound(0, index, 5);
    int s_final = (idx == 5) ? sMapped / 2 : sMapped;
    return QColor::fromHsl(hues[idx], s_final, lMapped);
}

QString Config::dateFormat() {
    return generalGroup().readEntry("dateFormat", QStringLiteral("yyyy-MM-dd HH:mm"));
}

bool Config::singleClickOpen() {
    return generalGroup().readEntry("singleClick", false);
}


bool Config::showHiddenFiles() {
    return generalGroup().readEntry("showHidden", false);
}

bool Config::showFileExtensions() {
    return generalGroup().readEntry("showExtensions", true);
}

int Config::startupBehavior() {
    return generalGroup().readEntry("startupBehavior", 1);
}

QString Config::startupPath() {
    return generalGroup().readEntry("startupPath", QDir::homePath());
}

bool Config::showNewIndicator() {
    return ageBadgeGroup().readEntry("showNewIndicator", true);
}

int Config::ageBadgeSaturation() {
    return ageBadgeGroup().readEntry("saturation", 220);
}

int Config::ageBadgeLightness() {
    return ageBadgeGroup().readEntry("lightness", 140);
}

QString Config::lastLeftPath() {
    return generalGroup().readEntry("lastLeftPath", QDir::homePath());
}

QString Config::lastRightPath() {
    return generalGroup().readEntry("lastRightPath", QDir::homePath());
}




void Config::setUseSystemTheme(bool b) {
    appearanceGroup().writeEntry("useSystemTheme", b);
    appearanceGroup().config()->sync();
}

void Config::setSelectedTheme(const QString &t) {
    appearanceGroup().writeEntry("theme", t);
    appearanceGroup().config()->sync();
}

void Config::setSingleClickOpen(bool b) {
    generalGroup().writeEntry("singleClick", b);
    generalGroup().config()->sync();
}

void Config::setShowHiddenFiles(bool b) {
    generalGroup().writeEntry("showHidden", b);
    generalGroup().config()->sync();
}

void Config::setShowFileExtensions(bool b) {
    generalGroup().writeEntry("showExtensions", b);
    generalGroup().config()->sync();
}

void Config::setStartupBehavior(int i) {
    generalGroup().writeEntry("startupBehavior", i);
    generalGroup().config()->sync();
}

void Config::setStartupPath(const QString &p) {
    generalGroup().writeEntry("startupPath", p);
    generalGroup().config()->sync();
}

void Config::setShowNewIndicator(bool b) {
    ageBadgeGroup().writeEntry("showNewIndicator", b);
    ageBadgeGroup().config()->sync();
}

void Config::setAgeBadgeSaturation(int i) {
    ageBadgeGroup().writeEntry("saturation", i);
    ageBadgeGroup().config()->sync();
}

void Config::setAgeBadgeLightness(int i) {
    ageBadgeGroup().writeEntry("lightness", i);
    ageBadgeGroup().config()->sync();
}


void Config::setLastPaths(const QString &left, const QString &right) {
    generalGroup().writeEntry("lastLeftPath", left);
    generalGroup().writeEntry("lastRightPath", right);
    generalGroup().config()->sync();
}

bool Config::useThumbnails() {
    return appearanceGroup().readEntry("useThumbnails", true);
}

int Config::maxThumbnailSize() {
    return appearanceGroup().readEntry("maxThumbnailSize", 50); // 50 MB
}

void Config::setUseThumbnails(bool b) {
    appearanceGroup().writeEntry("useThumbnails", b);
    appearanceGroup().config()->sync();
}

void Config::setMaxThumbnailSize(int i) {
    appearanceGroup().writeEntry("maxThumbnailSize", i);
    appearanceGroup().config()->sync();
}

QStringList Config::fileTypeColors() {
    QStringList defaults;
    defaults << ".js:#f7df1e" << ".cpp:#00599c" << ".h:#00599c" 
             << ".pdf:#ff0000" << ".zip:#ffa500" << ".tar:#ffa500" << ".gz:#ffa500"
             << ".md:#42b883" << ".txt:#42b883"
             << ".png:#bd93f9" << ".jpg:#bd93f9" << ".svg:#bd93f9"
             << ".mp4:#ff79c6" << ".mp3:#ff79c6";

    return appearanceGroup().readEntry("fileTypeColors", defaults);
}

void Config::setFileTypeColors(const QStringList &list) {
    appearanceGroup().writeEntry("fileTypeColors", list);
    appearanceGroup().config()->sync();
}

KConfigGroup Config::group(const QString &name) {
    return KSharedConfig::openConfig("splitcommanderrc")->group(name);
}

static KConfigGroup adminGroup() {
    return KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("Admin"));
}

bool Config::showDriveIp()        { return adminGroup().readEntry("showDriveIp", true); }
bool Config::showMillerIp()       { return adminGroup().readEntry("showMillerIp", false); }
QStringList Config::driveBlacklist() {
    return adminGroup().readEntry("driveBlacklist",
        QStringList{"/var/lib/docker", "/var/lib/containers"});
}
int Config::sidebarIconSize()  { return adminGroup().readEntry("sidebarIconSize", 22); }
int Config::driveIconSize()    { return uiFontSize(); }
int Config::millerIconSize()   { return driveIconSize(); }
int Config::listIconSize()     { return adminGroup().readEntry("listIconSize", 16); }

void Config::setShowDriveIp(bool b)          { adminGroup().writeEntry("showDriveIp", b); adminGroup().config()->sync(); }
void Config::setShowMillerIp(bool b)         { adminGroup().writeEntry("showMillerIp", b); adminGroup().config()->sync(); }
void Config::setDriveBlacklist(const QStringList &l) { adminGroup().writeEntry("driveBlacklist", l); adminGroup().config()->sync(); }
void Config::setSidebarIconSize(int i)       { adminGroup().writeEntry("sidebarIconSize", i); adminGroup().config()->sync(); }
void Config::setDriveIconSize(int i)         { adminGroup().writeEntry("driveIconSize", i); adminGroup().config()->sync(); }
void Config::setMillerIconSize(int)        { /* Ignored, using driveIconSize */ }
void Config::setListIconSize(int i)          { adminGroup().writeEntry("listIconSize", i); adminGroup().config()->sync(); }

int Config::sidebarRowHeight()       { return uiFontSize() + uiSpacing() * 2 + 4; }
int Config::sidebarDriveRowHeight()  { return uiFontSize() + 4 + uiSpacing() * 4 + 8; }
int Config::sidebarNetRowHeight()    { return sidebarDriveRowHeight(); }
int Config::millerDriveRowHeight()   { return sidebarDriveRowHeight(); }
int Config::uiFontSize() { return adminGroup().readEntry("uiFontSize", 14); }
int Config::uiSpacing()  { return adminGroup().readEntry("uiSpacing",  2); }
void Config::setUiFontSize(int i) { adminGroup().writeEntry("uiFontSize", i); adminGroup().config()->sync(); }
void Config::setUiSpacing(int i)  { adminGroup().writeEntry("uiSpacing",  i); adminGroup().config()->sync(); }

QString Config::uiFontFamily() { return adminGroup().readEntry("uiFontFamily", QString()); }
void Config::setUiFontFamily(const QString &f) { adminGroup().writeEntry("uiFontFamily", f); adminGroup().config()->sync(); }

QString Config::appLanguage() { return adminGroup().readEntry("appLanguage", QString()); }
void Config::setAppLanguage(const QString &lang) { adminGroup().writeEntry("appLanguage", lang); adminGroup().config()->sync(); }

int Config::millerHeaderHeight()     { return 38; }

void Config::setSidebarRowHeight(int i)      { adminGroup().writeEntry("sidebarRowHeight", i); adminGroup().config()->sync(); }
void Config::setSidebarDriveRowHeight(int i) { adminGroup().writeEntry("sidebarDriveRowHeight", i); adminGroup().config()->sync(); }
void Config::setSidebarNetRowHeight(int)     { /* Alias for sidebarDriveRowHeight */ }
void Config::setMillerDriveRowHeight(int)    { /* Alias for sidebarDriveRowHeight */ }
void Config::setMillerHeaderHeight(int i)    { adminGroup().writeEntry("millerHeaderHeight", i); adminGroup().config()->sync(); }

#ifdef SC_PLUGIN_GIT
QString Config::gitLocalDir()  { return adminGroup().readEntry("gitLocalDir", QString()); }
QString Config::gitRemoteUrl() { return adminGroup().readEntry("gitRemoteUrl", QString()); }
QString Config::gitUsername()  { return adminGroup().readEntry("gitUsername", QString()); }

QList<Config::GitRepo> Config::gitRepos() {
    QList<GitRepo> out;
    auto g = KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("GitRepos"));
    const int count = g.readEntry("count", 0);
    for (int i = 0; i < count; ++i) {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        GitRepo r;
        r.name      = g.readEntry(prefix + "name", QString());
        r.localDir  = g.readEntry(prefix + "localDir", QString());
        r.remoteUrl = g.readEntry(prefix + "remoteUrl", QString());
        r.username  = g.readEntry(prefix + "username", QString());
        out << r;
    }
    // Migration vom alten Single-Repo
    if (out.isEmpty()) {
        const QString lo = gitLocalDir();
        if (!lo.isEmpty()) {
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

void Config::setGitRepos(const QList<GitRepo> &repos) {
    auto g = KSharedConfig::openConfig("splitcommanderrc")->group(QStringLiteral("GitRepos"));
    // Alte Einträge entfernen
    const int oldCount = g.readEntry("count", 0);
    for (int i = 0; i < oldCount; ++i) {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        g.deleteEntry(prefix + "name");
        g.deleteEntry(prefix + "localDir");
        g.deleteEntry(prefix + "remoteUrl");
        g.deleteEntry(prefix + "username");
    }
    g.writeEntry("count", repos.size());
    for (int i = 0; i < repos.size(); ++i) {
        const QString prefix = QStringLiteral("repo%1_").arg(i);
        g.writeEntry(prefix + "name",      repos[i].name);
        g.writeEntry(prefix + "localDir",  repos[i].localDir);
        g.writeEntry(prefix + "remoteUrl", repos[i].remoteUrl);
        g.writeEntry(prefix + "username",  repos[i].username);
    }
    g.config()->sync();
}

bool Config::gitShowSidebar()              { return adminGroup().readEntry("gitShowSidebar", true); }
QString Config::gitRefreshMode()           { return adminGroup().readEntry("gitRefreshMode", QStringLiteral("onchange")); }
int Config::gitRefreshIntervalMinutes()    { return adminGroup().readEntry("gitRefreshIntervalMinutes", 60); }
void Config::setGitShowSidebar(bool b)             { adminGroup().writeEntry("gitShowSidebar", b); adminGroup().config()->sync(); }
void Config::setGitRefreshMode(const QString &m)   { adminGroup().writeEntry("gitRefreshMode", m); adminGroup().config()->sync(); }
void Config::setGitRefreshIntervalMinutes(int i)   { adminGroup().writeEntry("gitRefreshIntervalMinutes", i); adminGroup().config()->sync(); }
QString Config::gitToken()     { return adminGroup().readEntry("gitToken", QString()); }

void Config::setGitLocalDir(const QString &s)  { adminGroup().writeEntry("gitLocalDir", s); adminGroup().config()->sync(); }
void Config::setGitRemoteUrl(const QString &s) { adminGroup().writeEntry("gitRemoteUrl", s); adminGroup().config()->sync(); }
void Config::setGitUsername(const QString &s)  { adminGroup().writeEntry("gitUsername", s); adminGroup().config()->sync(); }
void Config::setGitToken(const QString &s)     { adminGroup().writeEntry("gitToken", s); adminGroup().config()->sync(); }
#endif // SC_PLUGIN_GIT

#ifdef SC_PLUGIN_PAPERLESS
QString Config::paperlessUrl()   { return adminGroup().readEntry("paperlessUrl", QString()); }
QString Config::paperlessToken() { return adminGroup().readEntry("paperlessToken", QString()); }
bool    Config::paperlessSslIgnore() { return adminGroup().readEntry("paperlessSslIgnore", false); }
void Config::setPaperlessUrl(const QString &s)   { adminGroup().writeEntry("paperlessUrl", s); adminGroup().config()->sync(); }
void Config::setPaperlessToken(const QString &s) { adminGroup().writeEntry("paperlessToken", s); adminGroup().config()->sync(); }
void Config::setPaperlessSslIgnore(bool b)       { adminGroup().writeEntry("paperlessSslIgnore", b); adminGroup().config()->sync(); }
#endif // SC_PLUGIN_PAPERLESS





