#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QList>

// --- ThemeColors — alle semantischen Farben eines Themes ---
struct ThemeColors
{
    // === Hintergründe ===
    QString bgMain;       // Sidebar-Hintergrund und allgemeine App-Basis
    QString bgDeep;       // Tiefster Hintergrund (Basis der Dateiansichten / Panes)
    QString bgBox;        // Hintergrund für Karten / Boxen in der Sidebar (z.B. Git, Paperless)
    QString bgList;       // Hintergrund für alle Datei- und Ordnerlisten
    QString bgAlternate;  // Farbe für jede zweite Zeile in Tabellen (Zeilen-Wechsel für bessere Lesbarkeit)
    QString bgHover;      // Hintergrundfarbe, wenn die Maus über ein Element fährt (Hover-Zustand)
    QString bgSelect;     // Hintergrundfarbe für ausgewählte / selektierte Zeilen
    QString bgPanel;      // Hintergrund für das Fußzeilen-Panel (Footer) und die Suchleiste
    QString bgTab;        // Hintergrund der Tab-Leiste (Tabs)
    QString bgInput;      // Hintergrund für Eingabefelder (z.B. Pfadeingabe, Suche)

    // === Rahmen & Trennlinien ===
    QString border;       // Standard-Rahmenlinie um Widgets
    QString borderAlt;    // Alternativer, hellerer Rahmen für sanftere Abgrenzungen
    QString separator;    // Horizontale/vertikale dünne Trennlinien zwischen Elementen

    // === Textfarben ===
    QString textPrimary;  // Hauptfarbe für normalen Text (Dateinamen, Menüs)
    QString textAccent;   // Akzentfarbe für wichtige Texte (z.B. Pfade, Spaltenüberschriften)
    QString textLight;    // Sehr heller Text (wird verwendet, wenn ein Element selektiert ist)
    QString textMuted;    // Abgedunkelter/gedimmter Text für weniger wichtige Zusatzinfos
    QString textInactive; // Farbe für inaktive, ausgegraute Elemente

    // === Akzentfarben (Highlights) ===
    QString accent;       // Farbe für aktive Schaltflächen, Buttons und Highlights (Hauptakzent)
    QString accentHover;  // Farbe für Highlight-Elemente, wenn die Maus darüber schwebt

    // === Layout-Trenner ===
    QString splitter;     // Farbe des verschiebbaren Trennbalkens zwischen den beiden Dateibereichen
    QString colActive;    // Rahmenfarbe der aktuell aktiven/fokussierten Spalte in den Miller Columns

    QString name;         // Der Anzeigename des Themes (z.B. "Catppuccin")
};

// --- ThemeManager — Singleton ---
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static ThemeManager &instance();

    // Aktuelles Theme anwenden (liest QSettings, emittiert themeChanged)
    void apply();
    void setTemporaryColors(const ThemeColors &c);
    bool saveTheme(const ThemeColors &c);

    // Alle verfügbaren Themes (intern + extern)
    [[nodiscard]] QList<ThemeColors> allThemes();
    void loadExternalThemes();

    // Direkter Zugriff auf Farben
    [[nodiscard]] const ThemeColors &colors() const { return m_colors; }

    // Stylesheet-Generatoren
    [[nodiscard]] QString ssToolBtn()     const;
    [[nodiscard]] QString ssActionBtn()   const;
    [[nodiscard]] QString ssColActive()   const;
    [[nodiscard]] QString ssColInactive() const;
    [[nodiscard]] QString ssColDrives()   const;
    [[nodiscard]] QString ssMenu()        const;
    [[nodiscard]] QString ssListWidget()  const;
    [[nodiscard]] QString ssDialog()      const;
    [[nodiscard]] QString ssSidebar()     const;
    [[nodiscard]] QString ssPane()        const;
    [[nodiscard]] QString ssToolbar()     const;
    [[nodiscard]] QString ssSearchPanel() const;
    [[nodiscard]] QString ssPathEdit()    const;
    [[nodiscard]] QString ssSplitter()    const;
    [[nodiscard]] QString ssBox()         const;
    [[nodiscard]] QString ssFooterBtn()   const;

    // Vordefinierte Themes (Nur noch als Vorlagen für den Export)
private:
    [[nodiscard]] static ThemeColors nordTheme();
    [[nodiscard]] static ThemeColors catppuccinTheme();
    [[nodiscard]] static ThemeColors gruvboxTheme();
    [[nodiscard]] static ThemeColors draculaTheme();
    [[nodiscard]] static ThemeColors oneDarkTheme();
    [[nodiscard]] static ThemeColors solarizedDarkTheme();

signals:
    void themeChanged();

private:
    ThemeManager();
    ThemeColors m_colors;
    QList<ThemeColors> m_externalThemes;

    void exportDefaultThemes(const QString &destDir);
    void writeThemeToFile(const ThemeColors &c, const QString &destDir) const;
    void exportTemplateTheme(const QString &destDir) const;

    void buildAppStyleSheet();
    QString buildGlobalQSS(const ThemeColors &c) const;
    QString buildInputQSS(const ThemeColors &c) const;
    QString buildButtonQSS(const ThemeColors &c) const;
    void applyUiFont();

    [[nodiscard]] static ThemeColors themeFromJson(const QByteArray &data);
};

// Kurzschreibweise
[[nodiscard]] inline ThemeManager &TM()
{
    return ThemeManager::instance();
}
