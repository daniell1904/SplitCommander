#pragma once
#include <QDialog>
#include <QStringList>
#include "../../config.h"

class QCheckBox;
class QListWidget;
class QLineEdit;
class QTextEdit;
class QLabel;
class QComboBox;
class QPushButton;
class QWidget;

class GitManagerDialog : public QDialog {
  Q_OBJECT
public:
  explicit GitManagerDialog(const QString &currentPath,
                            QWidget *parent = nullptr);

signals:
  void settingsChanged();

private:
  void buildUI();
  void load();
  void save();

  void runGitCommand(const QStringList &args);
  void refreshGitStatus();
  void createGitHubRelease(const QString &tag, const QString &title,
                           const QString &body, bool isLatest, bool isPrerelease);
  void doCommitPush();
  void doPull();
  void doDiscard();

  // Multi-Repo helpers
  void loadRepoToFields(int index);
  void saveCurrentFieldsToRepo();
  void rebuildRepoCombo();

  QString m_gitPath;
  QLabel        *m_gitBranchLabel  = nullptr;
  QTextEdit     *m_gitLog          = nullptr;
  QLineEdit     *m_gitCommitMsg    = nullptr;
  QListWidget   *m_gitStatusList   = nullptr;

  // Repo config
  QComboBox     *m_repoCombo       = nullptr;
  QList<Config::GitRepo> m_repos;
  int            m_currentRepo     = -1;
  bool           m_blockSave       = false;
  QLineEdit     *m_gitLocalDir     = nullptr;
  QLineEdit     *m_gitRemoteUrl    = nullptr;
  QLineEdit     *m_gitUsername     = nullptr;
  QLineEdit     *m_gitToken        = nullptr;
  QLineEdit     *m_gitRepoName     = nullptr;

  // Push-Optionen (Checkboxen)
  QCheckBox     *m_optPushTags     = nullptr;
  QCheckBox     *m_optCreateRelease= nullptr;
  QCheckBox     *m_optForceWithLease = nullptr;

  // Pull-Optionen
  QCheckBox     *m_optPullRebase   = nullptr;

  // Erweitert-Bereich
  QWidget       *m_advancedWidget  = nullptr;
};
