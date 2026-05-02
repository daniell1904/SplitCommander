// ─────────────────────────────────────────────────────────────────────────────
// settingsdialog.cpp — SplitCommander Einstellungen
// ─────────────────────────────────────────────────────────────────────────────

#include "settingsdialog.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QSpacerItem>

// ─────────────────────────────────────────────────────────────────────────────
// Interne Stylesheet-Definitionen
// ─────────────────────────────────────────────────────────────────────────────
namespace SD_Styles {

const QString DIALOG = QStringLiteral(
    "QDialog { background:#1a1f2e; color:#ccd4e8; }"
    "QTabWidget::pane { border:1px solid #2c3245; background:#1a1f2e; }"
    "QTabBar::tab { background:#202530; color:#4c566a; padding:8px 18px;"
    "  border:1px solid #2c3245; border-bottom:none; border-radius:4px 4px 0 0; margin-right:2px; }"
    "QTabBar::tab:selected { background:#2e3440; color:#ccd4e8; border-color:#3b4252; }"
    "QTabBar::tab:hover:!selected { background:#252b3a; color:#88c0d0; }"
    "QGroupBox { background:#202530; border:1px solid #2c3245; border-radius:6px;"
    "  margin-top:14px; padding:10px; color:#88c0d0; font-size:11px; font-weight:bold; }"
    "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }"
    "QLabel { color:#ccd4e8; font-size:11px; background:transparent; }"
    "QCheckBox { color:#ccd4e8; font-size:11px; spacing:8px; }"
    "QCheckBox::indicator { width:16px; height:16px; border-radius:3px;"
    "  border:1px solid #3b4252; background:#2e3440; }"
    "QCheckBox::indicator:checked { background:#5e81ac; border-color:#81a1c1; }"
    "QRadioButton { color:#ccd4e8; font-size:11px; spacing:8px; }"
    "QRadioButton::indicator { width:14px; height:14px; border-radius:7px;"
    "  border:1px solid #3b4252; background:#2e3440; }"
    "QRadioButton::indicator:checked { background:#5e81ac; border-color:#81a1c1; }"
    "QComboBox { background:#23283a; border:1px solid #2c3245; color:#ccd4e8;"
    "  padding:5px 8px; border-radius:4px; font-size:11px; }"
    "QComboBox::drop-down { border:none; width:20px; }"
    "QComboBox QAbstractItemView { background:#23283a; border:1px solid #2c3245; color:#cdd6f4; }"
    "QKeySequenceEdit { background:#23283a; border:1px solid #2c3245; color:#ccd4e8;"
    "  padding:4px; border-radius:4px; font-size:11px; }"
    "QPushButton { background:#3b4252; color:#ccd4e8; border:1px solid #2c3245;"
    "  padding:7px 16px; border-radius:4px; font-size:11px; }"
    "QPushButton:hover { background:#4c566a; }"
    "QPushButton:pressed { background:#5e81ac; border-color:#81a1c1; }"
    "QScrollArea { border:none; background:transparent; }"
    "QScrollBar:vertical { width:5px; background:transparent; border:none; }"
    "QScrollBar::handle:vertical { background:#3b4252; border-radius:2px; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

} // namespace SD_Styles

// ─────────────────────────────────────────────────────────────────────────────
// THEMES Definition
// ─────────────────────────────────────────────────────────────────────────────
// Themenvorschau-Farben: { name, BG, BOX, ACCENT, TEXT, stylesheet }
const QList<SD_Styles::ThemePreview> SD_Styles::THEMES = {
    {
        "Nord",
        "#0f1218", "#202530", "#5e81ac", "#ccd4e8",
        // Nord — aktuell bereits in SC_Colors definiert, muss konsistent sein
        "QWidget { background-color:#0f1218; color:#ccd4e8; }"
        "QTreeView, QListWidget { background:#2e3440; color:#ccd4e8; border:1px solid #2c3245; }"
        "QTreeView::item:hover, QListWidget::item:hover { background:#3b4252; }"
        "QTreeView::item:selected, QListWidget::item:selected { background:#434c5e; color:#eceff4; }"
        "QHeaderView::section { background:#202530; color:#88c0d0; border:none;"
        "  border-right:1px solid #2c3245; padding:4px 8px; font-size:11px; }"
        "QLineEdit { background:#23283a; border:1px solid #2c3245; color:#ccd4e8; padding:4px; border-radius:4px; }"
        "QPushButton { background:#3b4252; color:#ccd4e8; border:1px solid #2c3245; padding:5px 12px; border-radius:4px; }"
        "QPushButton:hover { background:#4c566a; }"
        "QScrollBar:vertical { width:5px; background:transparent; }"
        "QScrollBar::handle:vertical { background:#4c566a; border-radius:2px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QToolButton { background:#202530; color:#4c566a; border:1px solid #2c3245; border-radius:6px; padding:6px; }"
        "QToolButton:hover { color:#ccd4e8; background:#2e3440; }"
        "QMenu { background:#2e3440; color:#eceff4; border:1px solid #434c5e; }"
        "QMenu::item:selected { background:#434c5e; }"
        "QSplitter::handle { background:#1e2330; }"
    },
    {
        "Catppuccin Mocha",
        "#1e1e2e", "#313244", "#cba6f7", "#cdd6f4",
        "QWidget { background-color:#1e1e2e; color:#cdd6f4; }"
        "QTreeView, QListWidget { background:#181825; color:#cdd6f4; border:1px solid #313244; }"
        "QTreeView::item:hover, QListWidget::item:hover { background:#313244; }"
        "QTreeView::item:selected, QListWidget::item:selected { background:#45475a; color:#cdd6f4; }"
        "QHeaderView::section { background:#1e1e2e; color:#cba6f7; border:none;"
        "  border-right:1px solid #313244; padding:4px 8px; font-size:11px; }"
        "QLineEdit { background:#181825; border:1px solid #313244; color:#cdd6f4; padding:4px; border-radius:4px; }"
        "QPushButton { background:#313244; color:#cdd6f4; border:1px solid #45475a; padding:5px 12px; border-radius:4px; }"
        "QPushButton:hover { background:#45475a; }"
        "QScrollBar:vertical { width:5px; background:transparent; }"
        "QScrollBar::handle:vertical { background:#585b70; border-radius:2px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QToolButton { background:#1e1e2e; color:#585b70; border:1px solid #313244; border-radius:6px; padding:6px; }"
        "QToolButton:hover { color:#cdd6f4; background:#313244; }"
        "QMenu { background:#1e1e2e; color:#cdd6f4; border:1px solid #45475a; }"
        "QMenu::item:selected { background:#45475a; }"
        "QSplitter::handle { background:#181825; }"
    },
    {
        "Gruvbox Dark",
        "#282828", "#3c3836", "#d79921", "#ebdbb2",
        "QWidget { background-color:#282828; color:#ebdbb2; }"
        "QTreeView, QListWidget { background:#1d2021; color:#ebdbb2; border:1px solid #3c3836; }"
        "QTreeView::item:hover, QListWidget::item:hover { background:#3c3836; }"
        "QTreeView::item:selected, QListWidget::item:selected { background:#504945; color:#fbf1c7; }"
        "QHeaderView::section { background:#282828; color:#d79921; border:none;"
        "  border-right:1px solid #3c3836; padding:4px 8px; font-size:11px; }"
        "QLineEdit { background:#1d2021; border:1px solid #3c3836; color:#ebdbb2; padding:4px; border-radius:4px; }"
        "QPushButton { background:#3c3836; color:#ebdbb2; border:1px solid #504945; padding:5px 12px; border-radius:4px; }"
        "QPushButton:hover { background:#504945; }"
        "QScrollBar:vertical { width:5px; background:transparent; }"
        "QScrollBar::handle:vertical { background:#665c54; border-radius:2px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QToolButton { background:#282828; color:#665c54; border:1px solid #3c3836; border-radius:6px; padding:6px; }"
        "QToolButton:hover { color:#ebdbb2; background:#3c3836; }"
        "QMenu { background:#282828; color:#ebdbb2; border:1px solid #504945; }"
        "QMenu::item:selected { background:#504945; }"
        "QSplitter::handle { background:#1d2021; }"
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Statische Hilfsmethoden
// ─────────────────────────────────────────────────────────────────────────────

bool SettingsDialog::useSystemTheme()
{
    QSettings s("SplitCommander", "Appearance");
    return s.value("useSystemTheme", false).toBool();
}

QString SettingsDialog::selectedTheme()
{
    QSettings s("SplitCommander", "Appearance");
    return s.value("theme", "Nord").toString();
}

QColor SettingsDialog::ageBadgeColor(int index)
{
    // Defaults: < heute, < 7d, < 30d, < 180d, < 365d, > 365d
    static const QStringList defaults = {
        "#00cc44", "#00aacc", "#ddaa00", "#ee6600", "#cc2200", "#6677aa"
    };
    QSettings s("SplitCommander", "AgeBadge");
    return QColor(s.value(QString("color%1").arg(index),
                          defaults.value(index, "#888888")).toString());
}

QString SettingsDialog::dateFormat()
{
    QSettings s("SplitCommander", "General");
    return s.value("dateFormat", "yyyy-MM-dd HH:mm").toString();
}

bool SettingsDialog::singleClickOpen()
{
    QSettings s("SplitCommander", "General");
    return s.value("singleClick", false).toBool();
}

bool SettingsDialog::confirmDelete()
{
    QSettings s("SplitCommander", "General");
    return s.value("confirmDelete", true).toBool();
}

bool SettingsDialog::showHiddenFiles()
{
    QSettings s("SplitCommander", "General");
    return s.value("showHidden", false).toBool();
}

QString SettingsDialog::shortcut(const QString &id)
{
    QSettings s("SplitCommander", "Shortcuts");
    for (const auto &e : allShortcuts())
        if (e.id == id)
            return s.value(id, e.defaultKey).toString();
    return {};
}

QList<ShortcutEntry> SettingsDialog::allShortcuts()
{
    return {
        // Navigation
        { "nav_back",        QObject::tr("Zurück"),                  "Alt+Left"    },
        { "nav_forward",     QObject::tr("Vorwärts"),                "Alt+Right"   },
        { "nav_up",          QObject::tr("Übergeordneter Ordner"),   "Alt+Up"      },
        { "nav_home",        QObject::tr("Home-Verzeichnis"),        "Alt+Home"    },
        { "nav_reload",      QObject::tr("Neu laden"),               "F5"          },
        // Pane
        { "pane_focus_left",  QObject::tr("Linke Pane fokussieren"), "Ctrl+Left"   },
        { "pane_focus_right", QObject::tr("Rechte Pane fokussieren"),"Ctrl+Right"  },
        { "pane_swap",        QObject::tr("Panes tauschen"),         "Ctrl+U"      },
        { "pane_sync",        QObject::tr("Pfade synchronisieren"),  "Ctrl+Shift+S"},
        // Dateioperationen
        { "file_rename",     QObject::tr("Umbenennen"),              "F2"          },
        { "file_delete",     QObject::tr("Löschen"),                 "Delete"      },
        { "file_newfolder",  QObject::tr("Neuer Ordner"),            "F7"          },
        { "file_copy",       QObject::tr("Kopieren"),                "F5"          },
        { "file_move",       QObject::tr("Verschieben"),             "F6"          },
        // Ansicht
        { "view_hidden",     QObject::tr("Versteckte Dateien"),      "Ctrl+H"      },
        { "view_layout",     QObject::tr("Layout wechseln"),         "Ctrl+L"      },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Hilfsfunktion — farbigen Chip zeichnen
// ─────────────────────────────────────────────────────────────────────────────
static QPixmap colorChip(const QColor &col, int w = 44, int h = 20)
{
    QPixmap px(w, h);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(col);
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.drawRoundedRect(1, 1, w - 2, h - 2, 3, 3);
    // Beispiel-Text wie im Badge
    p.setPen(Qt::black);
    QFont f; f.setPixelSize(8); f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(1, 1, w - 2, h - 2), Qt::AlignCenter, "7d");
    return px;
}

// ─────────────────────────────────────────────────────────────────────────────
// Konstruktor
// ─────────────────────────────────────────────────────────────────────────────
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SplitCommander — Einstellungen"));
    setMinimumSize(580, 480);
    setStyleSheet(SD_Styles::DIALOG);

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    auto *tabs = new QTabWidget(this);
    mainLay->addWidget(tabs, 1);

    buildTabGeneral(tabs);
    buildTabDesign(tabs);
    buildTabAdvanced(tabs);
    buildTabShortcuts(tabs);

    // ── Button-Leiste ──────────────────────────────────────────────────────
    auto *btnBar = new QWidget(this);
    btnBar->setStyleSheet("background:#202530; border-top:1px solid #2c3245;");
    auto *btnLay = new QHBoxLayout(btnBar);
    btnLay->setContentsMargins(12, 8, 12, 8);
    btnLay->setSpacing(8);
    btnLay->addStretch();

    auto *cancelBtn = new QPushButton(tr("Abbrechen"), btnBar);
    auto *applyBtn  = new QPushButton(tr("Übernehmen"), btnBar);
    applyBtn->setStyleSheet(
        "QPushButton { background:#5e81ac; color:#eceff4; border-color:#81a1c1; }"
        "QPushButton:hover { background:#81a1c1; }");

    btnLay->addWidget(cancelBtn);
    btnLay->addWidget(applyBtn);
    mainLay->addWidget(btnBar);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn,  &QPushButton::clicked, this, [this]() {
        applyAndSave();
        accept();
    });

    loadValues();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Allgemein
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::buildTabGeneral(QTabWidget *tabs)
{
    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(12);

    // ── Verhalten ─────────────────────────────────────────────────────────
    auto *grpBeh = new QGroupBox(tr("Verhalten"), page);
    auto *grpLay = new QVBoxLayout(grpBeh);
    grpLay->setSpacing(8);

    m_singleClickCheck   = new QCheckBox(tr("Einfachklick zum Öffnen"), grpBeh);
    m_confirmDeleteCheck = new QCheckBox(tr("Löschen bestätigen"), grpBeh);
    m_showHiddenCheck    = new QCheckBox(tr("Versteckte Dateien anzeigen (Ctrl+H)"), grpBeh);

    grpLay->addWidget(m_singleClickCheck);
    grpLay->addWidget(m_confirmDeleteCheck);
    grpLay->addWidget(m_showHiddenCheck);
    lay->addWidget(grpBeh);

    // ── Datum & Zeit ──────────────────────────────────────────────────────
    auto *grpDate = new QGroupBox(tr("Datum & Zeitformat"), page);
    auto *dateLay = new QHBoxLayout(grpDate);
    dateLay->setSpacing(10);

    dateLay->addWidget(new QLabel(tr("Format:")));
    m_dateFormatCombo = new QComboBox(grpDate);
    m_dateFormatCombo->addItem("yyyy-MM-dd HH:mm",  "yyyy-MM-dd HH:mm");
    m_dateFormatCombo->addItem("dd.MM.yyyy HH:mm",  "dd.MM.yyyy HH:mm");
    m_dateFormatCombo->addItem("MM/dd/yyyy hh:mm AP","MM/dd/yyyy hh:mm AP");
    m_dateFormatCombo->addItem("dd. MMM yyyy",       "dd. MMM yyyy");
    m_dateFormatCombo->addItem("yyyy-MM-dd",         "yyyy-MM-dd");
    dateLay->addWidget(m_dateFormatCombo, 1);
    lay->addWidget(grpDate);

    lay->addStretch();
    tabs->addTab(page, tr("Allgemein"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Design
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::buildTabDesign(QTabWidget *tabs)
{
    auto *page    = new QWidget();
    auto *scroll  = new QScrollArea();
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(12);

    // ── System-Theme ──────────────────────────────────────────────────────
    auto *grpSys = new QGroupBox(tr("System-Theme"), page);
    auto *sysLay = new QVBoxLayout(grpSys);

    m_systemThemeCheck = new QCheckBox(
        tr("KDE Global Theme verwenden (ignoriert eigene Themes)"), grpSys);

    auto *sysHint = new QLabel(
        tr("Übernimmt automatisch das unter Systemeinstellungen → Globales Design\n"
           "aktive KDE-Theme — inklusive Farben, Schriften und Stile."), grpSys);
    sysHint->setStyleSheet("color:#4c566a; font-size:10px;");
    sysHint->setWordWrap(true);

    sysLay->addWidget(m_systemThemeCheck);
    sysLay->addWidget(sysHint);
    lay->addWidget(grpSys);

    // ── Eigene Themes ─────────────────────────────────────────────────────
    m_themeBox = new QGroupBox(tr("Eigenes Theme"), page);
    auto *themeLay = new QVBoxLayout(m_themeBox);
    themeLay->setSpacing(8);
    m_themeGroup = new QButtonGroup(m_themeBox);

    for (int i = 0; i < SD_Styles::THEMES.size(); ++i) {
        const auto &t = SD_Styles::THEMES.at(i);

        auto *row = new QWidget(m_themeBox);
        auto *rLay = new QHBoxLayout(row);
        rLay->setContentsMargins(0, 0, 0, 0);
        rLay->setSpacing(10);

        auto *rb = new QRadioButton(t.name, row);
        m_themeGroup->addButton(rb, i);
        rLay->addWidget(rb);

        // Farbvorschau-Chips
        auto makeChip = [](const QString &col) {
            QLabel *l = new QLabel();
            QPixmap px(16, 16);
            px.fill(QColor(col));
            l->setPixmap(px);
            l->setFixedSize(16, 16);
            l->setStyleSheet(QString("background:%1; border-radius:2px; border:1px solid rgba(255,255,255,20);").arg(col));
            return l;
        };
        rLay->addWidget(makeChip(t.bg));
        rLay->addWidget(makeChip(t.box));
        rLay->addWidget(makeChip(t.accent));
        rLay->addWidget(makeChip(t.text));
        rLay->addStretch();

        themeLay->addWidget(row);
    }

    // System-Theme-Check aktiviert/deaktiviert die Theme-Box
    connect(m_systemThemeCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);

    lay->addWidget(m_themeBox);

    // ── Altersbadge-Farben ─────────────────────────────────────────────────
    auto *grpAge = new QGroupBox(tr("Altersbadge-Farben"), page);
    auto *ageLay = new QGridLayout(grpAge);
    ageLay->setSpacing(8);

    const QStringList ageLabels = {
        tr("< 1 Tag"),
        tr("< 7 Tage"),
        tr("< 30 Tage"),
        tr("< 6 Monate"),
        tr("< 1 Jahr"),
        tr("> 1 Jahr")
    };

    m_ageColors.clear();
    m_ageBtns.clear();

    for (int i = 0; i < 6; ++i) {
        QColor col = ageBadgeColor(i);
        m_ageColors.append(col);

        auto *lbl = new QLabel(ageLabels.at(i), grpAge);
        auto *btn = new QToolButton(grpAge);
        btn->setFixedSize(60, 24);
        btn->setIcon(QIcon(colorChip(col)));
        btn->setIconSize(QSize(50, 18));
        btn->setStyleSheet(
            "QToolButton { background:#23283a; border:1px solid #2c3245; border-radius:3px; }"
            "QToolButton:hover { border-color:#5e81ac; }");

        const int idx = i;
        connect(btn, &QToolButton::clicked, this, [this, idx, btn]() {
            QColor c = QColorDialog::getColor(m_ageColors.at(idx), this, tr("Farbe wählen"));
            if (!c.isValid()) return;
            m_ageColors[idx] = c;
            btn->setIcon(QIcon(colorChip(c)));
        });

        ageLay->addWidget(lbl, i / 2, (i % 2) * 2);
        ageLay->addWidget(btn, i / 2, (i % 2) * 2 + 1);
        m_ageBtns.append(btn);
    }

    lay->addWidget(grpAge);
    lay->addStretch();

    tabs->addTab(scroll, tr("Design"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Erweitert
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::buildTabAdvanced(QTabWidget *tabs)
{
    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(12);

    // ── Hinweis ───────────────────────────────────────────────────────────
    auto *hint = new QLabel(
        tr("Erweiterte Optionen wirken sich auf Leistung und Verhalten aus."), page);
    hint->setStyleSheet("color:#4c566a; font-size:10px;");
    hint->setWordWrap(true);
    lay->addWidget(hint);

    // ── Laufwerke ─────────────────────────────────────────────────────────
    auto *grpDrv = new QGroupBox(tr("Laufwerke"), page);
    auto *drvLay = new QVBoxLayout(grpDrv);

    auto *showBarsCheck = new QCheckBox(tr("Speicherbalken in der Geräteliste anzeigen"), grpDrv);
    showBarsCheck->setObjectName("showDriveBars");
    QSettings s("SplitCommander", "Advanced");
    showBarsCheck->setChecked(s.value("showDriveBars", true).toBool());

    connect(showBarsCheck, &QCheckBox::toggled, this, [](bool on) {
        QSettings s2("SplitCommander", "Advanced");
        s2.setValue("showDriveBars", on);
        s2.sync();
    });

    drvLay->addWidget(showBarsCheck);
    lay->addWidget(grpDrv);

    // ── Vorschau ──────────────────────────────────────────────────────────
    auto *grpPrev = new QGroupBox(tr("Vorschau"), page);
    auto *prevLay = new QVBoxLayout(grpPrev);

    auto *previewCheck = new QCheckBox(tr("Vorschau-Panel standardmäßig anzeigen"), grpPrev);
    previewCheck->setObjectName("showPreview");
    previewCheck->setChecked(s.value("showPreview", false).toBool());

    connect(previewCheck, &QCheckBox::toggled, this, [](bool on) {
        QSettings s2("SplitCommander", "Advanced");
        s2.setValue("showPreview", on);
        s2.sync();
    });

    prevLay->addWidget(previewCheck);
    lay->addWidget(grpPrev);

    // ── Thumbnails ────────────────────────────────────────────────────────
    auto *grpThumb = new QGroupBox(tr("Thumbnails"), page);
    auto *thumbLay = new QVBoxLayout(grpThumb);

    auto *thumbCheck = new QCheckBox(tr("Bild-Thumbnails in der Dateiliste laden"), grpThumb);
    thumbCheck->setObjectName("loadThumbnails");
    thumbCheck->setChecked(s.value("loadThumbnails", true).toBool());

    auto *thumbHint = new QLabel(
        tr("Kann bei großen Ordnern die Ladezeit erhöhen."), grpThumb);
    thumbHint->setStyleSheet("color:#4c566a; font-size:10px;");

    connect(thumbCheck, &QCheckBox::toggled, this, [](bool on) {
        QSettings s2("SplitCommander", "Advanced");
        s2.setValue("loadThumbnails", on);
        s2.sync();
    });

    thumbLay->addWidget(thumbCheck);
    thumbLay->addWidget(thumbHint);
    lay->addWidget(grpThumb);

    lay->addStretch();
    tabs->addTab(page, tr("Erweitert"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab: Shortcuts
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::buildTabShortcuts(QTabWidget *tabs)
{
    auto *page   = new QWidget();
    auto *scroll = new QScrollArea();
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(4);

    // Kategorien
    struct Category {
        QString label;
        QStringList ids;
    };
    const QList<Category> cats = {
        { tr("Navigation"), { "nav_back","nav_forward","nav_up","nav_home","nav_reload" } },
        { tr("Pane"),       { "pane_focus_left","pane_focus_right","pane_swap","pane_sync" } },
        { tr("Datei"),      { "file_rename","file_delete","file_newfolder","file_copy","file_move" } },
        { tr("Ansicht"),    { "view_hidden","view_layout" } },
    };

    const auto &entries = allShortcuts();
    QSettings  sc("SplitCommander", "Shortcuts");
    m_shortcutEdits.clear();

    for (const auto &cat : cats) {
        auto *grp  = new QGroupBox(cat.label, page);
        auto *grid = new QGridLayout(grp);
        grid->setSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 0);

        int row = 0;
        for (const QString &id : cat.ids) {
            // Eintrag suchen
            ShortcutEntry found;
            bool ok = false;
            for (const auto &e : entries) {
                if (e.id == id) { found = e; ok = true; break; }
            }
            if (!ok) continue;

            auto *lbl  = new QLabel(found.label, grp);
            auto *edit = new QKeySequenceEdit(
                QKeySequence(sc.value(found.id, found.defaultKey).toString()), grp);
            edit->setObjectName(found.id);

            auto *resetBtn = new QPushButton(tr("↺"), grp);
            resetBtn->setFixedSize(28, 28);
            resetBtn->setToolTip(tr("Standard wiederherstellen"));
            resetBtn->setStyleSheet(
                "QPushButton { font-size:14px; padding:0; background:#2e3440; border:1px solid #2c3245; border-radius:4px; }"
                "QPushButton:hover { background:#3b4252; }");

            const QString defKey = found.defaultKey;
            connect(resetBtn, &QPushButton::clicked, this, [edit, defKey]() {
                edit->setKeySequence(QKeySequence(defKey));
            });

            grid->addWidget(lbl,      row, 0);
            grid->addWidget(edit,     row, 1);
            grid->addWidget(resetBtn, row, 2);
            m_shortcutEdits.append(edit);
            ++row;
        }
        lay->addWidget(grp);
    }

    // Reset-All-Button
    auto *resetAllBtn = new QPushButton(tr("Alle Shortcuts zurücksetzen"), page);
    connect(resetAllBtn, &QPushButton::clicked, this, [this]() {
        const auto &entries2 = allShortcuts();
        for (auto *edit : m_shortcutEdits) {
            const QString id = edit->objectName();
            for (const auto &e : entries2) {
                if (e.id == id) {
                    edit->setKeySequence(QKeySequence(e.defaultKey));
                    break;
                }
            }
        }
    });

    lay->addWidget(resetAllBtn);
    lay->addStretch();

    tabs->addTab(scroll, tr("Shortcuts"));
}

// ─────────────────────────────────────────────────────────────────────────────
// loadValues — Widgets mit gespeicherten Werten befüllen
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::loadValues()
{
    // Allgemein
    m_singleClickCheck->setChecked(singleClickOpen());
    m_confirmDeleteCheck->setChecked(confirmDelete());
    m_showHiddenCheck->setChecked(showHiddenFiles());

    const QString fmt = dateFormat();
    for (int i = 0; i < m_dateFormatCombo->count(); ++i) {
        if (m_dateFormatCombo->itemData(i).toString() == fmt) {
            m_dateFormatCombo->setCurrentIndex(i);
            break;
        }
    }

    // Design
    m_systemThemeCheck->setChecked(useSystemTheme());
    m_themeBox->setDisabled(useSystemTheme());

    const QString cur = selectedTheme();
    for (int i = 0; i < SD_Styles::THEMES.size(); ++i) {
        if (SD_Styles::THEMES.at(i).name == cur) {
            if (auto *rb = qobject_cast<QRadioButton*>(m_themeGroup->button(i)))
                rb->setChecked(true);
            break;
        }
    }
    // Falls nichts ausgewählt: ersten nehmen
    if (!m_themeGroup->checkedButton())
        if (auto *rb = qobject_cast<QRadioButton*>(m_themeGroup->button(0)))
            rb->setChecked(true);

    // Age-Badge Farben sind schon in loadValues durch ageBadgeColor() im Konstruktor gesetzt
}

// ─────────────────────────────────────────────────────────────────────────────
// applyAndSave — Werte in QSettings schreiben + Signale emittieren
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::applyAndSave()
{
    // ── Allgemein ─────────────────────────────────────────────────────────
    {
        QSettings s("SplitCommander", "General");
        const bool oldHidden = s.value("showHidden", false).toBool();
        const bool oldClick  = s.value("singleClick", false).toBool();

        s.setValue("singleClick",    m_singleClickCheck->isChecked());
        s.setValue("confirmDelete",  m_confirmDeleteCheck->isChecked());
        s.setValue("showHidden",     m_showHiddenCheck->isChecked());
        s.setValue("dateFormat",     m_dateFormatCombo->currentData().toString());
        s.sync();

        if (m_showHiddenCheck->isChecked() != oldHidden)
            emit hiddenFilesChanged(m_showHiddenCheck->isChecked());
        if (m_singleClickCheck->isChecked() != oldClick)
            emit singleClickChanged(m_singleClickCheck->isChecked());
    }

    // ── Design ────────────────────────────────────────────────────────────
    {
        QSettings s("SplitCommander", "Appearance");
        const bool useSys = m_systemThemeCheck->isChecked();
        s.setValue("useSystemTheme", useSys);

        if (useSys) {
            // KDE-Palette übernehmen: leeres Stylesheet → Qt nutzt QPalette von KStyle
            qApp->setStyleSheet(QString());
        } else {
            // Ausgewähltes eigenes Theme
            int idx = -1;
            if (m_themeGroup->checkedButton())
                idx = m_themeGroup->id(m_themeGroup->checkedButton());
            if (idx >= 0 && idx < SD_Styles::THEMES.size()) {
                const QString name = SD_Styles::THEMES.at(idx).name;
                s.setValue("theme", name);
                qApp->setStyleSheet(SD_Styles::THEMES.at(idx).stylesheet);
            }
        }
        s.sync();
        emit themeChanged();
    }

    // ── Age-Badge Farben ──────────────────────────────────────────────────
    {
        QSettings s("SplitCommander", "AgeBadge");
        for (int i = 0; i < m_ageColors.size(); ++i)
            s.setValue(QString("color%1").arg(i), m_ageColors.at(i).name());
        s.sync();
    }

    // ── Shortcuts ─────────────────────────────────────────────────────────
    {
        QSettings s("SplitCommander", "Shortcuts");
        for (auto *edit : m_shortcutEdits) {
            const QString id  = edit->objectName();
            const QString seq = edit->keySequence().toString();
            if (!id.isEmpty())
                s.setValue(id, seq);
        }
        s.sync();
        emit shortcutsChanged();
    }
}
