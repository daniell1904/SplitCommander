// --- thememanager_themes.cpp ------------------------------------------------
// Vordefinierte Theme-Definitionen: Nord, Catppuccin, Gruvbox, Dracula,
// One Dark, Solarized Dark.
// ---------------------------------------------------------------------------

// --- thememanager.cpp — SplitCommander Theme-System ---

#include "thememanager.h"
#include "config.h"

#include <QApplication>

#include <QPalette>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QElapsedTimer>


ThemeColors ThemeManager::nordTheme()
{
    ThemeColors c;
    c.name        = "Nord";
    c.bgMain      = "#0a0d14";
    c.bgDeep      = "#161b24";
    c.bgAlternate = "#343b4a";
    c.bgBox       = "#202530";
    c.bgList      = "#2e3440";
    c.bgHover     = "#3b4252";
    c.bgSelect    = "#434c5e";
    c.bgPanel     = "#161a22";
    c.bgTab       = "#161a22";
    c.bgInput     = "#23283a";
    c.border      = "#3b4252";
    c.borderAlt   = "#4c566a";
    c.separator   = "#222733";
    c.textPrimary = "#ccd4e8";
    c.textAccent  = "#88c0d0";
    c.textLight   = "#eceff4";
    c.textMuted   = "#4c566a";
    c.textInactive= "#8a94a8";
    c.accent      = "#5e81ac";
    c.accentHover = "#81a1c1";
    c.splitter    = "#0a0d14";
    c.colActive   = "#4c566a";
    return c;
}

ThemeColors ThemeManager::catppuccinTheme()
{
    ThemeColors c;
    c.name        = "Catppuccin Mocha";
    c.bgMain      = "#11111b";  // dunkelst — Sidebar
    c.bgDeep      = "#11111b";  // App-Hintergrund (wie bgPanel)
    c.bgPanel     = "#11111b";  // Panel
    c.bgTab       = "#11111b";  // Tab
    c.bgInput     = "#181825";  // Input
    c.bgBox       = "#1e1e2e";  // Karten/Gruppen
    c.bgList      = "#313244";  // Dateilisten — heller als bgBox
    c.bgAlternate = "#313244";  // Alternierend
    c.bgHover     = "#45475a";  // Hover
    c.bgSelect    = "#585b70";  // Selektion
    c.border      = "#313244";
    c.borderAlt   = "#45475a";
    c.separator   = "#1e1e2e";
    c.textPrimary = "#cdd6f4";
    c.textAccent  = "#cba6f7";
    c.textLight   = "#cdd6f4";
    c.textMuted   = "#585b70";
    c.textInactive= "#6c7086";
    c.accent      = "#cba6f7";
    c.accentHover = "#b4befe";
    c.splitter    = "#11111b";
    c.colActive   = "#585b70";
    return c;
}

ThemeColors ThemeManager::gruvboxTheme()
{
    ThemeColors c;
    c.name        = "Gruvbox Dark";
    c.bgMain      = "#1d2021";  // dunkelst — Sidebar
    c.bgDeep      = "#1d2021";  // App-Hintergrund
    c.bgPanel     = "#1d2021";  // Panel
    c.bgTab       = "#1d2021";  // Tab
    c.bgInput     = "#282828";  // Input
    c.bgBox       = "#282828";  // Karten/Gruppen
    c.bgList      = "#32302f";  // Dateilisten
    c.bgAlternate = "#3c3836";  // Alternierend
    c.bgHover     = "#3c3836";  // Hover
    c.bgSelect    = "#504945";  // Selektion
    c.border      = "#3c3836";
    c.borderAlt   = "#504945";
    c.separator   = "#1d2021";
    c.textPrimary = "#ebdbb2";
    c.textAccent  = "#d79921";
    c.textLight   = "#fbf1c7";
    c.textMuted   = "#665c54";
    c.textInactive= "#928374";
    c.accent      = "#d79921";
    c.accentHover = "#fabd2f";
    c.splitter    = "#1d2021";
    c.colActive   = "#665c54";
    return c;
}

ThemeColors ThemeManager::draculaTheme()
{
    ThemeColors c;
    c.name        = "Dracula";
    c.bgMain      = "#191a21";  // dunkelst — Sidebar
    c.bgDeep      = "#1e1f29";  // App-Hintergrund
    c.bgPanel     = "#1e1f29";
    c.bgTab       = "#1e1f29";
    c.bgInput     = "#282a36";
    c.bgBox       = "#282a36";  // Karten
    c.bgList      = "#343746";  // Dateilisten — heller
    c.bgAlternate = "#343746";
    c.bgHover     = "#44475a";
    c.bgSelect    = "#6272a4";
    c.border      = "#44475a";
    c.borderAlt   = "#6272a4";
    c.separator   = "#44475a";
    c.textPrimary = "#f8f8f2";
    c.textAccent  = "#bd93f9";
    c.textLight   = "#ffffff";
    c.textMuted   = "#6272a4";
    c.textInactive= "#44475a";
    c.accent      = "#bd93f9";
    c.accentHover = "#ff79c6";
    c.splitter    = "#191a21";
    c.colActive   = "#bd93f9";
    return c;
}

ThemeColors ThemeManager::oneDarkTheme()
{
    ThemeColors c;
    c.name        = "One Dark";
    c.bgMain      = "#181a1f";  // dunkelst — Sidebar
    c.bgDeep      = "#181a1f";  // App-Hintergrund
    c.bgPanel     = "#181a1f";
    c.bgTab       = "#181a1f";
    c.bgInput     = "#21252b";
    c.bgBox       = "#21252b";  // Karten
    c.bgList      = "#282c34";  // Dateilisten
    c.bgAlternate = "#2c313a";
    c.bgHover     = "#3e4451";
    c.bgSelect    = "#528bff";
    c.border      = "#3e4451";
    c.borderAlt   = "#528bff";
    c.separator   = "#181a1f";
    c.textPrimary = "#abb2bf";
    c.textAccent  = "#61afef";
    c.textLight   = "#ffffff";
    c.textMuted   = "#5c6370";
    c.textInactive= "#3e4451";
    c.accent      = "#61afef";
    c.accentHover = "#98c379";
    c.splitter    = "#181a1f";
    c.colActive   = "#61afef";
    return c;
}

ThemeColors ThemeManager::solarizedDarkTheme()
{
    ThemeColors c;
    c.name        = "Solarized Dark";
    c.bgMain      = "#001e26";  // dunkelst — Sidebar
    c.bgDeep      = "#001e26";  // App-Hintergrund
    c.bgPanel     = "#001e26";
    c.bgTab       = "#001e26";
    c.bgInput     = "#002b36";
    c.bgBox       = "#002b36";  // Karten
    c.bgList      = "#073642";  // Dateilisten
    c.bgAlternate = "#073642";
    c.bgHover     = "#094554";
    c.bgSelect    = "#268bd2";
    c.border      = "#073642";
    c.borderAlt   = "#268bd2";
    c.separator   = "#001e26";
    c.textPrimary = "#839496";
    c.textAccent  = "#268bd2";
    c.textLight   = "#93a1a1";
    c.textMuted   = "#586e75";
    c.textInactive= "#073642";
    c.accent      = "#268bd2";
    c.accentHover = "#2aa198";
    c.splitter    = "#001e26";
    c.colActive   = "#268bd2";
    return c;
}

// --- allThemes — Liste aller verfügbaren Designs (Statisch seit Start) ---
QList<ThemeColors> ThemeManager::allThemes()
{
    QList<ThemeColors> list;
    list << nordTheme() << catppuccinTheme() << gruvboxTheme() 
         << draculaTheme() << oneDarkTheme() << solarizedDarkTheme();

    for (const auto &ext : m_externalThemes) {
        bool exists = false;
        for (const auto &base : list) {
            if (base.name == ext.name) { exists = true; break; }
        }
        if (!exists) list << ext;
    }
    return list;
}

// --- loadExternalThemes — scannt den themes-Ordner ---
