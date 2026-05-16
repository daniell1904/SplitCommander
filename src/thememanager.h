#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QList>

// --- ThemeColors — alle semantischen Farben eines Themes ---
struct ThemeColors {
    // Hintergründe
    QString bgMain;       // Sidebar-Hintergrund, App-Basis
    QString bgDeep;       // Tiefster Hintergrund (Pane-Basis)
    QString bgBox;        // Karten / Boxen in der Sidebar
    QString bgList;       // Listen-Hintergrund
    QString bgAlternate;
    QString bgHover;      // Hover-Zustand
    QString bgSelect;     // Selektiert
    QString bgPanel;      // Footer-Panel, Suchleiste
    QString bgTab;        // Tab-Leiste
    QString bgInput;      // Eingabefelder

    // Rahmen
    QString border;       // Standard-Rahmen
    QString borderAlt;    // Alternativer Rahmen (heller)
    QString separator;    // Trennlinie

    // Text
    QString textPrimary;  // Haupttext
    QString textAccent;   // Akzenttext (Pfade, Header)
    QString textLight;    // Heller Text (selektiert)
    QString textMuted;    // Gedimmter Text
    QString textInactive; // Inaktive Elemente

    // Akzentfarbe
    QString accent;       // Buttons, Highlights
    QString accentHover;  // Akzent-Hover

    // Splitter / Divider
    QString splitter;
    QString colActive;    // Aktive Miller-Spalte Border

    QString name;
};

// --- ThemeManager — Singleton ---
class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager &instance();

    // Aktuelles Theme anwenden (liest QSettings, emittiert themeChanged)
    void apply();
    bool saveTheme(const ThemeColors &c);

    // Alle verfügbaren Themes (intern + extern)
    QList<ThemeColors> allThemes();
    void loadExternalThemes();

    // Direkter Zugriff auf Farben
    const ThemeColors &colors() const { return m_colors; }

    // Stylesheet-Generatoren
    QString ssToolBtn()     const;
    QString ssActionBtn()   const;
    QString ssColActive()   const;
    QString ssColInactive() const;
    QString ssColDrives()   const;
    QString ssMenu()        const;
    QString ssListWidget()  const;
    QString ssDialog()      const;
    QString ssSidebar()     const;
    QString ssPane()        const;
    QString ssToolbar()     const;
    QString ssSearchPanel() const;
    QString ssPathEdit()    const;
    QString ssSplitter()    const;
    QString ssBox()         const;
    QString ssFooterBtn()   const;

    // Vordefinierte Themes (Nur noch als Vorlagen für den Export)
private:
    static ThemeColors nordTheme();
    static ThemeColors catppuccinTheme();
    static ThemeColors gruvboxTheme();
    static ThemeColors draculaTheme();
    static ThemeColors oneDarkTheme();
    static ThemeColors solarizedDarkTheme();

signals:
    void themeChanged();

private:
    ThemeManager();
    ThemeColors m_colors;
    QList<ThemeColors> m_externalThemes;

    void exportDefaultThemes(const QString &destDir);
    void buildAppStyleSheet();
    static ThemeColors themeFromJson(const QByteArray &data);
};

// Kurzschreibweise
inline ThemeManager &TM() { return ThemeManager::instance(); }

