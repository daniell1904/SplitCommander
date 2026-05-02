// ─────────────────────────────────────────────────────────────────────────────
// settingsdialog.cpp — SplitCommander Einstellungen
// ─────────────────────────────────────────────────────────────────────────────

#include "settingsdialog.h"
#include "thememanager.h"

#include <QApplication>
#include <QColorDialog>
#include <QProcess>
#include <QMessageBox>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QListWidget>
#include <QStackedWidget>

// ─────────────────────────────────────────────────────────────────────────────
// THEMES Definition
// ─────────────────────────────────────────────────────────────────────────────
const QList<SD_Styles::ThemePreview> SD_Styles::THEMES = {
    { "Nord",             "#0f1218", "#202530", "#5e81ac", "#ccd4e8", "" },
    { "Catppuccin Mocha", "#1e1e2e", "#313244", "#cba6f7", "#cdd6f4", "" },
    { "Gruvbox Dark",     "#282828", "#3c3836", "#d79921", "#ebdbb2", "" },
};

const QString SD_Styles::DIALOG = "";

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
        { "nav_back",         QObject::tr("Zurück"),                  "Alt+Left"     },
        { "nav_forward",      QObject::tr("Vorwärts"),                "Alt+Right"    },
        { "nav_up",           QObject::tr("Übergeordneter Ordner"),   "Alt+Up"       },
        { "nav_home",         QObject::tr("Home-Verzeichnis"),        "Alt+Home"     },
        { "nav_reload",       QObject::tr("Neu laden"),               "F5"           },
        { "pane_focus_left",  QObject::tr("Linke Pane fokussieren"),  "Ctrl+Left"    },
        { "pane_focus_right", QObject::tr("Rechte Pane fokussieren"), "Ctrl+Right"   },
        { "pane_swap",        QObject::tr("Panes tauschen"),          "Ctrl+U"       },
        { "pane_sync",        QObject::tr("Pfade synchronisieren"),   "Ctrl+Shift+S" },
        { "file_rename",      QObject::tr("Umbenennen"),              "F2"           },
        { "file_delete",      QObject::tr("Löschen"),                 "Delete"       },
        { "file_newfolder",   QObject::tr("Neuer Ordner"),            "F7"           },
        { "file_copy",        QObject::tr("Kopieren"),                "F5"           },
        { "file_move",        QObject::tr("Verschieben"),             "F6"           },
        { "view_hidden",      QObject::tr("Versteckte Dateien"),      "Ctrl+H"       },
        { "view_layout",      QObject::tr("Layout wechseln"),         "Ctrl+L"       },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Farb-Chip Hilfsfunktion
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
    p.setPen(Qt::black);
    QFont f; f.setPixelSize(8); f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(1, 1, w - 2, h - 2), Qt::AlignCenter, "7d");
    return px;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stylesheet — basiert auf ThemeManager-Farben
// ─────────────────────────────────────────────────────────────────────────────
static QString dialogSS()
{
    const auto &c = TM().colors();
    return QString(
        // Basis
        "QDialog { background:%1; color:%2; }"
        // Kategorie-Liste links
        "QListWidget#navList { background:%3; border:none; outline:none; color:%2; font-size:12px; }"
        "QListWidget#navList::item { padding:10px 16px; border-radius:4px; margin:2px 6px; }"
        "QListWidget#navList::item:selected { background:%4; color:%5; }"
        "QListWidget#navList::item:hover:!selected { background:%6; }"
        // Inhaltsbereich
        "QWidget#contentArea { background:%1; }"
        "QGroupBox { background:%3; border:1px solid %7; border-radius:6px; margin-top:16px; padding:12px 10px 10px 10px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:12px; top:-1px; padding:0 6px; color:%8; font-size:11px; font-weight:600; background:%3; }"
        "QLabel { color:%2; font-size:11px; background:transparent; }"
        "QLabel#hint { color:%9; font-size:10px; }"
        "QCheckBox { color:%2; font-size:11px; spacing:8px; }"
        "QCheckBox::indicator { width:16px; height:16px; border-radius:3px; border:1px solid %7; background:%3; }"
        "QCheckBox::indicator:checked { background:%4; border-color:%5; image:url(none); }"
        "QCheckBox::indicator:checked::after { content:''; }"
        "QRadioButton { color:%2; font-size:11px; spacing:8px; }"
        "QRadioButton::indicator { width:14px; height:14px; border-radius:7px; border:1px solid %7; background:%3; }"
        "QRadioButton::indicator:checked { background:%4; border-color:%5; }"
        "QComboBox { background:%3; border:1px solid %7; color:%2; padding:5px 8px; border-radius:4px; font-size:11px; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView { background:%3; color:%2; border:1px solid %7; selection-background-color:%4; }"
        "QKeySequenceEdit { background:%3; border:1px solid %7; color:%2; padding:4px; border-radius:4px; font-size:11px; }"
        "QPushButton { background:%6; color:%2; border:1px solid %7; padding:6px 14px; border-radius:4px; font-size:11px; }"
        "QPushButton:hover { background:%10; }"
        "QPushButton#applyBtn { background:%4; color:%5; border:none; font-weight:600; }"
        "QPushButton#applyBtn:hover { background:%11; }"
        "QScrollArea { border:none; background:transparent; }"
        "QScrollBar:vertical { width:4px; background:transparent; border:none; }"
        "QScrollBar::handle:vertical { background:%6; border-radius:2px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QToolButton { background:%3; border:1px solid %7; border-radius:3px; }"
        "QToolButton:hover { border-color:%4; }"
        // Trennlinie
        "QFrame#divider { background:%7; }"
    )
    .arg(c.bgMain)        // %1 bgMain
    .arg(c.textPrimary)   // %2 textPrimary
    .arg(c.bgBox)         // %3 bgBox
    .arg(c.accent)        // %4 accent
    .arg(c.textLight)     // %5 textLight
    .arg(c.bgHover)       // %6 bgHover
    .arg(c.borderAlt)     // %7 borderAlt
    .arg(c.textAccent)    // %8 textAccent
    .arg(c.textMuted)     // %9 textMuted
    .arg(c.bgSelect)      // %10 bgSelect
    .arg(c.accentHover);  // %11 accentHover
}

// ─────────────────────────────────────────────────────────────────────────────
// Konstruktor
// ─────────────────────────────────────────────────────────────────────────────
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SplitCommander — Einstellungen"));
    setMinimumSize(640, 500);
    setStyleSheet(dialogSS());

    // ── Haupt-Layout: links Navigationsliste, rechts Inhalt ───────────────
    auto *rootLay = new QHBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // Links: Kategorie-Liste
    auto *navList = new QListWidget(this);
    navList->setObjectName("navList");
    navList->setFixedWidth(150);
    navList->setFrameShape(QFrame::NoFrame);
    navList->setSpacing(0);
    navList->addItem(tr("Allgemein"));
    navList->addItem(tr("Design"));
    navList->addItem(tr("Erweitert"));
    navList->addItem(tr("Shortcuts"));
    navList->setCurrentRow(0);

    // Trennlinie
    auto *divider = new QFrame(this);
    divider->setObjectName("divider");
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedWidth(1);

    // Rechts: Stacked Widget
    auto *stack = new QStackedWidget(this);
    stack->setObjectName("contentArea");

    rootLay->addWidget(navList);
    rootLay->addWidget(divider);
    rootLay->addWidget(stack, 1);

    // Seiten aufbauen
    stack->addWidget(buildPageGeneral());
    stack->addWidget(buildPageDesign());
    stack->addWidget(buildPageAdvanced());
    stack->addWidget(buildPageShortcuts());

    // Navigation
    connect(navList, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);

    // ── Bottom-Bar ────────────────────────────────────────────────────────
    // Wrapper um rootLay + Bottom-Bar zu kombinieren
    auto *outerLay = new QVBoxLayout();
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    auto *centerWidget = new QWidget(this);
    centerWidget->setLayout(rootLay);

    auto *bottomBar = new QWidget(this);
    bottomBar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                             .arg(TM().colors().bgBox, TM().colors().borderAlt));
    auto *bottomLay = new QHBoxLayout(bottomBar);
    bottomLay->setContentsMargins(16, 10, 16, 10);
    bottomLay->addStretch();

    auto *cancelBtn = new QPushButton(tr("Abbrechen"), bottomBar);
    cancelBtn->setFixedWidth(100);

    m_applyBtn = new QPushButton(tr("Übernehmen"), bottomBar);
    m_applyBtn->setObjectName("applyBtn");
    m_applyBtn->setFixedWidth(120);

    bottomLay->addWidget(cancelBtn);
    bottomLay->addSpacing(8);
    bottomLay->addWidget(m_applyBtn);

    auto *mainVLay = new QVBoxLayout(this);
    mainVLay->setContentsMargins(0, 0, 0, 0);
    mainVLay->setSpacing(0);
    mainVLay->addWidget(centerWidget, 1);
    mainVLay->addWidget(bottomBar);

    connect(cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() {
        applyAndSave();
        accept();
    });

    loadValues();
}

// ─────────────────────────────────────────────────────────────────────────────
// Seite: Allgemein
// ─────────────────────────────────────────────────────────────────────────────
QWidget *SettingsDialog::buildPageGeneral()
{
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // Verhalten
    auto *grpBeh = new QGroupBox(tr("Verhalten"), page);
    auto *behLay = new QVBoxLayout(grpBeh);
    behLay->setSpacing(10);
    m_singleClickCheck   = new QCheckBox(tr("Einfachklick zum Öffnen"), grpBeh);
    m_confirmDeleteCheck = new QCheckBox(tr("Löschen bestätigen"), grpBeh);
    m_showHiddenCheck    = new QCheckBox(tr("Versteckte Dateien anzeigen  (Ctrl+H)"), grpBeh);
    behLay->addWidget(m_singleClickCheck);
    behLay->addWidget(m_confirmDeleteCheck);
    behLay->addWidget(m_showHiddenCheck);
    lay->addWidget(grpBeh);

    // Datum & Zeit
    auto *grpDate = new QGroupBox(tr("Datum & Zeitformat"), page);
    auto *dateLay = new QHBoxLayout(grpDate);
    dateLay->addWidget(new QLabel(tr("Format:")));
    m_dateFormatCombo = new QComboBox(grpDate);
    m_dateFormatCombo->addItem("yyyy-MM-dd HH:mm",   "yyyy-MM-dd HH:mm");
    m_dateFormatCombo->addItem("dd.MM.yyyy HH:mm",   "dd.MM.yyyy HH:mm");
    m_dateFormatCombo->addItem("MM/dd/yyyy hh:mm AP","MM/dd/yyyy hh:mm AP");
    m_dateFormatCombo->addItem("dd. MMM yyyy",        "dd. MMM yyyy");
    m_dateFormatCombo->addItem("yyyy-MM-dd",          "yyyy-MM-dd");
    dateLay->addWidget(m_dateFormatCombo, 1);
    lay->addWidget(grpDate);

    lay->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// Seite: Design
// ─────────────────────────────────────────────────────────────────────────────
QWidget *SettingsDialog::buildPageDesign()
{
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    // System-Theme
    auto *grpSys = new QGroupBox(tr("System-Theme"), page);
    auto *sysLay = new QVBoxLayout(grpSys);
    m_systemThemeCheck = new QCheckBox(tr("KDE Global Theme verwenden"), grpSys);
    auto *sysHint = new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."), grpSys);
    sysHint->setObjectName("hint");
    sysHint->setWordWrap(true);
    sysLay->addWidget(m_systemThemeCheck);
    sysLay->addWidget(sysHint);
    lay->addWidget(grpSys);

    // Eigene Themes
    m_themeBox = new QGroupBox(tr("Eigenes Theme"), page);
    auto *themeLay = new QVBoxLayout(m_themeBox);
    themeLay->setSpacing(6);
    m_themeGroup = new QButtonGroup(m_themeBox);

    for (int i = 0; i < SD_Styles::THEMES.size(); ++i) {
        const auto &t = SD_Styles::THEMES.at(i);
        auto *row  = new QWidget(m_themeBox);
        auto *rLay = new QHBoxLayout(row);
        rLay->setContentsMargins(4, 4, 4, 4);
        rLay->setSpacing(12);

        auto *rb = new QRadioButton(t.name, row);
        m_themeGroup->addButton(rb, i);
        rLay->addWidget(rb, 1);

        // Farbvorschau
        for (const QString &col : { t.bg, t.box, t.accent, t.text }) {
            auto *chip = new QLabel(row);
            chip->setFixedSize(18, 18);
            chip->setStyleSheet(QString(
                "background:%1; border-radius:3px; border:1px solid rgba(255,255,255,15);").arg(col));
            rLay->addWidget(chip);
        }
        themeLay->addWidget(row);
    }

    connect(m_systemThemeCheck, &QCheckBox::toggled, m_themeBox, &QWidget::setDisabled);
    lay->addWidget(m_themeBox);

    // Altersbadge-Farben
    auto *grpAge = new QGroupBox(tr("Altersbadge-Farben"), page);
    auto *ageLay = new QGridLayout(grpAge);
    ageLay->setSpacing(10);
    ageLay->setColumnStretch(1, 1);
    ageLay->setColumnStretch(3, 1);

    const QStringList ageLabels = {
        tr("< 1 Tag"), tr("< 7 Tage"), tr("< 30 Tage"),
        tr("< 6 Monate"), tr("< 1 Jahr"), tr("> 1 Jahr")
    };
    m_ageColors.clear();
    m_ageBtns.clear();

    for (int i = 0; i < 6; ++i) {
        QColor col = ageBadgeColor(i);
        m_ageColors.append(col);

        auto *lbl = new QLabel(ageLabels.at(i), grpAge);
        auto *btn = new QToolButton(grpAge);
        btn->setFixedSize(56, 22);
        btn->setIcon(QIcon(colorChip(col)));
        btn->setIconSize(QSize(46, 16));

        const int idx = i;
        connect(btn, &QToolButton::clicked, this, [this, idx, btn]() {
            QColor c = QColorDialog::getColor(m_ageColors.at(idx), this, tr("Farbe wählen"));
            if (!c.isValid()) return;
            m_ageColors[idx] = c;
            btn->setIcon(QIcon(colorChip(c)));
        });

        const int col2 = (i % 2) * 2;
        ageLay->addWidget(lbl, i / 2, col2);
        ageLay->addWidget(btn, i / 2, col2 + 1);
        m_ageBtns.append(btn);
    }
    lay->addWidget(grpAge);
    lay->addStretch();

    scroll->setWidget(page);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// Seite: Erweitert
// ─────────────────────────────────────────────────────────────────────────────
QWidget *SettingsDialog::buildPageAdvanced()
{
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    QSettings s("SplitCommander", "Advanced");

    auto *grpThumb = new QGroupBox(tr("Thumbnails"), page);
    auto *thumbLay = new QVBoxLayout(grpThumb);
    auto *thumbCheck = new QCheckBox(tr("Bild-Thumbnails in der Dateiliste laden"), grpThumb);
    thumbCheck->setChecked(s.value("loadThumbnails", true).toBool());
    auto *thumbHint = new QLabel(tr("Kann bei großen Ordnern die Ladezeit erhöhen."), grpThumb);
    thumbHint->setObjectName("hint");
    connect(thumbCheck, &QCheckBox::toggled, [](bool on) {
        QSettings s2("SplitCommander", "Advanced");
        s2.setValue("loadThumbnails", on); s2.sync();
    });
    thumbLay->addWidget(thumbCheck);
    thumbLay->addWidget(thumbHint);
    lay->addWidget(grpThumb);

    lay->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// Seite: Shortcuts
// ─────────────────────────────────────────────────────────────────────────────
QWidget *SettingsDialog::buildPageShortcuts()
{
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget();
    auto *lay  = new QVBoxLayout(page);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(16);

    struct Cat { QString label; QStringList ids; };
    const QList<Cat> cats = {
        { tr("Navigation"), { "nav_back","nav_forward","nav_up","nav_home","nav_reload" } },
        { tr("Pane"),       { "pane_focus_left","pane_focus_right","pane_swap","pane_sync" } },
        { tr("Datei"),      { "file_rename","file_delete","file_newfolder","file_copy","file_move" } },
        { tr("Ansicht"),    { "view_hidden","view_layout" } },
    };

    const auto &entries = allShortcuts();
    QSettings sc("SplitCommander", "Shortcuts");
    m_shortcutEdits.clear();

    for (const auto &cat : cats) {
        auto *grp  = new QGroupBox(cat.label, page);
        auto *grid = new QGridLayout(grp);
        grid->setSpacing(8);
        grid->setColumnStretch(1, 1);

        int row = 0;
        for (const QString &id : cat.ids) {
            ShortcutEntry found;
            bool ok = false;
            for (const auto &e : entries) { if (e.id == id) { found = e; ok = true; break; } }
            if (!ok) continue;

            auto *lbl  = new QLabel(found.label, grp);
            auto *edit = new QKeySequenceEdit(
                QKeySequence(sc.value(found.id, found.defaultKey).toString()), grp);
            edit->setObjectName(found.id);

            auto *resetBtn = new QPushButton("↺", grp);
            resetBtn->setFixedSize(28, 28);
            resetBtn->setToolTip(tr("Standard wiederherstellen"));
            const QString defKey = found.defaultKey;
            connect(resetBtn, &QPushButton::clicked, this,
                    [edit, defKey]() { edit->setKeySequence(QKeySequence(defKey)); });

            grid->addWidget(lbl,      row, 0);
            grid->addWidget(edit,     row, 1);
            grid->addWidget(resetBtn, row, 2);
            m_shortcutEdits.append(edit);
            ++row;
        }
        lay->addWidget(grp);
    }

    auto *resetAll = new QPushButton(tr("Alle Shortcuts zurücksetzen"), page);
    connect(resetAll, &QPushButton::clicked, this, [this]() {
        const auto &entries2 = allShortcuts();
        for (auto *edit : m_shortcutEdits)
            for (const auto &e : entries2)
                if (e.id == edit->objectName())
                    { edit->setKeySequence(QKeySequence(e.defaultKey)); break; }
    });
    lay->addWidget(resetAll);
    lay->addStretch();

    scroll->setWidget(page);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadValues
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::loadValues()
{
    m_singleClickCheck->setChecked(singleClickOpen());
    m_confirmDeleteCheck->setChecked(confirmDelete());
    m_showHiddenCheck->setChecked(showHiddenFiles());

    const QString fmt = dateFormat();
    for (int i = 0; i < m_dateFormatCombo->count(); ++i)
        if (m_dateFormatCombo->itemData(i).toString() == fmt)
            { m_dateFormatCombo->setCurrentIndex(i); break; }

    m_systemThemeCheck->setChecked(useSystemTheme());
    m_themeBox->setDisabled(useSystemTheme());

    const QString cur = selectedTheme();
    bool found = false;
    for (int i = 0; i < SD_Styles::THEMES.size(); ++i) {
        if (SD_Styles::THEMES.at(i).name == cur) {
            if (auto *rb = qobject_cast<QRadioButton*>(m_themeGroup->button(i)))
                { rb->setChecked(true); found = true; break; }
        }
    }
    if (!found)
        if (auto *rb = qobject_cast<QRadioButton*>(m_themeGroup->button(0)))
            rb->setChecked(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// applyAndSave
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDialog::applyAndSave()
{
    // Allgemein
    {
        QSettings s("SplitCommander", "General");
        const bool oldHidden = s.value("showHidden", false).toBool();
        const bool oldClick  = s.value("singleClick", false).toBool();
        s.setValue("singleClick",   m_singleClickCheck->isChecked());
        s.setValue("confirmDelete", m_confirmDeleteCheck->isChecked());
        s.setValue("showHidden",    m_showHiddenCheck->isChecked());
        s.setValue("dateFormat",    m_dateFormatCombo->currentData().toString());
        s.sync();
        if (m_showHiddenCheck->isChecked() != oldHidden)
            emit hiddenFilesChanged(m_showHiddenCheck->isChecked());
        if (m_singleClickCheck->isChecked() != oldClick)
            emit singleClickChanged(m_singleClickCheck->isChecked());
    }

    // Design — Theme ermitteln und speichern
    bool themeChanged = false;
    {
        QSettings s("SplitCommander", "Appearance");
        const bool oldSys   = s.value("useSystemTheme", false).toBool();
        const QString oldTh = s.value("theme", "Nord").toString();
        const bool newSys   = m_systemThemeCheck->isChecked();
        s.setValue("useSystemTheme", newSys);

        if (!newSys) {
            int idx = -1;
            if (m_themeGroup->checkedButton())
                idx = m_themeGroup->id(m_themeGroup->checkedButton());
            if (idx >= 0 && idx < SD_Styles::THEMES.size())
                s.setValue("theme", SD_Styles::THEMES.at(idx).name);
        }
        s.sync();

        const QString newTh = s.value("theme", "Nord").toString();
        themeChanged = (oldSys != newSys) || (oldTh != newTh);
    }

    // Age-Badge Farben
    {
        QSettings s("SplitCommander", "AgeBadge");
        for (int i = 0; i < m_ageColors.size(); ++i)
            s.setValue(QString("color%1").arg(i), m_ageColors.at(i).name());
        s.sync();
    }

    // Shortcuts
    {
        QSettings s("SplitCommander", "Shortcuts");
        for (auto *edit : m_shortcutEdits) {
            if (!edit->objectName().isEmpty())
                s.setValue(edit->objectName(), edit->keySequence().toString());
        }
        s.sync();
        emit shortcutsChanged();
    }

    // Theme-Wechsel: Neustart anbieten
    if (themeChanged) {
        emit this->themeChanged();

        QMessageBox msg(this);
        msg.setWindowTitle(tr("Neustart erforderlich"));
        msg.setText(tr("Das Theme wird nach einem Neustart vollständig übernommen."));
        msg.setInformativeText(tr("SplitCommander jetzt neu starten?"));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setDefaultButton(QMessageBox::Yes);
        msg.setStyleSheet(dialogSS());
        msg.button(QMessageBox::Yes)->setText(tr("Jetzt neu starten"));
        msg.button(QMessageBox::No)->setText(tr("Später"));

        if (msg.exec() == QMessageBox::Yes) {
            // Neustart: aktuelles Binary neu starten
            QProcess::startDetached(QApplication::applicationFilePath(),
                                    QApplication::arguments());
            QApplication::quit();
        }
    }
}
