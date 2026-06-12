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
class QVBoxLayout;
class QFormLayout;
class QGridLayout;

class GitManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitManagerDialog(const QString &currentPath, QWidget *parent = nullptr);
    ~GitManagerDialog() override = default;

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
    void doCreateTag();
    void doCheckoutBranch();
    void doMergeBranch();
    void doRevertCommit();

    // Aktions-Hilfsfunktionen
    QDialog* createCheckoutDialog(const QStringList &branches, QComboBox *&branchCombo, QLineEdit *&newBranchEdit, QCheckBox *&forceCheck);
    QString promptForPushRemote(const QStringList &remotes);
    void performCommitAndPush(const QString &msg, const QString &relTag, const QString &relTitle, const QString &relBody, bool relLatest, bool relPrerelease, const QString &pushRemote, bool forcePush);
    QStringList injectGitCredentials(const QStringList &args);
    void executeGitProcess(const QStringList &finalArgs, const QStringList &originalArgs);

    // Abschnitts-Builder (NASA Regel 4 Compliance)
    void setupRepoSelectionSection(QVBoxLayout *root, const QString &normBtnSS, const QString &inputSS, const QString &labelSS);
    void setupBranchStatusSection(QVBoxLayout *root, const QString &labelSS, const QString &accentColor);
    void setupCommitMessageSection(QVBoxLayout *root, const QString &labelSS, const QString &inputSS);
    void setupMainActionsSection(QVBoxLayout *root, const QString &primBtnSS, const QString &normBtnSS, const QString &dangerBtnSS);
    void setupPushOptionsSection(QVBoxLayout *root, const QString &checkSS, const QString &textAccent, const QString &borderAlt);
    void setupPullOptionsSection(QVBoxLayout *root, const QString &checkSS, const QString &textAccent, const QString &borderAlt);
    void setupAdvancedFunctionsSection(QVBoxLayout *root, const QString &normBtnSS, const QString &textAccent, const QString &accentColor);
    void setupConnectionSettingsSection(QVBoxLayout *root, const QString &inputSS, const QString &normBtnSS, const QString &textAccent, const QString &borderAlt);
    void setupLogOutputSection(QVBoxLayout *root, const QString &labelSS, const QString &bgDeep, const QString &textPrimary, const QString &borderAlt);

    // Multi-Repo-Hilfsfunktionen
    void loadRepoToFields(int index);
    void saveCurrentFieldsToRepo();
    void rebuildRepoCombo();

    // Refaktorierungs-Helfer
    void buildFooter(QVBoxLayout *root, const QString &normBtnSS);
    void connectRepoSelectionButtons(QPushButton *btnNewRepo, QPushButton *btnDelRepo);
    void buildAdvancedButtons(QGridLayout *advLay, const QString &normBtnSS);
    void connectAdvancedButtons(QPushButton *btnLog, QPushButton *btnDiff, QPushButton *btnTag, QPushButton *btnBranch, QPushButton *btnMerge, QPushButton *btnRevert, QPushButton *btnStash, QPushButton *btnStashPop);
    
    // Extrahierte Verbindungseinstellungen
    void buildLocalDirField(QFormLayout *form, const QString &inputSS, const QString &normBtnSS);
    void buildRemoteUrlField(QFormLayout *form, const QString &inputSS);
    void buildCredentialsFields(QFormLayout *form, const QString &inputSS);
    void buildAuthActionButtons(QVBoxLayout *root, const QString &normBtnSS, const QString &borderAlt);

    void connectConnectionSettingsButtons(QPushButton *btnSaveConfig, QPushButton *btnTestAuth, QPushButton *btnClone);
    void connectCloneButton(QPushButton *btnClone);
    void connectConnectionFieldsToSave();

    QString m_gitPath;
    QLabel        *m_gitBranchLabel  = nullptr;
    QTextEdit     *m_gitLog          = nullptr;
    QLineEdit     *m_gitCommitMsg    = nullptr;
    QListWidget   *m_gitStatusList   = nullptr;

    // Repo-Konfiguration
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
