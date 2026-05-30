// --- thememanager_themes.cpp ------------------------------------------------
// Vordefinierte Theme-Definitionen: Nord, Catppuccin, Gruvbox, Dracula,
// One Dark, Solarized Dark.
// ---------------------------------------------------------------------------

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
    c.name        = QStringLiteral("Nord");
    c.bgMain      = QStringLiteral("#0a0d14");
    c.bgDeep      = QStringLiteral("#161b24");
    c.bgAlternate = QStringLiteral("#343b4a");
    c.bgBox       = QStringLiteral("#202530");
    c.bgList      = QStringLiteral("#2e3440");
    c.bgHover     = QStringLiteral("#3b4252");
    c.bgSelect    = QStringLiteral("#434c5e");
    c.bgPanel     = QStringLiteral("#161a22");
    c.bgTab       = QStringLiteral("#161a22");
    c.bgInput     = QStringLiteral("#23283a");
    c.border      = QStringLiteral("#3b4252");
    c.borderAlt   = QStringLiteral("#4c566a");
    c.separator   = QStringLiteral("#222733");
    c.textPrimary = QStringLiteral("#ccd4e8");
    c.textAccent  = QStringLiteral("#88c0d0");
    c.textLight   = QStringLiteral("#eceff4");
    c.textMuted   = QStringLiteral("#4c566a");
    c.textInactive= QStringLiteral("#8a94a8");
    c.accent      = QStringLiteral("#5e81ac");
    c.accentHover = QStringLiteral("#81a1c1");
    c.splitter    = QStringLiteral("#0a0d14");
    c.colActive   = QStringLiteral("#4c566a");
    return c;
}

ThemeColors ThemeManager::catppuccinTheme()
{
    ThemeColors c;
    c.name        = QStringLiteral("Catppuccin Mocha");
    c.bgMain      = QStringLiteral("#11111b");  // dunkelst — Sidebar
    c.bgDeep      = QStringLiteral("#11111b");  // App-Hintergrund (wie bgPanel)
    c.bgPanel     = QStringLiteral("#11111b");  // Panel
    c.bgTab       = QStringLiteral("#11111b");  // Tab
    c.bgInput     = QStringLiteral("#181825");  // Input
    c.bgBox       = QStringLiteral("#1e1e2e");  // Karten/Gruppen
    c.bgList      = QStringLiteral("#313244");  // Dateilisten — heller als bgBox
    c.bgAlternate = QStringLiteral("#313244");  // Alternierend
    c.bgHover     = QStringLiteral("#45475a");  // Hover
    c.bgSelect    = QStringLiteral("#585b70");  // Selektion
    c.border      = QStringLiteral("#313244");
    c.borderAlt   = QStringLiteral("#45475a");
    c.separator   = QStringLiteral("#1e1e2e");
    c.textPrimary = QStringLiteral("#cdd6f4");
    c.textAccent  = QStringLiteral("#cba6f7");
    c.textLight   = QStringLiteral("#cdd6f4");
    c.textMuted   = QStringLiteral("#585b70");
    c.textInactive= QStringLiteral("#6c7086");
    c.accent      = QStringLiteral("#cba6f7");
    c.accentHover = QStringLiteral("#b4befe");
    c.splitter    = QStringLiteral("#11111b");
    c.colActive   = QStringLiteral("#585b70");
    return c;
}

ThemeColors ThemeManager::gruvboxTheme()
{
    ThemeColors c;
    c.name        = QStringLiteral("Gruvbox Dark");
    c.bgMain      = QStringLiteral("#1d2021");  // dunkelst — Sidebar
    c.bgDeep      = QStringLiteral("#1d2021");  // App-Hintergrund
    c.bgPanel     = QStringLiteral("#1d2021");  // Panel
    c.bgTab       = QStringLiteral("#1d2021");  // Tab
    c.bgInput     = QStringLiteral("#282828");  // Input
    c.bgBox       = QStringLiteral("#282828");  // Karten/Gruppen
    c.bgList      = QStringLiteral("#32302f");  // Dateilisten
    c.bgAlternate = QStringLiteral("#3c3836");  // Alternierend
    c.bgHover     = QStringLiteral("#3c3836");  // Hover
    c.bgSelect    = QStringLiteral("#504945");  // Selektion
    c.border      = QStringLiteral("#3c3836");
    c.borderAlt   = QStringLiteral("#504945");
    c.separator   = QStringLiteral("#1d2021");
    c.textPrimary = QStringLiteral("#ebdbb2");
    c.textAccent  = QStringLiteral("#d79921");
    c.textLight   = QStringLiteral("#fbf1c7");
    c.textMuted   = QStringLiteral("#665c54");
    c.textInactive= QStringLiteral("#928374");
    c.accent      = QStringLiteral("#d79921");
    c.accentHover = QStringLiteral("#fabd2f");
    c.splitter    = QStringLiteral("#1d2021");
    c.colActive   = QStringLiteral("#665c54");
    return c;
}

ThemeColors ThemeManager::draculaTheme()
{
    ThemeColors c;
    c.name        = QStringLiteral("Dracula");
    c.bgMain      = QStringLiteral("#191a21");  // dunkelst — Sidebar
    c.bgDeep      = QStringLiteral("#1e1f29");  // App-Hintergrund
    c.bgPanel     = QStringLiteral("#1e1f29");
    c.bgTab       = QStringLiteral("#1e1f29");
    c.bgInput     = QStringLiteral("#282a36");
    c.bgBox       = QStringLiteral("#282a36");  // Karten
    c.bgList      = QStringLiteral("#343746");  // Dateilisten — heller
    c.bgAlternate = QStringLiteral("#343746");
    c.bgHover     = QStringLiteral("#44475a");
    c.bgSelect    = QStringLiteral("#6272a4");
    c.border      = QStringLiteral("#44475a");
    c.borderAlt   = QStringLiteral("#6272a4");
    c.separator   = QStringLiteral("#44475a");
    c.textPrimary = QStringLiteral("#f8f8f2");
    c.textAccent  = QStringLiteral("#bd93f9");
    c.textLight   = QStringLiteral("#ffffff");
    c.textMuted   = QStringLiteral("#6272a4");
    c.textInactive= QStringLiteral("#44475a");
    c.accent      = QStringLiteral("#bd93f9");
    c.accentHover = QStringLiteral("#ff79c6");
    c.splitter    = QStringLiteral("#191a21");
    c.colActive   = QStringLiteral("#bd93f9");
    return c;
}

ThemeColors ThemeManager::oneDarkTheme()
{
    ThemeColors c;
    c.name        = QStringLiteral("One Dark");
    c.bgMain      = QStringLiteral("#181a1f");  // dunkelst — Sidebar
    c.bgDeep      = QStringLiteral("#181a1f");  // App-Hintergrund
    c.bgPanel     = QStringLiteral("#181a1f");
    c.bgTab       = QStringLiteral("#181a1f");
    c.bgInput     = QStringLiteral("#21252b");
    c.bgBox       = QStringLiteral("#21252b");  // Karten
    c.bgList      = QStringLiteral("#282c34");  // Dateilisten
    c.bgAlternate = QStringLiteral("#2c313a");
    c.bgHover     = QStringLiteral("#3e4451");
    c.bgSelect    = QStringLiteral("#528bff");
    c.border      = QStringLiteral("#3e4451");
    c.borderAlt   = QStringLiteral("#528bff");
    c.separator   = QStringLiteral("#181a1f");
    c.textPrimary = QStringLiteral("#abb2bf");
    c.textAccent  = QStringLiteral("#61afef");
    c.textLight   = QStringLiteral("#ffffff");
    c.textMuted   = QStringLiteral("#5c6370");
    c.textInactive= QStringLiteral("#3e4451");
    c.accent      = QStringLiteral("#61afef");
    c.accentHover = QStringLiteral("#98c379");
    c.splitter    = QStringLiteral("#181a1f");
    c.colActive   = QStringLiteral("#61afef");
    return c;
}

ThemeColors ThemeManager::solarizedDarkTheme()
{
    ThemeColors c;
    c.name        = QStringLiteral("Solarized Dark");
    c.bgMain      = QStringLiteral("#001e26");  // dunkelst — Sidebar
    c.bgDeep      = QStringLiteral("#001e26");  // App-Hintergrund
    c.bgPanel     = QStringLiteral("#001e26");
    c.bgTab       = QStringLiteral("#001e26");
    c.bgInput     = QStringLiteral("#002b36");
    c.bgBox       = QStringLiteral("#002b36");  // Karten
    c.bgList      = QStringLiteral("#073642");  // Dateilisten
    c.bgAlternate = QStringLiteral("#073642");
    c.bgHover     = QStringLiteral("#094554");
    c.bgSelect    = QStringLiteral("#268bd2");
    c.border      = QStringLiteral("#073642");
    c.borderAlt   = QStringLiteral("#268bd2");
    c.separator   = QStringLiteral("#001e26");
    c.textPrimary = QStringLiteral("#839496");
    c.textAccent  = QStringLiteral("#268bd2");
    c.textLight   = QStringLiteral("#93a1a1");
    c.textMuted   = QStringLiteral("#586e75");
    c.textInactive= QStringLiteral("#073642");
    c.accent      = QStringLiteral("#268bd2");
    c.accentHover = QStringLiteral("#2aa198");
    c.splitter    = QStringLiteral("#001e26");
    c.colActive   = QStringLiteral("#268bd2");
    return c;
}

QList<ThemeColors> ThemeManager::allThemes()
{
    QList<ThemeColors> list;
    list << nordTheme() << catppuccinTheme() << gruvboxTheme() 
         << draculaTheme() << oneDarkTheme() << solarizedDarkTheme();

    for (const auto &ext : m_externalThemes)
    {
        bool exists = false;
        for (const auto &base : list)
        {
            if (base.name == ext.name)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            list << ext;
        }
    }
    return list;
}
