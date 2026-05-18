#include "gitmanagerdialog.h"
#include "config.h"
#include "thememanager.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// ─── Hilfsfunktion: Release-Dialog ──────────────────────────────────────────
static bool showReleaseDialog(QWidget *parent,
                              QString &outTag, QString &outTitle,
                              QString &outBody, bool &outLatest,
                              bool &outPrerelease,
                              const ThemeManager &tm)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("GitHub Release erstellen"));
    dlg.setMinimumWidth(460);
    dlg.setStyleSheet(tm.ssDialog());

    auto *lay = new QVBoxLayout(&dlg);
    lay->setSpacing(8);
    lay->setContentsMargins(16, 16, 16, 16);

    const QString inputSS = QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:2px 8px; font-size:13px; min-height:22px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().textPrimary);
    const QString checkSS = QString("color:%1; font-size:12px;").arg(tm.colors().textPrimary);

    auto addLabel = [&](const QString &text) {
        auto *l = new QLabel(text, &dlg);
        l->setStyleSheet(labelSS);
        lay->addWidget(l);
    };

    addLabel(QObject::tr("Tag-Name (z.B. v1.0.0):"));
    auto *tagEdit = new QLineEdit(&dlg);
    tagEdit->setStyleSheet(inputSS);
    tagEdit->setPlaceholderText("v1.0.0");
    lay->addWidget(tagEdit);

    addLabel(QObject::tr("Release-Titel:"));
    auto *titleEdit = new QLineEdit(&dlg);
    titleEdit->setStyleSheet(inputSS);
    titleEdit->setPlaceholderText(QObject::tr("z.B. Version 1.0.0"));
    lay->addWidget(titleEdit);

    addLabel(QObject::tr("Beschreibung / Changelog:"));
    auto *bodyEdit = new QTextEdit(&dlg);
    bodyEdit->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "font-size:13px; padding:4px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt));
    bodyEdit->setMinimumHeight(100);
    bodyEdit->setPlaceholderText(QObject::tr("Was ist neu in dieser Version?"));
    lay->addWidget(bodyEdit);

    auto *latestCheck = new QCheckBox(QObject::tr("Als \"Latest\" markieren"), &dlg);
    latestCheck->setStyleSheet(checkSS);
    latestCheck->setChecked(true);
    lay->addWidget(latestCheck);

    auto *prereleaseCheck = new QCheckBox(QObject::tr("Pre-release"), &dlg);
    prereleaseCheck->setStyleSheet(checkSS);
    lay->addWidget(prereleaseCheck);

    auto *btnRow = new QHBoxLayout();
    const QString btnSS = QString(
        "QPushButton { border-radius:3px; padding:4px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:none; }"
        "QPushButton:hover { background:%3; }")
        .arg(tm.colors().accent, tm.colors().textLight, tm.colors().accentHover);
    const QString cancelSS = QString(
        "QPushButton { border-radius:3px; padding:4px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:1px solid %3; }"
        "QPushButton:hover { background:%4; }")
        .arg(tm.colors().bgPanel, tm.colors().textPrimary,
             tm.colors().borderAlt, tm.colors().bgHover);
    auto *okBtn = new QPushButton(QObject::tr("Release erstellen"), &dlg);
    okBtn->setStyleSheet(btnSS);
    auto *cancelBtn = new QPushButton(QObject::tr("Abbrechen"), &dlg);
    cancelBtn->setStyleSheet(cancelSS);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    QObject::connect(okBtn,     &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;
    outTag        = tagEdit->text().trimmed();
    outTitle      = titleEdit->text().trimmed();
    outBody       = bodyEdit->toPlainText().trimmed();
    outLatest     = latestCheck->isChecked();
    outPrerelease = prereleaseCheck->isChecked();
    return !outTag.isEmpty();
}

// ─── Konstruktor ─────────────────────────────────────────────────────────────
GitManagerDialog::GitManagerDialog(const QString &currentPath, QWidget *parent)
    : QDialog(parent), m_gitPath(currentPath)
{
    setWindowTitle(tr("Git Manager"));
    setMinimumSize(860, 780);
    resize(900, 820);
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    buildUI();
    load();
    refreshGitStatus();
}

// ─── UI aufbauen ─────────────────────────────────────────────────────────────
void GitManagerDialog::buildUI()
{
    const ThemeManager &tm = TM();
    const QString inputSS = QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:2px 8px; font-size:13px; min-height:22px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().textPrimary);
    const QString checkSS = QString("color:%1; font-size:12px;").arg(tm.colors().textPrimary);
    const QString sepSS   = QString("background:%1; max-height:1px; margin:8px 0;")
        .arg(tm.colors().borderAlt);

    // Primär-Button (Commit & Push)
    const QString primBtnSS = QString(
        "QPushButton { border-radius:3px; padding:3px 16px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:none; }"
        "QPushButton:hover { background:%3; }"
        "QPushButton:disabled { background:%4; color:%5; }")
        .arg(tm.colors().accent, tm.colors().textLight, tm.colors().accentHover,
             tm.colors().bgPanel, tm.colors().borderAlt);

    // Normal-Button
    const QString normBtnSS = QString(
        "QPushButton { border-radius:3px; padding:3px 12px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:%1; color:%2; border:1px solid %3; }"
        "QPushButton:hover { background:%4; }"
        "QPushButton:disabled { color:%5; }")
        .arg(tm.colors().bgPanel, tm.colors().textPrimary,
             tm.colors().borderAlt, tm.colors().bgHover, tm.colors().borderAlt);

    // Danger-Button (Verwerfen)
    const QString dangerBtnSS = QString(
        "QPushButton { border-radius:3px; padding:3px 12px; font-size:12px; "
        "font-weight:bold; min-height:26px; background:transparent; "
        "color:#ff6b6b; border:1px solid #ff6b6b; }"
        "QPushButton:hover { background:#ff6b6b22; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(6);

    // ── Repo-Auswahl ──────────────────────────────────────────────────────────
    auto *repoRow = new QHBoxLayout();
    auto *repoLbl = new QLabel(tr("Repository:"), this);
    repoLbl->setStyleSheet(labelSS);
    m_repoCombo = new QComboBox(this);
    m_repoCombo->setStyleSheet(inputSS);
    auto *btnNewRepo = new QPushButton(tr("Neu"), this);
    auto *btnDelRepo = new QPushButton(tr("Entfernen"), this);
    btnNewRepo->setStyleSheet(normBtnSS);
    btnDelRepo->setStyleSheet(normBtnSS);
    btnNewRepo->setMinimumHeight(28);
    btnDelRepo->setMinimumHeight(28);
    repoRow->addWidget(repoLbl);
    repoRow->addWidget(m_repoCombo, 1);
    repoRow->addWidget(btnNewRepo);
    repoRow->addWidget(btnDelRepo);
    root->addLayout(repoRow);

    // ── Branch + Status ───────────────────────────────────────────────────────
    auto *statusHeaderRow = new QHBoxLayout();
    auto *statusLbl = new QLabel(tr("Geänderte Dateien:"), this);
    statusLbl->setStyleSheet(labelSS);
    m_gitBranchLabel = new QLabel(tr("Branch: …"), this);
    m_gitBranchLabel->setStyleSheet(QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().accent));
    statusHeaderRow->addWidget(statusLbl);
    statusHeaderRow->addStretch();
    statusHeaderRow->addWidget(m_gitBranchLabel);
    root->addLayout(statusHeaderRow);

    m_gitStatusList = new QListWidget(this);
    m_gitStatusList->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt));
    m_gitStatusList->setMaximumHeight(130);
    root->addWidget(m_gitStatusList);

    // ── Commit-Nachricht ──────────────────────────────────────────────────────
    auto *msgLbl = new QLabel(tr("Beschreibung der Änderungen:"), this);
    msgLbl->setStyleSheet(labelSS);
    root->addWidget(msgLbl);

    m_gitCommitMsg = new QLineEdit(this);
    m_gitCommitMsg->setPlaceholderText(tr("z.B. Fehler in der Suche behoben..."));
    m_gitCommitMsg->setStyleSheet(inputSS);
    root->addWidget(m_gitCommitMsg);

    // ── Hauptaktionen ─────────────────────────────────────────────────────────
    auto *mainActRow = new QHBoxLayout();
    mainActRow->setSpacing(8);

    auto *btnPush = new QPushButton(tr("Commit && Push"), this);
    btnPush->setStyleSheet(primBtnSS);
    btnPush->setToolTip(tr("Speichert deine Änderungen und lädt sie zu GitHub hoch."));

    auto *btnFetch = new QPushButton(tr("Fetch"), this);
    btnFetch->setStyleSheet(normBtnSS);
    btnFetch->setToolTip(tr("Prüft ob es neue Änderungen auf GitHub gibt, ohne sie herunterzuladen."));

    auto *btnPull = new QPushButton(tr("Pull"), this);
    btnPull->setStyleSheet(normBtnSS);
    btnPull->setToolTip(tr("Holt die neuesten Änderungen von GitHub."));

    auto *btnDiscard = new QPushButton(tr("Änderungen verwerfen"), this);
    btnDiscard->setStyleSheet(dangerBtnSS);
    btnDiscard->setToolTip(tr("Setzt alle lokalen Änderungen auf den Stand von GitHub zurück."));

    mainActRow->addWidget(btnPush, 1);
    mainActRow->addWidget(btnFetch, 1);
    mainActRow->addWidget(btnPull, 1);
    mainActRow->addWidget(btnDiscard, 1);
    root->addLayout(mainActRow);

    // ── Push-Optionen ─────────────────────────────────────────────────────────
    auto *pushOptBox = new QGroupBox(tr("Optionen für Commit && Push"), this);
    pushOptBox->setStyleSheet(QString(
        "QGroupBox { color:%1; font-size:11px; border:1px solid %2; "
        "border-radius:4px; margin-top:6px; padding-top:4px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; }")
        .arg(tm.colors().textAccent, tm.colors().borderAlt));
    auto *pushOptLay = new QHBoxLayout(pushOptBox);
    pushOptLay->setContentsMargins(8, 6, 8, 6);
    pushOptLay->setSpacing(16);

    m_optPushTags      = new QCheckBox(tr("Tags mit hochladen"), pushOptBox);
    m_optCreateRelease = new QCheckBox(tr("Release auf GitHub erstellen"), pushOptBox);
    m_optPushTags->setStyleSheet(checkSS);
    m_optCreateRelease->setStyleSheet(checkSS);
    pushOptLay->addWidget(m_optPushTags);
    pushOptLay->addWidget(m_optCreateRelease);
    pushOptLay->addStretch();
    root->addWidget(pushOptBox);

    // ── Pull-Optionen ─────────────────────────────────────────────────────────
    auto *pullOptBox = new QGroupBox(tr("Optionen für Pull"), this);
    pullOptBox->setStyleSheet(pushOptBox->styleSheet());
    auto *pullOptLay = new QHBoxLayout(pullOptBox);
    pullOptLay->setContentsMargins(8, 6, 8, 6);

    m_optPullRebase = new QCheckBox(tr("Rebase statt Merge"), pullOptBox);
    m_optPullRebase->setStyleSheet(checkSS);
    m_optPullRebase->setToolTip(tr("git pull --rebase — hält den Verlauf sauber."));
    pullOptLay->addWidget(m_optPullRebase);
    pullOptLay->addStretch();
    root->addWidget(pullOptBox);

    // ── Trennlinie + Erweitert ────────────────────────────────────────────────
    auto *sepAdv = new QFrame(this);
    sepAdv->setFrameShape(QFrame::HLine);
    sepAdv->setStyleSheet(sepSS);
    root->addWidget(sepAdv);

    auto *advToggleBtn = new QToolButton(this);
    advToggleBtn->setText(tr("▶  Erweiterte Funktionen"));
    advToggleBtn->setCheckable(true);
    advToggleBtn->setChecked(false);
    advToggleBtn->setStyleSheet(QString(
        "QToolButton { background:transparent; border:none; color:%1; "
        "font-size:12px; font-weight:bold; }"
        "QToolButton:hover { color:%2; }")
        .arg(tm.colors().textAccent, tm.colors().accent));
    root->addWidget(advToggleBtn);

    m_advancedWidget = new QWidget(this);
    m_advancedWidget->setVisible(false);
    auto *advLay = new QGridLayout(m_advancedWidget);
    advLay->setSpacing(8);
    advLay->setContentsMargins(0, 4, 0, 0);

    auto makeAdvBtn = [&](const QString &label, const QString &tip) {
        auto *b = new QPushButton(label, m_advancedWidget);
        b->setStyleSheet(normBtnSS);
        b->setToolTip(tip);
        return b;
    };

    auto *btnLog    = makeAdvBtn(tr("Verlauf (Log)"),
                                 tr("Zeigt die letzten 20 Commits."));
    auto *btnDiff   = makeAdvBtn(tr("Diff"),
                                 tr("Zeigt welche Zeilen du geändert hast."));
    auto *btnTag    = makeAdvBtn(tr("Tag erstellen"),
                                 tr("Markiert den aktuellen Stand als Version, z.B. v1.0."));
    auto *btnBranch = makeAdvBtn(tr("Branch wechseln / erstellen"),
                                 tr("Wechselt zu einem anderen Zweig oder erstellt einen neuen."));
    auto *btnMerge  = makeAdvBtn(tr("Merge"),
                                 tr("Führt einen anderen Branch in den aktuellen zusammen."));
    auto *btnRevert = makeAdvBtn(tr("Revert"),
                                 tr("Macht einen bestimmten Commit rückgängig — History bleibt erhalten."));
    auto *btnStash  = makeAdvBtn(tr("Stash (Parken)"),
                                 tr("Legt deine Änderungen zur Seite ohne sie zu speichern."));
    auto *btnStashPop = makeAdvBtn(tr("Stash anwenden"),
                                   tr("Holt die zuletzt geparkten Änderungen zurück."));

    advLay->addWidget(btnLog,      0, 0);
    advLay->addWidget(btnDiff,     0, 1);
    advLay->addWidget(btnTag,      0, 2);
    advLay->addWidget(btnBranch,   1, 0);
    advLay->addWidget(btnMerge,    1, 1);
    advLay->addWidget(btnRevert,   1, 2);
    advLay->addWidget(btnStash,    2, 0);
    advLay->addWidget(btnStashPop, 2, 1);
    root->addWidget(m_advancedWidget);

    // ── Trennlinie + Verbindung ───────────────────────────────────────────────
    auto *sepCfg = new QFrame(this);
    sepCfg->setFrameShape(QFrame::HLine);
    sepCfg->setStyleSheet(sepSS);
    root->addWidget(sepCfg);

    auto *cfgToggleBtn = new QToolButton(this);
    cfgToggleBtn->setText(tr("▶  Verbindungseinstellungen"));
    cfgToggleBtn->setCheckable(true);
    cfgToggleBtn->setChecked(false);
    cfgToggleBtn->setStyleSheet(advToggleBtn->styleSheet());
    root->addWidget(cfgToggleBtn);

    auto *cfgWidget = new QWidget(this);
    cfgWidget->setVisible(false);
    auto *form = new QFormLayout(cfgWidget);
    form->setSpacing(6);
    form->setContentsMargins(0, 4, 0, 0);

    m_gitRepoName = new QLineEdit(cfgWidget);
    m_gitRepoName->setStyleSheet(inputSS);
    m_gitRepoName->setPlaceholderText(tr("z.B. SplitCommander"));
    form->addRow(tr("Name:"), m_gitRepoName);

    m_gitLocalDir = new QLineEdit(cfgWidget);
    m_gitLocalDir->setStyleSheet(inputSS);
    auto *btnBrowse = new QPushButton(tr("Durchsuchen..."), cfgWidget);
    btnBrowse->setStyleSheet(normBtnSS);
    btnBrowse->setMinimumHeight(28);
    auto *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_gitLocalDir, 1);
    pathRow->addWidget(btnBrowse);
    form->addRow(tr("Projekt-Ordner:"), pathRow);

    m_gitRemoteUrl = new QLineEdit(cfgWidget);
    m_gitRemoteUrl->setStyleSheet(inputSS);
    m_gitRemoteUrl->setPlaceholderText("https://github.com/user/repo.git");
    form->addRow(tr("GitHub URL:"), m_gitRemoteUrl);

    m_gitUsername = new QLineEdit(cfgWidget);
    m_gitUsername->setStyleSheet(inputSS);
    form->addRow(tr("GitHub Benutzername:"), m_gitUsername);

    m_gitToken = new QLineEdit(cfgWidget);
    m_gitToken->setEchoMode(QLineEdit::Password);
    m_gitToken->setStyleSheet(inputSS);
    auto *btnGenToken = new QPushButton(tr("Token generieren..."), cfgWidget);
    btnGenToken->setStyleSheet(normBtnSS);
    btnGenToken->setMinimumHeight(28);
    auto *tokenRow = new QHBoxLayout();
    tokenRow->addWidget(m_gitToken, 1);
    tokenRow->addWidget(btnGenToken);
    form->addRow(tr("Token / Passwort:"), tokenRow);

    auto *btnClone = new QPushButton(tr("Repository klonen"), cfgWidget);
    btnClone->setStyleSheet(normBtnSS);
    btnClone->setMinimumHeight(28);
    btnClone->setToolTip(tr("Klont das eingetragene Remote-Repository in den Projekt-Ordner."));
    form->addRow(tr("Klonen:"), btnClone);

    root->addWidget(cfgWidget);

    // ── Log-Ausgabe ───────────────────────────────────────────────────────────
    auto *sepLog = new QFrame(this);
    sepLog->setFrameShape(QFrame::HLine);
    sepLog->setStyleSheet(sepSS);
    root->addWidget(sepLog);

    auto *logLbl = new QLabel(tr("Ausgabe:"), this);
    logLbl->setStyleSheet(labelSS);
    root->addWidget(logLbl);

    m_gitLog = new QTextEdit(this);
    m_gitLog->setReadOnly(true);
    m_gitLog->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; "
        "font-family:monospace; font-size:11px; padding:6px; border-radius:4px;")
        .arg(tm.colors().bgDeep, tm.colors().textPrimary, tm.colors().borderAlt));
    m_gitLog->append("<span style='color:#50fa7b;'>✓ Git Manager bereit.</span>");
    root->addWidget(m_gitLog, 1);

    // ── Footer ────────────────────────────────────────────────────────────────
    auto *footer = new QHBoxLayout();
    auto *btnClose = new QPushButton(tr("Schließen"), this);
    btnClose->setStyleSheet(normBtnSS);
    footer->addStretch();
    footer->addWidget(btnClose);
    root->addLayout(footer);

    // ── Signals ───────────────────────────────────────────────────────────────
    connect(advToggleBtn, &QToolButton::toggled, this, [this, advToggleBtn](bool on) {
        m_advancedWidget->setVisible(on);
        advToggleBtn->setText((on ? tr("▼  Erweiterte Funktionen")
                                  : tr("▶  Erweiterte Funktionen")));
    });
    connect(cfgToggleBtn, &QToolButton::toggled, this, [cfgWidget, cfgToggleBtn](bool on) {
        cfgWidget->setVisible(on);
        cfgToggleBtn->setText((on ? QObject::tr("▼  Verbindungseinstellungen")
                                  : QObject::tr("▶  Verbindungseinstellungen")));
    });

    connect(btnPush,    &QPushButton::clicked, this, &GitManagerDialog::doCommitPush);
    connect(btnFetch, &QPushButton::clicked, this,
            [this]() { runGitCommand({"fetch", "--all"}); });
    connect(btnPull,    &QPushButton::clicked, this, &GitManagerDialog::doPull);
    connect(btnDiscard, &QPushButton::clicked, this, &GitManagerDialog::doDiscard);

    connect(btnLog,   &QPushButton::clicked, this,
            [this]() { runGitCommand({"log", "--oneline", "-n", "20"}); });
    connect(btnDiff,  &QPushButton::clicked, this,
            [this]() { runGitCommand({"diff"}); });
    connect(btnTag,   &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Tag erstellen"),
                                             tr("Tag-Name (z.B. v1.0.0):"),
                                             QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty())
            runGitCommand({"tag", name.trimmed()});
    });
    connect(btnBranch, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Branch"),
                                             tr("Name des Branch:"),
                                             QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty())
            runGitCommand({"checkout", "-b", name.trimmed()});
    });
    connect(btnMerge, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Merge"),
                                             tr("Branch-Name der zusammengeführt werden soll:"),
                                             QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty())
            runGitCommand({"merge", name.trimmed()});
    });
    connect(btnRevert, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString hash = QInputDialog::getText(this, tr("Revert"),
                                             tr("Commit-Hash (z.B. a1b2c3d):"),
                                             QLineEdit::Normal, QString(), &ok);
        if (ok && !hash.trimmed().isEmpty())
            runGitCommand({"revert", "--no-edit", hash.trimmed()});
    });
    connect(btnStash,    &QPushButton::clicked, this,
            [this]() { runGitCommand({"stash"}); });
    connect(btnStashPop, &QPushButton::clicked, this,
            [this]() { runGitCommand({"stash", "pop"}); });

    connect(btnClone, &QPushButton::clicked, this, [this]() {
        const QString url = m_gitRemoteUrl->text().trimmed();
        const QString dir = m_gitLocalDir->text().trimmed();
        if (url.isEmpty() || dir.isEmpty()) {
            QMessageBox::warning(this, tr("Hinweis"),
                                 tr("Bitte GitHub URL und Projekt-Ordner eintragen."));
            return;
        }
        QProcess proc;
        proc.setWorkingDirectory(QFileInfo(dir).absolutePath());
        proc.start("git", {"clone", url, dir});
        proc.waitForFinished(60000);
        m_gitLog->append("<b>> git clone " + url + "</b>");
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QString err = QString::fromUtf8(proc.readAllStandardError());
        if (!out.isEmpty()) m_gitLog->append(out);
        if (!err.isEmpty())
            m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
        refreshGitStatus();
    });

    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Projekt-Ordner wählen"), m_gitLocalDir->text());
        if (!dir.isEmpty()) {
            m_gitLocalDir->setText(dir);
            m_gitPath = dir;
            save();
            refreshGitStatus();
        }
    });
    connect(btnGenToken, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/settings/tokens/new"));
    });

    connect(btnNewRepo, &QPushButton::clicked, this, [this]() {
        Config::GitRepo r;
        r.name = tr("Neues Repo");
        m_repos.append(r);
        Config::setGitRepos(m_repos);
        rebuildRepoCombo();
        m_repoCombo->setCurrentIndex(m_repos.size() - 1);
        emit settingsChanged();
    });
    connect(btnDelRepo, &QPushButton::clicked, this, [this]() {
        if (m_currentRepo < 0 || m_currentRepo >= m_repos.size()) return;
        m_repos.removeAt(m_currentRepo);
        Config::setGitRepos(m_repos);
        rebuildRepoCombo();
        if (m_repos.isEmpty()) {
            m_blockSave = true;
            m_gitRepoName->clear(); m_gitLocalDir->clear();
            m_gitRemoteUrl->clear(); m_gitUsername->clear();
            m_gitPath.clear(); m_currentRepo = -1;
            m_blockSave = false;
        } else {
            m_repoCombo->setCurrentIndex(0);
        }
        emit settingsChanged();
    });

    auto saveAndRefresh = [this]() { save(); refreshGitStatus(); };
    connect(m_gitRepoName, &QLineEdit::textEdited, this, [this]() {
        if (m_blockSave) return;
        saveCurrentFieldsToRepo();
        if (m_currentRepo >= 0 && m_currentRepo < m_repos.size()) {
            const QString n = m_gitRepoName->text().trimmed();
            m_repoCombo->blockSignals(true);
            m_repoCombo->setItemText(m_currentRepo, n.isEmpty() ? tr("(unbenannt)") : n);
            m_repoCombo->blockSignals(false);
        }
        save();
    });
    connect(m_gitLocalDir,  &QLineEdit::textEdited, this, [this, saveAndRefresh]() {
        if (m_blockSave) return;
        saveCurrentFieldsToRepo();
        saveAndRefresh();
    });
    connect(m_gitRemoteUrl, &QLineEdit::textEdited, this, [this]() {
        if (m_blockSave) return;
        saveCurrentFieldsToRepo();
        save();
    });
    connect(m_gitUsername,  &QLineEdit::textEdited, this, [this]() {
        if (m_blockSave) return;
        saveCurrentFieldsToRepo();
        save();
    });
    connect(m_gitToken, &QLineEdit::textEdited, this, [this]() { save(); });
    connect(m_repoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (m_blockSave) return;
        loadRepoToFields(idx);
        refreshGitStatus();
    });

    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
}

// ─── Commit & Push ────────────────────────────────────────────────────────────
void GitManagerDialog::doCommitPush()
{
    QString msg = m_gitCommitMsg->text().trimmed();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, tr("Hinweis"),
                             tr("Bitte gib eine Beschreibung ein."));
        return;
    }

    // Release-Dialog zuerst — bevor wir pushen
    QString relTag, relTitle, relBody;
    bool relLatest = true, relPrerelease = false;
    if (m_optCreateRelease->isChecked()) {
        if (!showReleaseDialog(this, relTag, relTitle, relBody,
                               relLatest, relPrerelease, TM())) {
            return; // Nutzer hat Abbrechen gedrückt
        }
    }

    runGitCommand({"add", "."});
    runGitCommand({"commit", "-m", msg});
    runGitCommand({"push"});

    if (m_optPushTags->isChecked())
        runGitCommand({"push", "--tags"});

    if (m_optCreateRelease->isChecked() && !relTag.isEmpty())
        createGitHubRelease(relTag, relTitle, relBody, relLatest, relPrerelease);

    m_gitCommitMsg->clear();
    m_gitLog->append("<span style='color:#50fa7b;'>✓ Fertig!</span>");
}

// ─── Pull ─────────────────────────────────────────────────────────────────────
void GitManagerDialog::doPull()
{
    if (m_optPullRebase->isChecked())
        runGitCommand({"pull", "--rebase"});
    else
        runGitCommand({"pull"});
}

// ─── Verwerfen ────────────────────────────────────────────────────────────────
void GitManagerDialog::doDiscard()
{
    if (QMessageBox::warning(this, tr("Änderungen verwerfen"),
            tr("Alle lokalen Änderungen werden unwiderruflich gelöscht "
               "und auf den Stand von GitHub zurückgesetzt.\n\nFortfahren?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes)
    {
        runGitCommand({"reset", "--hard", "HEAD"});
    }
}

// ─── Git-Befehl ausführen ─────────────────────────────────────────────────────
void GitManagerDialog::runGitCommand(const QStringList &args)
{
    if (m_gitPath.isEmpty()) return;

    QStringList finalArgs = args;
    const QString token  = Config::gitToken();
    const QString remote = Config::gitRemoteUrl();

    if (!token.isEmpty() && !remote.isEmpty() &&
        (args.contains("push") || args.contains("pull")))
    {
        QString authUrl = remote;
        if (authUrl.startsWith("https://")) {
            authUrl.replace("https://", "https://" + token + "@");
            if (args.contains("push")) {
                finalArgs.clear();
                finalArgs << "push" << authUrl;
                const QString branch = m_gitBranchLabel->text().section(": ", 1);
                if (!branch.isEmpty() && branch != tr("Kein Repository"))
                    finalArgs << branch;
                if (args.contains("--tags")) finalArgs << "--tags";
            } else {
                finalArgs.clear();
                finalArgs << "pull";
                if (args.contains("--rebase")) finalArgs << "--rebase";
                finalArgs << authUrl;
            }
        }
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(m_gitPath);
    proc.start("git", finalArgs);
    proc.waitForFinished(30000);

    m_gitLog->append("<b>> git " + args.join(" ") + "</b>");
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QString err = QString::fromUtf8(proc.readAllStandardError());
    if (!out.isEmpty()) m_gitLog->append(out);
    if (!err.isEmpty())
        m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
    m_gitLog->ensureCursorVisible();

    refreshGitStatus();
}

// ─── GitHub Release erstellen ─────────────────────────────────────────────────
void GitManagerDialog::createGitHubRelease(const QString &tag,
                                            const QString &title,
                                            const QString &body,
                                            bool isLatest,
                                            bool isPrerelease)
{
    const QString token  = Config::gitToken();
    const QString remote = Config::gitRemoteUrl();
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Fehler"), tr("Kein GitHub Token hinterlegt!"));
        return;
    }

    QString repoPath = remote;
    repoPath.remove("https://github.com/");
    repoPath.remove(".git");

    const QString releaseTitle = title.isEmpty() ? tag : title;
    const QString releaseBody  = body.isEmpty()  ? ("Release " + tag) : body;

    // JSON manuell bauen (kein Qt JSON nötig, Werte sind kontrolliert)
    const QString json = QString(
        "{\"tag_name\":\"%1\","
        "\"name\":\"%2\","
        "\"body\":\"%3\","
        "\"draft\":false,"
        "\"prerelease\":%4,"
        "\"make_latest\":\"%5\"}")
        .arg(tag,
             releaseTitle,
             releaseBody.toHtmlEscaped().replace("\"", "\\\"").replace("\n", "\\n"),
             isPrerelease ? "true" : "false",
             isLatest ? "true" : "false");

    QProcess *proc = new QProcess(this);
    QStringList curlArgs;
    curlArgs << "-L" << "-X" << "POST"
             << "-H" << "Accept: application/vnd.github+json"
             << "-H" << QString("Authorization: Bearer %1").arg(token)
             << "-H" << "X-GitHub-Api-Version: 2022-11-28"
             << QString("https://api.github.com/repos/%1/releases").arg(repoPath)
             << "-d" << json;

    m_gitLog->append("<b>> GitHub API: Release " + tag + " wird erstellt…</b>");
    proc->start("curl", curlArgs);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, tag](int exitCode, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (!out.isEmpty()) m_gitLog->append(out);
        if (!err.isEmpty())
            m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
        if (exitCode == 0)
            m_gitLog->append("<span style='color:#50fa7b;'>✓ Release " + tag + " erstellt!</span>");
        proc->deleteLater();
    });
}

// ─── Status aktualisieren ─────────────────────────────────────────────────────
void GitManagerDialog::refreshGitStatus()
{
    if (m_gitPath.isEmpty()) return;

    QProcess bProc;
    bProc.setWorkingDirectory(m_gitPath);
    bProc.start("git", {"branch", "--show-current"});
    if (bProc.waitForFinished(5000)) {
        QString branch = bProc.readAllStandardOutput().trimmed();
        m_gitBranchLabel->setText(tr("Branch: %1")
            .arg(branch.isEmpty() ? tr("Kein Repository") : branch));
    }

    QProcess proc;
    proc.setWorkingDirectory(m_gitPath);
    proc.start("git", {"status", "--porcelain"});
    if (!proc.waitForFinished(5000)) return;

    const QString out = proc.readAllStandardOutput().trimmed();
    const QString err = proc.readAllStandardError().trimmed();
    if (!err.isEmpty())
        m_gitLog->append("<span style='color:#ff6b6b;'>Git: " + err + "</span>");

    m_gitStatusList->clear();
    if (out.isEmpty()) {
        m_gitStatusList->addItem(tr("✓ Keine ungespeicherten Änderungen"));
        return;
    }
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        if (line.length() < 3) continue;
        auto *item = new QListWidgetItem(
            QString("[%1] %2").arg(line.left(2), line.mid(3)),
            m_gitStatusList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(Qt::Checked);
    }
}

// ─── Repo-Combo ───────────────────────────────────────────────────────────────
void GitManagerDialog::rebuildRepoCombo()
{
    m_blockSave = true;
    m_repoCombo->clear();
    for (const auto &r : m_repos)
        m_repoCombo->addItem(r.name.isEmpty() ? tr("(unbenannt)") : r.name);
    m_blockSave = false;
}

void GitManagerDialog::loadRepoToFields(int index)
{
    m_blockSave = true;
    m_currentRepo = index;
    if (index >= 0 && index < m_repos.size()) {
        const auto &r = m_repos[index];
        m_gitRepoName->setText(r.name);
        m_gitLocalDir->setText(r.localDir);
        m_gitRemoteUrl->setText(r.remoteUrl);
        m_gitUsername->setText(r.username);
        m_gitPath = r.localDir;
    }
    m_blockSave = false;
}

void GitManagerDialog::saveCurrentFieldsToRepo()
{
    if (m_currentRepo < 0 || m_currentRepo >= m_repos.size()) return;
    auto &r     = m_repos[m_currentRepo];
    r.name      = m_gitRepoName->text().trimmed();
    r.localDir  = m_gitLocalDir->text().trimmed();
    r.remoteUrl = m_gitRemoteUrl->text().trimmed();
    r.username  = m_gitUsername->text().trimmed();
    m_gitPath   = r.localDir;
}

// ─── Load / Save ──────────────────────────────────────────────────────────────
void GitManagerDialog::load()
{
    m_repos = Config::gitRepos();
    rebuildRepoCombo();
    if (m_repos.isEmpty()) {
        m_currentRepo = -1;
    } else {
        m_repoCombo->setCurrentIndex(0);
        loadRepoToFields(0);
    }
    m_gitToken->setText(Config::gitToken());
}

void GitManagerDialog::save()
{
    saveCurrentFieldsToRepo();
    Config::setGitRepos(m_repos);
    Config::setGitToken(m_gitToken->text().trimmed());
    if (!m_repos.isEmpty()) {
        Config::setGitLocalDir(m_repos.first().localDir);
        Config::setGitRemoteUrl(m_repos.first().remoteUrl);
        Config::setGitUsername(m_repos.first().username);
    }
    emit settingsChanged();
}
