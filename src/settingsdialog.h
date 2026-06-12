#pragma once

#include <QDialog>
#include <QStringList>
#include <QColor>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class QSlider;
class QGroupBox;
class QButtonGroup;
class KShortcutsEditor;
class QFormLayout;
class QVBoxLayout;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Page
    {
        GeneralPage,
        AppearancePage,
        ShortcutsPage
    };

    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

    void showPage(Page page);

signals:
    void settingsChanged();

private:
    void buildUI();
    void buildSidebar(class QHBoxLayout *root);
    void buildFooter(class QVBoxLayout *rightSide);

    void load();
    void loadAppearance();
    void loadBehavior();
    void save();

    // Pages & Sections (NASA Rule 4 Compliance)
    [[nodiscard]] QWidget* createGeneralPage();
    void setupLanguageSection(QVBoxLayout *lay);
    void setupStartupSection(QVBoxLayout *lay);
    void setupBehaviorSection(QVBoxLayout *lay);
    void setupDrivesSection(QVBoxLayout *lay);
    void setupDisplaySection(QVBoxLayout *lay);
    void setupBlacklistSection(QVBoxLayout *lay);

    [[nodiscard]] QWidget* createAppearancePage();
    void setupThemesSection(QVBoxLayout *lay);
    void buildThemeCard(int index, const struct ThemeColors &t, class QGridLayout *themeGrid);
    void setupThumbnailsSection(QVBoxLayout *lay);
    void setupFileTypeColorsSection(QVBoxLayout *lay);
    void connectFileTypeColorButtons(QPushButton *btnPickColor, QPushButton *btnAddExt, QPushButton *btnDelExt, QLineEdit *extEdit);
    void setupAgeBadgesSection(QVBoxLayout *lay);

    [[nodiscard]] QWidget* createShortcutsPage();
    void updateDynamicColors();

    QStackedWidget *m_stack = nullptr;
    QListWidget    *m_sidebar = nullptr;

    // Themes
    QCheckBox    *m_sysCheck   = nullptr;
    QWidget      *m_themeBox   = nullptr;
    QButtonGroup *m_themeGroup = nullptr;

    // Laufwerke
    QCheckBox   *m_showDriveIp    = nullptr;
#ifdef SC_PLUGIN_GIT
    QCheckBox   *m_gitShowSidebar    = nullptr;
    QComboBox   *m_gitRefreshMode    = nullptr;
    QSpinBox    *m_gitRefreshInterval = nullptr;
#endif
    QListWidget *m_driveBlacklist = nullptr;
    QLineEdit   *m_blacklistEdit  = nullptr;

    // View
    QCheckBox   *m_showMillerIp   = nullptr;
    QCheckBox   *m_showHidden     = nullptr;
    QCheckBox   *m_singleClick    = nullptr;
    QCheckBox   *m_showExtensions = nullptr;

    // Startup
    QButtonGroup *m_startupGroup    = nullptr;
    QLineEdit    *m_startupPathEdit = nullptr;

    // Darstellung
    QSpinBox      *m_uiFontSize      = nullptr;
    QFontComboBox *m_fontCombo       = nullptr;
    QSlider       *m_uiSpacing       = nullptr;
    QLabel        *m_uiSpacingLabel  = nullptr;

    // Sprache
    QComboBox     *m_languageCombo   = nullptr;
    QLabel        *m_langHint        = nullptr;

    // Thumbnails
    QCheckBox *m_useThumbnails = nullptr;
    QSpinBox  *m_maxThumbSize  = nullptr;

    // File type colors
    QListWidget *m_fileTypeColorList = nullptr;

    // Icons
    QSpinBox    *m_sidebarIconSize  = nullptr;
    QSpinBox    *m_driveIconSize    = nullptr;
    QSpinBox    *m_listIconSize     = nullptr;
    QSpinBox    *m_sidebarRowHeight = nullptr;
    QSpinBox    *m_sidebarDriveRowHeight = nullptr;
    QSpinBox    *m_millerHeaderHeight = nullptr;

    // AgeBadge
    QSlider      *m_sSlider       = nullptr;
    QSlider      *m_lSlider       = nullptr;
    QWidget      *m_gradBar       = nullptr;
    QCheckBox    *m_indicatorCheck = nullptr;
    QList<QColor> m_ageColors;

    KShortcutsEditor *m_shortcutsEditor = nullptr;
};
