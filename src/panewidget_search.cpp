// --- panewidget_search.cpp --------------------------------------------------
// Such-Panel für PaneWidget (Filter, Suchergebnisse, Tag-Suche).
// ---------------------------------------------------------------------------

#include "panewidget.h"
#include "mainwindow.h"
#include <QShortcut>
#include <QKeySequence>
#include "config.h"
#include "dialogutils.h"
#include "panecomponents.h"
#include "scglobal.h"
#include "thememanager.h"
#include "thumbnailmanager.h"
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KDialogJobUiDelegate>
#include <KFileItem>
#include <KFileWidget>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/EmptyTrashJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/StoredTransferJob>
#include <KPropertiesDialog>
#include <KShortcutsDialog>
#include <KShortcutsEditor>
#include <KTerminalLauncherJob>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeDatabase>
#include <QPointer>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

// --- PaneWidget ---

void PaneWidget::initSearchPanel(QVBoxLayout *rootLay) {
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

  m_searchEdit = new QLineEdit();
  m_searchEdit->setPlaceholderText(tr("Suchen ..."));
  m_searchEdit->setStyleSheet(
      QString("QLineEdit{background:%1;border:1px solid %2;color:%3;")
          .arg(TM().colors().bgMain, TM().colors().accent,
               TM().colors().textPrimary) +
      "font-size:11px;padding:2px 6px;border-radius:2px;}");
  m_searchEdit->setClearButtonEnabled(true);

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
      QString("QToolButton{background:transparent;border:none;color:%1;"
              "border-radius:10px;}"
              "QToolButton:hover{background:%1;color:%2;}")
          .arg(TM().colors().accent, TM().colors().bgMain));

  spLay->addWidget(m_searchEdit, 1);
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
  m_searchOverlay = new QWidget(this);
  m_searchOverlay->hide();
  m_searchOverlay->setStyleSheet(
      QString("background:%1;border:1px solid %2;border-top:none;")
          .arg(TM().colors().bgBox, TM().colors().separator));
  auto *ovLay = new QVBoxLayout(m_searchOverlay);
  ovLay->setContentsMargins(1, 1, 1, 1);
  ovLay->setSpacing(0);

  m_searchResults = new QTreeWidget(m_searchOverlay);
  m_searchResults->setHeaderLabels({tr("Name"), tr("Pfad"), tr("Geändert")});
  m_searchResults->setRootIsDecorated(false);
  m_searchResults->setStyleSheet(
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
  m_searchResults->header()->setStretchLastSection(false);
  m_searchResults->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_searchResults->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_searchResults->header()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  ovLay->addWidget(m_searchResults, 1);

  // Suchpanel-Verbindungen
  connect(m_searchBtn, &QToolButton::toggled, this,
          [searchPanel, spTabRow, this](bool on) {
            searchPanel->setVisible(on);
            if (on) {
              m_searchEdit->clear();
              m_searchEdit->setFocus();
            } else {
              m_searchEdit->clear();
              m_filePane->setNameFilter(QString());
              spTabRow->hide();
              m_searchOverlay->hide();
            }
          });
  connect(searchCloseBtn, &QToolButton::clicked, this, [this]() {
    m_searchBtn->setChecked(false);
    m_filePane->setNameFilter(QString());
  });
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          [this, searchByName](const QString &text) {
            if (!*searchByName)
              return;
            m_filePane->setNameFilter(text);
          });
  connect(m_searchEdit, &QLineEdit::returnPressed, this, [this, spTabRow]() {
    const QString term = m_searchEdit->text().trimmed();
    if (term.isEmpty())
      return;
    m_searchResults->clear();
    auto *loading = new QTreeWidgetItem(m_searchResults);
    loading->setText(0, tr("Suche läuft..."));

    const QPoint topLeft = m_vSplit->mapTo(this, QPoint(0, 0));
    m_searchOverlay->setGeometry(topLeft.x(), topLeft.y(), m_vSplit->width(),
                                 qMin(300, m_vSplit->height()));
    m_searchOverlay->show();
    m_searchOverlay->raise();
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
            [this, watcher]() {
              if (watcher != m_searchWatcher) {
                watcher->deleteLater();
                return;
              }
              m_searchWatcher = nullptr;
              const QStringList paths = watcher->result();
              watcher->deleteLater();
              m_searchResults->clear();
              if (paths.isEmpty()) {
                auto *empty = new QTreeWidgetItem(m_searchResults);
                empty->setText(0, tr("Keine Ergebnisse"));
                return;
              }
              for (const QString &path : paths) {
                const QFileInfo fi(path);
                const QUrl url = QUrl::fromLocalFile(path);
                auto *it = new QTreeWidgetItem(m_searchResults);
                it->setIcon(0, QIcon::fromTheme(KIO::iconNameForUrl(url)));
                it->setText(0, fi.fileName());
                it->setText(
                    1, QString("~/%1").arg(
                           QDir::home().relativeFilePath(fi.absolutePath())));
                it->setText(2, fi.lastModified().toString("dd.MM.yy"));
                it->setData(0, Qt::UserRole, path);
              }
              if (m_searchResults->topLevelItemCount() == 0) {
                auto *empty = new QTreeWidgetItem(m_searchResults);
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
  connect(m_searchResults, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem *it, int) {
            const QString path = it->data(0, Qt::UserRole).toString();
            if (path.isEmpty())
              return;
            m_searchEdit->clear();
            m_filePane->setNameFilter(QString());
            navigateTo(QFileInfo(path).isDir()
                           ? path
                           : QFileInfo(path).absolutePath());
            m_searchOverlay->hide();
            m_searchBtn->setChecked(false);
          });
}

// --- PaneWidget::initSplitter ---
