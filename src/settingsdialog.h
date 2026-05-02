#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

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
#include <QColor>
#include <QList>

struct ShortcutEntry {
    QString id;
    QString label;
    QString defaultKey;
};

namespace SD_Styles {
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

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    static bool    useSystemTheme();
    static QString selectedTheme();
    static QColor  ageBadgeColor(int index);
    static QString dateFormat();
    static bool    singleClickOpen();
    static bool    confirmDelete();
    static bool    showHiddenFiles();
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

    QCheckBox    *m_systemThemeCheck  = nullptr;
    QButtonGroup *m_themeGroup        = nullptr;
    QWidget      *m_themeBox          = nullptr;
    QPushButton  *m_applyBtn          = nullptr;

    QList<QToolButton*> m_ageBtns;
    QList<QColor>       m_ageColors;

    QCheckBox *m_singleClickCheck   = nullptr;
    QCheckBox *m_confirmDeleteCheck = nullptr;
    QCheckBox *m_showHiddenCheck    = nullptr;
    QComboBox *m_dateFormatCombo    = nullptr;

    QList<QKeySequenceEdit*> m_shortcutEdits;
};

#endif // SETTINGSDIALOG_H
