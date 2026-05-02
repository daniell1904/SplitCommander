#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSettings>
#include <QScrollArea>
#include <QToolButton>
#include <QColor>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// ShortcutEntry — eine konfigurierbare Tastenkombination
// ─────────────────────────────────────────────────────────────────────────────
struct ShortcutEntry {
    QString id;
    QString label;
    QString defaultKey;
};

// ─────────────────────────────────────────────────────────────────────────────
// ThemePreview — Theme-Definition (auch für mainwindow.cpp nutzbar)
// ─────────────────────────────────────────────────────────────────────────────
namespace SD_Styles {
struct ThemePreview {
    QString name;
    QString bg;
    QString box;
    QString accent;
    QString text;
    QString stylesheet;
};
// Deklaration — Definition in settingsdialog.cpp
extern const QList<ThemePreview> THEMES;
extern const QString DIALOG;
} // namespace SD_Styles

// ─────────────────────────────────────────────────────────────────────────────
// SettingsDialog
// ─────────────────────────────────────────────────────────────────────────────
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Statische Hilfsmethoden — werden von anderen Klassen genutzt
    static bool   useSystemTheme();
    static QString selectedTheme();
    static QColor ageBadgeColor(int index);  // 0..5
    static QString dateFormat();
    static bool   singleClickOpen();
    static bool   confirmDelete();
    static bool   showHiddenFiles();
    static QString shortcut(const QString &id);

    // Liste aller konfigurierbaren Shortcuts
    static QList<ShortcutEntry> allShortcuts();

signals:
    void themeChanged();
    void shortcutsChanged();
    void hiddenFilesChanged(bool show);
    void singleClickChanged(bool singleClick);

private:
    void buildTabGeneral(QTabWidget *tabs);
    void buildTabDesign(QTabWidget *tabs);
    void buildTabAdvanced(QTabWidget *tabs);
    void buildTabShortcuts(QTabWidget *tabs);

    void applyAndSave();
    void loadValues();

    // ── Design-Tab Widgets ─────────────────────────────────────────────────
    QCheckBox    *m_systemThemeCheck  = nullptr;
    QButtonGroup *m_themeGroup        = nullptr;
    QWidget      *m_themeBox          = nullptr;

    // Age-Badge Farb-Buttons (6 Bereiche)
    QList<QToolButton*> m_ageBtns;
    QList<QColor>       m_ageColors;

    // ── Allgemein-Tab ──────────────────────────────────────────────────────
    QCheckBox *m_singleClickCheck  = nullptr;
    QCheckBox *m_confirmDeleteCheck = nullptr;
    QCheckBox *m_showHiddenCheck   = nullptr;
    QComboBox *m_dateFormatCombo   = nullptr;

    // ── Shortcuts-Tab ─────────────────────────────────────────────────────
    QList<QKeySequenceEdit*> m_shortcutEdits;
};

#endif // SETTINGSDIALOG_H
