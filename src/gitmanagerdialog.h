#pragma once
#include <QDialog>
#include <QStringList>

class QCheckBox;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QTextEdit;
class QLabel;

class GitManagerDialog : public QDialog {
  Q_OBJECT
public:
  explicit GitManagerDialog(const QString &currentPath,
                            QWidget *parent = nullptr);

private:
  void buildUI();
  void load();
  void save();

  void runGitCommand(const QStringList &args);
  void refreshGitStatus();
  void createGitHubRelease(const QString &tag);

  QString m_gitPath;
  QLabel *m_gitBranchLabel = nullptr;
  QTextEdit *m_gitLog = nullptr;
  QLineEdit *m_gitCommitMsg = nullptr;
  QListWidget *m_gitStatusList = nullptr;

  QLineEdit *m_gitLocalDir = nullptr;
  QLineEdit *m_gitRemoteUrl = nullptr;
  QLineEdit *m_gitUsername = nullptr;
  QLineEdit *m_gitToken = nullptr;

  QCheckBox *m_pushTagsOpt = nullptr;
  QCheckBox *m_createReleaseOpt = nullptr;
signals:
  void settingsChanged();
};
