#include "gitmanagerdialog.h"
#include "config.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
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
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(4);

  // Styles (Professional Desktop - 22px Nano)
  const QString labelStyle = QString("color:%1; font-size:12px; font-weight:bold;").arg(TM().colors().textPrimary);
  const QString inputStyle = QString("background:%1; color:%2; border:1px solid %3; border-radius:3px; padding:2px 8px; font-size:13px; min-height:22px;")
                                    .arg(TM().colors().bgInput, TM().colors().textPrimary, TM().colors().borderAlt);
  const QString btnBase = "QPushButton { border-radius:3px; padding:2px 10px; font-size:12px; font-weight:bold; min-height:22px; }";
  
  const QString gitNormBtnSS = btnBase + QString("QPushButton { background:%1; color:%2; border:1px solid %3; } QPushButton:hover { background:%4; }")
                                        .arg(TM().colors().bgPanel, TM().colors().textPrimary, TM().colors().borderAlt, TM().colors().bgHover);
  const QString gitPrimBtnSS = btnBase + QString("QPushButton { background:%1; color:%2; border:none; } QPushButton:hover { background:%3; }")
                                           .arg(TM().colors().accent, TM().colors().textLight, TM().colors().accentHover);


  // --- COMPACT ACTION GRID ---
  auto addCompactBtn = [&](const QString &title, const QString &tooltip, const QString &style, std::function<void()> onClick) {
      auto *btn = new QPushButton(title);
      btn->setStyleSheet(style);
      btn->setMinimumHeight(32);
      btn->setToolTip(tooltip);
      connect(btn, &QPushButton::clicked, this, onClick);
      return btn;
  };

  // 1. STATUS
  auto *statusHeaderRow = new QHBoxLayout();
  auto *statusLbl = new QLabel(tr("<b>1. Was hat sich geändert?</b>"));
  statusLbl->setStyleSheet(labelStyle);
  m_gitBranchLabel = new QLabel(tr("Branch: ..."));
  m_gitBranchLabel->setStyleSheet("color: " + TM().colors().accent + "; font-size: 12px; font-weight: bold;");
  statusHeaderRow->addWidget(statusLbl);
  statusHeaderRow->addStretch();
  statusHeaderRow->addWidget(m_gitBranchLabel);
  root->addLayout(statusHeaderRow);

  m_gitStatusList = new QListWidget();
  m_gitStatusList->setStyleSheet(QString("background:%1; color:%2; border: 1px solid %3; border-radius: 3px;")
                                  .arg(TM().colors().bgInput, TM().colors().textPrimary, TM().colors().borderAlt));
  m_gitStatusList->setMaximumHeight(150);
  root->addWidget(m_gitStatusList);

  // 2. COMMIT MESSAGE
  auto *msgLbl = new QLabel(tr("<b>2. Kurze Beschreibung deiner Arbeit</b>"));
  msgLbl->setStyleSheet(labelStyle);
  root->addWidget(msgLbl);

  m_gitCommitMsg = new QLineEdit();
  m_gitCommitMsg->setPlaceholderText(tr("z.B. Fehler in der Suche behoben..."));
  m_gitCommitMsg->setStyleSheet(inputStyle);
  root->addWidget(m_gitCommitMsg);

  // 3. ACTIONS
  auto *actionLbl = new QLabel(tr("<b>3. Aktionen (Tipp: Maus über Button halten für Hilfe)</b>"));
  actionLbl->setStyleSheet(labelStyle);
  root->addWidget(actionLbl);

  auto *actionGrid = new QGridLayout();
  actionGrid->setSpacing(10);

  // Row 0
  actionGrid->addWidget(addCompactBtn(tr("Hochladen (Push)"), tr("Sendet deine Änderungen an GitHub, damit sie dort gespeichert sind."), gitPrimBtnSS, [this]() {
      QString msg = m_gitCommitMsg->text().trimmed();
      if(msg.isEmpty()) msg = "Automatisches Speichern";
      runGitCommand({"add", "."});
      runGitCommand({"commit", "-m", msg});
      runGitCommand({"push"});
      if (m_pushTagsOpt->isChecked()) runGitCommand({"push", "--tags"});
      if (m_createReleaseOpt->isChecked()) {
          bool ok;
          QString tag = QInputDialog::getText(this, tr("GitHub Release"), tr("Tag Name für das Release:"), QLineEdit::Normal, "", &ok);
          if (ok && !tag.isEmpty()) createGitHubRelease(tag);
      }
      m_gitCommitMsg->clear();
      QMessageBox::information(this, tr("Fertig"), tr("Deine Änderungen wurden erfolgreich hochgeladen!"));
  }), 0, 0);

  actionGrid->addWidget(addCompactBtn(tr("Aktualisieren (Pull)"), tr("Holt die neuesten Änderungen von GitHub auf deinen Computer."), gitNormBtnSS, [this]() {
      runGitCommand({"pull"});
  }), 0, 1);

  actionGrid->addWidget(addCompactBtn(tr("Unterschiede (Diff)"), tr("Zeigt dir genau an, welche Zeilen im Code du geändert hast."), gitNormBtnSS, [this]() {
      runGitCommand({"diff"});
  }), 0, 2);

  // Row 1 (The new ones)
  actionGrid->addWidget(addCompactBtn(tr("Verlauf (Log)"), tr("Zeigt eine Liste der letzten Änderungen und wer sie gemacht hat."), gitNormBtnSS, [this]() {
      runGitCommand({"log", "--oneline", "-n", "20"});
  }), 1, 0);

  actionGrid->addWidget(addCompactBtn(tr("Tag erstellen"), tr("Markiert den aktuellen Stand als eine feste Version (z.B. v1.0)."), gitNormBtnSS, [this]() {
      bool ok;
      QString name = QInputDialog::getText(this, tr("Tag erstellen"), tr("Name für den Tag:"), QLineEdit::Normal, "", &ok);
      if (ok && !name.isEmpty()) runGitCommand({"tag", name});
  }), 1, 1);

  actionGrid->addWidget(addCompactBtn(tr("Branch wechseln"), tr("Erstellt einen neuen Zweig für Experimente oder wechselt den aktuellen."), gitNormBtnSS, [this]() {
      bool ok;
      QString name = QInputDialog::getText(this, tr("Branch wechseln/erstellen"), tr("Name des Branch:"), QLineEdit::Normal, "", &ok);
      if (ok && !name.isEmpty()) runGitCommand({"checkout", "-b", name});
  }), 1, 2);

  // Row 2
  actionGrid->addWidget(addCompactBtn(tr("Stash (Parken)"), tr("Verschiebt deine aktuellen Änderungen in eine Warteschlange, um später daran weiterzuarbeiten."), gitNormBtnSS, [this]() {
      runGitCommand({"stash"});
  }), 2, 0);

  root->addLayout(actionGrid);

  // Options row
  auto *optRow = new QHBoxLayout();
  m_pushTagsOpt = new QCheckBox(tr("Tags (Versionen) mit hochladen"));
  m_pushTagsOpt->setStyleSheet("color: " + TM().colors().textPrimary + "; font-size: 11px;");
  m_createReleaseOpt = new QCheckBox(tr("Release auf GitHub erstellen"));
  m_createReleaseOpt->setStyleSheet("color: " + TM().colors().textPrimary + "; font-size: 11px;");
  optRow->addWidget(m_pushTagsOpt);
  optRow->addWidget(m_createReleaseOpt);
  optRow->addStretch();
  root->addLayout(optRow);

  // Separator for Advanced
  auto *line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setStyleSheet(QString("background: %1; max-height: 1px; margin: 10px 0;").arg(TM().colors().borderAlt));
  root->addWidget(line);

  // 4. ADVANCED / DANGER
  auto *advLbl = new QLabel(tr("<b>Erweiterte Funktionen</b>"));
  advLbl->setStyleSheet(labelStyle);
  root->addWidget(advLbl);

  addCompactBtn(tr("Alles Verwerfen"), tr("Löscht ALLE deine ungespeicherten Änderungen und setzt alles auf den Stand von GitHub zurück. Vorsicht!"), btnBase + "QPushButton { color:#ff6b6b; border:1px solid #ff6b6b; }", [this]() {
      if (QMessageBox::critical(this, tr("Vorsicht"), tr("Willst du wirklich alle deine Änderungen unwiderruflich löschen?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
          runGitCommand({"reset", "--hard", "HEAD"});
  });

  // 5. CONFIGURATION
  auto *cfgLbl = new QLabel(tr("<b>Verbindungseinstellungen</b>"));
  cfgLbl->setStyleSheet(labelStyle);
  root->addWidget(cfgLbl);

  auto *formWrap = new QWidget();
  auto *form = new QFormLayout(formWrap);
  form->setSpacing(6);
  m_gitLocalDir = new QLineEdit(); m_gitLocalDir->setStyleSheet(inputStyle);
  auto *btnBrowse = new QPushButton(tr("Durchsuchen...")); btnBrowse->setStyleSheet(gitNormBtnSS);
  auto *pathRow = new QHBoxLayout(); pathRow->addWidget(m_gitLocalDir, 1); pathRow->addWidget(btnBrowse);
  form->addRow(tr("Projekt-Ordner:"), pathRow);
  
  m_gitRemoteUrl = new QLineEdit(); m_gitRemoteUrl->setStyleSheet(inputStyle);
  form->addRow(tr("GitHub Link:"), m_gitRemoteUrl);
  
  m_gitUsername = new QLineEdit(); m_gitUsername->setStyleSheet(inputStyle);
  form->addRow(tr("GitHub Name:"), m_gitUsername);
  
  m_gitToken = new QLineEdit(); m_gitToken->setEchoMode(QLineEdit::Password); m_gitToken->setStyleSheet(inputStyle);
  form->addRow(tr("Passwort/Token:"), m_gitToken);
  
  auto *btnGenToken = new QPushButton(tr("Token generieren..."));
  btnGenToken->setStyleSheet(gitNormBtnSS);
  form->addRow(tr("Hilfe:"), btnGenToken);
  root->addWidget(formWrap);

  connect(btnBrowse, &QPushButton::clicked, this, [this]() {
      QString dir = QFileDialog::getExistingDirectory(this, tr("Ordner wählen"), m_gitLocalDir->text());
      if (!dir.isEmpty()) { m_gitLocalDir->setText(dir); m_gitPath = dir; save(); refreshGitStatus(); }
  });
  connect(btnGenToken, &QPushButton::clicked, []() {
      QDesktopServices::openUrl(QUrl("https://github.com/settings/tokens/new"));
  });

  auto *line2 = new QFrame();
  line2->setFrameShape(QFrame::HLine);
  line2->setStyleSheet(QString("background: %1; max-height: 1px; margin: 10px 0;").arg(TM().colors().borderAlt));
  root->addWidget(line2);

  // 6. LOG
  auto *logLbl = new QLabel(tr("<b>Prozess-Ausgabe (Log)</b>"));
  logLbl->setStyleSheet(labelStyle);
  root->addWidget(logLbl);

  m_gitLog = new QTextEdit();
  m_gitLog->setReadOnly(true);
  m_gitLog->setMinimumHeight(150);
  m_gitLog->setStyleSheet(QString("background:%1; color:%2; border:2px solid %3; font-family:monospace; font-size:11px; padding:6px; border-radius:4px;")
                           .arg(TM().colors().bgDeep, TM().colors().textPrimary, TM().colors().borderAlt));
  m_gitLog->append("<span style='color:#50fa7b;'>✓ Git Manager bereit.</span>");
  root->addWidget(m_gitLog, 1); // Nimmt den restlichen Platz ein

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
}

void GitManagerDialog::load() {
  m_gitLocalDir->setText(Config::gitLocalDir());
  m_gitPath = Config::gitLocalDir();
  m_gitRemoteUrl->setText(Config::gitRemoteUrl());
  m_gitUsername->setText(Config::gitUsername());
  m_gitToken->setText(Config::gitToken());
}

void GitManagerDialog::save() {
  QString path = m_gitLocalDir->text().trimmed();
  Config::setGitLocalDir(path);
  m_gitPath = path; // WICHTIG: Pfad synchronisieren!
  Config::setGitRemoteUrl(m_gitRemoteUrl->text().trimmed());
  Config::setGitUsername(m_gitUsername->text().trimmed());
  Config::setGitToken(m_gitToken->text().trimmed());
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
    
    // Auto-scroll to bottom
    m_gitLog->ensureCursorVisible();
    
    refreshGitStatus();
}

void GitManagerDialog::refreshGitStatus() {
    if(m_gitPath.isEmpty()) {
        m_gitLog->append("<span style='color:#ffaa00;'>Warnung: Kein Projekt-Ordner festgelegt.</span>");
        return;
    }
    
    // Check branch
    QProcess bProc;
    bProc.setWorkingDirectory(m_gitPath);
    bProc.start("git", {"branch", "--show-current"});
    if (!bProc.waitForFinished(5000)) {
        m_gitBranchLabel->setText(tr("Branch: Fehler"));
    } else {
        QString branch = bProc.readAllStandardOutput().trimmed();
        if(branch.isEmpty()) branch = tr("Kein Repository");
        m_gitBranchLabel->setText(tr("Branch: %1").arg(branch));
    }

    // Check status
    QProcess proc;
    proc.setWorkingDirectory(m_gitPath);
    proc.start("git", {"status", "--porcelain"});
    if (!proc.waitForFinished(5000)) {
        m_gitLog->append("<span style='color:#ff6b6b;'>Fehler: 'git status' konnte nicht ausgeführt werden (Timeout).</span>");
        return;
    }

    QString out = proc.readAllStandardOutput().trimmed();
    QString err = proc.readAllStandardError().trimmed();
    
    if (!err.isEmpty()) {
        m_gitLog->append("<span style='color:#ff6b6b;'>Git Fehler: " + err + "</span>");
    }

    m_gitStatusList->clear();
    if(out.isEmpty()) {
        m_gitStatusList->addItem(tr("✓ Keine ungespeicherten Änderungen"));
        // Debug info in log
        m_gitLog->append("<i>Status-Check in: " + m_gitPath + " (Alles aktuell)</i>");
        return;
    }

    QStringList lines = out.split("\n", Qt::SkipEmptyParts);
    for(const QString &line : lines) {
        if(line.length() >= 3) {
            QString status = line.left(2);
            QString file = line.mid(3);
            auto *item = new QListWidgetItem(QString("[%1] %2").arg(status, file), m_gitStatusList);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState(Qt::Checked); // Standardmäßig ausgewählt
        }
    }
    m_gitLog->append("<i>Änderungen in " + m_gitPath + " gefunden.</i>");
}

void GitManagerDialog::createGitHubRelease(const QString &tag) {
    QString token = Config::gitToken();
    QString remote = Config::gitRemoteUrl();
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Fehler"), tr("Kein Git Token hinterlegt!"));
        return;
    }
    
    // Parse Owner/Repo: https://github.com/OWNER/REPO.git
    QString repoPath = remote;
    repoPath.remove("https://github.com/");
    repoPath.remove(".git");
    
    QString json = QString("{\"tag_name\":\"%1\",\"name\":\"%1\",\"body\":\"Release %1\",\"draft\":false,\"prerelease\":false}").arg(tag);
    
    QProcess *proc = new QProcess(this);
    QStringList args;
    args << "-L" << "-X" << "POST" 
         << "-H" << "Accept: application/vnd.github+json"
         << "-H" << QString("Authorization: Bearer %1").arg(token)
         << "-H" << "X-GitHub-Api-Version: 2022-11-28"
         << QString("https://api.github.com/repos/%1/releases").arg(repoPath)
         << "-d" << json;
         
    m_gitLog->append("<b>> GitHub API: Creating Release " + tag + "...</b>");
    proc->start("curl", args);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, proc, tag]() {
        m_gitLog->append(QString::fromUtf8(proc->readAllStandardOutput()));
        QString err = QString::fromUtf8(proc->readAllStandardError());
        if (!err.isEmpty()) m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
        m_gitLog->append("<b>✓ Release " + tag + " erstellt!</b>");
        proc->deleteLater();
    });
}
