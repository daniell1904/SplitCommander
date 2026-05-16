#pragma once
#include <QDialog>
#include <QStringList>
#include <QColor>

class QCheckBox;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class QSlider;
class QGroupBox;
class QButtonGroup;
class KShortcutsEditor;

class SettingsDialog : public QDialog {
  Q_OBJECT
public:
  enum Page { GeneralPage, AppearancePage, ShortcutsPage };
  explicit SettingsDialog(QWidget *parent = nullptr);
  void showPage(Page page);

signals:
  void settingsChanged();

private:
  void buildUI();
  void load();
  void save();

  // Pages
  QWidget* createGeneralPage();
  QWidget* createAppearancePage();
  QWidget* createShortcutsPage();

  QStackedWidget *m_stack = nullptr;
  QListWidget    *m_sidebar = nullptr;

  // Themes
  QCheckBox    *m_sysCheck   = nullptr;
  QWidget      *m_themeBox   = nullptr;
  QButtonGroup *m_themeGroup = nullptr;

  // Laufwerke
  QCheckBox   *m_showDriveIp    = nullptr;
  QListWidget *m_driveBlacklist = nullptr;
  QLineEdit   *m_blacklistEdit  = nullptr;

  // View
  QCheckBox   *m_showMillerIp   = nullptr;
  QCheckBox   *m_showHidden     = nullptr;
  QCheckBox   *m_singleClick    = nullptr;
  QCheckBox   *m_showExtensions = nullptr;

  // Startup
  QButtonGroup *m_startupGroup = nullptr;
  QLineEdit    *m_startupPathEdit = nullptr;

  // Thumbnails
  QCheckBox *m_useThumbnails = nullptr;
  QSpinBox  *m_maxThumbSize  = nullptr;

  // File type colors
  QListWidget *m_fileTypeColorList = nullptr;

  // Icons
  QSpinBox    *m_sidebarIconSize  = nullptr;
  QSpinBox    *m_driveIconSize    = nullptr;
  QSpinBox    *m_listIconSize     = nullptr;

  // AgeBadge
  QSlider      *m_sSlider       = nullptr;
  QSlider      *m_lSlider       = nullptr;
  QWidget      *m_gradBar       = nullptr;
  QCheckBox    *m_indicatorCheck = nullptr;
  QList<QColor> m_ageColors;
  void updateDynamicColors();


  KShortcutsEditor *m_shortcutsEditor = nullptr;
};
