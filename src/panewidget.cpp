#include "panewidget.h"
#include "mainwindow.h"
#include "config.h"
#include "dialogutils.h"
#include "thememanager.h"
#include "scglobal.h"
#include "panewidgets.h"
#include <KFileItem>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/EmptyTrashJob>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KShortcutsDialog>
#include <KShortcutsEditor>
#include "agebadgedialog.h"
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <QActionGroup>
#include <QRadioButton>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QtConcurrent>

// --- PaneWidget ---
PaneWidget::PaneWidget(const QString &settingsKey, QWidget *parent)
    : QWidget(parent), m_settingsKey(settingsKey) {
  setStyleSheet(QString("background:%1;").arg(TM().colors().bgDeep));
  auto *rootLay = new QVBoxLayout(this);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  // --- Tab-Leiste mit Breadcrumb ---
  auto *tabBar = new QWidget();
  tabBar->setFixedHeight(46);
  tabBar->setStyleSheet(
      QString("background:%1; border:none;").arg(TM().colors().bgMain));
  auto *tabLay = new QHBoxLayout(tabBar);
  tabLay->setContentsMargins(4, 0, 4, 0);
  tabLay->setSpacing(2);

  auto *pathStack = new QStackedWidget();
  pathStack->setFixedHeight(26);
  pathStack->setStyleSheet(
      QString("background-color:%1; border:1px solid %2; border-radius:0px;")
          .arg(TM().colors().bgBox, TM().colors().borderAlt));

  m_pathEdit = new QLineEdit(QDir::homePath());
  m_pathEdit->setStyleSheet(
      QString("QLineEdit{background:transparent; border:none; color:%1; "
              "font-size:14px; padding:2px 8px; border-radius:0px;}")
          .arg(TM().colors().textPrimary));

  auto *breadcrumbBtn = new QPushButton(QDir::homePath());
  breadcrumbBtn->setStyleSheet(
      QString("QPushButton { background:transparent; border:none; color:%1; "
              "font-size:15px; font-weight:300; padding:2px 10px; "
              "border-radius:0px; }")
          .arg(TM().colors().textAccent));
  pathStack->addWidget(breadcrumbBtn);
  pathStack->addWidget(m_pathEdit);
  pathStack->setCurrentIndex(0);

  connect(breadcrumbBtn, &QPushButton::clicked, this, [pathStack, this]() {
    m_pathEdit->setText(currentPath());
    m_pathEdit->selectAll();
    pathStack->setCurrentIndex(1);
    m_pathEdit->setFocus();
  });

  auto commitPath = [pathStack, breadcrumbBtn, this]() {
    const QString p = m_pathEdit->text().trimmed();
    if (!p.isEmpty() && QFileInfo::exists(p))
      navigateTo(p);
    breadcrumbBtn->setText(currentPath());
    pathStack->setCurrentIndex(0);
  };
  connect(m_pathEdit, &QLineEdit::returnPressed, this, commitPath);
  connect(m_pathEdit, &QLineEdit::editingFinished, this,
          [pathStack, commitPath]() {
            if (pathStack->currentIndex() == 1)
              commitPath();
          });
  connect(this, &PaneWidget::pathUpdated, this,
          [breadcrumbBtn](const QString &p) {
            QUrl url(p);
            if (p == "__drives__") {
              breadcrumbBtn->setText(tr("Dieser PC"));
            } else {
              QString name = url.fileName();
              if (name.isEmpty())
                name = p;
              breadcrumbBtn->setText(name);
            }
          });

  m_millerToggle = new QToolButton();
  m_millerToggle->setFixedSize(24, 24);
  m_millerToggle->setCheckable(true);
  m_millerToggle->setChecked(true);
  m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
  m_millerToggle->setIconSize(QSize(14, 14));
  m_millerToggle->setToolTip(tr("Miller-Columns ein-/ausklappen"));
  m_millerToggle->setStyleSheet(TM().ssToolBtn());

  auto *searchBtn = new QToolButton();
  searchBtn->setFixedSize(30, 30);
  searchBtn->setIcon(QIcon::fromTheme("system-search"));
  searchBtn->setIconSize(QSize(18, 18));
  searchBtn->setToolTip(tr("Suchen"));
  searchBtn->setCheckable(true);
  searchBtn->setStyleSheet(TM().ssToolBtn());

  auto *layoutBtn = new QToolButton();
  layoutBtn->setFixedSize(30, 30);
  layoutBtn->setIcon(QIcon::fromTheme("view-split-left-right"));
  layoutBtn->setIconSize(QSize(18, 18));
  layoutBtn->setToolTip(tr("Layout wählen"));
  layoutBtn->setStyleSheet(TM().ssToolBtn());

  auto *hamburgerBtn = new QToolButton();
  hamburgerBtn->setFixedSize(30, 30);
  hamburgerBtn->setIcon(QIcon::fromTheme("application-menu"));
  hamburgerBtn->setIconSize(QSize(18, 18));
  hamburgerBtn->setToolTip(tr("Menü"));
  hamburgerBtn->setStyleSheet(TM().ssToolBtn() +
                              " QToolButton::menu-indicator { image: none; }");
  hamburgerBtn->setPopupMode(QToolButton::InstantPopup);

  // --- Hamburger-Menü ---
  auto *hamburgerMenu = new QMenu(hamburgerBtn);
  hamburgerMenu->setStyleSheet(TM().ssMenu());

  // Neu erstellen
  auto *menuNew = hamburgerMenu->addMenu(QIcon::fromTheme("folder-new"),
                                         tr("Neu erstellen"));
  menuNew->setStyleSheet(TM().ssMenu());
  auto *actNewFolder =
      menuNew->addAction(QIcon::fromTheme("folder-new"), tr("Ordner …"));
  auto *actNewText =
      menuNew->addAction(QIcon::fromTheme("text-plain"), tr("Textdatei …"));
  auto *actNewHtml =
      menuNew->addAction(QIcon::fromTheme("text-html"), tr("HTML-Datei …"));
  auto *actNewEmpty =
      menuNew->addAction(QIcon::fromTheme("document-new"), tr("Leere Datei …"));
  menuNew->addSeparator();
  auto *actNewLinkFile =
      menuNew->addAction(QIcon::fromTheme("inode-symlink"),
                         tr("Verknüpfung zu Datei oder Ordner …"));

  hamburgerMenu->addSeparator();

  // Toggle-Aktionen (direkt im Hauptmenü lassen, wie bei Dolphin)
  auto *actHidden = hamburgerMenu->addAction(QIcon::fromTheme("view-hidden"),
                                             tr("Versteckte Dateien anzeigen"));
  actHidden->setCheckable(true);
  actHidden->setChecked(Config::showHiddenFiles());
  connect(actHidden, &QAction::toggled, this, [](bool on) {
    Config::setShowHiddenFiles(on);
    if (auto *mw = MW()) {
      emit mw->sidebar()->hiddenFilesChanged(on);
      mw->leftPane()->filePane()->setShowHiddenFiles(on);
      mw->rightPane()->filePane()->setShowHiddenFiles(on);
      mw->leftPane()->miller()->refresh();
      mw->rightPane()->miller()->refresh();
    }
  });

  auto *actSingleClick = hamburgerMenu->addAction(
      QIcon::fromTheme("input-mouse"), tr("Einfachklick zum Öffnen"));
  actSingleClick->setCheckable(true);
  actSingleClick->setChecked(Config::singleClickOpen());
  connect(actSingleClick, &QAction::toggled, this, [](bool on) {
    Config::setSingleClickOpen(on);
    if (auto *mw = MW())
      emit mw->sidebar()->settingsChanged();
  });

  auto *actExtensions = hamburgerMenu->addAction(
      QIcon::fromTheme("text-x-generic"), tr("Dateiendungen anzeigen"));
  actExtensions->setCheckable(true);
  actExtensions->setChecked(Config::showFileExtensions());
  connect(actExtensions, &QAction::toggled, this, [](bool on) {
    Config::setShowFileExtensions(on);
    if (auto *mw = MW()) {
      mw->leftPane()->filePane()->setRootPath(mw->leftPane()->currentPath());
      mw->rightPane()->filePane()->setRootPath(mw->rightPane()->currentPath());
    }
  });

  auto *menuTerminal = hamburgerMenu->addMenu(
      QIcon::fromTheme("utilities-terminal"), tr("Terminal"));
  menuTerminal->setStyleSheet(TM().ssMenu());

  menuTerminal->addAction(QIcon::fromTheme("utilities-terminal"),
                          tr("Im Terminal öffnen"), this, [this]() {
                            const QString path = this->currentPath();
                            auto *job = new KTerminalLauncherJob(QString());
                            job->setWorkingDirectory(path.isEmpty() ? QDir::homePath() : path);
                            job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
                            job->start();
                          });

  hamburgerMenu->addSeparator();

  // --- Einrichten Sub-Menü (Dolphin-Style) ---
  auto *menuConfigure =
      hamburgerMenu->addMenu(QIcon::fromTheme("configure"), tr("Einrichten"));
  menuConfigure->setStyleSheet(TM().ssMenu());

  // 1. Fenster-Farbschema (Theme)
  auto *menuTheme = menuConfigure->addMenu(
      QIcon::fromTheme("preferences-desktop-color"), tr("Fenster-Farbschema"));
  menuTheme->setStyleSheet(TM().ssMenu());

  // Widget-Inhalt des Untermenüs (Themen-Auswahl)
  auto *themeWidget = new QWidget();
  themeWidget->setStyleSheet(
      QString(
          "QWidget { background:%1; }"
          "QCheckBox { color:%2; font-size:11px; background:transparent; }"
          "QCheckBox::indicator { width:14px; height:14px; border:1px solid "
          "%3; border-radius:2px; background:transparent; }"
          "QCheckBox::indicator:checked { background:%4; border-color:%4; }"
          "QLabel#hint { color:%5; font-size:10px; background:transparent; }"
          "QRadioButton { color:%2; font-size:11px; background:transparent; "
          "padding:4px 0; }"
          "QRadioButton::indicator { width:14px; height:14px; }"
          "QRadioButton:disabled { color:%5; }")
          .arg(TM().colors().bgPanel, TM().colors().textLight,
               TM().colors().borderAlt, TM().colors().accent,
               TM().colors().textMuted));

  auto *twLay = new QVBoxLayout(themeWidget);
  twLay->setContentsMargins(16, 10, 16, 4);
  twLay->setSpacing(4);

  auto *twSysCheck =
      new QCheckBox(tr("KDE Global Theme verwenden"), themeWidget);
  twSysCheck->setChecked(Config::useSystemTheme());
  auto *twHint =
      new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."),
                 themeWidget);
  twHint->setObjectName("hint");
  twHint->setWordWrap(true);
  twLay->addWidget(twSysCheck);
  twLay->addWidget(twHint);

  auto *twSep = new QFrame(themeWidget);
  twSep->setFrameShape(QFrame::HLine);
  twSep->setStyleSheet(QString("background:%1;").arg(TM().colors().separator));
  twSep->setFixedHeight(1);
  twLay->addSpacing(4);
  twLay->addWidget(twSep);
  twLay->addSpacing(4);

  auto *twThemeGroup = new QButtonGroup(themeWidget);
  auto *twThemesContainer = new QWidget(themeWidget);
  auto *twThemesLay = new QVBoxLayout(twThemesContainer);
  twThemesLay->setContentsMargins(0, 0, 0, 0);
  twThemesLay->setSpacing(2);
  twLay->addWidget(twThemesContainer);

  auto refreshThemeList = [twThemesLay, twThemeGroup, twThemesContainer,
                           themeWidget]() {
    QLayoutItem *child;
    while ((child = twThemesLay->takeAt(0)) != nullptr) {
      if (child->widget())
        child->widget()->deleteLater();
      delete child;
    }
    const QString curTheme = Config::selectedTheme();
    const auto allThemes = TM().allThemes();
    const auto &colors = TM().colors();
    themeWidget->setObjectName("themeContainer");
    themeWidget->setStyleSheet(
        QString("#themeContainer { background:%1; }"
                "QWidget { color:%2; font-size:11px; background:transparent; }"
                "QPushButton { background:%3; border:1px solid %4; "
                "border-radius:4px; padding:6px; color:%2; }"
                "QPushButton:hover { background:%5; }")
            .arg(colors.bgList, colors.textPrimary, colors.bgHover,
                 colors.borderAlt, colors.bgSelect));

    for (int i = 0; i < allThemes.size(); ++i) {
      const auto &t = allThemes.at(i);
      auto *rb = new QRadioButton(t.name, twThemesContainer);
      rb->setStyleSheet(QString("QRadioButton { color: %1; spacing: 8px; }"
                                "QRadioButton::indicator { width: 14px; "
                                "height: 14px; border-radius: 7px; border: 1px "
                                "solid %2; background: transparent; }"
                                "QRadioButton::indicator:checked { background: "
                                "%3; border: 2px solid %3; }"
                                "QRadioButton:disabled { color: %4; }")
                            .arg(colors.textPrimary, colors.borderAlt,
                                 colors.accent, colors.textMuted));
      rb->setChecked(!Config::useSystemTheme() && t.name == curTheme);
      rb->setEnabled(!Config::useSystemTheme());
      twThemeGroup->addButton(rb, i);
      twThemesLay->addWidget(rb);
    }
    themeWidget->adjustSize();
  };
  connect(menuTheme, &QMenu::aboutToShow, themeWidget, refreshThemeList);
  refreshThemeList();

  auto *btnReload = new QPushButton(QIcon::fromTheme("view-refresh"),
                                    tr("Designs neu laden"), themeWidget);
  btnReload->setStyleSheet(
      "QPushButton{margin-top:8px; font-size:10px; padding:4px;}");
  twLay->addWidget(btnReload);
  connect(btnReload, &QPushButton::clicked, themeWidget, [refreshThemeList]() {
    TM().loadExternalThemes();
    refreshThemeList();
  });

  auto *twBtnRow = new QWidget(themeWidget);
  auto *twBtnLay = new QHBoxLayout(twBtnRow);
  twBtnLay->setContentsMargins(0, 0, 0, 0);
  twBtnLay->setSpacing(8);
  twBtnLay->addStretch();
  auto *twCancel = new QPushButton(tr("Abbrechen"), twBtnRow);
  twCancel->setFixedWidth(90);
  auto *twApply = new QPushButton(tr("Übernehmen"), twBtnRow);
  twApply->setObjectName("applyBtn");
  twApply->setFixedWidth(110);
  twApply->setStyleSheet(QString("QPushButton{background:%1; color:%2; "
                                 "font-weight:bold; border-color:%1;}")
                             .arg(TM().colors().accent, TM().colors().bgMain));
  twBtnLay->addWidget(twCancel);
  twBtnLay->addWidget(twApply);
  twLay->addWidget(twBtnRow);

  auto *twAction = new QWidgetAction(menuTheme);
  twAction->setDefaultWidget(themeWidget);
  menuTheme->addAction(twAction);

  connect(twCancel, &QPushButton::clicked, menuTheme, &QMenu::close);
  connect(twApply, &QPushButton::clicked, this,
          [this, twSysCheck, twThemeGroup, menuTheme, hamburgerMenu]() {
            Config::setUseSystemTheme(twSysCheck->isChecked());
            if (!twSysCheck->isChecked()) {
              int idx = twThemeGroup->checkedId();
              const auto allThemes = TM().allThemes();
              if (idx >= 0 && idx < allThemes.size())
                Config::setSelectedTheme(allThemes.at(idx).name);
            }
            menuTheme->close();
            hamburgerMenu->close();

            DialogUtils::message(
                this, tr("Neustart erforderlich"),
                tr("Das Theme wird nach einem Neustart angewendet."));
            QProcess::startDetached(QApplication::applicationFilePath(),
                                    QApplication::arguments());
            QApplication::quit();
          });

  // 2. Tastaturkurzbefehle festlegen ...
  auto *actShortcuts =
      menuConfigure->addAction(QIcon::fromTheme("configure-shortcuts"),
                               tr("Tastaturkurzbefehle festlegen …"));
  connect(actShortcuts, &QAction::triggered, this, [this]() {
    if (auto *mw = MW())
      KShortcutsDialog::showDialog(mw->actionCollection(),
                                   KShortcutsEditor::LetterShortcutsAllowed,
                                   this);
  });

  // 3. Altersbadges
  auto *actAgeBadge = menuConfigure->addAction(QIcon::fromTheme("chronometer"),
                                               tr("Altersbadges"));
  connect(actAgeBadge, &QAction::triggered, this,
          [this]() { (new AgeBadgeDialog(this))->open(); });

  hamburgerMenu->addSeparator();

  // Über SplitCommander
  auto *actAbout = hamburgerMenu->addAction(QIcon::fromTheme("help-about"),
                                            tr("Über SplitCommander"));
  connect(actAbout, &QAction::triggered, this, [this]() {
    KAboutData aboutData(
        QStringLiteral("splitcommander"),
        tr("SplitCommander"),
        QCoreApplication::applicationVersion(),
        tr("Nativer Dual-Pane-Dateimanager für Linux mit KDE Plasma.\n"
           "Miller-Column-Interface mit KIO-Integration für lokale und Remote-Dateisysteme."),
        KAboutLicense::GPL_V3,
        tr("© 2025–2026 D. Lange"),
        tr("Unterstützt Google Drive, SFTP, SMB, MTP und weitere KIO-Protokolle.\n"
           "Inspiriert von OneCommander."),
        QStringLiteral("https://github.com/daniell1904/SplitCommander"),
        QStringLiteral("https://github.com/daniell1904/SplitCommander/issues")
    );
    aboutData.addAuthor(
        QStringLiteral("D. Lange"),
        tr("Entwickler und Maintainer"),
        QStringLiteral(""),
        QStringLiteral("https://github.com/daniell1904")
    );
    aboutData.setOrganizationDomain("github.com/daniell1904");
    aboutData.setDesktopFileName(QStringLiteral("splitcommander"));

    KAboutApplicationDialog *dlg = new KAboutApplicationDialog(aboutData, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumHeight(480);
    dlg->exec();
  });

  hamburgerBtn->setMenu(hamburgerMenu);

  tabLay->addWidget(m_millerToggle);
  tabLay->addWidget(pathStack, 1);
  tabLay->addWidget(searchBtn);
  tabLay->addWidget(layoutBtn);
  tabLay->addWidget(hamburgerBtn);
  rootLay->addWidget(tabBar);

  // --- Layout-Button Connect ---
  connect(layoutBtn, &QToolButton::clicked, this, [this, layoutBtn]() {
    auto *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    const auto &c = TM().colors();
    popup->setStyleSheet(
        TM().ssDialog() +
        QString("QPushButton { background:%1; border:1px solid %2; "
                "color:%3;"
                " border-radius:4px; padding:8px; font-size:10px; }"
                "QPushButton:hover { background:%4; border-color:%5; }"
                "QPushButton:checked { background:%4; border:2px solid %5; "
                "color:%6; }")
            .arg(c.bgInput, c.borderAlt, c.textPrimary, c.bgHover, c.accent,
                 c.textAccent));

    auto *lay2 = new QHBoxLayout(popup);
    lay2->setContentsMargins(8, 8, 8, 8);
    lay2->setSpacing(6);

    struct ModeEntry {
      QString label, sub, icon;
      int mode;
    };
    const QList<ModeEntry> modes = {
        {tr("Klassisch"), tr("Einzeln"), "view-list-details", 0},
        {tr("Standard"), tr("Dual"), "view-split-left-right", 1},
        {tr("Spalten"), tr("Dual"), "view-split-top-bottom", 2},
    };

    auto *grp = new QButtonGroup(popup);
    auto s = Config::group("UI");

    int current = s.readEntry("layoutMode", 1);

    for (const auto &entry : modes) {
      auto *btn = new QPushButton();
      btn->setCheckable(true);
      btn->setChecked(entry.mode == current);
      btn->setFixedSize(72, 68);

      auto *vl = new QVBoxLayout(btn);
      vl->setContentsMargins(4, 6, 4, 4);
      vl->setSpacing(3);
      auto *ic = new QLabel();
      ic->setPixmap(QIcon::fromTheme(entry.icon).pixmap(24, 24));
      ic->setAlignment(Qt::AlignCenter);
      ic->setStyleSheet("background:transparent;border:none;");
      auto *lb1 = new QLabel(entry.label);
      lb1->setAlignment(Qt::AlignCenter);
      lb1->setStyleSheet("background:transparent;border:none;font-weight:bold;"
                         "font-size:10px;");
      auto *lb2 = new QLabel(entry.sub);
      lb2->setAlignment(Qt::AlignCenter);
      lb2->setStyleSheet(
          "background:transparent;border:none;color:#4c566a;font-size:9px;");
      vl->addWidget(ic);
      vl->addWidget(lb1);
      vl->addWidget(lb2);

      grp->addButton(btn, entry.mode);
      lay2->addWidget(btn);

      connect(btn, &QPushButton::clicked, this, [this, popup, entry]() {
        auto ss = Config::group("UI");

        ss.writeEntry("layoutMode", entry.mode);
        ss.config()->sync();
        emit layoutChangeRequested(entry.mode);
        popup->close();
      });
    }

    popup->move(layoutBtn->mapToGlobal(QPoint(0, layoutBtn->height() + 2)));
    popup->exec();
  });

  // --- Hamburger-Connects ---
  connect(actNewFolder, &QAction::triggered, this,
          [this]() { emit newFolderRequested(); });

  connect(actNewText, &QAction::triggered, this, [this]() {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (!mw)
      return;
    const QString dir = mw->activePane()->currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Neue Textdatei"), tr("Name:"),
                                        tr("Neue Datei.txt"), &ok);
    if (ok && !name.isEmpty()) {
      QFile f(dir + "/" + name);
      (void)f.open(QIODevice::WriteOnly);
    }
  });

  connect(actNewHtml, &QAction::triggered, this, [this]() {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (!mw)
      return;
    const QString dir = mw->activePane()->currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Neue HTML-Datei"),
                                        tr("Name:"), tr("index.html"), &ok);
    if (!ok || name.isEmpty())
      return;
    QFile f(dir + "/" + name);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
      f.write("<!DOCTYPE html>\n<html>\n<head><meta "
              "charset=\"utf-8\"><title></title></head>\n<body>\n\n</body>\n</"
              "html>\n");
  });

  connect(actNewEmpty, &QAction::triggered, this, [this]() {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (!mw)
      return;
    const QString dir = mw->activePane()->currentPath();
    bool ok;
    QString name = DialogUtils::getText(this, tr("Leere Datei"), tr("Name:"),
                                        tr("Neue Datei"), &ok);
    if (ok && !name.isEmpty()) {
      QFile f(dir + "/" + name);
      (void)f.open(QIODevice::WriteOnly);
    }
  });

  connect(actNewLinkFile, &QAction::triggered, this, [this]() {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (!mw)
      return;
    const QString dir = mw->activePane()->currentPath();
    QString target =
        QFileDialog::getExistingDirectory(this, tr("Ziel waehlen"), dir);
    if (target.isEmpty())
      target = QFileDialog::getOpenFileName(this, tr("Ziel waehlen"), dir);
    if (target.isEmpty())
      return;
    bool ok;
    QString name = DialogUtils::getText(this, tr("Verknuepfungsname"),
                                        tr("Name:"), tr("Link"), &ok);
    if (ok && !name.isEmpty()) {
      auto *job = KIO::symlink(target, QUrl::fromLocalFile(dir + "/" + name), KIO::HideProgressInfo);
      job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    }
  });

  // (Connections were already set up above)

  // --- Such-Panel ---
  auto *searchPanel = new QWidget();
  searchPanel->setStyleSheet(TM().ssSearchPanel());
  searchPanel->hide();
  auto *spVLay = new QVBoxLayout(searchPanel);
  spVLay->setContentsMargins(0, 0, 0, 0);
  spVLay->setSpacing(0);

  auto *spTopRow = new QWidget();
  spTopRow->setFixedHeight(36);
  spTopRow->setStyleSheet(
      QString("background:%1;border-bottom:1px solid %2;")
          .arg(TM().colors().bgPanel, TM().colors().separator));
  auto *spLay = new QHBoxLayout(spTopRow);
  spLay->setContentsMargins(6, 4, 6, 4);
  spLay->setSpacing(4);

  auto *searchEdit = new QLineEdit();
  searchEdit->setPlaceholderText(tr("Suchen ..."));
  searchEdit->setStyleSheet(
      QString("QLineEdit{background:%1;border:1px solid %2;color:%3;")
          .arg(TM().colors().bgMain, TM().colors().accent,
               TM().colors().textPrimary) +
      "font-size:11px;padding:2px 6px;border-radius:2px;}");
  searchEdit->setClearButtonEnabled(true);

  auto *filterBtn = new QToolButton();
  filterBtn->setText(tr("Filtern"));
  filterBtn->setIcon(QIcon::fromTheme("view-filter"));
  filterBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  filterBtn->setIconSize(QSize(14, 14));
  filterBtn->setPopupMode(QToolButton::MenuButtonPopup);
  filterBtn->setStyleSheet(
      QString("QToolButton{background:%1;border:1px solid %2;color:%3;"
              "font-size:11px;padding:2px 6px;border-radius:2px;}"
              "QToolButton:hover{border-color:%4;}"
              "QToolButton::menu-indicator{image:none;}"
              "QToolButton::menu-button{border-left:1px solid %2;width:14px;}")
          .arg(TM().colors().bgBox, TM().colors().borderAlt,
               TM().colors().textPrimary, TM().colors().accent));

  auto *filterMenu = new QMenu(filterBtn);
  mw_applyMenuShadow(filterMenu);
  filterMenu->setStyleSheet(TM().ssMenu());
  auto *actNames = filterMenu->addAction(tr("Dateinamen"));
  auto *actContent = filterMenu->addAction(tr("Dateiinhalt"));
  actNames->setCheckable(true);
  actNames->setChecked(true);
  actContent->setCheckable(true);
  auto *filterGroup = new QActionGroup(filterMenu);
  filterGroup->addAction(actNames);
  filterGroup->addAction(actContent);
  filterGroup->setExclusive(true);
  filterMenu->addSeparator();
  filterMenu->addAction(QIcon::fromTheme("system-search"), tr("KFind öffnen"),
                        this, []() { QProcess::startDetached("kfind", {}); });
  filterMenu->addAction(
      QIcon::fromTheme("configure"), tr("Sucheinstellungen"), this,
      []() { QProcess::startDetached("kcmshell6", {"kcm_baloofile"}); });
  filterBtn->setMenu(filterMenu);

  auto searchByName = std::make_shared<bool>(true);
  connect(actNames, &QAction::toggled, this,
          [searchByName](bool on) { *searchByName = on; });
  connect(actContent, &QAction::toggled, this,
          [searchByName](bool on) { *searchByName = !on; });

  auto *searchCloseBtn = new QToolButton();
  searchCloseBtn->setIcon(QIcon::fromTheme("window-close"));
  searchCloseBtn->setIconSize(QSize(12, 12));
  searchCloseBtn->setFixedSize(20, 20);
  searchCloseBtn->setStyleSheet(
      "QToolButton{background:transparent;border:none;color:#bf616a;border-"
      "radius:10px;}"
      "QToolButton:hover{background:#bf616a;color:#eceff4;}");

  spLay->addWidget(searchEdit, 1);
  spLay->addWidget(filterBtn);
  spLay->addWidget(searchCloseBtn);
  spVLay->addWidget(spTopRow);

  auto *spTabRow = new QWidget();
  spTabRow->setFixedHeight(28);
  spTabRow->setStyleSheet(
      QString("background:%1;border-bottom:1px solid %2;")
          .arg(TM().colors().bgPanel, TM().colors().separator));
  spTabRow->hide();
  auto *spTabLay = new QHBoxLayout(spTabRow);
  spTabLay->setContentsMargins(6, 0, 6, 0);
  spTabLay->setSpacing(0);
  auto mkTab = [](const QString &lbl) {
    auto *b = new QToolButton();
    b->setText(lbl);
    b->setCheckable(true);
    b->setStyleSheet(
        QString("QToolButton{background:transparent;border:none;color:%1;"
                "font-size:11px;padding:2px 10px;border-bottom:2px solid "
                "transparent;}"
                "QToolButton:checked{color:%2;border-bottom:2px solid %3;}"
                "QToolButton:hover{color:%2;}")
            .arg(TM().colors().textMuted, TM().colors().textAccent,
                 TM().colors().accent));
    return b;
  };
  auto *tabHere = mkTab(tr("Ab hier"));
  auto *tabOverall = mkTab(tr("Überall"));
  tabOverall->setChecked(true);
  auto *tabGrp = new QButtonGroup(spTabRow);
  tabGrp->addButton(tabHere);
  tabGrp->addButton(tabOverall);
  tabGrp->setExclusive(true);
  spTabLay->addWidget(tabHere);
  spTabLay->addWidget(tabOverall);
  spTabLay->addStretch();
  spVLay->addWidget(spTabRow);
  rootLay->addWidget(searchPanel);

  // --- Suchergebnis-Overlay ---
  auto *searchOverlay = new QWidget(this);
  searchOverlay->hide();
  searchOverlay->setStyleSheet(
      QString("background:%1;border:1px solid %2;border-top:none;")
          .arg(TM().colors().bgBox, TM().colors().separator));
  auto *ovLay = new QVBoxLayout(searchOverlay);
  ovLay->setContentsMargins(1, 1, 1, 1);
  ovLay->setSpacing(0);

  auto *searchResults = new QTreeWidget(searchOverlay);
  searchResults->setHeaderLabels({tr("Name"), tr("Pfad"), tr("Geändert")});
  searchResults->setRootIsDecorated(false);
  searchResults->setStyleSheet(
      QString(
          "QTreeWidget{background:%1;border:none;color:%2;font-size:11px;"
          "outline:none;}"
          "QTreeWidget::item{padding:4px 4px;border-bottom:1px solid %3;}"
          "QTreeWidget::item:selected{background:%4;color:%5;}"
          "QTreeWidget::item:hover{background:%6;}"
          "QHeaderView::section{background:%7;color:%8;border:none;"
          "border-bottom:1px solid %3;padding:3px 8px;font-size:10px;}"
          "QTreeWidget "
          "QScrollBar:vertical{width:0px;background:transparent;border:none;}"
          "QTreeWidget "
          "QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-"
          "radius:2px;min-height:20px;}"
          "QTreeWidget:hover "
          "QScrollBar::handle:vertical{background:rgba(255,255,255,40);}"
          "QTreeWidget QScrollBar::add-line:vertical,QTreeWidget "
          "QScrollBar::sub-line:vertical{height:0;}"
          "QTreeWidget "
          "QScrollBar:horizontal{height:0px;background:transparent;border:none;"
          "}"
          "QTreeWidget "
          "QScrollBar::handle:horizontal{background:rgba(255,255,255,0);border-"
          "radius:2px;min-width:20px;}"
          "QTreeWidget:hover "
          "QScrollBar::handle:horizontal{background:rgba(255,255,255,40);}"
          "QTreeWidget QScrollBar::add-line:horizontal,QTreeWidget "
          "QScrollBar::sub-line:horizontal{width:0;}")
          .arg(TM().colors().bgList, TM().colors().textPrimary,
               TM().colors().separator, TM().colors().bgSelect,
               TM().colors().textLight, TM().colors().bgHover,
               TM().colors().bgPanel, TM().colors().textMuted));
  searchResults->header()->setStretchLastSection(false);
  searchResults->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  searchResults->header()->setSectionResizeMode(1,
                                                QHeaderView::ResizeToContents);
  searchResults->header()->setSectionResizeMode(2,
                                                QHeaderView::ResizeToContents);
  ovLay->addWidget(searchResults, 1);

  // Suchpanel-Verbindungen
  connect(searchBtn, &QToolButton::toggled, this,
          [searchPanel, searchEdit, spTabRow, searchOverlay, this](bool on) {
            searchPanel->setVisible(on);
            if (on) {
              searchEdit->clear();
              searchEdit->setFocus();
            } else {
              searchEdit->clear();
              m_filePane->setNameFilter(QString());
              spTabRow->hide();
              searchOverlay->hide();
            }
          });
  connect(searchCloseBtn, &QToolButton::clicked, this, [searchBtn, this]() {
    searchBtn->setChecked(false);
    m_filePane->setNameFilter(QString());
  });
  connect(searchEdit, &QLineEdit::textChanged, this,
          [this, searchByName](const QString &text) {
            if (!*searchByName)
              return;
            m_filePane->setNameFilter(text);
          });
  connect(
      searchEdit, &QLineEdit::returnPressed, this,
      [this, searchEdit, searchResults, searchOverlay, spTabRow]() {
        const QString term = searchEdit->text().trimmed();
        if (term.isEmpty())
          return;
        searchResults->clear();
        auto *loading = new QTreeWidgetItem(searchResults);
        loading->setText(0, tr("Suche läuft..."));

        const QPoint topLeft = m_vSplit->mapTo(this, QPoint(0, 0));
        searchOverlay->setGeometry(topLeft.x(), topLeft.y(), m_vSplit->width(),
                                   qMin(300, m_vSplit->height()));
        searchOverlay->show();
        searchOverlay->raise();
        spTabRow->show();

        if (m_searchWatcher) {
          m_searchWatcher->cancel();
          m_searchWatcher->deleteLater();
          m_searchWatcher = nullptr;
        }

        Baloo::Query query;
        query.setSearchString(term);
        query.setLimit(200);

        auto *watcher = new QFutureWatcher<QStringList>(this);
        m_searchWatcher = watcher;
        connect(watcher, &QFutureWatcher<QStringList>::finished, this,
                [this, watcher, searchResults]() {
                  if (watcher != m_searchWatcher) {
                    watcher->deleteLater();
                    return;
                  }
                  m_searchWatcher = nullptr;
                  const QStringList paths = watcher->result();
                  watcher->deleteLater();
                  searchResults->clear();
                  if (paths.isEmpty()) {
                    auto *empty = new QTreeWidgetItem(searchResults);
                    empty->setText(0, tr("Keine Ergebnisse"));
                    return;
                  }
                  for (const QString &path : paths) {
                    const QFileInfo fi(path);
                    const QUrl url = QUrl::fromLocalFile(path);
                    auto *it = new QTreeWidgetItem(searchResults);
                    it->setIcon(0, QIcon::fromTheme(KIO::iconNameForUrl(url)));
                    it->setText(0, fi.fileName());
                    it->setText(1, QString("~/%1").arg(
                                       QDir::home().relativeFilePath(fi.absolutePath())));
                    it->setText(2, fi.lastModified().toString("dd.MM.yy"));
                    it->setData(0, Qt::UserRole, path);
                  }
                  if (searchResults->topLevelItemCount() == 0) {
                    auto *empty = new QTreeWidgetItem(searchResults);
                    empty->setText(0, tr("Keine Ergebnisse"));
                  }
                });
        watcher->setFuture(QtConcurrent::run([query]() mutable -> QStringList {
          QStringList results;
          Baloo::ResultIterator it = query.exec();
          while (it.next())
            results << it.filePath();
          return results;
        }));
      });
  connect(
      searchResults, &QTreeWidget::itemClicked, this,
      [this, searchBtn, searchOverlay, searchEdit](QTreeWidgetItem *it, int) {
        const QString path = it->data(0, Qt::UserRole).toString();
        if (path.isEmpty())
          return;
        searchEdit->clear();
        m_filePane->setNameFilter(QString());
        navigateTo(QFileInfo(path).isDir() ? path
                                           : QFileInfo(path).absolutePath());
        searchOverlay->hide();
        searchBtn->setChecked(false);
      });

  // --- Vertikaler Splitter ---
  m_vSplit = new QSplitter(Qt::Vertical);
  m_vSplit->setChildrenCollapsible(true);
  m_vSplit->setHandleWidth(4);
  m_vSplit->setStyleSheet(QString("QSplitter::handle { background:%1; }"
                                  "QSplitter::handle:hover { background:%2; }"
                                  "QSplitter { background:%3; }")
                              .arg(TM().colors().splitter,
                                   TM().colors().colActive,
                                   TM().colors().bgDeep));
  rootLay->addWidget(m_vSplit, 1);
  m_vSplit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_vSplit->setMinimumHeight(200);
 
  m_miller = new MillerArea();
  m_miller->setMinimumHeight(150);
  m_miller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_vSplit->addWidget(m_miller);
 
  auto *lowerWidget = new QWidget();
  lowerWidget->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgDeep));
  auto *lowerLay = new QVBoxLayout(lowerWidget);
  lowerLay->setContentsMargins(0, 0, 0, 0);
  lowerLay->setSpacing(0);
  m_toolbar = new PaneToolbar();
  m_filePane = new FilePane(nullptr, m_settingsKey);
  m_filePane->setStyleSheet(
      QString("border:none;background:%1;").arg(TM().colors().bgDeep));
  auto *updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  updateTimer->setInterval(50); // 50ms debounce
  connect(updateTimer, &QTimer::timeout, this, [this]() {
    const int count =
        m_filePane->view()->model()->rowCount(m_filePane->view()->rootIndex());
    const qint64 sz = m_filePane->currentTotalSize();
    m_toolbar->setCount(count, sz);
    refreshFooter(currentPath(),
                 m_filePane->view()->selectionModel()->selectedRows().count());
  });
  connect(m_filePane, &FilePane::modelUpdated, this,
          [updateTimer]() { updateTimer->start(); });
  lowerLay->addWidget(m_toolbar);
  lowerLay->addWidget(m_filePane, 1);
  m_vSplit->addWidget(lowerWidget);
  m_vSplit->setSizes({200, 450});
  m_vSplit->setStretchFactor(0, 0);
  m_vSplit->setStretchFactor(1, 1);

  // Gespeicherte Position wiederherstellen (pane-spezifischer Key)
  {
    auto s = Config::group("UI");
    const QString key = m_settingsKey + "/vSplitState";
    const QByteArray state = s.readEntry(key, QByteArray());
    if (!state.isEmpty())
      m_vSplit->restoreState(state);
  }
 
  // Position beim Verschieben speichern — nur als Backup bei Drag
  connect(m_vSplit, &QSplitter::splitterMoved, this, [](int, int) {
    // m_millerCollapsed nicht über splitterMoved setzen — nur über Toggle
    // State wird beim App-Beenden in saveState() gespeichert
  });
 
  connect(m_millerToggle, &QToolButton::toggled, this,
          [this](bool checked) {
            if (checked) {
              // Aufklappen: gespeicherte Größe wiederherstellen
              auto s = Config::group("UI");
              const QByteArray saved =
                  s.readEntry(m_settingsKey + "/vSplitState", QByteArray());
              if (!saved.isEmpty()) {
                m_vSplit->restoreState(saved);
                if (m_vSplit->sizes().value(0) == 0)
                  m_vSplit->setSizes({200, 450});
              } else {
                m_vSplit->setSizes({200, 450});
              }
 
              m_millerToggle->setIcon(QIcon::fromTheme("go-up"));
              m_millerToggle->setToolTip("Miller-Columns ausklappen");
              m_millerCollapsed = false;
            } else {
              // Einklappen: aktuelle Größe vorher sichern
              if (m_vSplit->sizes().value(0) > 0) {
                auto s = Config::group("UI");
                s.writeEntry(m_settingsKey + "/vSplitState",
                             m_vSplit->saveState());
                s.config()->sync();
              }
 
              m_vSplit->setSizes({0, 1});
              m_millerToggle->setIcon(QIcon::fromTheme("go-down"));
              m_millerToggle->setToolTip("Miller-Columns einblenden");
              m_millerCollapsed = true;
            }
          });
 
  // Miller-Toggle-State beim Start setzen
  {
    auto s = Config::group("UI");
    if (s.readEntry(m_settingsKey + "/millerCollapsed", false)) {
      m_millerCollapsed = true;
      m_millerToggle->blockSignals(true);
      m_millerToggle->setChecked(false);
      m_millerToggle->setIcon(QIcon::fromTheme("go-down"));
      m_millerToggle->setToolTip("Miller-Columns einblenden");
      m_millerToggle->blockSignals(false);
      m_vSplit->setSizes({0, 1});
    }
  }

  // Verbindungen
  connect(m_miller, &MillerArea::pathChanged, this,
          [this](const QString &path) { navigateTo(path, true, false); });
  connect(m_miller, &MillerArea::kioPathRequested, this,
          [this](const QString &path) { navigateTo(path); });
  connect(m_miller, &MillerArea::openInLeft, this,
          &PaneWidget::openInLeftRequested);
  connect(m_miller, &MillerArea::openInRight, this,
          &PaneWidget::openInRightRequested);
  connect(m_miller, &MillerArea::propertiesRequested, this,
          [](const QString &path) {
            auto *dlg =
                new KPropertiesDialog(QUrl::fromUserInput(path), nullptr);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
          });
  connect(m_miller, &MillerArea::focusRequested, this,
          &PaneWidget::focusRequested);
  connect(m_miller, &MillerArea::headerClicked, this,
          [pathStack, this](const QString &path) {
            emit focusRequested();
            if (path == "__drives__") {
              m_miller->navigateTo("__drives__");
            } else if (!path.isEmpty()) {
              navigateTo(path);
            } else {
              m_pathEdit->setText(currentPath());
              m_pathEdit->selectAll();
              pathStack->setCurrentIndex(1);
              m_pathEdit->setFocus();
            }
          });
  connect(m_filePane, &FilePane::selectionChanged, this,
          [this](int count, const QString &path) {
            emit focusRequested();
            refreshFooter(path, count);
          });
  connect(m_filePane, &FilePane::focusRequested, this,
          &PaneWidget::focusRequested);
  connect(m_filePane, &FilePane::fileActivated, this,
          [this](const QString &path) {
            emit focusRequested();
            // KIO-URL oder lokales Verzeichnis
            const bool isKio = !path.startsWith("/") && path.contains(":/");
            if (isKio || QFileInfo(path).isDir()) {
              navigateTo(path);
            } else {
              auto *job = new KIO::OpenUrlJob(QUrl::fromUserInput(path));
              job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
              job->start();
            }
          });

  // Toolbar-Verbindungen
  connect(m_toolbar, &PaneToolbar::newFolderClicked, this, [this]() {
    bool ok;
    QString name = DialogUtils::getText(
        this, tr("Neuer Ordner"), tr("Ordnername:"), tr("Neuer Ordner"), &ok);
    if (!ok || name.isEmpty())
      return;
    auto *job = KIO::mkdir(QUrl::fromLocalFile(currentPath() + "/" + name));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    connect(job, &KJob::result, this, [this, job]() {
      if (job->error())
        DialogUtils::message(this, tr("Fehler"), tr("Ordner konnte nicht erstellt werden."));
    });
  });
  connect(m_toolbar, &PaneToolbar::deleteClicked, this, [this]() {
    emit filePane()->deleteRequested();
  });

  connect(m_toolbar, &PaneToolbar::emptyTrashClicked, this, [this]() {
    if (!DialogUtils::question(
            this, tr("Papierkorb leeren"),
            tr("Möchten Sie den Papierkorb wirklich leeren?")))
      return;
    auto *job = KIO::emptyTrash();
    job->start();
    if (job->uiDelegate())
      job->uiDelegate()->setAutoErrorHandlingEnabled(true);
  });
  connect(m_toolbar, &PaneToolbar::copyClicked, this, [this]() {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (!mw)
      return;
    auto *src = mw->activePane()->filePane();
    auto *dest =
        (mw->activePane() == mw->leftPane()) ? mw->rightPane() : mw->leftPane();
    const QList<QUrl> urls = src->selectedUrls();
    if (urls.isEmpty() || dest->currentPath().isEmpty())
      return;
    auto *job = KIO::copy(urls, QUrl::fromLocalFile(dest->currentPath()),
                          KIO::DefaultFlags);
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    if (mw)
      mw->registerJob(job, tr("Kopiere Dateien..."));
  });

  connect(m_toolbar, &PaneToolbar::sortClicked, this, [this]() {
    auto *hdr = m_filePane->view()->header();
    m_filePane->view()->sortByColumn(
        hdr->sortIndicatorSection(),
        hdr->sortIndicatorOrder() == Qt::AscendingOrder ? Qt::DescendingOrder
                                                        : Qt::AscendingOrder);
  });
  connect(m_toolbar, &PaneToolbar::actionsClicked, this, [this]() {
    QModelIndex cur = m_filePane->view()->currentIndex();
    if (!cur.isValid())
      return;
    m_filePane->view()->customContextMenuRequested(
        m_filePane->view()->visualRect(cur).center());
  });

  connect(m_toolbar, &PaneToolbar::upClicked, this, [this]() {
    QDir d(currentPath());
    if (d.cdUp())
      navigateTo(d.absolutePath());
  });
  connect(m_toolbar, &PaneToolbar::foldersFirstToggled, this,
          [this](bool on) { m_filePane->setFoldersFirst(on); });
  connect(m_toolbar, &PaneToolbar::viewModeChanged, this,
          [this](int mode) { m_filePane->setViewMode(mode); });

  // Toolbar bleibt synchron wenn FilePane den Mode selbst setzt (z.B. beim Laden)
  connect(m_filePane, &FilePane::viewModeChanged, m_toolbar,
          &PaneToolbar::setViewMode);

  // Gespeicherte Ansicht nach erstem Laden wiederherstellen
  connect(m_filePane, &FilePane::directoryLoaded, this, [this]() {
    const QString key = QStringLiteral("FilePane/") + m_settingsKey + QStringLiteral("/viewMode");
    const int savedMode = Config::group("UI").readEntry(key, 0);
    if (savedMode != 0)
      m_filePane->setViewMode(savedMode);
  }, Qt::SingleShotConnection);
  connect(m_toolbar, &PaneToolbar::backClicked, this, [this]() {
    if (!m_histBack.isEmpty()) {
      m_histFwd.push(currentPath());
      navigateTo(m_histBack.pop(), false);
    }
  });
  connect(m_toolbar, &PaneToolbar::forwardClicked, this, [this]() {
    if (!m_histFwd.isEmpty()) {
      m_histBack.push(currentPath());
      navigateTo(m_histFwd.pop(), false);
    }
  });
  connect(m_pathEdit, &QLineEdit::returnPressed, this,
          [this]() { navigateTo(m_pathEdit->text()); });

  const QString home = QDir::homePath();
  m_filePane->setRootPath(home);
  m_pathEdit->setText(home);
  m_toolbar->setPath(home);
  m_miller->init();
  buildFooter(rootLay);
}

void PaneWidget::setFocused(bool f) {
  m_focused = f;
  setStyleSheet(QString("background:%1;")
                    .arg(f ? TM().colors().bgBox : TM().colors().bgDeep));

  if (auto *d =
          static_cast<FilePaneDelegate *>(m_filePane->view()->itemDelegate())) {
    d->focused = f;
    m_filePane->view()->viewport()->update();
  }

  auto *v = m_filePane->view();
  // FrameStyle auf NoFrame setzen minimiert die Ränder automatisch
  v->setFrameStyle(QFrame::NoFrame);
  v->viewport()->setAttribute(Qt::WA_TranslucentBackground);
  v->verticalScrollBar()->hide();
  v->horizontalScrollBar()->hide();

  v->setStyleSheet(
      QString(
          "QTreeView{background:%1;border:none;color:%2;outline:none;font-size:"
          "10px;}"
          "QTreeView::item{padding:2px 4px;}"
          "QTreeView::item:hover{background:%3;}"
          "QTreeView::item:selected{background:%4;color:%5;}"

          /* Header-Fix: Hintergrund geht bis zum Rand */
          "QHeaderView{background:%6;border:none;margin:0px;padding:0px;}"
          "QHeaderView::section{background:%6;color:%7;border:none;"
          "border-bottom:1px solid %3;border-right:1px solid %3;"
          "padding:3px 6px;font-size:10px;}"
          "QHeaderView::section:last{border-right:none;}"

          "QTreeView::corner{background:transparent;border:none;}"

          /* Scrollbar komplett versteckt */
          "QTreeView "
          "QScrollBar:vertical{width:0px;background:transparent;border:none;}"
          "QTreeView "
          "QScrollBar:horizontal{height:0px;background:transparent;border:none;"
          "}")
          .arg(f ? TM().colors().bgList : TM().colors().bgDeep)
          .arg(TM().colors().textPrimary, TM().colors().bgHover,
               TM().colors().bgSelect)
          .arg(TM().colors().textLight, TM().colors().bgBox,
               TM().colors().textAccent));
}

QString PaneWidget::currentPath() const {
  // m_filePane->currentPath() gibt den echten Pfad zurück (nicht den Anzeigenamen)
  if (m_filePane)
    return m_filePane->currentPath();
  return m_pathEdit ? m_pathEdit->text() : QDir::homePath();
}

void PaneWidget::navigateTo(const QString &path, bool clearForward, bool updateMiller) {
  const bool isKio = !path.startsWith("/") && path.contains(":/");
  if (path.isEmpty())
    return;
  // Lokale Pfade die nicht existieren überspringen; KIO-URLs und Spezialpfade
  // durchlassen
  if (!isKio && path != "__drives__" && !QFileInfo::exists(path))
    return;

  const QString cur = currentPath();
  if (!cur.isEmpty() && cur != path) {
    if (clearForward)
      m_histFwd.clear();
    m_histBack.push(cur);
  }

  const QUrl url = QUrl::fromUserInput(path);
  if (path == "__drives__") {
    m_filePane->setRootPath("remote:/");
    m_pathEdit->setText(tr("Dieser PC"));
    m_toolbar->setPath(path);
    if (updateMiller)
      m_miller->navigateTo("__drives__");
    return;
  }
  m_filePane->setRootPath(path);
  m_pathEdit->setText(url.isLocalFile() ? url.toLocalFile() : path);
  m_toolbar->setPath(path);
  if (updateMiller && !m_miller->cols().isEmpty())
    m_miller->navigateTo(path);

  m_toolbar->setCount(0, 0);
  emit pathUpdated(path);
}

void PaneWidget::buildFooter(QVBoxLayout *rootLay) {
  auto *fw = new FooterWidget(this);
  fw->onHeightChanged = [this]() {
    positionFooterPanel();
    const QString p =
        m_lastPreviewPath.isEmpty() ? currentPath() : m_lastPreviewPath;
    refreshFooter(p,
                 m_filePane->view()->selectionModel()->selectedRows().count());
  };
  m_footerBar = fw;
  m_footerCount = fw->countLbl;
  m_footerSelected = fw->selectedLbl;
  m_footerSize = fw->sizeLbl;
  m_previewIcon = fw->previewIcon;
  m_previewInfo = fw->previewInfo;
  rootLay->addWidget(fw);
}

void PaneWidget::positionFooterPanel() {
  if (!m_footerBar)
    return;
  const int h = m_footerBar->height();
  m_footerBar->setGeometry(0, height() - h, width(), h);
  m_footerBar->raise();
}

void PaneWidget::refreshFooter(const QString &path, int selectedCount) {
  if (!m_footerCount)
    return;
  const QUrl url(path);
  const bool isLocal = url.isLocalFile() || url.scheme().isEmpty();

  m_lastPreviewPath = path;

  // 1. Wenn der Pfad dem aktuellen Verzeichnis des Panes entspricht:
  // Model-Daten nutzen
  if (path == currentPath()) {
    const int count =
        m_filePane->view()->model()->rowCount(m_filePane->view()->rootIndex());
    m_footerCount->setText(tr("%1 Elemente").arg(count));
    m_footerSize->setText(QString());
    if (m_footerSelected) {
      if (selectedCount > 0) {
        m_footerSelected->setText(tr("%1 ausgewählt").arg(selectedCount));
        m_footerSelected->show();
      } else {
        m_footerSelected->hide();
      }
    }
  } else if (isLocal) {
    // 2. Metadaten für einzelne Datei/Ordner (Selection)
    const QFileInfo fi(url.isLocalFile() ? url.toLocalFile() : path);
    if (!fi.exists())
      return;

    if (fi.isDir()) {
      m_footerCount->setText(tr("Ordner"));
      m_footerSize->setText(tr("…"));
      if (m_footerSelected)
        m_footerSelected->hide();

      auto *watcher = new QFutureWatcher<quint64>(this);
      connect(
          watcher, &QFutureWatcher<quint64>::finished, this, [this, watcher]() {
            const quint64 sz = watcher->result();
            watcher->deleteLater();
            if (!m_footerSize)
              return;
            if (sz < 1024)
              m_footerSize->setText(QString("%1 B").arg(sz));
            else if (sz < 1024 * 1024)
              m_footerSize->setText(QString("%1 KB").arg(sz / 1024));
            else
              m_footerSize->setText(QString("%1 MB").arg(sz / (1024 * 1024)));
          });
      watcher->setFuture(QtConcurrent::run([path]() -> quint64 {
        quint64 total = 0;
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
          it.next();
          total += it.fileInfo().size();
        }
        return total;
      }));
    } else {
      m_footerCount->setText(fi.fileName());
      m_footerSize->setText(QString(" | %1").arg(mw_fmtSize(fi.size())));
      if (m_footerSelected) {
        m_footerSelected->setText(tr("%1 ausgewählt").arg(qMax(1, selectedCount)));
        m_footerSelected->show();
      }
    }
  } else {
    // KIO-Pfad: Basis-Informationen anzeigen
    m_footerCount->setText(url.fileName().isEmpty() ? path : url.fileName());
    m_footerSize->setText(QString());
  }

  // --- Vorschau-Bereich ---
  if (!m_previewIcon || !m_previewInfo)
    return;

  const int footerH = m_footerBar ? m_footerBar->height() : 120;
  const int iconSize = qBound(32, footerH - 40, 300);
  m_previewIcon->setFixedSize(iconSize, iconSize);

  if (isLocal) {
    const QFileInfo fi(url.isLocalFile() ? url.toLocalFile() : path);
    if (!fi.exists())
      return;

    static const QStringList IMG_EXT = {"jpg", "jpeg", "png",  "gif",
                                        "bmp", "webp", "tiff", "tif",
                                        "ico", "ppm",  "pgm"};
    const QString ext = fi.suffix().toLower();
    if (!fi.isDir() && IMG_EXT.contains(ext)) {
      QPixmap px(path);
      if (!px.isNull()) {
        m_previewIcon->setPixmap(px.scaled(
            iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      } else {
        m_previewIcon->setPixmap(
            QIcon::fromTheme(KIO::iconNameForUrl(url.isLocalFile() ? url : QUrl::fromLocalFile(path))).pixmap(iconSize, iconSize));
      }
    } else {
      m_previewIcon->setPixmap(
          QIcon::fromTheme(KIO::iconNameForUrl(url.isLocalFile() ? url : QUrl::fromLocalFile(path))).pixmap(iconSize, iconSize));
    }

    QString info = QString("<table cellpadding='1' cellspacing='0' "
                           "style='color:%1;font-size:11px;'>")
                       .arg(TM().colors().textPrimary);
    auto addRow = [&info](const QString &label, const QString &val) {
      info +=
          QString(
              "<tr><td style='padding-right:20px;white-space:nowrap;'>%1</td>"
              "<td style='white-space:nowrap;'>%2</td></tr>")
              .arg(label, val);
    };

    addRow(tr("Name"), fi.fileName().toHtmlEscaped());
    addRow(tr("Typ"), fi.isDir() ? tr("Ordner")
                  : fi.suffix().isEmpty()
                      ? tr("Datei")
                      : fi.suffix().toUpper() + tr("-Datei"));
    addRow(tr("Erstellt"), fi.birthTime().toString("yyyy-MM-dd  hh:mm"));
    addRow(tr("Geändert"), fi.lastModified().toString("yyyy-MM-dd  hh:mm"));

    const qint64 days = fi.lastModified().daysTo(QDateTime::currentDateTime());
    addRow(tr("Alter"), days == 0    ? tr("Heute")
                    : days == 1  ? tr("Gestern")
                    : days < 30  ? tr("%1 t").arg(days)
                    : days < 365 ? tr("%1 m").arg(days / 30)
                                 : tr("%1 j").arg(days / 365));

    if (!fi.isDir()) {
      addRow(tr("Größe:"), mw_fmtSize(fi.size()));
    }

    const QFile::Permissions p = fi.permissions();
    QString perm;
    perm += fi.isDir() ? "d" : "-";
    perm += (p & QFile::ReadOwner) ? "r" : "-";
    perm += (p & QFile::WriteOwner) ? "w" : "-";
    perm += (p & QFile::ExeOwner) ? "x" : "-";
    perm += (p & QFile::ReadGroup) ? "r" : "-";
    perm += (p & QFile::WriteGroup) ? "w" : "-";
    perm += (p & QFile::ExeGroup) ? "x" : "-";
    perm += (p & QFile::ReadOther) ? "r" : "-";
    perm += (p & QFile::WriteOther) ? "w" : "-";
    perm += (p & QFile::ExeOther) ? "x" : "-";
    addRow(tr("Attribute"), perm);

    info += "</table>";
    m_previewInfo->setText(info);
  } else {
    // KIO-Vorschau: Nur Icon und Basis-Info
    m_previewIcon->setPixmap(
        QIcon::fromTheme(KFileItem(url).isDir() ? "folder" : "text-x-generic")
            .pixmap(iconSize, iconSize));
    m_previewInfo->setText(
        QString("<b>%1</b><br>%2").arg(url.fileName(), url.scheme()));
  }
}

bool PaneWidget::eventFilter(QObject *obj, QEvent *ev) {
  return QWidget::eventFilter(obj, ev);
}

QList<QUrl> PaneWidget::selectedUrls() const {
  return m_filePane->selectedUrls();
}

void PaneWidget::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFooterPanel();
}

