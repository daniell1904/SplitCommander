#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
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
#include <QSlider>
#include <QColor>
#include <QListWidget>
#include <QStackedWidget>
#include <QList>

// --- ShortcutEntry --- (Struktur zur Speicherung eines Tastenkürzels)
struct ShortcutEntry {
    QString id;
    QString label;
    QString defaultKey;
};

namespace SD_Styles {
// --- ThemePreview --- (Vorschau-Struktur für ein Theme)
struct ThemePreview {
    QString name;
    QString bg;
    QString box;
    QString accent;
    QString text;
    QString stylesheet;
};
extern const QList<ThemePreview> THEMES;
extern const QString DIALOG;
} // namespace SD_Styles

// --- SettingsDialog --- (Einstellungsfenster der Anwendung)
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void setInitialPage(int index);
    ~SettingsDialog() override { if (s_instance == this) s_instance = nullptr; }

    static bool    useSystemTheme();
    static QString selectedTheme();
    static QColor  ageBadgeColor(int index);
    static QString dateFormat();
    static bool    singleClickOpen();
    static bool    confirmDelete();
    static bool    showHiddenFiles();
    static bool    showFileExtensions();
    static QString shortcut(const QString &id);
    static QList<ShortcutEntry> allShortcuts();

signals:
    void themeChanged();
    void shortcutsChanged();
    void hiddenFilesChanged(bool show);
    void singleClickChanged(bool singleClick);

private:
    QWidget *buildPageGeneral();
    QWidget *buildPageDesign();
    QWidget *buildPageAdvanced();
    QWidget *buildPageShortcuts();

    void applyAndSave();
    void loadValues();

private slots:
    void updateDynamicColors(); // Neu: Berechnet Farben live

private:
    static SettingsDialog* s_instance; // Neu: Für Live-Zugriff
    static SettingsDialog* instance() { return s_instance; }

    QCheckBox    *m_systemThemeCheck  = nullptr;
    QButtonGroup *m_themeGroup        = nullptr;
    QWidget      *m_themeBox          = nullptr;
    QPushButton  *m_applyBtn          = nullptr;

    QList<QToolButton*> m_ageBtns;
    QList<QColor>       m_ageColors;
    QListWidget        *m_navList  = nullptr;
    QStackedWidget     *m_stack    = nullptr;
    QWidget    *m_gradBar  = nullptr;
    QCheckBox  *m_ageCheck = nullptr;

    QSlider    *m_sSlider = nullptr; // Neu: Member für Sättigung
    QSlider    *m_lSlider = nullptr; // Neu: Member für Helligkeit

    QCheckBox *m_singleClickCheck   = nullptr;
    QCheckBox *m_confirmDeleteCheck = nullptr;
    QCheckBox *m_showHiddenCheck    = nullptr;
    QCheckBox *m_showExtCheck       = nullptr;
    QComboBox *m_dateFormatCombo    = nullptr;

    QList<QKeySequenceEdit*> m_shortcutEdits;
};

