#include "gitmanagerdialog.h"
#include "../../config.h"
#include "../../thememanager.h"

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
    Q_ASSERT(lay != nullptr);
    lay->setSpacing(8);
    lay->setContentsMargins(16, 16, 16, 16);

    const QString inputSS = QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:2px 8px; font-size:13px; min-height:22px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
    const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
        .arg(tm.colors().textPrimary);
    const QString checkSS = QString("color:%1; font-size:12px;").arg(tm.colors().textPrimary);

    auto addLabel = [&](const QString &text)
    {
        auto *l = new QLabel(text, &dlg);
        Q_ASSERT(l != nullptr);
        l->setStyleSheet(labelSS);
        lay->addWidget(l);
    };

    addLabel(QObject::tr("Tag-Name (z.B. v1.0.0):"));
    auto *tagEdit = new QLineEdit(&dlg);
    Q_ASSERT(tagEdit != nullptr);
    tagEdit->setStyleSheet(inputSS);
    tagEdit->setPlaceholderText(QStringLiteral("v1.0.0"));
    lay->addWidget(tagEdit);

    addLabel(QObject::tr("Release-Titel:"));
    auto *titleEdit = new QLineEdit(&dlg);
    Q_ASSERT(titleEdit != nullptr);
    titleEdit->setStyleSheet(inputSS);
    titleEdit->setPlaceholderText(QObject::tr("z.B. Version 1.0.0"));
    lay->addWidget(titleEdit);

    addLabel(QObject::tr("Beschreibung / Changelog:"));
    auto *bodyEdit = new QTextEdit(&dlg);
    Q_ASSERT(bodyEdit != nullptr);
    bodyEdit->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "font-size:13px; padding:4px;")
        .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt));
    bodyEdit->setMinimumHeight(100);
    bodyEdit->setPlaceholderText(QObject::tr("Was ist neu in dieser Version?"));
    lay->addWidget(bodyEdit);

    auto *latestCheck = new QCheckBox(QObject::tr("Als \"Latest\" markieren"), &dlg);
    Q_ASSERT(latestCheck != nullptr);
    latestCheck->setStyleSheet(checkSS);
    latestCheck->setChecked(true);
    lay->addWidget(latestCheck);

    auto *prereleaseCheck = new QCheckBox(QObject::tr("Pre-release"), &dlg);
    Q_ASSERT(prereleaseCheck != nullptr);
    prereleaseCheck->setStyleSheet(checkSS);
    lay->addWidget(prereleaseCheck);

    auto *btnRow = new QHBoxLayout();
    Q_ASSERT(btnRow != nullptr);
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
    Q_ASSERT(okBtn != nullptr);
    okBtn->setStyleSheet(btnSS);
    auto *cancelBtn = new QPushButton(QObject::tr("Abbrechen"), &dlg);
    Q_ASSERT(cancelBtn != nullptr);
    cancelBtn->setStyleSheet(cancelSS);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    QObject::connect(okBtn,     &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
    {
        return false;
    }
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
    setMinimumWidth(860);
    resize(900, 1000);
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
    Q_ASSERT(root != nullptr);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(6);

    setupRepoSelectionSection(root, normBtnSS, inputSS, labelSS);
    setupBranchStatusSection(root, labelSS, tm.colors().accent);
    setupCommitMessageSection(root, labelSS, inputSS);
    setupMainActionsSection(root, primBtnSS, normBtnSS, dangerBtnSS);
    setupPushOptionsSection(root, checkSS, tm.colors().textAccent, tm.colors().borderAlt);
    setupPullOptionsSection(root, checkSS, tm.colors().textAccent, tm.colors().borderAlt);
    setupAdvancedFunctionsSection(root, normBtnSS, tm.colors().textAccent, tm.colors().accent);
    setupConnectionSettingsSection(root, inputSS, normBtnSS, tm.colors().textAccent, tm.colors().borderAlt);
    setupLogOutputSection(root, labelSS, tm.colors().bgDeep, tm.colors().textPrimary, tm.colors().borderAlt);

    // Footer
    auto *footer = new QHBoxLayout();
    Q_ASSERT(footer != nullptr);
    auto *btnClose = new QPushButton(tr("Schließen"), this);
    Q_ASSERT(btnClose != nullptr);
    btnClose->setStyleSheet(normBtnSS);
    footer->addStretch();
    footer->addWidget(btnClose);
    root->addLayout(footer);

    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
}

void GitManagerDialog::setupRepoSelectionSection(QVBoxLayout *root, const QString &normBtnSS, const QString &inputSS, const QString &labelSS)
{
    Q_ASSERT(root != nullptr);
    auto *repoRow = new QHBoxLayout();
    Q_ASSERT(repoRow != nullptr);
    auto *repoLbl = new QLabel(tr("Repository:"), this);
    Q_ASSERT(repoLbl != nullptr);
    repoLbl->setStyleSheet(labelSS);
    m_repoCombo = new QComboBox(this);
    Q_ASSERT(m_repoCombo != nullptr);
    m_repoCombo->setStyleSheet(inputSS);
    auto *btnNewRepo = new QPushButton(tr("Neu"), this);
    Q_ASSERT(btnNewRepo != nullptr);
    btnNewRepo->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    btnNewRepo->setStyleSheet(normBtnSS);
    btnNewRepo->setMinimumHeight(28);
    auto *btnDelRepo = new QPushButton(tr("Entfernen"), this);
    Q_ASSERT(btnDelRepo != nullptr);
    btnDelRepo->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    btnDelRepo->setStyleSheet(normBtnSS);
    btnDelRepo->setMinimumHeight(28);
    repoRow->addWidget(repoLbl);
    repoRow->addWidget(m_repoCombo, 1);
    repoRow->addWidget(btnNewRepo);
    repoRow->addWidget(btnDelRepo);
    root->addLayout(repoRow);

    connect(btnNewRepo, &QPushButton::clicked, this, [this]()
    {
        Config::GitRepo r;
        r.name = tr("Neues Repo");
        m_repos.append(r);
        Config::setGitRepos(m_repos);
        rebuildRepoCombo();
        m_repoCombo->setCurrentIndex(m_repos.size() - 1);
        emit settingsChanged();
    });
    connect(btnDelRepo, &QPushButton::clicked, this, [this]()
    {
        if (m_currentRepo < 0 || m_currentRepo >= m_repos.size())
        {
            return;
        }
        m_repos.removeAt(m_currentRepo);
        Config::setGitRepos(m_repos);
        rebuildRepoCombo();
        if (m_repos.isEmpty())
        {
            m_blockSave = true;
            m_gitRepoName->clear();
            m_gitLocalDir->clear();
            m_gitRemoteUrl->clear();
            m_gitUsername->clear();
            m_gitPath.clear();
            m_currentRepo = -1;
            m_blockSave = false;
        }
        else
        {
            m_repoCombo->setCurrentIndex(0);
        }
        emit settingsChanged();
    });
    connect(m_repoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx)
    {
        if (m_blockSave)
        {
            return;
        }
        loadRepoToFields(idx);
        refreshGitStatus();
    });
}

void GitManagerDialog::setupBranchStatusSection(QVBoxLayout *root, const QString &labelSS, const QString &accentColor)
{
    Q_ASSERT(root != nullptr);
    auto *statusHeaderRow = new QHBoxLayout();
    Q_ASSERT(statusHeaderRow != nullptr);
    auto *statusLbl = new QLabel(tr("Geänderte Dateien:"), this);
    Q_ASSERT(statusLbl != nullptr);
    statusLbl->setStyleSheet(labelSS);
    m_gitBranchLabel = new QLabel(tr("Branch: …"), this);
    Q_ASSERT(m_gitBranchLabel != nullptr);
    m_gitBranchLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:bold;").arg(accentColor));
    statusHeaderRow->addWidget(statusLbl);
    statusHeaderRow->addStretch();
    statusHeaderRow->addWidget(m_gitBranchLabel);
    root->addLayout(statusHeaderRow);

    m_gitStatusList = new QListWidget(this);
    Q_ASSERT(m_gitStatusList != nullptr);
    m_gitStatusList->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:3px;")
        .arg(TM().colors().bgInput, TM().colors().textPrimary, TM().colors().borderAlt));
    m_gitStatusList->setMaximumHeight(130);
    root->addWidget(m_gitStatusList);
}

void GitManagerDialog::setupCommitMessageSection(QVBoxLayout *root, const QString &labelSS, const QString &inputSS)
{
    Q_ASSERT(root != nullptr);
    auto *msgLbl = new QLabel(tr("Beschreibung der Änderungen:"), this);
    Q_ASSERT(msgLbl != nullptr);
    msgLbl->setStyleSheet(labelSS);
    root->addWidget(msgLbl);

    m_gitCommitMsg = new QLineEdit(this);
    Q_ASSERT(m_gitCommitMsg != nullptr);
    m_gitCommitMsg->setPlaceholderText(tr("z.B. Fehler in der Suche behoben..."));
    m_gitCommitMsg->setStyleSheet(inputSS);
    root->addWidget(m_gitCommitMsg);
}

void GitManagerDialog::setupMainActionsSection(QVBoxLayout *root, const QString &primBtnSS, const QString &normBtnSS, const QString &dangerBtnSS)
{
    Q_ASSERT(root != nullptr);
    auto *mainActRow = new QHBoxLayout();
    Q_ASSERT(mainActRow != nullptr);
    mainActRow->setSpacing(8);

    auto *btnPush = new QPushButton(tr("Commit && Push"), this);
    Q_ASSERT(btnPush != nullptr);
    btnPush->setIcon(QIcon::fromTheme(QStringLiteral("vcs-commit")));
    btnPush->setStyleSheet(primBtnSS);
    btnPush->setToolTip(tr("Speichert deine Änderungen und lädt sie zu GitHub hoch."));

    auto *btnFetch = new QPushButton(tr("Fetch"), this);
    Q_ASSERT(btnFetch != nullptr);
    btnFetch->setIcon(QIcon::fromTheme(QStringLiteral("vcs-update-required")));
    btnFetch->setStyleSheet(normBtnSS);
    btnFetch->setToolTip(tr("Prüft ob es neue Änderungen auf GitHub gibt, ohne sie herunterzuladen."));

    auto *btnPull = new QPushButton(tr("Pull"), this);
    Q_ASSERT(btnPull != nullptr);
    btnPull->setIcon(QIcon::fromTheme(QStringLiteral("vcs-pull")));
    btnPull->setStyleSheet(normBtnSS);
    btnPull->setToolTip(tr("Holt die neuesten Änderungen von GitHub."));

    auto *btnDiscard = new QPushButton(tr("Änderungen verwerfen"), this);
    Q_ASSERT(btnDiscard != nullptr);
    btnDiscard->setIcon(QIcon::fromTheme(QStringLiteral("document-revert")));
    btnDiscard->setStyleSheet(dangerBtnSS);
    btnDiscard->setToolTip(tr("Setzt alle lokalen Änderungen auf den Stand von GitHub zurück."));

    mainActRow->addWidget(btnPush, 1);
    mainActRow->addWidget(btnFetch, 1);
    mainActRow->addWidget(btnPull, 1);
    mainActRow->addWidget(btnDiscard, 1);
    root->addLayout(mainActRow);

    connect(btnPush,    &QPushButton::clicked, this, &GitManagerDialog::doCommitPush);
    connect(btnFetch,   &QPushButton::clicked, this, [this]() { runGitCommand({QStringLiteral("fetch"), QStringLiteral("--all")}); });
    connect(btnPull,    &QPushButton::clicked, this, &GitManagerDialog::doPull);
    connect(btnDiscard, &QPushButton::clicked, this, &GitManagerDialog::doDiscard);
}

void GitManagerDialog::setupPushOptionsSection(QVBoxLayout *root, const QString &checkSS, const QString &textAccent, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    auto *pushOptBox = new QGroupBox(tr("Optionen für Commit && Push"), this);
    Q_ASSERT(pushOptBox != nullptr);
    pushOptBox->setStyleSheet(QString(
        "QGroupBox { color:%1; font-size:11px; border:1px solid %2; "
        "border-radius:4px; margin-top:6px; padding-top:4px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; }")
        .arg(textAccent, borderAlt));
    auto *pushOptLay = new QHBoxLayout(pushOptBox);
    Q_ASSERT(pushOptLay != nullptr);
    pushOptLay->setContentsMargins(8, 6, 8, 6);
    pushOptLay->setSpacing(16);

    m_optPushTags      = new QCheckBox(tr("Tags mit hochladen"), pushOptBox);
    Q_ASSERT(m_optPushTags != nullptr);
    m_optCreateRelease = new QCheckBox(tr("Release auf GitHub erstellen"), pushOptBox);
    Q_ASSERT(m_optCreateRelease != nullptr);
    m_optForceWithLease = new QCheckBox(tr("Force with lease"), pushOptBox);
    Q_ASSERT(m_optForceWithLease != nullptr);
    m_optForceWithLease->setToolTip(tr("Pusht auch wenn der Remote-Branch voraus ist — "
                                       "aber nur wenn sich der Remote seit dem letzten Fetch nicht verändert hat."));
    m_optPushTags->setStyleSheet(checkSS);
    m_optCreateRelease->setStyleSheet(checkSS);
    m_optForceWithLease->setStyleSheet(checkSS);
    pushOptLay->addWidget(m_optPushTags);
    pushOptLay->addWidget(m_optCreateRelease);
    pushOptLay->addWidget(m_optForceWithLease);
    pushOptLay->addStretch();
    root->addWidget(pushOptBox);
}

void GitManagerDialog::setupPullOptionsSection(QVBoxLayout *root, const QString &checkSS, const QString &textAccent, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    auto *pullOptBox = new QGroupBox(tr("Optionen für Pull"), this);
    Q_ASSERT(pullOptBox != nullptr);
    pullOptBox->setStyleSheet(QString(
        "QGroupBox { color:%1; font-size:11px; border:1px solid %2; "
        "border-radius:4px; margin-top:6px; padding-top:4px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; }")
        .arg(textAccent, borderAlt));
    auto *pullOptLay = new QHBoxLayout(pullOptBox);
    Q_ASSERT(pullOptLay != nullptr);
    pullOptLay->setContentsMargins(8, 6, 8, 6);

    m_optPullRebase = new QCheckBox(tr("Rebase statt Merge"), pullOptBox);
    Q_ASSERT(m_optPullRebase != nullptr);
    m_optPullRebase->setStyleSheet(checkSS);
    m_optPullRebase->setToolTip(tr("git pull --rebase — hält den Verlauf sauber."));
    pullOptLay->addWidget(m_optPullRebase);
    pullOptLay->addStretch();
    root->addWidget(pullOptBox);
}

void GitManagerDialog::setupAdvancedFunctionsSection(QVBoxLayout *root, const QString &normBtnSS, const QString &textAccent, const QString &accentColor)
{
    Q_ASSERT(root != nullptr);
    const QString sepSS = QStringLiteral("background:%1; max-height:1px; margin:8px 0;").arg(TM().colors().borderAlt);
    auto *sepAdv = new QFrame(this);
    Q_ASSERT(sepAdv != nullptr);
    sepAdv->setFrameShape(QFrame::HLine);
    sepAdv->setStyleSheet(sepSS);
    root->addWidget(sepAdv);

    auto *advToggleBtn = new QToolButton(this);
    Q_ASSERT(advToggleBtn != nullptr);
    advToggleBtn->setText(tr("▶  Erweiterte Funktionen"));
    advToggleBtn->setCheckable(true);
    advToggleBtn->setChecked(false);
    advToggleBtn->setStyleSheet(QString(
        "QToolButton { background:transparent; border:none; color:%1; "
        "font-size:12px; font-weight:bold; }"
        "QToolButton:hover { color:%2; }")
        .arg(textAccent, accentColor));
    root->addWidget(advToggleBtn);

    m_advancedWidget = new QWidget(this);
    Q_ASSERT(m_advancedWidget != nullptr);
    m_advancedWidget->setVisible(false);
    auto *advLay = new QGridLayout(m_advancedWidget);
    Q_ASSERT(advLay != nullptr);
    advLay->setSpacing(8);
    advLay->setContentsMargins(0, 4, 0, 0);

    auto makeAdvBtn = [&](const QString &label, const QString &tip, const QString &icon = QString())
    {
        auto *b = new QPushButton(m_advancedWidget);
        Q_ASSERT(b != nullptr);
        if (!icon.isEmpty())
        {
            b->setIcon(QIcon::fromTheme(icon));
        }
        b->setText(label);
        b->setStyleSheet(normBtnSS);
        b->setToolTip(tip);
        return b;
    };

    auto *btnLog    = makeAdvBtn(tr("Verlauf (Log)"), tr("Zeigt die letzten 20 Commits."), QStringLiteral("vcs-diff-cvs-cervisia"));
    auto *btnDiff   = makeAdvBtn(tr("Diff"), tr("Zeigt welche Zeilen du geändert hast."), QStringLiteral("vcs-diff"));
    auto *btnTag    = makeAdvBtn(tr("Tag erstellen"), tr("Markiert den aktuellen Stand als Version, z.B. v1.0."), QStringLiteral("tag-assigned"));
    auto *btnBranch = makeAdvBtn(tr("Branch wechseln / erstellen"), tr("Wechselt zu einem anderen Zweig oder erstellt einen neuen."), QStringLiteral("vcs-branch"));
    auto *btnMerge  = makeAdvBtn(tr("Merge"), tr("Führt einen anderen Branch in den aktuellen zusammen."), QStringLiteral("vcs-merge"));
    auto *btnRevert = makeAdvBtn(tr("Revert"), tr("Macht einen bestimmten Commit rückgängig — History bleibt erhalten."), QStringLiteral("edit-undo-symbolic"));
    auto *btnStash  = makeAdvBtn(tr("Stash (Parken)"), tr("Legt deine Änderungen zur Seite ohne sie zu speichern."), QStringLiteral("vcs-stash"));
    auto *btnStashPop = makeAdvBtn(tr("Stash anwenden"), tr("Holt die zuletzt geparkten Änderungen zurück."), QStringLiteral("vcs-stash-pop"));

    advLay->addWidget(btnLog,      0, 0);
    advLay->addWidget(btnDiff,     0, 1);
    advLay->addWidget(btnTag,      0, 2);
    advLay->addWidget(btnBranch,   1, 0);
    advLay->addWidget(btnMerge,    1, 1);
    advLay->addWidget(btnRevert,   1, 2);
    advLay->addWidget(btnStash,    2, 0);
    advLay->addWidget(btnStashPop, 2, 1);
    root->addWidget(m_advancedWidget);

    connect(advToggleBtn, &QToolButton::toggled, this, [this, advToggleBtn](bool on)
    {
        m_advancedWidget->setVisible(on);
        advToggleBtn->setText((on ? tr("▼  Erweiterte Funktionen") : tr("▶  Erweiterte Funktionen")));
    });

    connect(btnLog,   &QPushButton::clicked, this, [this]() { runGitCommand({QStringLiteral("log"), QStringLiteral("--oneline"), QStringLiteral("-n"), QStringLiteral("20")}); });
    connect(btnDiff,  &QPushButton::clicked, this, [this]() { runGitCommand({QStringLiteral("diff")}); });
    connect(btnStash, &QPushButton::clicked, this, [this]() { runGitCommand({QStringLiteral("stash")}); });
    connect(btnStashPop, &QPushButton::clicked, this, [this]() { runGitCommand({QStringLiteral("stash"), QStringLiteral("pop")}); });

    connect(btnTag, &QPushButton::clicked, this, [this]()
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Tag erstellen"), tr("Tag-Name (z.B. v1.0.0):"), QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty())
        {
            runGitCommand({QStringLiteral("tag"), name.trimmed()});
        }
    });

    connect(btnBranch, &QPushButton::clicked, this, [this, normBtnSS]()
    {
        const ThemeManager &tm = TM();
        const QString inputSS = QString(
            "background:%1; color:%2; border:1px solid %3; border-radius:3px; "
            "padding:2px 8px; font-size:13px; min-height:22px;")
            .arg(tm.colors().bgInput, tm.colors().textPrimary, tm.colors().borderAlt);
        const QString labelSS = QString("color:%1; font-size:12px; font-weight:bold;")
            .arg(tm.colors().textPrimary);

        // Branches aus git holen
        QProcess p;
        p.setWorkingDirectory(m_gitPath);
        p.start(QStringLiteral("git"), {QStringLiteral("branch"), QStringLiteral("--all"), QStringLiteral("--format=%(refname:short)")});
        p.waitForFinished(5000);
        QStringList branches = QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

        QDialog dlg(this);
        dlg.setWindowTitle(tr("Branch wechseln / erstellen"));
        dlg.setMinimumWidth(360);
        auto *lay = new QVBoxLayout(&dlg);
        Q_ASSERT(lay != nullptr);
        lay->setSpacing(8);
        lay->setContentsMargins(12, 12, 12, 12);

        auto *lblExist = new QLabel(tr("Vorhandenen Branch auschecken:"), &dlg);
        Q_ASSERT(lblExist != nullptr);
        lblExist->setStyleSheet(labelSS);
        lay->addWidget(lblExist);
        auto *branchCombo = new QComboBox(&dlg);
        Q_ASSERT(branchCombo != nullptr);
        branchCombo->setStyleSheet(inputSS);
        branchCombo->addItems(branches);
        lay->addWidget(branchCombo);

        auto *lblNew = new QLabel(tr("Oder neuen Branch erstellen:"), &dlg);
        Q_ASSERT(lblNew != nullptr);
        lblNew->setStyleSheet(labelSS);
        lay->addWidget(lblNew);
        auto *newBranchEdit = new QLineEdit(&dlg);
        Q_ASSERT(newBranchEdit != nullptr);
        newBranchEdit->setStyleSheet(inputSS);
        newBranchEdit->setPlaceholderText(tr("Neuer Branch-Name (leer lassen zum Auschecken)"));
        lay->addWidget(newBranchEdit);

        auto *forceCheck = new QCheckBox(tr("Force (lokale Änderungen verwerfen)"), &dlg);
        Q_ASSERT(forceCheck != nullptr);
        forceCheck->setStyleSheet(QStringLiteral("color:%1; font-size:12px;").arg(TM().colors().textPrimary));
        lay->addWidget(forceCheck);

        auto *btnRow = new QHBoxLayout();
        Q_ASSERT(btnRow != nullptr);
        auto *okBtn = new QPushButton(tr("Ausführen"), &dlg);
        Q_ASSERT(okBtn != nullptr);
        auto *cancelBtn = new QPushButton(tr("Abbrechen"), &dlg);
        Q_ASSERT(cancelBtn != nullptr);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        lay->addLayout(btnRow);

        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted)
        {
            return;
        }

        const QString newName = newBranchEdit->text().trimmed();
        if (!newName.isEmpty())
        {
            QStringList args = {QStringLiteral("checkout"), QStringLiteral("-b"), newName};
            if (forceCheck->isChecked())
            {
                args << QStringLiteral("-f");
            }
            runGitCommand(args);
        }
        else if (!branchCombo->currentText().isEmpty())
        {
            QStringList args = {QStringLiteral("checkout"), branchCombo->currentText()};
            if (forceCheck->isChecked())
            {
                args << QStringLiteral("-f");
            }
            runGitCommand(args);
        }
    });

    connect(btnMerge, &QPushButton::clicked, this, [this]()
    {
        QProcess p;
        p.setWorkingDirectory(m_gitPath);
        p.start(QStringLiteral("git"), {QStringLiteral("branch"), QStringLiteral("--all"), QStringLiteral("--format=%(refname:short)")});
        p.waitForFinished(5000);
        QStringList branches = QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

        bool ok = false;
        QString name = QInputDialog::getItem(this, tr("Merge"), tr("Branch der zusammengeführt werden soll:"), branches, 0, true, &ok);
        if (ok && !name.trimmed().isEmpty())
        {
            runGitCommand({QStringLiteral("merge"), name.trimmed()});
        }
    });

    connect(btnRevert, &QPushButton::clicked, this, [this]()
    {
        QProcess p;
        p.setWorkingDirectory(m_gitPath);
        p.start(QStringLiteral("git"), {QStringLiteral("log"), QStringLiteral("--oneline"), QStringLiteral("-n"), QStringLiteral("20")});
        p.waitForFinished(5000);
        QStringList commits = QString::fromUtf8(p.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

        bool ok = false;
        QString entry = QInputDialog::getItem(this, tr("Revert"), tr("Commit rückgängig machen:"), commits, 0, false, &ok);
        if (ok && !entry.isEmpty())
        {
            QString hash = entry.section(' ', 0, 0);
            runGitCommand({QStringLiteral("revert"), QStringLiteral("--no-edit"), hash});
        }
    });
}

void GitManagerDialog::setupConnectionSettingsSection(QVBoxLayout *root, const QString &inputSS, const QString &normBtnSS, const QString &textAccent, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    Q_UNUSED(borderAlt)
    const QString sepSS = QStringLiteral("background:%1; max-height:1px; margin:8px 0;").arg(TM().colors().borderAlt);
    auto *sepCfg = new QFrame(this);
    Q_ASSERT(sepCfg != nullptr);
    sepCfg->setFrameShape(QFrame::HLine);
    sepCfg->setStyleSheet(sepSS);
    root->addWidget(sepCfg);

    auto *cfgToggleBtn = new QToolButton(this);
    Q_ASSERT(cfgToggleBtn != nullptr);
    cfgToggleBtn->setText(tr("▶  Verbindungseinstellungen"));
    cfgToggleBtn->setCheckable(true);
    cfgToggleBtn->setChecked(false);
    cfgToggleBtn->setStyleSheet(QString(
        "QToolButton { background:transparent; border:none; color:%1; "
        "font-size:12px; font-weight:bold; }"
        "QToolButton:hover { color:%2; }")
        .arg(textAccent, TM().colors().accent));
    root->addWidget(cfgToggleBtn);

    auto *cfgWidget = new QWidget(this);
    Q_ASSERT(cfgWidget != nullptr);
    cfgWidget->setVisible(false);
    auto *form = new QFormLayout(cfgWidget);
    Q_ASSERT(form != nullptr);
    form->setSpacing(6);
    form->setContentsMargins(0, 4, 0, 0);

    m_gitRepoName = new QLineEdit(cfgWidget);
    Q_ASSERT(m_gitRepoName != nullptr);
    m_gitRepoName->setStyleSheet(inputSS);
    m_gitRepoName->setPlaceholderText(tr("z.B. SplitCommander"));
    form->addRow(tr("Name:"), m_gitRepoName);

    m_gitLocalDir = new QLineEdit(cfgWidget);
    Q_ASSERT(m_gitLocalDir != nullptr);
    m_gitLocalDir->setStyleSheet(inputSS);
    auto *btnBrowse = new QPushButton(tr("Durchsuchen..."), cfgWidget);
    Q_ASSERT(btnBrowse != nullptr);
    btnBrowse->setStyleSheet(normBtnSS);
    btnBrowse->setMinimumHeight(28);
    auto *pathRow = new QHBoxLayout();
    Q_ASSERT(pathRow != nullptr);
    pathRow->addWidget(m_gitLocalDir, 1);
    pathRow->addWidget(btnBrowse);
    form->addRow(tr("Projekt-Ordner:"), pathRow);

    m_gitRemoteUrl = new QLineEdit(cfgWidget);
    Q_ASSERT(m_gitRemoteUrl != nullptr);
    m_gitRemoteUrl->setStyleSheet(inputSS);
    m_gitRemoteUrl->setPlaceholderText(QStringLiteral("https://github.com/user/repo.git"));
    form->addRow(tr("GitHub URL:"), m_gitRemoteUrl);

    m_gitUsername = new QLineEdit(cfgWidget);
    Q_ASSERT(m_gitUsername != nullptr);
    m_gitUsername->setStyleSheet(inputSS);
    form->addRow(tr("GitHub Benutzername:"), m_gitUsername);

    m_gitToken = new QLineEdit(cfgWidget);
    Q_ASSERT(m_gitToken != nullptr);
    m_gitToken->setEchoMode(QLineEdit::Password);
    m_gitToken->setStyleSheet(inputSS);
    auto *btnTokenVisible = new QPushButton(cfgWidget);
    Q_ASSERT(btnTokenVisible != nullptr);
    btnTokenVisible->setIcon(QIcon::fromTheme(QStringLiteral("password-show-on")));
    btnTokenVisible->setStyleSheet(normBtnSS);
    btnTokenVisible->setCheckable(true);
    btnTokenVisible->setMinimumHeight(28);
    btnTokenVisible->setToolTip(tr("Token anzeigen"));
    auto *btnGenToken = new QPushButton(tr("Token generieren..."), cfgWidget);
    Q_ASSERT(btnGenToken != nullptr);
    btnGenToken->setStyleSheet(normBtnSS);
    btnGenToken->setMinimumHeight(28);
    auto *tokenRow = new QHBoxLayout();
    Q_ASSERT(tokenRow != nullptr);
    tokenRow->addWidget(m_gitToken, 1);
    tokenRow->addWidget(btnTokenVisible);
    tokenRow->addWidget(btnGenToken);
    form->addRow(tr("Token / Passwort:"), tokenRow);

    connect(btnTokenVisible, &QPushButton::toggled, this, [this, btnTokenVisible](bool visible)
    {
        m_gitToken->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        btnTokenVisible->setIcon(QIcon::fromTheme(visible ? QStringLiteral("password-show-off") : QStringLiteral("password-show-on")));
    });

    auto *btnClone = new QPushButton(tr("Repository klonen"), cfgWidget);
    Q_ASSERT(btnClone != nullptr);
    btnClone->setIcon(QIcon::fromTheme(QStringLiteral("folder-download")));
    btnClone->setStyleSheet(normBtnSS);
    btnClone->setMinimumHeight(28);
    btnClone->setToolTip(tr("Klont das eingetragene Remote-Repository in den Projekt-Ordner."));
    form->addRow(tr("Klonen:"), btnClone);

    root->addWidget(cfgWidget);

    connect(cfgToggleBtn, &QToolButton::toggled, this, [cfgWidget, cfgToggleBtn](bool on)
    {
        cfgWidget->setVisible(on);
        cfgToggleBtn->setText((on ? QObject::tr("▼  Verbindungseinstellungen") : QObject::tr("▶  Verbindungseinstellungen")));
    });

    connect(btnClone, &QPushButton::clicked, this, [this]()
    {
        const QString url = m_gitRemoteUrl->text().trimmed();
        const QString dir = m_gitLocalDir->text().trimmed();
        if (url.isEmpty() || dir.isEmpty())
        {
            QMessageBox::warning(this, tr("Hinweis"), tr("Bitte GitHub URL und Projekt-Ordner eintragen."));
            return;
        }
        QProcess proc;
        proc.setWorkingDirectory(QFileInfo(dir).absolutePath());
        proc.start(QStringLiteral("git"), {QStringLiteral("clone"), url, dir});
        proc.waitForFinished(60000);
        m_gitLog->append("<b>> git clone " + url + "</b>");
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        const QString err = QString::fromUtf8(proc.readAllStandardError());
        if (!out.isEmpty())
        {
            m_gitLog->append(out);
        }
        if (!err.isEmpty())
        {
            m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
        }
        refreshGitStatus();
    });

    connect(btnBrowse, &QPushButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Projekt-Ordner wählen"), m_gitLocalDir->text());
        if (!dir.isEmpty())
        {
            m_gitLocalDir->setText(dir);
            m_gitPath = dir;
            save();
            refreshGitStatus();
        }
    });

    connect(btnGenToken, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/settings/tokens/new")));
    });

    auto saveAndRefresh = [this]() { save(); refreshGitStatus(); };
    connect(m_gitRepoName, &QLineEdit::textEdited, this, [this]()
    {
        if (m_blockSave)
        {
            return;
        }
        saveCurrentFieldsToRepo();
        if (m_currentRepo >= 0 && m_currentRepo < m_repos.size())
        {
            const QString n = m_gitRepoName->text().trimmed();
            m_repoCombo->blockSignals(true);
            m_repoCombo->setItemText(m_currentRepo, n.isEmpty() ? tr("(unbenannt)") : n);
            m_repoCombo->blockSignals(false);
        }
        save();
    });
    connect(m_gitLocalDir,  &QLineEdit::textEdited, this, [this, saveAndRefresh]()
    {
        if (m_blockSave)
        {
            return;
        }
        saveCurrentFieldsToRepo();
        saveAndRefresh();
    });
    connect(m_gitRemoteUrl, &QLineEdit::textEdited, this, [this]()
    {
        if (m_blockSave)
        {
            return;
        }
        saveCurrentFieldsToRepo();
        save();
    });
    connect(m_gitUsername,  &QLineEdit::textEdited, this, [this]()
    {
        if (m_blockSave)
        {
            return;
        }
        saveCurrentFieldsToRepo();
        save();
    });
    connect(m_gitToken, &QLineEdit::textEdited, this, [this]()
    {
        save();
    });
}

void GitManagerDialog::setupLogOutputSection(QVBoxLayout *root, const QString &labelSS, const QString &bgDeep, const QString &textPrimary, const QString &borderAlt)
{
    Q_ASSERT(root != nullptr);
    const QString sepSS = QStringLiteral("background:%1; max-height:1px; margin:8px 0;").arg(TM().colors().borderAlt);
    auto *sepLog = new QFrame(this);
    Q_ASSERT(sepLog != nullptr);
    sepLog->setFrameShape(QFrame::HLine);
    sepLog->setStyleSheet(sepSS);
    root->addWidget(sepLog);

    auto *logLbl = new QLabel(tr("Ausgabe:"), this);
    Q_ASSERT(logLbl != nullptr);
    logLbl->setStyleSheet(labelSS);
    root->addWidget(logLbl);

    m_gitLog = new QTextEdit(this);
    Q_ASSERT(m_gitLog != nullptr);
    m_gitLog->setReadOnly(true);
    m_gitLog->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; "
        "font-family:monospace; font-size:11px; padding:6px; border-radius:4px;")
        .arg(bgDeep, textPrimary, borderAlt));
    m_gitLog->append(QStringLiteral("<span style='color:#50fa7b;'>✓ Git Manager bereit.</span>"));
    root->addWidget(m_gitLog, 1);
}

// ─── Commit & Push ────────────────────────────────────────────────────────────
void GitManagerDialog::doCommitPush()
{
    QString msg = m_gitCommitMsg->text().trimmed();
    if (msg.isEmpty())
    {
        QMessageBox::warning(this, tr("Hinweis"), tr("Bitte gib eine Beschreibung ein."));
        return;
    }

    // Release-Dialog zuerst — bevor wir irgendetwas tun
    QString relTag, relTitle, relBody;
    bool relLatest = true, relPrerelease = false;
    if (m_optCreateRelease->isChecked())
    {
        if (!showReleaseDialog(this, relTag, relTitle, relBody, relLatest, relPrerelease, TM()))
        {
            return;
        }
    }

    // Push-Optionen-Dialog
    QProcess rp;
    rp.setWorkingDirectory(m_gitPath);
    rp.start(QStringLiteral("git"), {QStringLiteral("remote")});
    rp.waitForFinished(3000);
    QStringList remotes = QString::fromUtf8(rp.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

    QString pushRemote = remotes.value(0, QStringLiteral("origin"));
    bool forcePush = false;

    if (remotes.size() > 1)
    {
        bool ok = false;
        pushRemote = QInputDialog::getItem(this, tr("Push"), tr("Remote auswählen:"), remotes, 0, false, &ok);
        if (!ok)
        {
            return;
        }
    }

    // Force with lease Checkbox abfragen
    if (m_optForceWithLease && m_optForceWithLease->isChecked())
    {
        forcePush = true;
    }

    // 1. Commit
    runGitCommand({QStringLiteral("add"), QStringLiteral(".")});
    runGitCommand({QStringLiteral("commit"), QStringLiteral("-m"), msg});

    // 2. Tag lokal erstellen (vor dem Push!)
    if (m_optCreateRelease->isChecked() && !relTag.isEmpty())
    {
        runGitCommand({QStringLiteral("tag"), relTag});
    }

    // 3. Push
    QStringList pushArgs = {QStringLiteral("push"), pushRemote};
    if (forcePush)
    {
        pushArgs << QStringLiteral("--force-with-lease");
    }
    runGitCommand(pushArgs);

    // 4. Tags pushen
    if (m_optPushTags->isChecked() || m_optCreateRelease->isChecked())
    {
        runGitCommand({QStringLiteral("push"), pushRemote, QStringLiteral("--tags")});
    }

    // 5. GitHub Release via API
    if (m_optCreateRelease->isChecked() && !relTag.isEmpty())
    {
        createGitHubRelease(relTag, relTitle, relBody, relLatest, relPrerelease);
    }

    m_gitCommitMsg->clear();
    m_gitLog->append(QStringLiteral("<span style='color:#50fa7b;'>✓ Fertig!</span>"));
}

// ─── Pull ─────────────────────────────────────────────────────────────────────
void GitManagerDialog::doPull()
{
    QProcess rp;
    rp.setWorkingDirectory(m_gitPath);
    rp.start(QStringLiteral("git"), {QStringLiteral("remote")});
    rp.waitForFinished(3000);
    QStringList remotes = QString::fromUtf8(rp.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

    QString pullRemote = remotes.value(0, QStringLiteral("origin"));
    if (remotes.size() > 1)
    {
        bool ok = false;
        pullRemote = QInputDialog::getItem(this, tr("Pull"), tr("Remote auswählen:"), remotes, 0, false, &ok);
        if (!ok)
        {
            return;
        }
    }

    QStringList args = {QStringLiteral("pull"), pullRemote};
    if (m_optPullRebase->isChecked())
    {
        args.insert(1, QStringLiteral("--rebase"));
    }
    runGitCommand(args);
}

// ─── Verwerfen ────────────────────────────────────────────────────────────────
void GitManagerDialog::doDiscard()
{
    if (QMessageBox::warning(this, tr("Änderungen verwerfen"),
            tr("Alle lokalen Änderungen werden unwiderruflich gelöscht und auf den Stand von GitHub zurückgesetzt.\n\nFortfahren?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        runGitCommand({QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")});
    }
}

// ─── Git-Befehl ausführen ─────────────────────────────────────────────────────
void GitManagerDialog::runGitCommand(const QStringList &args)
{
    if (m_gitPath.isEmpty())
    {
        return;
    }

    QStringList finalArgs = args;
    const QString token  = Config::gitToken();
    const QString remote = Config::gitRemoteUrl();

    if (!token.isEmpty() && !remote.isEmpty() && (args.contains(QStringLiteral("push")) || args.contains(QStringLiteral("pull"))))
    {
        QString authUrl = remote;
        if (authUrl.startsWith(QStringLiteral("https://")))
        {
            authUrl.replace(QStringLiteral("https://"), "https://" + token + "@");
            if (args.contains(QStringLiteral("push")))
            {
                finalArgs.clear();
                finalArgs << QStringLiteral("push") << authUrl;
                const QString branch = m_gitBranchLabel->text().section(QStringLiteral(": "), 1);
                if (!branch.isEmpty() && branch != tr("Kein Repository"))
                {
                    finalArgs << branch;
                }
                if (args.contains(QStringLiteral("--tags")))
                {
                    finalArgs << QStringLiteral("--tags");
                }
            }
            else
            {
                finalArgs.clear();
                finalArgs << QStringLiteral("pull");
                if (args.contains(QStringLiteral("--rebase")))
                {
                    finalArgs << QStringLiteral("--rebase");
                }
                finalArgs << authUrl;
            }
        }
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(m_gitPath);
    proc.start(QStringLiteral("git"), finalArgs);
    proc.waitForFinished(30000);

    m_gitLog->append("<b>> git " + args.join(QStringLiteral(" ")) + "</b>");
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QString err = QString::fromUtf8(proc.readAllStandardError());
    if (!out.isEmpty())
    {
        m_gitLog->append(out);
    }
    if (!err.isEmpty())
    {
        m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
    }
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
    if (token.isEmpty())
    {
        QMessageBox::warning(this, tr("Fehler"), tr("Kein GitHub Token hinterlegt!"));
        return;
    }

    QString repoPath = remote;
    repoPath.remove(QStringLiteral("https://github.com/"));
    repoPath.remove(QStringLiteral(".git"));

    const QString releaseTitle = title.isEmpty() ? tag : title;
    const QString releaseBody  = body.isEmpty()  ? ("Release " + tag) : body;

    const QString json = QString(
        "{\"tag_name\":\"%1\","
        "\"name\":\"%2\","
        "\"body\":\"%3\","
        "\"draft\":false,"
        "\"prerelease\":%4,"
        "\"make_latest\":\"%5\"}")
        .arg(tag,
             releaseTitle,
             releaseBody.toHtmlEscaped().replace(QStringLiteral("\""), QStringLiteral("\\\"")).replace(QStringLiteral("\n"), QStringLiteral("\\n")),
             isPrerelease ? QStringLiteral("true") : QStringLiteral("false"),
             isLatest ? QStringLiteral("true") : QStringLiteral("false"));

    auto *proc = new QProcess(this);
    Q_ASSERT(proc != nullptr);
    QStringList curlArgs;
    curlArgs << QStringLiteral("-L") << QStringLiteral("-X") << QStringLiteral("POST")
             << QStringLiteral("-H") << QStringLiteral("Accept: application/vnd.github+json")
             << QStringLiteral("-H") << QString("Authorization: Bearer %1").arg(token)
             << QStringLiteral("-H") << QStringLiteral("X-GitHub-Api-Version: 2022-11-28")
             << QString("https://api.github.com/repos/%1/releases").arg(repoPath)
             << QStringLiteral("-d") << json;

    m_gitLog->append("<b>> GitHub API: Release " + tag + " wird erstellt…</b>");
    proc->start(QStringLiteral("curl"), curlArgs);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, proc, tag](int exitCode, QProcess::ExitStatus)
    {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (!out.isEmpty())
        {
            m_gitLog->append(out);
        }
        if (!err.isEmpty())
        {
            m_gitLog->append("<span style='color:#ff6b6b;'>" + err + "</span>");
        }
        if (exitCode == 0)
        {
            m_gitLog->append("<span style='color:#50fa7b;'>✓ Release " + tag + " erstellt!</span>");
        }
        proc->deleteLater();
    });
}

// ─── Status aktualisieren ─────────────────────────────────────────────────────
void GitManagerDialog::refreshGitStatus()
{
    if (m_gitPath.isEmpty())
    {
        return;
    }

    QProcess bProc;
    bProc.setWorkingDirectory(m_gitPath);
    bProc.start(QStringLiteral("git"), {QStringLiteral("branch"), QStringLiteral("--show-current")});
    if (bProc.waitForFinished(5000))
    {
        QString branch = bProc.readAllStandardOutput().trimmed();
        m_gitBranchLabel->setText(tr("Branch: %1").arg(branch.isEmpty() ? tr("Kein Repository") : branch));
    }

    QProcess proc;
    proc.setWorkingDirectory(m_gitPath);
    proc.start(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")});
    if (!proc.waitForFinished(5000))
    {
        return;
    }

    const QString out = proc.readAllStandardOutput().trimmed();
    const QString err = proc.readAllStandardError().trimmed();
    if (!err.isEmpty())
    {
        m_gitLog->append("<span style='color:#ff6b6b;'>Git: " + err + "</span>");
    }

    m_gitStatusList->clear();
    if (out.isEmpty())
    {
        m_gitStatusList->addItem(tr("✓ Keine ungespeicherten Änderungen"));
        return;
    }
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts))
    {
        if (line.length() < 3)
        {
            continue;
        }
        auto *item = new QListWidgetItem(QString("[%1] %2").arg(line.left(2), line.mid(3)), m_gitStatusList);
        Q_ASSERT(item != nullptr);
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
    {
        m_repoCombo->addItem(r.name.isEmpty() ? tr("(unbenannt)") : r.name);
    }
    m_blockSave = false;
}

void GitManagerDialog::loadRepoToFields(int index)
{
    m_blockSave = true;
    m_currentRepo = index;
    if (index >= 0 && index < m_repos.size())
    {
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
    if (m_currentRepo < 0 || m_currentRepo >= m_repos.size())
    {
        return;
    }
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
    if (m_repos.isEmpty())
    {
        m_currentRepo = -1;
    }
    else
    {
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
    if (!m_repos.isEmpty())
    {
        Config::setGitLocalDir(m_repos.first().localDir);
        Config::setGitRemoteUrl(m_repos.first().remoteUrl);
        Config::setGitUsername(m_repos.first().username);
    }
    emit settingsChanged();
}
