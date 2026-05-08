// --- thememanager.cpp — SplitCommander Theme-System ---

#include "thememanager.h"
#include "settingsdialog.h"

#include <QSettings>
#include <QApplication>
#include <QPalette>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

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
    c.bgMain      = "#1e1e2e";
    c.bgAlternate = "#1e1e2e";
    c.bgDeep      = "#11111b";
    c.bgBox       = "#313244";
    c.bgList      = "#181825";
    c.bgHover     = "#313244";
    c.bgSelect    = "#45475a";
    c.bgPanel     = "#11111b";
    c.bgTab       = "#11111b";
    c.bgInput     = "#181825";
    c.border      = "#313244";
    c.borderAlt   = "#45475a";
    c.separator   = "#313244";
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
    c.bgMain      = "#282828";
    c.bgAlternate = "#3c3836";
    c.bgDeep      = "#1d2021";
    c.bgBox       = "#3c3836";
    c.bgList      = "#32302f";
    c.bgHover     = "#3c3836";
    c.bgSelect    = "#504945";
    c.bgPanel     = "#1d2021";
    c.bgTab       = "#1d2021";
    c.bgInput     = "#1d2021";
    c.border      = "#3c3836";
    c.borderAlt   = "#504945";
    c.separator   = "#3c3836";
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

// --- allThemes — Liste aller verfügbaren Designs ---
QList<ThemeColors> ThemeManager::allThemes()
{
    QList<ThemeColors> list;
    list << nordTheme();
    list << catppuccinTheme();
    list << gruvboxTheme();
    list << m_externalThemes;
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
        return;
    }

    QDirIterator it(path, QStringList() << "*.json", QDir::Files);
    while (it.hasNext()) {
        QFile f(it.next());
        if (f.open(QIODevice::ReadOnly)) {
            ThemeColors c = themeFromJson(f.readAll());
            if (!c.name.isEmpty())
                m_externalThemes << c;
        }
    }
}

// --- apply — Theme aus Settings laden und anwenden ---
void ThemeManager::apply()
{
    if (SettingsDialog::useSystemTheme()) {
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

    const QString name = SettingsDialog::selectedTheme();
    
    // Zuerst in externen Themes suchen
    bool found = false;
    for (const auto &c : m_externalThemes) {
        if (c.name == name) {
            m_colors = c;
            found = true;
            break;
        }
    }

    if (!found) {
        if (name == "Catppuccin Mocha")
            m_colors = catppuccinTheme();
        else if (name == "Gruvbox Dark")
            m_colors = gruvboxTheme();
        else
            m_colors = nordTheme();
    }

    buildAppStyleSheet();
    emit themeChanged();
}

// --- themeFromJson — Parsen einer Theme-Datei ---
ThemeColors ThemeManager::themeFromJson(const QByteArray &data)
{
    ThemeColors c;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return c;

    QJsonObject obj = doc.object();
    c.name         = obj.value("name").toString();
    c.bgMain       = obj.value("bgMain").toString();
    c.bgDeep       = obj.value("bgDeep").toString();
    c.bgBox        = obj.value("bgBox").toString();
    c.bgList       = obj.value("bgList").toString();
    c.bgAlternate  = obj.value("bgAlternate").toString();
    c.bgHover      = obj.value("bgHover").toString();
    c.bgSelect     = obj.value("bgSelect").toString();
    c.bgPanel      = obj.value("bgPanel").toString();
    c.bgTab        = obj.value("bgTab").toString();
    c.bgInput      = obj.value("bgInput").toString();
    c.border       = obj.value("border").toString();
    c.borderAlt    = obj.value("borderAlt").toString();
    c.separator    = obj.value("separator").toString();
    c.textPrimary  = obj.value("textPrimary").toString();
    c.textAccent   = obj.value("textAccent").toString();
    c.textLight    = obj.value("textLight").toString();
    c.textMuted    = obj.value("textMuted").toString();
    c.textInactive = obj.value("textInactive").toString();
    c.accent       = obj.value("accent").toString();
    c.accentHover  = obj.value("accentHover").toString();
    c.splitter     = obj.value("splitter").toString();
    c.colActive    = obj.value("colActive").toString();
    return c;
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

    // Scrollbars — global unsichtbar, Overlay-Bars übernehmen
    ss += QString("QScrollBar:vertical { width:0px; background:transparent; border:none; }"
                  "QScrollBar::handle:vertical { background:transparent; }"
                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
                  "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }");
    ss += QString("QScrollBar:horizontal { height:0px; background:transparent; border:none; }"
                  "QScrollBar::handle:horizontal { background:transparent; }"
                  "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }");

    // Splitter
    ss += QString("QSplitter::handle { background:%1; }").arg(c.splitter);

    // Menus
    ss += QString("QMenu { background:%1; color:%2; border:1px solid %3; }"
                  "QMenu::item { padding:6px 20px; }"
                  "QMenu::item:selected { background:%4; }"
                  "QMenu::separator { background:%5; height:1px; margin:4px 8px; }")
              .arg(c.bgList, c.textLight, c.bgSelect, c.bgSelect, c.borderAlt);

    // Header
    ss += QString("QHeaderView::section { background:%1; color:%2; border:none;"
                  "  border-right:1px solid %3; padding:4px 8px; font-size:11px; }")
              .arg(c.bgBox, c.textAccent, c.borderAlt);

    // Inputs
    ss += QString("QLineEdit { background:%1; border:1px solid %2; color:%3;"
                  "  padding:4px; border-radius:4px; }")
              .arg(c.bgInput, c.borderAlt, c.textPrimary);

    // Buttons
    ss += QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                  "  padding:5px 12px; border-radius:4px; }"
                  "QPushButton:hover { background:%4; }"
                  "QPushButton:checked { background:%5; border-color:%6; color:%7; }")
              .arg(c.bgHover, c.textPrimary, c.borderAlt,
                   c.bgSelect, c.accent, c.accentHover, c.textLight);

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
        "QToolButton{color:%1;background:transparent;border:1px solid %2;border-radius:4px;padding:4px;margin:0 1px;}"
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
        .arg(c.bgList, c.textPrimary, c.bgMain, c.bgHover, c.textAccent, c.border);
}

QString ThemeManager::ssMenu() const {
    const auto &c = m_colors;
    return QString(
        "QMenu{background:%1;color:%2;border:1px solid %3;}"
        "QMenu::item{padding:6px 24px 6px 28px;}"
        "QMenu::item:selected{background:%4;}"
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
        "QListWidget{background:%1;border:none;border-radius:0px;outline:none;}"
        "QListWidget::item{color:%2;border-bottom:1px solid %3;padding:2px 8px;}"
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
        "QSpinBox::up-button,QSpinBox::down-button{background:%5;border:none;width:14px;}"
        "QSpinBox::up-button:hover,QSpinBox::down-button:hover{background:%6;}"
        "QPushButton{background:%5;color:%2;border:1px solid %4;padding:7px 14px;border-radius:4px;font-size:11px;}"
        "QPushButton:hover{background:%6;}"
        "QPushButton:checked{background:%7;border-color:%8;color:%9;}"
        "QToolButton{background:%5;color:%2;border:1px solid %4;border-radius:4px;padding:3px;}"
        "QToolButton:hover{background:%6;}"
        "QScrollBar:vertical{background:%1;width:8px;border:none;}"
        "QScrollBar::handle:vertical{background:%4;border-radius:4px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(c.bgList, c.textPrimary, c.bgInput, c.borderAlt,
             c.bgHover, c.bgSelect, c.accent, c.accentHover, c.textLight);
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
