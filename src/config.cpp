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
    return generalGroup().readEntry("startupBehavior", 0);
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

KConfigGroup Config::group(const QString &name) {
    return KSharedConfig::openConfig("splitcommanderrc")->group(name);
}




