#include "config.h"
#include <KConfigGroup>
#include <QDir>

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

bool Config::confirmDelete() {
    return generalGroup().readEntry("confirmDelete", true);
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

QString Config::terminalApp() {

    return generalGroup().readEntry("terminalApp", QString());
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

void Config::setTerminalApp(const QString &t) {
    generalGroup().writeEntry("terminalApp", t);
    generalGroup().config()->sync();
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
int Config::driveIconSize()    { return adminGroup().readEntry("driveIconSize", 32); }
int Config::millerIconSize()   { return driveIconSize(); }
int Config::listIconSize()     { return adminGroup().readEntry("listIconSize", 16); }

void Config::setShowDriveIp(bool b)          { adminGroup().writeEntry("showDriveIp", b); adminGroup().config()->sync(); }
void Config::setShowMillerIp(bool b)         { adminGroup().writeEntry("showMillerIp", b); adminGroup().config()->sync(); }
void Config::setDriveBlacklist(const QStringList &l) { adminGroup().writeEntry("driveBlacklist", l); adminGroup().config()->sync(); }
void Config::setSidebarIconSize(int i)       { adminGroup().writeEntry("sidebarIconSize", i); adminGroup().config()->sync(); }
void Config::setDriveIconSize(int i)         { adminGroup().writeEntry("driveIconSize", i); adminGroup().config()->sync(); }
void Config::setMillerIconSize(int)        { /* Ignored, using driveIconSize */ }
void Config::setListIconSize(int i)          { adminGroup().writeEntry("listIconSize", i); adminGroup().config()->sync(); }

QString Config::gitLocalDir()  { return adminGroup().readEntry("gitLocalDir", QString()); }
QString Config::gitRemoteUrl() { return adminGroup().readEntry("gitRemoteUrl", QString()); }
QString Config::gitUsername()  { return adminGroup().readEntry("gitUsername", QString()); }
QString Config::gitToken()     { return adminGroup().readEntry("gitToken", QString()); }

void Config::setGitLocalDir(const QString &s)  { adminGroup().writeEntry("gitLocalDir", s); adminGroup().config()->sync(); }
void Config::setGitRemoteUrl(const QString &s) { adminGroup().writeEntry("gitRemoteUrl", s); adminGroup().config()->sync(); }
void Config::setGitUsername(const QString &s)  { adminGroup().writeEntry("gitUsername", s); adminGroup().config()->sync(); }
void Config::setGitToken(const QString &s)     { adminGroup().writeEntry("gitToken", s); adminGroup().config()->sync(); }




