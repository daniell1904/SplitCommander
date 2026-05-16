#include "gitmanagerdialog.h"
#include "config.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProcess>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

GitManagerDialog::GitManagerDialog(const QString &currentPath, QWidget *parent) 
    : QDialog(parent), m_gitPath(currentPath) {
  setWindowTitle(tr("SplitCommander GitHub Manager"));
  setMinimumSize(950, 850);
  resize(950, 850);
  
  setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
  
  buildUI();
  load();
  refreshGitStatus();
}

void GitManagerDialog::buildUI() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(30, 30, 30, 30);
  root->setSpacing(24);

  // Styles (Professional Desktop - 22px Nano)
  const QString labelStyle = QString("color:%1; font-size:12px; font-weight:bold;").arg(TM().colors().textPrimary);
  const QString inputStyle = QString("background:%1; color:%2; border:1px solid %3; border-radius:3px; padding:2px 8px; font-size:13px; min-height:22px;")
                                    .arg(TM().colors().bgInput, TM().colors().textPrimary, TM().colors().borderAlt);
  const QString btnBase = "QPushButton { border-radius:3px; padding:2px 10px; font-size:12px; font-weight:bold; min-height:22px; }";
  
  const QString gitNormBtnSS = btnBase + QString("QPushButton { background:%1; color:%2; border:1px solid %3; } QPushButton:hover { background:%4; }")
                                        .arg(TM().colors().bgPanel, TM().colors().textPrimary, TM().colors().borderAlt, TM().colors().bgHover);
  const QString gitPrimBtnSS = btnBase + QString("QPushButton { background:%1; color:%2; border:none; } QPushButton:hover { background:%3; }")
                                           .arg(TM().colors().accent, TM().colors().textLight, TM().colors().accentHover);

  auto addSeparator = [&](QVBoxLayout *l) {
      auto *line = new QFrame();
      line->setFrameShape(QFrame::HLine);
      line->setStyleSheet(QString("background: %1; max-height: 1px; margin: 10px 0;").arg(TM().colors().borderAlt));
      l->addWidget(line);
  };

  // 1. CONFIGURATION
  auto *formWrap = new QWidget();
  auto *form = new QFormLayout(formWrap);
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(12);
  form->setLabelAlignment(Qt::AlignRight);

  m_gitLocalDir = new QLineEdit(); m_gitLocalDir->setStyleSheet(inputStyle);
  auto *lblPath = new QLabel(tr("Local Path:")); lblPath->setStyleSheet(labelStyle);
  auto *pathRow = new QHBoxLayout();
  pathRow->addWidget(m_gitLocalDir, 1);
  auto *btnBrowse = new QPushButton(tr("Browse..."));
  btnBrowse->setStyleSheet(gitNormBtnSS);
  pathRow->addWidget(btnBrowse);
  form->addRow(lblPath, pathRow);

  m_gitRemoteUrl = new QLineEdit(); m_gitRemoteUrl->setStyleSheet(inputStyle);
  m_gitRemoteUrl->setPlaceholderText("https://github.com/user/repo.git");
  auto *lblRemote = new QLabel(tr("Remote URL:")); lblRemote->setStyleSheet(labelStyle);
  form->addRow(lblRemote, m_gitRemoteUrl);

  m_gitUsername = new QLineEdit(); m_gitUsername->setStyleSheet(inputStyle);
  auto *lblUser = new QLabel(tr("Username:")); lblUser->setStyleSheet(labelStyle);
  form->addRow(lblUser, m_gitUsername);

  m_gitToken = new QLineEdit(); m_gitToken->setEchoMode(QLineEdit::Password); m_gitToken->setStyleSheet(inputStyle);
  auto *lblToken = new QLabel(tr("Git Token (PAT):")); lblToken->setStyleSheet(labelStyle);
  auto *tokenRow = new QHBoxLayout();
  tokenRow->addWidget(m_gitToken, 1);
  auto *btnGenToken = new QPushButton(tr("Token generieren..."));
  btnGenToken->setStyleSheet(gitNormBtnSS);
  tokenRow->addWidget(btnGenToken);
  form->addRow(lblToken, tokenRow);

  root->addWidget(formWrap);
  addSeparator(root);

  // 2. STATUS & CHANGES
  auto *statusHeaderRow = new QHBoxLayout();
  auto *statusLbl = new QLabel(tr("Status & Änderungen"));
  statusLbl->setStyleSheet(labelStyle);
  m_gitBranchLabel = new QLabel(tr("Branch: ..."));
  m_gitBranchLabel->setStyleSheet("color: " + TM().colors().accent + "; font-size: 13px; font-weight: bold;");
  statusHeaderRow->addWidget(statusLbl);
  statusHeaderRow->addStretch();
  statusHeaderRow->addWidget(m_gitBranchLabel);
  root->addLayout(statusHeaderRow);

  m_gitStatusList = new QListWidget();
  m_gitStatusList->setStyleSheet(QString("background:%1; color:%2; border: 1px solid %3; border-radius: 3px;")
                                  .arg(TM().colors().bgInput, TM().colors().textPrimary, TM().colors().borderAlt));
  m_gitStatusList->setFixedHeight(200);
  root->addWidget(m_gitStatusList);

  m_gitCommitMsg = new QLineEdit();
  m_gitCommitMsg->setPlaceholderText(tr("Commit message..."));
  m_gitCommitMsg->setStyleSheet(inputStyle);
  root->addWidget(m_gitCommitMsg);

  // 3. ACTIONS
  auto *actionsGrid = new QGridLayout();
  actionsGrid->setSpacing(10);
  
  auto setupGitBtn = [&](const QString &t, bool prim = false, bool danger = false) {
      auto *b = new QPushButton(t);
      if (prim) b->setStyleSheet(gitPrimBtnSS);
      else if (danger) b->setStyleSheet(btnBase + "QPushButton { background:transparent; color:#ff6b6b; border:1px solid #ff6b6b; } QPushButton:hover { background:#ff6b6b; color:white; }");
      else b->setStyleSheet(gitNormBtnSS);
      return b;
  };

  auto *btnPush   = setupGitBtn(tr("Push"), true);
  auto *btnPull   = setupGitBtn(tr("Pull"));
  auto *btnMerge  = setupGitBtn(tr("Merge"));
  auto *btnLog    = setupGitBtn(tr("Log"));
  auto *btnDiff   = setupGitBtn(tr("Diff"));
  auto *btnPushTags = setupGitBtn(tr("Tags pushen"), true);
  auto *btnBranch = setupGitBtn(tr("Branch"));
  auto *btnStash  = setupGitBtn(tr("Stash"));
  auto *btnTag    = setupGitBtn(tr("Tag"));
  auto *btnDelTag = setupGitBtn(tr("Tag löschen"), false, true);
  auto *btnReset  = setupGitBtn(tr("Reset"), false, true);

  actionsGrid->addWidget(btnPush,   0, 0);
  actionsGrid->addWidget(btnPull,   0, 1);
  actionsGrid->addWidget(btnMerge,  0, 2);
  actionsGrid->addWidget(btnLog,    0, 3);
  actionsGrid->addWidget(btnDiff,   0, 4);
  
  actionsGrid->addWidget(btnBranch, 1, 0);
  actionsGrid->addWidget(btnStash,  1, 1);
  actionsGrid->addWidget(btnTag,    1, 2);
  actionsGrid->addWidget(btnDelTag, 1, 3);
  actionsGrid->addWidget(btnReset,  1, 4);
  actionsGrid->addWidget(btnPushTags, 2, 0);
  root->addLayout(actionsGrid);

  // 4. LOG
  auto *logLbl = new QLabel(tr("Prozess-Ausgabe"));
  logLbl->setStyleSheet(labelStyle);
  root->addWidget(logLbl);

  m_gitLog = new QTextEdit();
  m_gitLog->setReadOnly(true);
  m_gitLog->setFixedHeight(100);
  m_gitLog->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; font-family:monospace; font-size:12px; padding:8px; border-radius:3px;")
                           .arg(TM().colors().bgDeep, TM().colors().textPrimary, TM().colors().borderAlt));
  root->addWidget(m_gitLog);

  // FOOTER
  auto *footer = new QHBoxLayout();
  auto *btnClose = new QPushButton(tr("Schließen"));
  const QString footerBtnBase = "QPushButton { border-radius: 4px; padding: 4px 16px; font-size: 13px; font-weight: bold; min-height: 22px; }";
  btnClose->setStyleSheet(footerBtnBase + QString("QPushButton { background:%1; color:%2; border:1px solid %3; } QPushButton:hover { background:%4; }")
                      .arg(TM().colors().bgPanel, TM().colors().textPrimary, TM().colors().borderAlt, TM().colors().bgHover));
  footer->addStretch();
  footer->addWidget(btnClose);
  root->addLayout(footer);

  auto saveAndRefresh = [this]() {
      save();
      refreshGitStatus();
  };

  connect(m_gitLocalDir,  &QLineEdit::textEdited, this, saveAndRefresh);
  connect(m_gitRemoteUrl, &QLineEdit::textEdited, this, [this]() { save(); });
  connect(m_gitUsername,  &QLineEdit::textEdited, this, [this]() { save(); });
  connect(m_gitToken,     &QLineEdit::textEdited, this, [this]() { save(); });

  connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
  connect(btnBrowse, &QPushButton::clicked, this, [this]() {
      QString dir = QFileDialog::getExistingDirectory(this, tr("Git Repository auswählen"), m_gitLocalDir->text());
      if (!dir.isEmpty()) {
          m_gitLocalDir->setText(dir);
          m_gitPath = dir;
          save();
          refreshGitStatus();
      }
  });

  connect(btnPush, &QPushButton::clicked, this, [this]() {
      QString msg = m_gitCommitMsg->text().trimmed();
      if(msg.isEmpty()) msg = "Automatisches Speichern";
      runGitCommand({"add", "."});
      runGitCommand({"commit", "-m", msg});
      runGitCommand({"push"});
      m_gitCommitMsg->clear();
  });
  connect(btnPull, &QPushButton::clicked, this, [this]() { runGitCommand({"pull"}); });
  connect(btnReset, &QPushButton::clicked, this, [this]() {
      if (QMessageBox::warning(this, tr("Reset"), tr("Wirklich alle Änderungen verwerfen?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
          runGitCommand({"reset", "--hard", "origin/" + m_gitBranchLabel->text().section(": ", 1)});
  });
  connect(btnMerge, &QPushButton::clicked, this, [this]() {
      bool ok;
      QString branch = QInputDialog::getText(this, tr("Merge"), tr("Welcher Branch soll eingepflegt werden?"), QLineEdit::Normal, "", &ok);
      if (ok && !branch.isEmpty()) runGitCommand({"merge", branch});
  });
  connect(btnLog, &QPushButton::clicked, this, [this]() { runGitCommand({"log", "--oneline", "-n", "20"}); });
  connect(btnDiff, &QPushButton::clicked, this, [this]() { runGitCommand({"diff"}); });
  connect(btnStash, &QPushButton::clicked, this, [this]() { runGitCommand({"stash"}); });
  connect(btnBranch, &QPushButton::clicked, this, [this]() {
      bool ok;
      QString name = QInputDialog::getText(this, tr("Branch wechseln/erstellen"), tr("Branch Name:"), QLineEdit::Normal, "", &ok);
      if (ok && !name.isEmpty()) runGitCommand({"checkout", "-b", name});
  });
  connect(btnTag, &QPushButton::clicked, this, [this]() {
      bool ok;
      QString name = QInputDialog::getText(this, tr("Tag erstellen"), tr("Tag Name (z.B. v1.0.0):"), QLineEdit::Normal, "", &ok);
      if (ok && !name.isEmpty()) runGitCommand({"tag", name});
  });
  connect(btnDelTag, &QPushButton::clicked, this, [this]() {
      bool ok;
      QString name = QInputDialog::getText(this, tr("Tag löschen"), tr("Welcher Tag soll gelöscht werden?"), QLineEdit::Normal, "", &ok);
      if (ok && !name.isEmpty()) {
          if (QMessageBox::question(this, tr("Tag löschen"), tr("Soll der Tag '%1' wirklich gelöscht werden?").arg(name)) == QMessageBox::Yes) {
              runGitCommand({"tag", "-d", name});
          }
      }
  });
  connect(btnPushTags, &QPushButton::clicked, this, [this]() {
      runGitCommand({"push", "--tags"});
  });
  connect(btnGenToken, &QPushButton::clicked, this, [this]() {
      QString url = m_gitRemoteUrl->text();
      QString target = "https://github.com/settings/tokens";
      if (url.contains("gitlab")) target = "https://gitlab.com/-/profile/personal_access_tokens";
      QDesktopServices::openUrl(QUrl(target));
  });
}

void GitManagerDialog::load() {
  m_gitLocalDir->setText(Config::gitLocalDir());
  m_gitPath = Config::gitLocalDir();
  m_gitRemoteUrl->setText(Config::gitRemoteUrl());
  m_gitUsername->setText(Config::gitUsername());
  m_gitToken->setText(Config::gitToken());
}

void GitManagerDialog::save() {
  Config::setGitLocalDir(m_gitLocalDir->text());
  Config::setGitRemoteUrl(m_gitRemoteUrl->text());
  Config::setGitUsername(m_gitUsername->text());
  Config::setGitToken(m_gitToken->text());
}

void GitManagerDialog::runGitCommand(const QStringList &args) {
    if(m_gitPath.isEmpty()) return;
    
    QStringList finalArgs = args;
    QString token = Config::gitToken();
    QString remote = Config::gitRemoteUrl();
    
    // Wenn Token vorhanden und wir pushen/pullen, versuchen wir die URL mit Token zu nutzen
    if (!token.isEmpty() && !remote.isEmpty() && (args.contains("push") || args.contains("pull"))) {
        // GitHub URL mit Token: https://TOKEN@github.com/user/repo.git
        QString authUrl = remote;
        if (authUrl.startsWith("https://")) {
            authUrl.replace("https://", "https://" + token + "@");
            
            // Wir müssen den Remote-Namen finden (meist origin)
            if (args.contains("push")) {
                finalArgs.clear();
                finalArgs << "push" << authUrl;
                // Aktuellen Branch hinzufügen
                QString branch = m_gitBranchLabel->text().section(": ", 1);
                if (!branch.isEmpty() && branch != tr("Kein Repository")) finalArgs << branch;
            } else if (args.contains("pull")) {
                finalArgs.clear();
                finalArgs << "pull" << authUrl;
            }
        }
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0"); // Keine interaktive Abfrage
    proc.setProcessEnvironment(env);
    
    proc.setWorkingDirectory(m_gitPath);
    proc.start("git", finalArgs);
    proc.waitForFinished(30000); // 30s Timeout für Netzwerk
    
    m_gitLog->append("<b>> git " + args.join(" ") + "</b>");
    QString out = QString::fromUtf8(proc.readAllStandardOutput());
    QString err = QString::fromUtf8(proc.readAllStandardError());
    if(!out.isEmpty()) m_gitLog->append(out);
    if(!err.isEmpty()) m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
    refreshGitStatus();
}

void GitManagerDialog::refreshGitStatus() {
    if(m_gitPath.isEmpty()) return;
    QProcess bProc;
    bProc.setWorkingDirectory(m_gitPath);
    bProc.start("git", {"branch", "--show-current"});
    bProc.waitForFinished();
    QString branch = bProc.readAllStandardOutput().trimmed();
    if(branch.isEmpty()) branch = tr("Kein Repository");
    m_gitBranchLabel->setText(tr("Branch: %1").arg(branch));

    QProcess proc;
    proc.setWorkingDirectory(m_gitPath);
    proc.start("git", {"status", "--porcelain"});
    proc.waitForFinished();
    m_gitStatusList->clear();
    QString out = proc.readAllStandardOutput().trimmed();
    if(out.isEmpty()) {
        m_gitStatusList->addItem(tr("✓ Keine ungespeicherten Änderungen"));
        return;
    }
    QStringList lines = out.split("\n", Qt::SkipEmptyParts);
    for(const QString &line : lines) {
        if(line.length() > 3) {
            QString status = line.left(2);
            QString file = line.mid(3);
            auto *item = new QListWidgetItem(QString("[%1] %2").arg(status, file), m_gitStatusList);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
    }
}
