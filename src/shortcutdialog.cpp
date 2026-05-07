#include "shortcutdialog.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QSettings>

// ─────────────────────────────────────────────────────────────────────────────
// Statische Daten
// ─────────────────────────────────────────────────────────────────────────────
QList<ShortcutEntry> ShortcutDialog::allShortcuts()
{
    return {
        { "nav_back",         tr("Zurück"),                  "Alt+Left"     },
        { "nav_forward",      tr("Vorwärts"),                "Alt+Right"    },
        { "nav_up",           tr("Übergeordneter Ordner"),   "Alt+Up"       },
        { "nav_home",         tr("Home-Verzeichnis"),        "Alt+Home"     },
        { "nav_reload",       tr("Neu laden"),               "Ctrl+R"       },
        { "pane_focus_left",  tr("Linke Pane fokussieren"),  "Ctrl+Left"    },
        { "pane_focus_right", tr("Rechte Pane fokussieren"), "Ctrl+Right"   },
        { "pane_swap",        tr("Panes tauschen"),          "Ctrl+U"       },
        { "pane_sync",        tr("Pfade synchronisieren"),   "Ctrl+Shift+S" },
        { "file_rename",      tr("Umbenennen"),              "F2"           },
        { "file_delete",      tr("Löschen"),                 "Delete"       },
        { "file_newfolder",   tr("Neuer Ordner"),            "F7"           },
        { "file_copy",        tr("Kopieren"),                "F5"           },
        { "file_move",        tr("Verschieben"),             "F6"           },
        { "view_hidden",      tr("Versteckte Dateien"),      "Ctrl+H"       },
        { "view_layout",      tr("Layout wechseln"),         "Ctrl+L"       },
    };
}

QString ShortcutDialog::shortcut(const QString &id)
{
    QSettings s("SplitCommander", "Shortcuts");
    for (const auto &e : allShortcuts())
        if (e.id == id)
            return s.value(id, e.defaultKey).toString();
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Konstruktor
// ─────────────────────────────────────────────────────────────────────────────
ShortcutDialog::ShortcutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Tastenkürzel"));
    setMinimumSize(500, 520);
    setStyleSheet(TM().ssDialog());
    buildUi();
}

void ShortcutDialog::buildUi()
{
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 12);
    rootLay->setSpacing(0);

    // ── Scrollbereich ─────────────────────────────────────────────────────
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;");

    auto *content = new QWidget();
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(20, 20, 20, 12);
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
    m_edits.clear();

    for (const auto &cat : cats) {
        auto *grp  = new QGroupBox(cat.label, content);
        grp->setStyleSheet(
            QString("QGroupBox { color:%1; border:1px solid %2; border-radius:4px;"
                    " margin-top:8px; padding-top:8px; }"
                    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }")
            .arg(TM().colors().textAccent, TM().colors().border));
        auto *grid = new QGridLayout(grp);
        grid->setSpacing(8);
        grid->setColumnStretch(1, 1);

        int row = 0;
        for (const QString &id : cat.ids) {
            ShortcutEntry found;
            bool ok = false;
            for (const auto &e : entries) { if (e.id == id) { found = e; ok = true; break; } }
            if (!ok) continue;

            auto *lbl = new QLabel(found.label, grp);
            lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(TM().colors().textPrimary));

            auto *edit = new QKeySequenceEdit(
                QKeySequence(sc.value(found.id, found.defaultKey).toString()), grp);
            edit->setObjectName(found.id);
            edit->setStyleSheet(
                QString("QKeySequenceEdit { background:%1; border:1px solid %2;"
                        " color:%3; border-radius:3px; padding:2px 6px; }")
                .arg(TM().colors().bgDeep, TM().colors().border, TM().colors().textPrimary));

            auto *resetBtn = new QPushButton("↺", grp);
            resetBtn->setFixedSize(28, 28);
            resetBtn->setToolTip(tr("Standard wiederherstellen"));
            resetBtn->setStyleSheet(
                QString("QPushButton { background:%1; border:1px solid %2; color:%3;"
                        " border-radius:3px; font-size:14px; }"
                        "QPushButton:hover { background:%4; }")
                .arg(TM().colors().bgPanel, TM().colors().border,
                     TM().colors().textPrimary, TM().colors().bgHover));
            const QString defKey = found.defaultKey;
            connect(resetBtn, &QPushButton::clicked, this,
                [edit, defKey]() { edit->setKeySequence(QKeySequence(defKey)); });

            grid->addWidget(lbl,      row, 0);
            grid->addWidget(edit,     row, 1);
            grid->addWidget(resetBtn, row, 2);
            m_edits.append(edit);
            ++row;
        }
        lay->addWidget(grp);
    }

    lay->addStretch();
    scroll->setWidget(content);
    rootLay->addWidget(scroll, 1);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(20, 0, 20, 0);

    auto *resetAllBtn = new QPushButton(tr("Alle zurücksetzen"));
    resetAllBtn->setStyleSheet(
        QString("QPushButton { background:%1; border:1px solid %2; color:%3;"
                " border-radius:4px; padding:6px 16px; }"
                "QPushButton:hover { background:%4; }")
        .arg(TM().colors().bgPanel, TM().colors().border,
             TM().colors().textPrimary, TM().colors().bgHover));
    connect(resetAllBtn, &QPushButton::clicked, this, &ShortcutDialog::resetAll);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->setStyleSheet(
        QString("QPushButton { background:%1; border:1px solid %2; color:%3;"
                " border-radius:4px; padding:6px 16px; }"
                "QPushButton:hover { background:%4; }")
        .arg(TM().colors().bgPanel, TM().colors().border,
             TM().colors().textPrimary, TM().colors().bgHover));
    connect(box, &QDialogButtonBox::accepted, this, [this]() { save(); accept(); });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    btnRow->addWidget(resetAllBtn);
    btnRow->addStretch();
    btnRow->addWidget(box);
    rootLay->addLayout(btnRow);
}

void ShortcutDialog::save()
{
    QSettings s("SplitCommander", "Shortcuts");
    for (auto *edit : m_edits)
        s.setValue(edit->objectName(), edit->keySequence().toString());
    s.sync();
    emit shortcutsChanged();
}

void ShortcutDialog::resetAll()
{
    for (auto *edit : m_edits)
        for (const auto &e : allShortcuts())
            if (e.id == edit->objectName())
                { edit->setKeySequence(QKeySequence(e.defaultKey)); break; }
}
