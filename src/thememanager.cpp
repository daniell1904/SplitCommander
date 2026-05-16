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

// --- Singleton ---
// --- Konstruktor ---
ThemeManager::ThemeManager()
{
    loadExternalThemes();
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

// --- Vordefinierte Themes ---
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
void ThemeManager::loadExternalThemes()
{
    m_externalThemes.clear();
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
    
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Standard-Themes exportieren, falls sie fehlen
    exportDefaultThemes(path);

    QDirIterator it(path, QStringList() << "*.json", QDir::Files);
    while (it.hasNext()) {
        it.next();
        QFile f(it.filePath());
        if (f.open(QIODevice::ReadOnly)) {
            ThemeColors c = themeFromJson(f.readAll());
            if (!c.name.isEmpty()) {
                // Duplikate nach Name vermeiden (z.B. falls nord.json und nord_neu.json existieren)
                bool exists = false;
                for (const auto &already : m_externalThemes) {
                    if (already.name == c.name) { exists = true; break; }
                }
                if (!exists) m_externalThemes << c;
            }
        }
    }
}

// --- apply — Theme aus Settings laden und anwenden ---
void ThemeManager::apply()
{
    if (Config::useSystemTheme()) {
        // ... KDE-Palette übernehmen ...
        qApp->setStyleSheet(QString());
        const QPalette &pal = qApp->palette();

        QColor windowColor = pal.color(QPalette::Window);
        QColor baseColor   = pal.color(QPalette::Base);

        m_colors.bgMain      = windowColor.darker(130).name();
        m_colors.bgDeep      = windowColor.darker(115).name();
        m_colors.bgBox       = baseColor.name();
        m_colors.bgList      = windowColor.name();
        m_colors.bgAlternate = pal.color(QPalette::AlternateBase).name();
        m_colors.bgHover     = pal.color(QPalette::Highlight).lighter(120).name();
        m_colors.bgSelect    = pal.color(QPalette::Highlight).name();
        m_colors.bgPanel     = windowColor.darker(120).name();
        m_colors.bgTab       = windowColor.darker(120).name();
        m_colors.bgInput     = baseColor.name();
        m_colors.border      = pal.color(QPalette::Mid).name();
        m_colors.borderAlt   = pal.color(QPalette::Dark).name();
        m_colors.separator   = pal.color(QPalette::Mid).name();
        m_colors.textPrimary = pal.color(QPalette::WindowText).name();
        m_colors.textAccent  = pal.color(QPalette::Link).name();
        m_colors.textLight   = pal.color(QPalette::HighlightedText).name();
        m_colors.textMuted   = pal.color(QPalette::Disabled, QPalette::WindowText).name();
        m_colors.textInactive= pal.color(QPalette::PlaceholderText).name();
        m_colors.accent      = pal.color(QPalette::Highlight).name();
        m_colors.accentHover = pal.color(QPalette::Highlight).lighter(120).name();
        m_colors.splitter    = windowColor.darker(140).name();
        m_colors.colActive   = pal.color(QPalette::Mid).name();
        m_colors.name        = "System";
        emit themeChanged();
        return;
    }

    const QString name = Config::selectedTheme();
    
    // In geladenen Themes suchen
    bool found = false;
    for (const auto &c : allThemes()) {
        if (c.name == name) {
            m_colors = c;
            found = true;
            break;
        }
    }

    if (!found) {
        // Fallback: Wenn gar nichts gefunden wird (Ordner leer), Nord-Design nutzen
        m_colors = nordTheme();
    }

    buildAppStyleSheet();
    emit themeChanged();
}

void ThemeManager::exportDefaultThemes(const QString &destDir)
{
    auto exportTheme = [&](const ThemeColors &c) {
        QString fileName = c.name.toLower().replace(" ", "_") + ".json";
        QFile file(destDir + "/" + fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonObject obj;
            obj["name"]         = c.name;
            obj["bgMain"]       = c.bgMain;
            obj["bgDeep"]       = c.bgDeep;
            obj["bgBox"]        = c.bgBox;
            obj["bgList"]       = c.bgList;
            obj["bgAlternate"]  = c.bgAlternate;
            obj["bgHover"]      = c.bgHover;
            obj["bgSelect"]     = c.bgSelect;
            obj["bgPanel"]      = c.bgPanel;
            obj["bgTab"]        = c.bgTab;
            obj["bgInput"]      = c.bgInput;
            obj["border"]       = c.border;
            obj["borderAlt"]    = c.borderAlt;
            obj["separator"]    = c.separator;
            obj["textPrimary"]  = c.textPrimary;
            obj["textAccent"]   = c.textAccent;
            obj["textLight"]    = c.textLight;
            obj["textMuted"]    = c.textMuted;
            obj["textInactive"] = c.textInactive;
            obj["accent"]       = c.accent;
            obj["accentHover"]  = c.accentHover;
            obj["splitter"]     = c.splitter;
            obj["colActive"]    = c.colActive;

            QJsonDocument doc(obj);
            file.write(doc.toJson());
        }
    };

    exportTheme(nordTheme());
    exportTheme(catppuccinTheme());
    exportTheme(gruvboxTheme());
    exportTheme(draculaTheme());
    exportTheme(oneDarkTheme());
    exportTheme(solarizedDarkTheme());

    // Saubere Vorlage exportieren (erzwingen)
    QFile guide(destDir + "/00_VORLAGE_THEME_ERSTELLEN.json");
    if (guide.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["name"]         = "Vorlage";
        obj["bgMain"]       = "#1e1e2e";
        obj["bgDeep"]       = "#11111b";
        obj["bgBox"]        = "#313244";
        obj["bgList"]       = "#1e1e2e";
        obj["bgAlternate"]  = "#181825";
        obj["bgHover"]      = "#313244";
        obj["bgSelect"]     = "#cba6f7";
        obj["bgPanel"]      = "#1e1e2e";
        obj["bgTab"]        = "#1e1e2e";
        obj["bgInput"]      = "#181825";
        obj["border"]       = "#313244";
        obj["borderAlt"]    = "#45475a";
        obj["separator"]    = "#181825";
        obj["textPrimary"]  = "#cdd6f4";
        obj["textAccent"]   = "#cba6f7";
        obj["textLight"]    = "#ffffff";
        obj["textMuted"]    = "#585b70";
        obj["textInactive"] = "#45475a";
        obj["accent"]       = "#cba6f7";
        obj["accentHover"]  = "#b4befe";
        obj["splitter"]     = "#11111b";
        obj["colActive"]    = "#cba6f7";
        
        QJsonDocument doc(obj);
        guide.write(doc.toJson());
        guide.close();
    }
    
    // Alte, fehlerhafte Anleitung ggf. löschen um Verwirrung zu vermeiden
    QFile::remove(destDir + "/00_ANLEITUNG_THEME_ERSTELLEN.json");
}

// --- sanitizeColor — Extrahiert Hex-Code falls Kommentare enthalten sind ---
static QString sanitizeColor(const QString &input, const QString &fallback)
{
    if (input.isEmpty()) return fallback;
    if (QColor::isValidColorName(input)) return input;
    
    // Suche nach #RRGGBB
    static QRegularExpression re("#[0-9a-fA-F]{6}");
    auto match = re.match(input);
    if (match.hasMatch()) return match.captured(0);
    
    return fallback;
}

ThemeColors ThemeManager::themeFromJson(const QByteArray &data)
{
    ThemeColors c;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return c;

    QJsonObject obj = doc.object();
    c.name         = obj.value("name").toString("Unbenannt");
    c.bgMain       = sanitizeColor(obj.value("bgMain").toString(), "#1e1e2e");
    c.bgDeep       = sanitizeColor(obj.value("bgDeep").toString(), "#11111b");
    c.bgBox        = sanitizeColor(obj.value("bgBox").toString(), "#313244");
    c.bgList       = sanitizeColor(obj.value("bgList").toString(), "#1e1e2e");
    c.bgAlternate  = sanitizeColor(obj.value("bgAlternate").toString(), "#181825");
    c.bgHover      = sanitizeColor(obj.value("bgHover").toString(), "#313244");
    c.bgSelect     = sanitizeColor(obj.value("bgSelect").toString(), "#cba6f7");
    c.bgPanel      = sanitizeColor(obj.value("bgPanel").toString(), "#1e1e2e");
    c.bgTab        = sanitizeColor(obj.value("bgTab").toString(), "#1e1e2e");
    c.bgInput      = sanitizeColor(obj.value("bgInput").toString(), "#181825");
    c.border       = sanitizeColor(obj.value("border").toString(), "#313244");
    c.borderAlt    = sanitizeColor(obj.value("borderAlt").toString(), "#45475a");
    c.separator    = sanitizeColor(obj.value("separator").toString(), "#181825");
    c.textPrimary  = sanitizeColor(obj.value("textPrimary").toString(), "#cdd6f4");
    c.textAccent   = sanitizeColor(obj.value("textAccent").toString(), "#cba6f7");
    c.textLight    = sanitizeColor(obj.value("textLight").toString(), "#ffffff");
    c.textMuted    = sanitizeColor(obj.value("textMuted").toString(), "#585b70");
    c.textInactive = sanitizeColor(obj.value("textInactive").toString(), "#45475a");
    c.accent       = sanitizeColor(obj.value("accent").toString(), "#cba6f7");
    c.accentHover  = sanitizeColor(obj.value("accentHover").toString(), "#b4befe");
    c.splitter     = sanitizeColor(obj.value("splitter").toString(), "#11111b");
    c.colActive    = sanitizeColor(obj.value("colActive").toString(), "#cba6f7");
    return c;
}

// --- saveTheme — Speichert ein Theme als JSON ---
bool ThemeManager::saveTheme(const ThemeColors &c)
{
    QString fileName = c.name.toLower().trimmed().replace(" ", "_");
    if (fileName.isEmpty()) return false;
    
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes/" + fileName + ".json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    QJsonObject obj;
    obj["name"]         = c.name;
    obj["bgMain"]       = c.bgMain;
    obj["bgDeep"]       = c.bgDeep;
    obj["bgBox"]        = c.bgBox;
    obj["bgList"]       = c.bgList;
    obj["bgAlternate"]  = c.bgAlternate;
    obj["bgHover"]      = c.bgHover;
    obj["bgSelect"]     = c.bgSelect;
    obj["bgPanel"]      = c.bgPanel;
    obj["bgTab"]        = c.bgTab;
    obj["bgInput"]      = c.bgInput;
    obj["border"]       = c.border;
    obj["borderAlt"]    = c.borderAlt;
    obj["separator"]    = c.separator;
    obj["textPrimary"]  = c.textPrimary;
    obj["textAccent"]   = c.textAccent;
    obj["textLight"]    = c.textLight;
    obj["textMuted"]    = c.textMuted;
    obj["textInactive"] = c.textInactive;
    obj["accent"]       = c.accent;
    obj["accentHover"]  = c.accentHover;
    obj["splitter"]     = c.splitter;
    obj["colActive"]    = c.colActive;

    QJsonDocument doc(obj);
    file.write(doc.toJson());
    
    loadExternalThemes(); // Liste aktualisieren
    return true;
}

// --- buildAppStyleSheet — globales QSS für alle Standard-Qt-Widgets ---
void ThemeManager::buildAppStyleSheet()
{
    const ThemeColors &c = m_colors;
    QString ss;

    // Nur globale Basis — KEIN QWidget background, das überschreibt widget-eigene Styles
    ss += QString("QMainWindow { background-color:%1; }").arg(c.bgMain);
    ss += QString("QDialog { background-color:%1; color:%2; }").arg(c.bgBox, c.textPrimary);
    ss += QString("QToolTip { background:%1; color:%2; border:1px solid %3; padding:4px; }")
              .arg(c.bgBox, c.textLight, c.borderAlt);

    // Scrollbars — global unsichtbar
    ss += "QScrollBar:vertical { width:0px; background:transparent; border:none; }";
    ss += "QScrollBar::handle:vertical { background:transparent; }";
    ss += "QScrollBar::add-line:vertical { height:0; }";
    ss += "QScrollBar::sub-line:vertical { height:0; }";
    ss += "QScrollBar::add-page:vertical { background:transparent; }";
    ss += "QScrollBar::sub-page:vertical { background:transparent; }";
    
    ss += "QScrollBar:horizontal { height:0px; background:transparent; border:none; }";
    ss += "QScrollBar::handle:horizontal { background:transparent; }";
    ss += "QScrollBar::add-line:horizontal { width:0; }";
    ss += "QScrollBar::sub-line:horizontal { width:0; }";

    // Splitter
    ss += QString("QSplitter::handle { background:%1; }").arg(c.splitter);

    // Menus
    ss += QString("QMenu { background:%1; color:%2; border:1px solid %3; min-width:180px; border-radius:6px; padding:4px 0px; }"
                  "QMenu::item { padding:6px 20px; }"
                  "QMenu::item:selected { background:%4; border-radius:4px; margin:0px 4px; }"
                  "QMenu::separator { background:%5; height:1px; margin:4px 8px; }")
              .arg(c.bgList, c.textLight, c.borderAlt, c.bgSelect, c.borderAlt);

    // Header
    ss += QString("QHeaderView::section { background-color:%1; color:%2; border:none;"
                  "  border-right:1px solid %3; border-bottom:1px solid %3; padding:4px 8px; font-size:11px; }")
              .arg(c.bgPanel, c.textAccent, c.borderAlt);

    // Inputs
    ss += QString("QLineEdit { background:%1; border:1px solid %2; color:%3;"
                  "  padding:4px; border-radius:8px; }")
              .arg(c.bgInput, c.borderAlt, c.textPrimary);

    // Buttons
    ss += QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                  "  padding:5px 12px; border-radius:6px; }"
                  "QPushButton:hover { background:%4; border-color:%5; }"
                  "QPushButton:checked { background:%6; border-color:%5; color:%7; }")
              .arg(c.bgAlternate, c.textPrimary, c.borderAlt,
                   c.bgSelect, c.accent, c.bgSelect, c.textLight);

    // Combo
    ss += QString("QComboBox { background:%1; border:1px solid %2; color:%3;"
                  "  padding:5px 8px; border-radius:4px; }"
                  "QComboBox::drop-down { border:none; width:20px; }"
                  "QComboBox QAbstractItemView { background:%1; color:%3; border:1px solid %2; }")
              .arg(c.bgInput, c.borderAlt, c.textPrimary);

    // Tabs
    ss += QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                  "QTabBar::tab { background:%3; color:%4; padding:8px 18px;"
                  "  border:1px solid %1; border-bottom:none; border-radius:4px 4px 0 0; margin-right:2px; }"
                  "QTabBar::tab:selected { background:%5; color:%6; }"
                  "QTabBar::tab:hover:!selected { background:%7; color:%8; }")
              .arg(c.borderAlt, c.bgPanel, c.bgBox, c.textMuted,
                   c.bgList, c.textPrimary, c.bgHover, c.textAccent);

    // GroupBox (SettingsDialog)
    ss += QString("QGroupBox { background:%1; border:1px solid %2; border-radius:6px;"
                  "  margin-top:14px; padding:10px; color:%3; font-size:11px; font-weight:bold; }"
                  "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }")
              .arg(c.bgBox, c.borderAlt, c.textAccent);

    // Checkboxes/Radios (SettingsDialog)
    ss += QString("QCheckBox { color:%1; font-size:11px; spacing:8px; }"
                  "QCheckBox::indicator { width:16px; height:16px; border-radius:3px;"
                  "  border:1px solid %2; background:%3; }"
                  "QCheckBox::indicator:checked { background:%4; border-color:%5; }")
              .arg(c.textPrimary, c.bgSelect, c.bgList, c.accent, c.accentHover);
    ss += QString("QRadioButton { color:%1; spacing:8px; }"
                  "QRadioButton::indicator { width:14px; height:14px; border-radius:7px;"
                  "  border:1px solid %2; background:%3; }"
                  "QRadioButton::indicator:checked { background:%4; border-color:%5; }")
              .arg(c.textPrimary, c.bgSelect, c.bgList, c.accent, c.accentHover);

    ss += QString("QKeySequenceEdit { background:%1; border:1px solid %2; color:%3;"
                  "  padding:4px; border-radius:4px; }")
              .arg(c.bgInput, c.borderAlt, c.textPrimary);

    qApp->setStyleSheet(ss);
}

// --- Stylesheet-Generatoren ---
QString ThemeManager::ssToolBtn() const {
    const auto &c = m_colors;
    return QString(
        "QToolButton{color:%1;background:transparent;border:none;border-radius:3px;padding:3px;}"
        "QToolButton:hover{color:%2;background:%3;}")
        .arg(c.textMuted, c.textPrimary, c.bgList);
}

QString ThemeManager::ssActionBtn() const {
    const auto &c = m_colors;
    return QString(
        "QToolButton{color:%1;background:transparent;border:1px solid %2;border-radius:8px;padding:4px;margin:0 1px;}"
        "QToolButton:hover{background:%3;border-color:%4;}"
        "QToolButton:checked{background:%5;border-color:%6;color:%7;}")
        .arg(c.textLight, c.borderAlt, c.bgList, c.bgSelect,
             c.bgHover, c.accent, c.textAccent);
}

QString ThemeManager::ssColActive() const {
    const auto &c = m_colors;
    return QString(
        "QListWidget{background:%1;border:none;color:%2;outline:none;}"
        "QListWidget::item{padding:3px 8px;border-bottom:1px solid %3;}"
        "QListWidget::item:selected{background:%4;color:%5;}"
        "QListWidget::item:hover{background:%6;}"
        "QListWidget QScrollBar:vertical{width:0px;background:transparent;border:none;}"
        "QListWidget QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget:hover QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
        "QListWidget QScrollBar:horizontal{height:0;}")
        .arg(c.bgList, c.textPrimary, c.border, c.bgHover, c.textAccent, c.bgHover);
}

QString ThemeManager::ssColInactive() const {
    const auto &c = m_colors;
    return QString(
        "QListWidget{background:%1;border:none;color:%2;outline:none;}"
        "QListWidget::item{padding:3px 8px;border-bottom:1px solid %3;}"
        "QListWidget::item:selected{background:%1;color:%4;}"
        "QListWidget::item:hover{background:%5;}"
        "QListWidget QScrollBar:vertical{width:0px;background:transparent;border:none;}"
        "QListWidget QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget:hover QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
        "QListWidget QScrollBar:horizontal{height:0;}")
        .arg(c.bgList, c.textInactive, c.bgDeep, c.textPrimary, c.border);
}

QString ThemeManager::ssColDrives() const {
    const auto &c = m_colors;
    return QString(
        "QListWidget{background:%1;border:none;color:%2;outline:none;}"
        "QListWidget::item{padding:5px 8px;border-bottom:1px solid %3;}"
        "QListWidget::item:selected{background:%4;color:%5;}"
        "QListWidget::item:hover{background:%6;}"
        "QListWidget QScrollBar:vertical{width:0px;background:transparent;border:none;}"
        "QListWidget QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget:hover QScrollBar::handle:vertical{background:transparent;}"
        "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
        "QListWidget QScrollBar:horizontal{height:0;}")
        .arg(c.bgList, c.textPrimary, c.bgHover, c.bgHover, c.textAccent, c.border);
}

QString ThemeManager::ssMenu() const {
    const auto &c = m_colors;
    return QString(
        "QMenu{background:%1;color:%2;border:1px solid %3;min-width:180px;border-radius:6px;padding:6px 4px;}"
        "QMenu::item{padding:6px 24px 6px 36px;}"
        "QMenu::item:selected{background:%4;border-radius:4px;margin:0px 4px;}"
        "QMenu::separator{background:%5;height:1px;margin:4px 8px;}"
        "QMenu::indicator{width:14px;height:14px;margin-left:6px;}"
        "QMenu::indicator:non-exclusive:checked{image:url(none);border:2px solid %6;background:%6;border-radius:2px;}"
        "QMenu::indicator:non-exclusive:unchecked{image:url(none);border:1px solid %5;background:transparent;border-radius:2px;}"
        "QMenu::indicator:exclusive:checked{image:url(none);border:2px solid %6;background:%6;border-radius:7px;}"
        "QMenu::indicator:exclusive:unchecked{image:url(none);border:1px solid %5;background:transparent;border-radius:7px;}")
        .arg(c.bgList, c.textLight, c.bgSelect, c.bgSelect, c.borderAlt, c.accent);
}

QString ThemeManager::ssListWidget() const {
    const auto &c = m_colors;
    return QString(
        "QListWidget{background:%1;border:none;border-radius:0px;outline:none;padding:0px;margin:0px;}"
        "QListWidget::item{color:%2;border-bottom:1px solid %3;padding:0px 8px;margin:0px;}"
        "QListWidget::item:hover{background:%4;}"
        "QListWidget::item:selected{background:%5;color:%6;}"
        "QListWidget QScrollBar:vertical{width:0px;background:transparent;border:none;}"
        "QListWidget QScrollBar::handle:vertical{background:%7;border-radius:2px;min-height:20px;}"
        "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}")
        .arg(c.bgList, c.textPrimary, c.bgHover,
             c.bgHover, c.bgSelect, c.textLight, c.textMuted);
}

QString ThemeManager::ssDialog() const {
    const auto &c = m_colors;
    return QString(
        "QDialog{background:%1;color:%2;}"
        "QFrame{background:%1;border:none;}"
        "QGroupBox{background:%1;color:%2;border:1px solid %4;border-radius:4px;margin-top:6px;padding-top:6px;font-size:11px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;color:%2;}"
        "QLabel{color:%2;font-size:11px;background:transparent;}"
        "QLineEdit{background:%3;border:1px solid %4;color:%2;padding:5px;border-radius:4px;font-size:11px;}"
        "QSpinBox{background:%3;border:1px solid %4;color:%2;padding:2px 4px;border-radius:4px;font-size:11px;}"
        "QSpinBox::up-button{background:%5;border:none;width:14px;}"
        "QSpinBox::down-button{background:%5;border:none;width:14px;}"
        "QSpinBox::up-button:hover{background:%6;}"
        "QSpinBox::down-button:hover{background:%6;}"
        "QPushButton{background:%9;color:%2;border:1px solid %4;padding:7px 14px;border-radius:6px;font-size:11px;}"
        "QPushButton:hover{background:%6; border-color:%7;}"
        "QPushButton:checked{background:%6;border:2px solid %7;color:%8;}"
        "QToolButton{background:%5;color:%2;border:1px solid %4;border-radius:4px;padding:3px;}"
        "QToolButton:hover{background:%6;}"
        "QScrollBar:vertical{background:%1;width:8px;border:none;}"
        "QScrollBar::handle:vertical{background:%4;border-radius:4px;min-height:20px;}"
        "QScrollBar::add-line:vertical{height:0;}"
        "QScrollBar::sub-line:vertical{height:0;}")
        .arg(c.bgBox, c.textPrimary, c.bgDeep, c.borderAlt, c.bgHover, c.bgSelect, c.accent, c.textLight, c.bgAlternate);
}

QString ThemeManager::ssSidebar() const {
    const auto &c = m_colors;
    return QString("background-color:%1; border:none;").arg(c.bgMain);
}

QString ThemeManager::ssPane() const {
    const auto &c = m_colors;
    return QString("background-color:%1; border:none;").arg(c.bgMain);
}

QString ThemeManager::ssToolbar() const {
    const auto &c = m_colors;
    return QString("PaneToolbar { background:%1; border-bottom:1px solid %2; }")
        .arg(c.bgBox, c.border);
}

QString ThemeManager::ssSearchPanel() const {
    const auto &c = m_colors;
    return QString("background:%1;").arg(c.bgPanel);
}

QString ThemeManager::ssPathEdit() const {
    const auto &c = m_colors;
    return QString(
        "QLineEdit{background:transparent;border:none;color:%1;font-size:13px;padding:2px 4px;}"
        "QLineEdit:focus{background:%2;border:1px solid %3;border-radius:3px;}")
        .arg(c.textAccent, c.bgInput, c.borderAlt);
}

QString ThemeManager::ssSplitter() const {
    return QString("QSplitter{background:%1;}").arg(m_colors.splitter);
}

QString ThemeManager::ssBox() const {
    const auto &c = m_colors;
    return QString(
        "background-color:%1; border-radius:8px; border:none;"
        " QLabel { background-color:rgba(0,0,0,0) !important; border:none; color:%2; }"
        " QPushButton { background-color:rgba(0,0,0,0) !important; border:none; color:%3; }")
        .arg(c.bgBox, c.textAccent, c.textMuted);
}

QString ThemeManager::ssFooterBtn() const {
    const auto &c = m_colors;
    return QString(
        "QToolButton { color:%1; background:%2; border:none; border-radius:6px; padding:6px; }"
        "QToolButton:hover { color:%3; background:%4; }")
        .arg(c.textMuted, c.bgBox, c.textPrimary, c.bgList);
}
