
// --- sidebar.cpp — SplitCommander Sidebar ---

#include "sidebar.h"
#include "addnetworkdialog.h"
#include "drivedelegate.h"
#include "drivemanager.h"
#include "hoverfader.h"
#include "scglobal.h"
#include <KDirWatch>

// 1. Qt Core / UI
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>

#include <KIO/JobUiDelegateFactory>
#include <KIO/StoredTransferJob>
#include <QFormLayout>
#include <QPointer>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

// 2. KDE / KIO / Solid
#include <KDirLister>
#include <KFile>
#include <KIO/CopyJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/ListJob>
#include <KIconDialog>
#include <KPropertiesDialog>
#include <KUrlRequester>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

// 3. SplitCommander
#include "config.h"
#include "dialogutils.h"
#include <KDialogJobUiDelegate>
#include <KTerminalLauncherJob>

#include "thememanager.h"
// Themed input dialog - styled like QMenu
static QString sc_getText(QWidget *parent, const QString &title,
                          const QString &label,
                          const QString &defaultText = QString()) {
  QDialog dlg(parent);
  dlg.setAttribute(Qt::WA_StyledBackground, true);
  dlg.setWindowTitle(title);
  dlg.setStyleSheet(TM().ssDialog());
  auto *lay = new QVBoxLayout(&dlg);
  lay->setSpacing(4);
  lay->setContentsMargins(12, 10, 12, 10);
  auto *lbl = new QLabel(label, &dlg);
  auto *edit = new QLineEdit(defaultText, &dlg);
  auto *btnRow = new QHBoxLayout();
  auto *ok = new QPushButton(
      QIcon::fromTheme(QStringLiteral("dialog-ok")),
      QCoreApplication::translate("SplitCommander", "OK"), &dlg);
  auto *can = new QPushButton(
      QIcon::fromTheme(QStringLiteral("dialog-cancel")),
      QCoreApplication::translate("SplitCommander", "Abbrechen"), &dlg);
  ok->setDefault(true);
  btnRow->addStretch();
  btnRow->addWidget(ok);
  btnRow->addWidget(can);
  lay->addWidget(lbl);
  lay->addWidget(edit);
  lay->addLayout(btnRow);
  QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
  QObject::connect(can, &QPushButton::clicked, &dlg, &QDialog::reject);
  dlg.adjustSize();
  if (parent) {
    QPoint center = parent->mapToGlobal(parent->rect().center());
    dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
  }
  if (dlg.exec() != QDialog::Accepted)
    return {};
  return edit->text();
}

// --- Hilfsfunktionen (file-scope) ---

// --- KDE-Style Edit Dialog ---
static bool sc_editPlaceDialog(QWidget *parent, QString &name, QString &path,
                               QString &icon) {
  QDialog dlg(parent);
  dlg.setWindowTitle(QObject::tr("Eintrag bearbeiten"));
  dlg.setFixedWidth(500);

  auto *mainVl = new QVBoxLayout(&dlg);
  mainVl->setSizeConstraint(QLayout::SetFixedSize);
  mainVl->setContentsMargins(12, 12, 12, 12);
  mainVl->setSpacing(12);

  auto *topHl = new QHBoxLayout();
  topHl->setSpacing(12);

  // Icon Button (Oben links)
  auto *iconBtn = new QPushButton(&dlg);
  QString currentIcon = icon.isEmpty() ? QStringLiteral("folder") : icon;
  iconBtn->setIcon(QIcon::fromTheme(currentIcon));
  iconBtn->setFixedSize(64, 64);
  iconBtn->setIconSize(QSize(32, 32));
  iconBtn->setCursor(Qt::PointingHandCursor);
  iconBtn->setStyleSheet(QString("QPushButton { background: %1; border: 1px "
                                 "solid %2; border-radius: 8px; }"
                                 "QPushButton:hover { background: %3; }")
                             .arg(TM().colors().bgBox, TM().colors().border,
                                  TM().colors().bgHover));
  topHl->addWidget(iconBtn, 0, Qt::AlignTop);

  // Formular (Rechts)
  auto *form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(8);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  auto *nameEdit = new QLineEdit(name, &dlg);
  form->addRow(QObject::tr("Name:"), nameEdit);

  auto *urlReq = new KUrlRequester(QUrl::fromUserInput(path), &dlg);
  urlReq->setMode(KFile::Directory | KFile::File | KFile::LocalOnly);
  form->addRow(QObject::tr("Adresse:"), urlReq);

  topHl->addLayout(form, 1);
  mainVl->addLayout(topHl);

  QObject::connect(iconBtn, &QPushButton::clicked, [&]() {
    KIconDialog iconDlg(&dlg);
    iconDlg.setSelectedIcon(currentIcon);
    QString newIcon = iconDlg.openDialog();
    if (!newIcon.isEmpty()) {
      currentIcon = newIcon;
      iconBtn->setIcon(QIcon::fromTheme(newIcon));
    }
  });

  auto *bbox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  mainVl->addWidget(bbox);

  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() == QDialog::Accepted) {
    name = nameEdit->text().trimmed();
    path = urlReq->url().toLocalFile();
    if (path.isEmpty())
      path = urlReq->text();
    icon = currentIcon;
    return true;
  }
  return false;
}

static void sc_buildPlaceMenu(
    QMenu &menu, const QString &path, const QString &name, const QString &icon,
    std::function<void()> removeAction,
    std::function<void(const QString &, const QString &, const QString &)>
        editAction) {
  // 1. Bearbeiten (Direkt-Aktion)
  menu.addAction(QIcon::fromTheme(QStringLiteral("edit-entry")),
                 QObject::tr("Bearbeiten..."),
                 [editAction, path, name, icon]() {
                   QString n = name;
                   QString p = path;
                   QString i = icon;
                   if (sc_editPlaceDialog(nullptr, n, p, i)) {
                     editAction(n, p, i);
                   }
                 });

  menu.addSeparator();

  // 2. System-Aktionen
  menu.addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                 QObject::tr("In Terminal öffnen"), [path]() {
                   auto *job = new KTerminalLauncherJob(QString());
                   job->setWorkingDirectory(path);
                   job->setUiDelegate(new KDialogJobUiDelegate(
                       KJobUiDelegate::AutoHandlingEnabled, nullptr));
                   job->start();
                 });
  menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                 QObject::tr("Pfad kopieren"),
                 [path]() { QGuiApplication::clipboard()->setText(path); });

  // 3. Entfernen
  if (removeAction) {
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")),
                   QObject::tr("Aus Gruppe entfernen"), removeAction);
  }

  // 4. Eigenschaften (Ganz unten)
  menu.addSeparator();
  menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                 QObject::tr("Eigenschaften"), [path]() {
                   KPropertiesDialog::showDialog(QUrl::fromUserInput(path));
                 });
}

// --- Konstanten ---

// --- DriveDelegate ---

// --- Sidebar::adjustListHeight ---
void Sidebar::adjustListHeight(QListWidget *list) {
  if (!list)
    return;
  const int n = list->count();

  if (n == 0) {
    list->setFixedHeight(0);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  } else {
    list->setFixedHeight(n * Config::sidebarRowHeight());
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }
  list->updateGeometry();
}

// --- Sidebar::Sidebar — Konstruktor ---
Sidebar::Sidebar(QWidget *parent) : QWidget(parent) {
  setStyleSheet(
      QString("background-color:%1; border:none;").arg(TM().colors().bgMain));

  auto *outerLay = new QVBoxLayout(this);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->setSpacing(0);

  buildLogo(outerLay);
  buildDrivesSection(outerLay);
  buildGroupsSection(outerLay);
  buildGitSection(outerLay);
  buildNewGroupFixedSection(outerLay);
  buildTagsSection(outerLay);

  setupTags();
  loadUserPlaces();
  DriveManager::instance()->refreshAll();
  connectDriveList();
  setupDriveContextMenu();
  connect(DriveManager::instance(), &DriveManager::drivesUpdated, this,
          [this]() {
            updateDrives();
            emit drivesChanged();
          });

  m_trashLister = new KDirLister(this);
  connect(m_trashLister, &KDirLister::completed, this,
          &Sidebar::onTrashChanged);
  connect(m_trashLister, &KDirLister::itemsAdded, this,
          &Sidebar::onTrashChanged);
  connect(m_trashLister, &KDirLister::itemsDeleted, this,
          &Sidebar::onTrashChanged);
  m_trashLister->openUrl(QUrl(QStringLiteral("trash:/")), KDirLister::Keep);
}

// --- Sidebar::buildLogo ---
void Sidebar::buildLogo(QVBoxLayout *parent) {
  auto *wrapper = new QWidget(this);
  wrapper->setStyleSheet(TM().ssSidebar());
  auto *lay = new QHBoxLayout(wrapper);
  lay->setContentsMargins(12, 6, 12, 4);
  lay->setSpacing(8);

  // --- Icon ---
  auto *iconLabel = new QLabel();
  QPixmap pix;

  // 1. System-Theme (funktioniert nach sc-install)
  QIcon themeIcon = QIcon::fromTheme(QStringLiteral("splitcommander"));
  if (!themeIcon.isNull()) {
    pix = themeIcon.pixmap(32, 32);
  }

  // 2. Fallback: Pfad relativ zum Binary (Dev-Build)
  if (pix.isNull()) {
    QString iconPath = QCoreApplication::applicationDirPath() +
                       "/../src/splitcommander_64.png";
    if (!QFile::exists(iconPath))
      iconPath =
          QCoreApplication::applicationDirPath() + "/splitcommander_64.png";
    pix = QPixmap(iconPath);
  }

  // 3. Letzter Fallback: generisches Icon
  if (pix.isNull())
    pix =
        QIcon::fromTheme(QStringLiteral("system-file-manager")).pixmap(32, 32);

  iconLabel->setPixmap(
      pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  iconLabel->setFixedSize(32, 32);
  iconLabel->setStyleSheet("background:transparent; border:none;");
  lay->addWidget(iconLabel);

  // --- Text ---
  auto *nameLbl = new QLabel(
      "<span style='font-weight:200;color:#ccd4e8;font-size:12px;'>Split</span>"
      "<span "
      "style='font-weight:600;color:#88c0d0;font-size:12px;'>Commander</span>"
      "<span style='color:#4c566a;font-size:9px;'> | Dateimanager</span>");
  nameLbl->setStyleSheet("background:transparent; border:none;");
  lay->addWidget(nameLbl);
  lay->addStretch();

  parent->addWidget(wrapper);
}

// --- Sidebar::buildDrivesSection ---
void Sidebar::buildDrivesSection(QVBoxLayout *parent) {
  auto *wrapper = new QWidget(this);
  wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
  auto *wLay = new QVBoxLayout(wrapper);
  wLay->setContentsMargins(10, 2, 6, 2);
  wLay->setSpacing(0);

  auto *box = new QWidget(wrapper);
  box->setObjectName(QStringLiteral("outerBox"));
  box->setStyleSheet(TM().ssBox());
  auto *vbox = new QVBoxLayout(box);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
  auto *header = new QWidget();
  header->setStyleSheet("background:transparent; border:none;");
  auto *hLay = new QHBoxLayout(header);
  hLay->setContentsMargins(12, 10, 8, 6);
  hLay->setSpacing(0);
  auto driveBoxSettings = Config::group("UI");
  const QString driveBoxLabel =
      driveBoxSettings.readEntry("driveBoxLabel", tr("LAUFWERKE"));
  auto *lbl = new QLabel(driveBoxLabel);

  lbl->setStyleSheet(QString("font-size:13px;font-weight:bold;text-transform:"
                             "uppercase;background:transparent;color:%1;")
                         .arg(TM().colors().textAccent));
  hLay->addWidget(lbl, 1);

  auto *menuBtn = new QPushButton();
  menuBtn->setIcon(QIcon::fromTheme(
      QStringLiteral("application-menu"))); // KDE Standard für "Drei Punkte"
  if (menuBtn->icon().isNull())
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
  menuBtn->setFixedSize(26, 22);
  menuBtn->setCursor(Qt::PointingHandCursor);
  menuBtn->setStyleSheet(
      QString(
          "QPushButton { background: transparent; border: none; padding: 2px; }"
          "QPushButton:hover { background: %1; border-radius: 4px; }")
          .arg(TM().colors().bgHover));
  hLay->addWidget(menuBtn);
  vbox->addWidget(header);
  auto *listCont = new QWidget();
  listCont->setStyleSheet("background:transparent; border:none;");
  auto *listLay = new QVBoxLayout(listCont);
  listLay->setContentsMargins(6, 0, 6, 0);
  listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
  m_driveList = new QListWidget();
  m_driveList->setSelectionMode(QAbstractItemView::NoSelection);
  m_driveList->setFrameShape(QFrame::NoFrame);
  m_driveList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_driveList->setIconSize(
      QSize(Config::driveIconSize(), Config::driveIconSize()));
  m_driveList->setStyleSheet(TM().ssListWidget());
  auto *driveDel = new DriveDelegate(true, this);
  driveDel->setHoverFader(new HoverFader(m_driveList, driveDel));
  m_driveList->setItemDelegate(driveDel);
  listLay->addWidget(m_driveList);
  m_netBox = new QWidget();
  m_netBox->setVisible(false);
  auto *netWLay = new QVBoxLayout(m_netBox);
  netWLay->setContentsMargins(6, 0, 6, 4);
  m_netBox->setStyleSheet("margin-top:0px; padding-top:0px;");
  netWLay->setSpacing(0);

  m_netList = new QListWidget();
  m_netList->setSelectionMode(QAbstractItemView::NoSelection);
  m_netList->setFrameShape(QFrame::NoFrame);
  m_netList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_netList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_netList->setIconSize(
      QSize(Config::driveIconSize(), Config::driveIconSize()));
  m_netList->setStyleSheet(TM().ssListWidget());
  auto *netDel = new DriveDelegate(true, this);
  netDel->setHoverFader(new HoverFader(m_netList, netDel));
  m_netList->setItemDelegate(netDel);
  netWLay->addWidget(m_netList);

  // Netzwerk-Box in vbox einfügen (nach listCont, vor Toggle)

  vbox->addWidget(listCont);
  vbox->addWidget(m_netBox);
  auto *toggleBtn = new QPushButton();
  toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
  toggleBtn->setIconSize(QSize(10, 10));
  toggleBtn->setCheckable(true);
  toggleBtn->setFixedHeight(12);
  toggleBtn->setStyleSheet(
      QString("QPushButton{background:transparent "
              "!important;font-size:7px;border:none;color:%1;}")
          .arg(TM().colors().textMuted));
  vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);
  connect(toggleBtn, &QPushButton::toggled, this,
          [this, listCont, toggleBtn](bool on) {
            listCont->setVisible(!on);
            if (m_netBox)
              m_netBox->setVisible(!on);
            toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
          });

  // --- Netzwerk-Untersektion innerhalb der Geräte-Box ---

  wLay->addWidget(box);
  parent->addWidget(wrapper);

  connect(m_netList, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *it) {
            const QString p = it->data(Qt::UserRole).toString();
            if (!p.isEmpty())
              emit driveClicked(p);
          });

  // Rechtsklick Netzwerk-Liste
  m_netList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      m_netList, &QListWidget::customContextMenuRequested, this,
      [this](const QPoint &pos) {
        auto *item = m_netList->itemAt(pos);
        if (!item)
          return;
        const QString path = item->data(Qt::UserRole).toString();
        const QString name = item->text();
        const QString accountId =
            item->data(Qt::UserRole + 2).toString(); // nur bei gdrive gesetzt

        QMenu menu(this);
        menu.setStyleSheet(TM().ssMenu());
        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                       tr("Öffnen"), this,
                       [this, path]() { emit driveClicked(path); });
        auto *openInMenu = menu.addMenu(
            QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
        openInMenu->setStyleSheet(TM().ssMenu());
        openInMenu->addAction(tr("Linke Pane"), this,
                              [this, path]() { emit driveClickedLeft(path); });
        openInMenu->addAction(tr("Rechte Pane"), this,
                              [this, path]() { emit driveClickedRight(path); });
        menu.addSeparator();

        // Umbenennen — für alle NetworkPlaces-Einträge
        {
          auto netCheck = Config::group("NetworkPlaces");
          const QStringList netPlaces =
              netCheck.readEntry("places", QStringList());
          const QString npath = mw_normalizePath(path);
          bool alreadyIn = false;
          for (const QString &p : netPlaces) {
            if (mw_normalizePath(p) == npath) {
              alreadyIn = true;
              break;
            }
          }

          if (alreadyIn) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                           tr("Umbenennen"), this, [this, path, name]() {
                             bool ok;
                             const QString newName = DialogUtils::getText(
                                 this, tr("Umbenennen"), tr("Anzeigename:"),
                                 name, &ok);
                             if (ok && !newName.trimmed().isEmpty()) {
                               renameNetworkPlace(path, newName.trimmed());
                             }
                           });
            menu.addSeparator();
          }
        }

        // Zu Laufwerken hinzufügen — nur wenn noch nicht drin
        {
          auto netCheck = Config::group("NetworkPlaces");
          const QStringList netPlaces =
              netCheck.readEntry("places", QStringList());
          const QString npath = mw_normalizePath(path);
          bool alreadyIn = false;
          for (const QString &p : netPlaces) {
            if (mw_normalizePath(p) == npath) {
              alreadyIn = true;
              break;
            }
          }

          if (!alreadyIn) {
            menu.addAction(
                QIcon::fromTheme(QStringLiteral("bookmark-new")),
                tr("Zu Laufwerken hinzufügen"), this, [this, path, name]() {
                  auto s = Config::group("NetworkPlaces");
                  QStringList saved = s.readEntry("places", QStringList());
                  const QString npath = mw_normalizePath(path);
                  if (!saved.contains(npath)) {
                    saved << npath;
                    s.writeEntry("places", saved);
                    s.writeEntry(
                        "name_" +
                            QString(npath).replace("/", "_").replace(":", "_"),
                        name);
                    s.config()->sync();
                    saveToUserPlaces(npath, name);
                  }
                  DriveManager::instance()->refreshAll();
                  emit drivesChanged();
                });
          }
        }

        // Netzwerklaufwerk trennen (gemountet) oder aus Liste entfernen (KIO)
        const bool isKioPlace = !path.startsWith("/") &&
                                !path.startsWith("solid:") &&
                                path.contains(":/");
        if (isKioPlace) {
          menu.addAction(
              QIcon::fromTheme(QStringLiteral("list-remove")),
              tr("Aus Laufwerken entfernen"), this, [this, path]() {
                auto s = Config::group("NetworkPlaces");
                QStringList saved = s.readEntry("places", QStringList());
                const QString npath = mw_normalizePath(path);
                saved.removeAll(npath);
                QString otherVersion = npath.endsWith('/')
                                           ? npath.left(npath.length() - 1)
                                           : npath + "/";
                if (otherVersion != "/")
                  saved.removeAll(otherVersion);

                s.writeEntry("places", saved);
                const QString key1 =
                    QString(npath).replace("/", "_").replace(":", "_");
                const QString key2 =
                    QString(otherVersion).replace("/", "_").replace(":", "_");
                s.deleteEntry("name_" + key1);
                s.deleteEntry("name_" + key2);
                s.deleteEntry("icon_" + key1);
                s.deleteEntry("icon_" + key2);
                s.config()->sync();

                // Auch aus XBEL entfernen damit es nach Neustart nicht
                // wiederkommt
                const QString xbelPath =
                    QStandardPaths::writableLocation(
                        QStandardPaths::GenericDataLocation) +
                    "/user-places.xbel";
                QFile f(xbelPath);
                if (f.open(QIODevice::ReadOnly)) {
                  QByteArray data = f.readAll();
                  f.close();
                  // URL ohne UserInfo für Vergleich
                  QUrl checkUrl(npath);
                  checkUrl.setUserInfo(QString());
                  QUrl checkUrl2(otherVersion);
                  checkUrl2.setUserInfo(QString());
                  // Alle Varianten der URL aus XBEL entfernen
                  for (const QUrl &u :
                       {QUrl(npath), QUrl(otherVersion), checkUrl, checkUrl2}) {
                    const QString tag =
                        QString("href=\"%1\"").arg(u.toString());
                    // Bookmark-Block entfernen
                    int start = data.indexOf(tag.toUtf8());
                    if (start < 0)
                      continue;
                    int bStart = data.lastIndexOf("<bookmark", start);
                    int bEnd = data.indexOf("</bookmark>", start);
                    if (bStart >= 0 && bEnd >= 0)
                      data.remove(bStart, bEnd - bStart + 11);
                  }
                  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    f.write(data);
                }

                DriveManager::instance()->refreshAll();
                emit drivesChanged();
                emit removeFromPlacesRequested(path);
              });
        } else {

          menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")),
                         tr("Trennen"), this,
                         [this, path]() { emit unmountRequested(path); });
        }
        menu.addSeparator();
        menu.addAction(
            QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Pfad kopieren"),
            this, [path]() { QGuiApplication::clipboard()->setText(path); });
        menu.exec(m_netList->mapToGlobal(pos));
      });

  // Drives-Menü
  connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl]() {
    auto *m = new QMenu(this);
    m->setStyleSheet(TM().ssMenu());
    m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                 tr("Box umbenennen …"), this, [this, lbl]() {
                   bool ok;
                   QString name = sc_getText(this, tr("Box umbenennen"),
                                             tr("Name:"), lbl->text());
                   ok = !name.isNull();
                   if (ok && !name.isEmpty()) {
                     lbl->setText(name);
                     auto s = Config::group("UI");
                     s.writeEntry("driveBoxLabel", name);
                     s.config()->sync();
                   }
                 });
    m->addSeparator();
    m->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                 tr("Alles aktualisieren"), this,
                 []() { DriveManager::instance()->refreshAll(); })
        ->setShortcut(Qt::Key_F5);
    m->addSeparator();
    m->addAction(QIcon::fromTheme(QStringLiteral("network-connect")),
                 tr("Netzwerklaufwerk verbinden"), this,
                 [this]() { emit driveClicked("remote:/"); });
    m->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                 tr("SMB Laufwerke verbinden"), this, [this]() {
                   auto *dlg = new AddNetworkDialog(this);
                   dlg->setAttribute(Qt::WA_DeleteOnClose);
                   connect(dlg, &QDialog::accepted, this, [this, dlg]() {
                     const QString url = dlg->url();
                     const QString name = dlg->name();
                     const QString icon = dlg->iconName();
                     if (url.isEmpty())
                       return;
                     auto s = Config::group("NetworkPlaces");
                     QStringList saved = s.readEntry("places", QStringList());
                     const QString key =
                         QString(url).replace("/", "_").replace(":", "_");
                     if (!saved.contains(url)) {
                       saved << url;
                       s.writeEntry("places", saved);
                       s.writeEntry("name_" + key, name);
                       s.writeEntry("icon_" + key, icon);
                       s.config()->sync();
                       // Auch in KDE-Places schreiben
                       saveToUserPlaces(url, name);
                     }
                     DriveManager::instance()->refreshAll();
                     emit drivesChanged();
                   });
                   dlg->open();
                 });
    m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
  });
}

// --- Sidebar::buildGroupsSection ---
// --- Sidebar::buildGroupsSection — NUR NOCH DER SCROLLBEREICH ---
void Sidebar::buildGroupsSection(QVBoxLayout *parent) {
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setStyleSheet(QString("QScrollArea{border:none;background:%1;}")
                                  .arg(TM().colors().bgMain));
  m_scrollArea->verticalScrollBar()->hide();
  m_scrollArea->horizontalScrollBar()->hide();

  auto *scrollWidget = new QWidget();
  scrollWidget->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgMain));
  m_scrollArea->setWidget(scrollWidget);

  m_contentLayout = new QVBoxLayout(scrollWidget);
  m_contentLayout->setContentsMargins(0, 0, 0, 0);
  m_contentLayout->setSpacing(0);

  loadCustomGroups();

  m_contentLayout->addStretch(1);
  parent->addWidget(m_scrollArea, 1);

  // Overlay-Scrollbar
  m_overlayBar = new QScrollBar(Qt::Vertical, this);
  m_overlayBar->setStyleSheet(
      QString("QScrollBar:vertical{background:transparent;width:8px;margin:0px;"
              "border:none;}"
              "QScrollBar::handle:vertical{background:%1;border-radius:4px;min-"
              "height:20px;margin:1px;}"
              "QScrollBar::handle:vertical:hover{background:%2;}"
              "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{"
              "height:0px;}"
              "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
              "background:transparent;}")
          .arg(TM().colors().separator, TM().colors().accent));
  m_overlayBar->hide();
  m_overlayBar->raise();

  auto *native = m_scrollArea->verticalScrollBar();
  connect(native, &QScrollBar::rangeChanged, m_overlayBar,
          &QScrollBar::setRange);
  connect(native, &QScrollBar::valueChanged, m_overlayBar,
          &QScrollBar::setValue);
  connect(m_overlayBar, &QScrollBar::valueChanged, native,
          &QScrollBar::setValue);
}

// --- Sidebar::buildNewGroupFixedSection — DER FESTE BUTTON UNTER DEN TAGS ---
void Sidebar::buildNewGroupFixedSection(QVBoxLayout *parent) {
  auto *ngWrapper = new QWidget(this);
  ngWrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
  auto *ngWLay = new QVBoxLayout(ngWrapper);
  ngWLay->setContentsMargins(10, 2, 6, 2);
  ngWLay->setSpacing(0);

  m_newGroupBox = new QWidget(ngWrapper);
  m_newGroupBox->setObjectName(QStringLiteral("ngBox"));
  m_newGroupBox->setStyleSheet(TM().ssBox());

  auto *ngLay = new QVBoxLayout(m_newGroupBox);
  ngLay->setContentsMargins(6, 6, 6, 6);
  ngLay->setSpacing(0);

  auto *ngBtn = new QPushButton(tr("+ Neue Gruppe"));
  ngBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  ngBtn->setStyleSheet(
      QString(
          "QPushButton { background:%1; border:none; color:%2; font-size:11px;"
          " padding:8px; text-align:center; border-radius:4px; }"
          "QPushButton:hover { background:%3; color:%4; }")
          .arg(TM().colors().bgList, TM().colors().textPrimary,
               TM().colors().bgHover, TM().colors().textLight));

  ngLay->addWidget(ngBtn);
  ngWLay->addWidget(m_newGroupBox);

  // Wichtig: In das Hauptlayout der Sidebar (fest stehend)
  parent->addWidget(ngWrapper);

  connect(ngBtn, &QPushButton::clicked, this, &Sidebar::onNewGroupDialog);
}
// --- Sidebar::buildGitSection ---
void Sidebar::buildGitSection(QVBoxLayout *parent) {
  m_gitWrap = new QWidget(this);
  m_gitWrap->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
  auto *wLay = new QVBoxLayout(m_gitWrap);
  wLay->setContentsMargins(10, 2, 6, 2);
  wLay->setSpacing(0);

  m_gitBox = new QWidget(m_gitWrap);
  m_gitBox->setObjectName(QStringLiteral("gitBox"));
  m_gitBox->setStyleSheet(TM().ssBox());
  auto *vbox = new QVBoxLayout(m_gitBox);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

  auto *header = new QWidget();
  header->setStyleSheet("background:transparent; border:none;");
  auto *hLay = new QHBoxLayout(header);
  hLay->setContentsMargins(12, 10, 8, 6);
  hLay->setSpacing(4);

  auto *lbl = new QLabel(tr("GIT REPOSITORIES"));
  lbl->setStyleSheet(QString("font-size:13px;font-weight:bold;text-transform:"
                             "uppercase;background:transparent;color:%1;")
                         .arg(TM().colors().textAccent));
  hLay->addWidget(lbl, 1);

  vbox->addWidget(header);

  auto *listCont = new QWidget();
  listCont->setStyleSheet("background:transparent; border:none;");
  auto *listLay = new QVBoxLayout(listCont);
  listLay->setContentsMargins(6, 0, 6, 4);
  listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

  m_gitList = new QListWidget();
  m_gitList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_gitList->setFrameShape(QFrame::NoFrame);
  m_gitList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_gitList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_gitList->setIconSize(
      QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
  m_gitList->setStyleSheet(
      QString("QListWidget { background:transparent; outline:none; }"
              "QListWidget::item { padding: 4px; border-radius:4px; color:%1; "
              "font-weight:500; font-size:14px; margin-bottom:2px; }"
              "QListWidget::item:hover { background:%2; }"
              "QListWidget::item:selected { background:%3; color:%4; "
              "font-weight:bold; }")
          .arg(TM().colors().textPrimary, TM().colors().bgHover,
               TM().colors().bgSelect, TM().colors().textLight));

  listLay->addWidget(m_gitList);
  vbox->addWidget(listCont);

  wLay->addWidget(m_gitBox);
  parent->addWidget(m_gitWrap);

  connect(m_gitList, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *it) {
            if (it) {
              if (m_driveList)
                m_driveList->clearSelection();
              if (m_tagList)
                m_tagList->clearSelection();
              emit driveClicked(it->data(Qt::UserRole).toString());
            }
          });

  refreshGitSection();
}

void Sidebar::refreshGitSection() {
  if (!m_gitList || !m_gitWrap)
    return;

  m_gitList->clear();
  QString gitDir = Config::gitLocalDir();
  if (gitDir.isEmpty()) {
    m_gitWrap->hide();
    return;
  }
  m_gitWrap->show();

  QDir dir(gitDir);
  if (dir.exists()) {
    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subdirs) {
      QDir sub(fi.absoluteFilePath());
      if (sub.exists(".git")) {
        auto *item = new QListWidgetItem(
            QIcon::fromTheme("vcs-git", QIcon::fromTheme("folder-git")),
            fi.fileName());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setSizeHint(QSize(0, Config::sidebarIconSize() + 16));
        m_gitList->addItem(item);
      }
    }
  }
  adjustListHeight(m_gitList);
}

// --- Sidebar::buildTagsSection ---
void Sidebar::buildTagsSection(QVBoxLayout *parent) {
  m_tagsWrap = new QWidget(this);
  m_tagsWrap->setStyleSheet(
      QString("background:%1;").arg(TM().colors().bgMain));
  auto *wLay = new QVBoxLayout(m_tagsWrap);
  wLay->setContentsMargins(10, 2, 6, 2);
  wLay->setSpacing(0);

  m_tagsBox = new QWidget(m_tagsWrap);
  m_tagsBox->setObjectName(QStringLiteral("tagsBox"));
  m_tagsBox->setStyleSheet(TM().ssBox());
  auto *vbox = new QVBoxLayout(m_tagsBox);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
  auto *header = new QWidget();
  header->setStyleSheet("background:transparent; border:none;");
  auto *hLay = new QHBoxLayout(header);
  hLay->setContentsMargins(12, 10, 8, 6);
  hLay->setSpacing(4);
  auto *lbl = new QLabel(tr("TAGS"));
  lbl->setStyleSheet(QString("font-size:13px;font-weight:bold;text-transform:"
                             "uppercase;background:transparent;color:%1;")
                         .arg(TM().colors().textAccent));
  hLay->addWidget(lbl, 1);
  auto *addBtn = new QPushButton();
  addBtn->setIcon(
      QIcon::fromTheme(QStringLiteral("list-add"))); // KDE Standard für "+"
  if (addBtn->icon().isNull())
    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
  addBtn->setFixedSize(26, 22);
  addBtn->setCursor(Qt::PointingHandCursor);
  addBtn->setStyleSheet(
      QString(
          "QPushButton { background:transparent; border:none; padding:2px; }"
          "QPushButton:hover { background:%1; border-radius:4px; }")
          .arg(TM().colors().bgHover));
  hLay->addWidget(addBtn);
  vbox->addWidget(header);
  auto *listCont = new QWidget();
  listCont->setStyleSheet("background:transparent; border:none;");
  auto *listLay = new QVBoxLayout(listCont);
  listLay->setContentsMargins(6, 0, 6, 4);
  listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);
  m_tagList = new QListWidget();
  m_tagList->setSelectionMode(QAbstractItemView::NoSelection);
  m_tagList->setFrameShape(QFrame::NoFrame);
  m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_tagList->setIconSize(
      QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
  m_tagList->setStyleSheet(TM().ssListWidget());
  listLay->addWidget(m_tagList);
  vbox->addWidget(listCont);
  auto *toggleBtn = new QPushButton();
  toggleBtn->setCheckable(true);
  toggleBtn->setFixedHeight(16);
  toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
  toggleBtn->setIconSize(QSize(10, 10));
  // Entferne 'color:%1', da Icons über das Theme gesteuert werden
  toggleBtn->setStyleSheet(
      "QPushButton{background:transparent !important; border:none;}");
  vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

  connect(toggleBtn, &QPushButton::toggled, this,
          [listCont, toggleBtn](bool on) {
            listCont->setVisible(!on);
            toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
          });

  connect(addBtn, &QPushButton::clicked, this, [this]() {
    bool ok;
    QString name = sc_getText(this, tr("Neuer Tag"), tr("Tag-Name:"));
    ok = !name.isNull();
    if (!ok || name.trimmed().isEmpty())
      return;
    QColorDialog dlg(QColor(TM().colors().textAccent), this);
    dlg.setWindowTitle(tr("Farbe wählen"));
    dlg.setOptions(QColorDialog::DontUseNativeDialog);
    dlg.setStyleSheet(TM().ssDialog());
    if (dlg.exec() != QDialog::Accepted)
      return;
    QColor col = dlg.currentColor();
    if (!col.isValid())
      return;
    addTagItem(name.trimmed(), col.name());
    adjustListHeight(m_tagList);
    if (m_tagsBox)
      m_tagsBox->updateGeometry();
    if (m_tagsWrap)
      m_tagsWrap->updateGeometry();
    saveTags();
  });

  wLay->addWidget(m_tagsBox);
  parent->addWidget(m_tagsWrap);
}

// --- Sidebar::buildFooter ---
void Sidebar::buildFooter(QVBoxLayout *parent) {
  auto *footer = new QWidget(this);
  footer->setStyleSheet("background:transparent; border:none;");
  auto *lay = new QHBoxLayout(footer);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(2);

  const QString btnSS = TM().ssFooterBtn();

  auto makeBtn = [&](const QString &icon, const QString &tip) -> QToolButton * {
    auto *b = new QToolButton();
    b->setIcon(QIcon::fromTheme(icon));
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(34, 34);
    b->setToolTip(tip);
    b->setStyleSheet(btnSS);
    return b;
  };

  auto *infoBtn = makeBtn("dialog-information", tr("Über"));
  auto *searchBtn = makeBtn("system-search", tr("Suchen"));
  auto *printBtn = makeBtn("document-print", tr("Drucken"));
  auto *chatBtn = makeBtn("mail-message-new", tr("Nachricht"));

  lay->addStretch();
  lay->addWidget(infoBtn);
  lay->addStretch();
  lay->addWidget(searchBtn);
  lay->addStretch();
  lay->addWidget(printBtn);
  lay->addStretch();
  lay->addWidget(chatBtn);
  lay->addStretch();

  parent->addWidget(footer);
}

// --- Sidebar::connectDriveList ---
void Sidebar::connectDriveList() {
  connect(m_driveList, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *it) {
            const QString p = it->data(Qt::UserRole).toString();
            if (p == "kcm_kaccounts") {
              QProcess::startDetached("kcmshell6", {"kcm_kaccounts"});
            } else
              emit driveClicked(p);
          });
}

// ---  ---
// --- Sidebar::resizeEvent ---
void Sidebar::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  if (m_overlayBar && m_scrollArea) {
    const int w = 8;
    const int x = m_scrollArea->x() + m_scrollArea->width() - w;
    const int y = m_scrollArea->y();
    const int h = m_scrollArea->height();
    m_overlayBar->setGeometry(x, y, w, h);
  }
}

// --- Sidebar::loadUserPlaces ---
void Sidebar::loadUserPlaces() {
  const QString xbelPath = QStandardPaths::locate(
      QStandardPaths::GenericDataLocation, "user-places.xbel");
  if (xbelPath.isEmpty())
    return;

  // KDirWatch: XBEL live überwachen — neu laden wenn Dolphin etc. Orte ändert
  static KDirWatch *s_placesWatcher = nullptr;
  if (!s_placesWatcher) {
    s_placesWatcher = new KDirWatch(this);
    s_placesWatcher->addFile(xbelPath);
    connect(s_placesWatcher, &KDirWatch::dirty, this, [this]() {
      loadUserPlaces();
      DriveManager::instance()->refreshAll();
    });
  }

  QFile f(xbelPath);
  if (!f.open(QIODevice::ReadOnly))
    return;

  QXmlStreamReader xml(&f);
  QString href, title;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("bookmark")) {
        href = xml.attributes().value(QStringLiteral("href")).toString();
        title.clear();
      } else if (xml.name() == QLatin1String("title")) {
        title = xml.readElementText();
      }
    } else if (xml.isEndElement() && xml.name() == QLatin1String("bookmark")) {
      if (href.isEmpty())
        continue;
      const QUrl url(href);
      const QString scheme = url.scheme().toLower();

      // gdrive ausschliesslich via KIO::listDir

      // Andere Netzwerkplätze persistent speichern
      static const QStringList netSchemes = {"smb",    "sftp",   "ftp", "ftps",
                                             "davs",   "dav",    "nfs", "fish",
                                             "webdav", "webdavs"};
      if (netSchemes.contains(scheme)) {
        auto s = Config::group("NetworkPlaces");
        QStringList saved = s.readEntry("places", QStringList());

        // Prüfen ob URL bereits vorhanden (mit oder ohne UserInfo)
        QUrl checkUrl(href);
        checkUrl.setUserInfo(QString());
        bool alreadyIn = false;
        for (const QString &p : saved) {
          QUrl pu(p);
          pu.setUserInfo(QString());
          if (pu == checkUrl) {
            alreadyIn = true;
            break;
          }
        }

        if (!alreadyIn) {
          saved << href;
          s.writeEntry("places", saved);
          const QString key = QString(href).replace("/", "_").replace(":", "_");
          const QString n = title.isEmpty() ? url.host() : title;
          s.writeEntry("name_" + key, n);
          s.config()->sync();
        }
      }
    }
  }
}

// Sidebar::updateDrives
// ---  ---
void Sidebar::renameNetworkPlace(const QString &path, const QString &newName) {
  // NetworkPlaces aktualisieren
  auto s = Config::group("NetworkPlaces");
  const QString npath = mw_normalizePath(path);
  s.writeEntry("name_" + QString(npath).replace("/", "_").replace(":", "_"),
               newName);
  s.config()->sync();

  // CustomGroups Settings aktualisieren
  auto gs = Config::group("CustomGroups");
  const QStringList groups = gs.readEntry("groups", QStringList());
  for (const QString &grp : groups) {
    KConfigGroup g(gs.config(), gs.name() + "/group_" + grp);
    int cnt = g.readEntry("size", 0);
    QList<QPair<QString, QString>> items;
    bool changed = false;
    for (int i = 1; i <= cnt; ++i) {
      KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
      QString p = itemG.readEntry("path", QString());
      QString n = itemG.readEntry("name", QString());
      if (p == path) {
        n = newName;
        changed = true;
      }
      items << qMakePair(p, n);
    }
    if (changed) {
      KConfigGroup(gs.config(), gs.name() + "/group_" + grp).deleteGroup();
      KConfigGroup gNew(gs.config(), gs.name() + "/group_" + grp);
      gNew.writeEntry("size", items.size());
      for (int i = 0; i < items.size(); ++i) {
        KConfigGroup itemG(gNew.config(),
                           gNew.name() + "/" + QString::number(i + 1));
        itemG.writeEntry("path", items[i].first);
        itemG.writeEntry("name", items[i].second);
      }
    }
  }
  gs.config()->sync();

  // Alle QListWidgets in der Sidebar direkt aktualisieren (kein Neu-Erstellen)
  auto updateList = [&](QListWidget *list) {
    if (!list)
      return;
    for (int i = 0; i < list->count(); ++i) {
      if (list->item(i)->data(Qt::UserRole).toString() == path)
        list->item(i)->setText(newName);
    }
  };
  updateList(m_driveList);
  updateList(m_netList);
  // Custom group lists
  for (QListWidget *list : findChildren<QListWidget *>())
    updateList(list);

  DriveManager::instance()->refreshAll();
  emit drivesChanged();
}

void Sidebar::applyIconSizes() {
  if (m_driveList)
    m_driveList->setIconSize(
        QSize(Config::driveIconSize(), Config::driveIconSize()));
  if (m_netList)
    m_netList->setIconSize(
        QSize(Config::driveIconSize(), Config::driveIconSize()));
  if (m_tagList)
    m_tagList->setIconSize(
        QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
  // CustomGroups (Orte, Favoriten etc.) — nur Listen die NICHT drive/net/tag
  // sind
  for (auto *list : findChildren<QListWidget *>()) {
    if (list == m_driveList || list == m_netList || list == m_tagList)
      continue;
    list->setIconSize(
        QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
  }
}

void Sidebar::updateDrives() {
  static bool s_updating = false;
  if (s_updating)
    return;
  s_updating = true;

  m_driveList->clear();
  if (m_netList)
    m_netList->clear();
  bool hasNet = false;

  auto *dm = DriveManager::instance();

  for (const auto &info : dm->localDrives()) {
    QString freeStr;
    if (info.isMounted && info.total > 0) {
      freeStr = QString("%1 frei / %2")
                    .arg(sc_fmtStorage(info.free))
                    .arg(sc_fmtStorage(info.total));
    }

    auto *it = new QListWidgetItem(QIcon::fromTheme(info.iconName), info.name,
                                   m_driveList);
    it->setData(Qt::UserRole, info.path);
    it->setData(Qt::UserRole + 1, freeStr);
    it->setData(Qt::UserRole + 2, info.udi);
    it->setData(Qt::UserRole + 10, info.total);
    it->setData(Qt::UserRole + 11, info.free);
    if (!info.isMounted) {
      it->setForeground(QColor(TM().colors().textMuted));
      it->setSizeHint(QSize(0, Config::sidebarDriveRowHeight()));
    }
  }

  if (m_netList) {
    for (const auto &info : dm->networkDrives()) {
      hasNet = true;
      auto *it = new QListWidgetItem(QIcon::fromTheme(info.iconName), info.name,
                                     m_netList);
      it->setData(Qt::UserRole, info.path);
      it->setData(Qt::UserRole + 1,
                  info.subtitle.isEmpty() ? info.path : info.subtitle);
      it->setSizeHint(QSize(0, info.subtitle.isEmpty()
                                   ? Config::sidebarDriveRowHeight()
                                   : Config::sidebarNetRowHeight() - 8));
      it->setData(Qt::UserRole + 10, info.total);
      it->setData(Qt::UserRole + 11, info.free);
    }
  }

  if (m_netBox) {
    m_netBox->setVisible(hasNet);
    if (hasNet && m_netList) {
      int netH = 0;
      for (int i = 0; i < m_netList->count(); ++i)
        netH += m_netList->sizeHintForRow(i);
      if (netH > 0 && m_netList->height() != netH) {
        m_netList->setFixedHeight(netH);
        m_netBox->adjustSize();
        if (m_netBox->parentWidget())
          m_netBox->parentWidget()->adjustSize();
      }
    }
  }

  int totalH = 0;
  for (int i = 0; i < m_driveList->count(); ++i)
    totalH += m_driveList->sizeHintForRow(i);
  if (m_driveList->height() != qMax(1, totalH))
    m_driveList->setFixedHeight(qMax(1, totalH));
  m_driveList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_driveList->updateGeometry();
  s_updating = false;
}

void Sidebar::setupDriveContextMenu() {
  m_driveList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_driveList, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            auto *item = m_driveList->itemAt(pos);
            if (!item)
              return;
            showDriveContextMenu(item, pos);
          });
}

void Sidebar::showDriveContextMenu(QListWidgetItem *item, const QPoint &pos) {
  const QString path = item->data(Qt::UserRole).toString();
  const QString name = item->text();
  const QString udi = item->data(Qt::UserRole + 2).toString();
  const bool isSolid = path.startsWith("solid:");
  const bool isGdrive = path.startsWith("gdrive:/");
  // Gemountetes Solid-Laufwerk: hat UDI aber kein solid:-Prefix
  const bool isMountedSolid = !udi.isEmpty() && !isSolid && !isGdrive;

  QMenu menu(this);
  menu.setStyleSheet(TM().ssMenu());

  menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen"),
                 this, [this, path]() { emit driveClicked(path); });
  auto *openInMenu = menu.addMenu(
      QIcon::fromTheme(QStringLiteral("folder-open")), tr("Öffnen in"));
  openInMenu->setStyleSheet(TM().ssMenu());
  openInMenu->addAction(tr("Linke Pane"), this,
                        [this, path]() { emit driveClickedLeft(path); });
  openInMenu->addAction(tr("Rechte Pane"), this,
                        [this, path]() { emit driveClickedRight(path); });
  menu.addSeparator();

  // Gemountetes Solid-Laufwerk: Aushängen via MainWindow koordinieren
  if (isMountedSolid) {
    menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")),
                   tr("Aushängen"), this,
                   [this, udi]() { emit teardownRequested(udi); });
  }

  // Nicht-gemountetes normales Laufwerk: umount via Solid
  if (!isSolid && !isMountedSolid && path != "/" && !path.isEmpty() &&
      !isGdrive) {
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("media-eject")), tr("Auswerfen"), this,
        [this, path]() {
          const auto devices = Solid::Device::listFromType(
              Solid::DeviceInterface::StorageAccess);
          QString foundUdi;
          for (const Solid::Device &d : devices) {
            const auto *a = d.as<Solid::StorageAccess>();
            if (a && a->filePath() == path) {
              foundUdi = d.udi();
              break;
            }
          }
          if (foundUdi.isEmpty()) {
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
            return;
          }
          auto *device = new Solid::Device(foundUdi);
          auto *acc = device->as<Solid::StorageAccess>();
          if (!acc) {
            delete device;
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
            return;
          }
          connect(
              acc, &Solid::StorageAccess::teardownDone, this,
              [this, device](Solid::ErrorType, QVariant, const QString &) {
                DriveManager::instance()->refreshAll();
                emit drivesChanged();
                delete device;
              },
              Qt::SingleShotConnection);
          acc->teardown();
        });
  }

  if (isSolid) {
    Solid::Device dev(path.mid(6));
    auto *acc = dev.as<Solid::StorageAccess>();
    bool mounted = acc && acc->isAccessible();
    const QString solidUdi = dev.udi();
    if (mounted) {
      menu.addAction(QIcon::fromTheme(QStringLiteral("media-eject")),
                     tr("Aushängen"), this,
                     [this, solidUdi]() { emit teardownRequested(solidUdi); });
    } else {
      menu.addAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")),
                     tr("Einhängen"), this, [this, solidUdi]() {
                       Solid::Device dev(solidUdi);
                       auto *acc = dev.as<Solid::StorageAccess>();
                       if (!acc)
                         return;
                       connect(
                           acc, &Solid::StorageAccess::setupDone, this,
                           [this](Solid::ErrorType, QVariant, const QString &) {
                             DriveManager::instance()->refreshAll();
                             emit drivesChanged();
                           },
                           Qt::SingleShotConnection);
                       acc->setup();
                     });
    }
  }

  menu.addSeparator();

  // Für manuell gepinnte Netzwerkeinträge: umbenennen + aus Liste entfernen
  {
    auto netCheck = Config::group("NetworkPlaces");
    const QStringList netPlaces = netCheck.readEntry("places", QStringList());
    if (netPlaces.contains(path)) {
      menu.addAction(
          QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen"),
          this, [this, path, name]() {
            QDialog dlg(this);
            dlg.setWindowTitle(tr("Umbenennen"));
            dlg.setStyleSheet(TM().ssDialog());
            auto *vl = new QVBoxLayout(&dlg);
            vl->addWidget(new QLabel(tr("Anzeigename:")));
            auto *edit = new QLineEdit(name, &dlg);
            vl->addWidget(edit);
            auto *box = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            vl->addWidget(box);
            connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            if (dlg.exec() == QDialog::Accepted &&
                !edit->text().trimmed().isEmpty()) {
              renameNetworkPlace(path, edit->text().trimmed());
            }
          });
      menu.addAction(
          QIcon::fromTheme(QStringLiteral("list-remove")),
          tr("Aus Laufwerken entfernen"), this, [this, path]() {
            auto s = Config::group("NetworkPlaces");
            QStringList saved = s.readEntry("places", QStringList());
            const QString npath = mw_normalizePath(path);
            saved.removeAll(npath);
            QString otherVersion = npath.endsWith('/')
                                       ? npath.left(npath.length() - 1)
                                       : npath + "/";
            if (otherVersion != "/")
              saved.removeAll(otherVersion);

            s.writeEntry("places", saved);
            s.deleteEntry("name_" +
                          QString(npath).replace("/", "_").replace(":", "_"));
            s.deleteEntry(
                "name_" +
                QString(otherVersion).replace("/", "_").replace(":", "_"));
            s.config()->sync();
            DriveManager::instance()->refreshAll();
            emit drivesChanged();
          });
      menu.addSeparator();
    }
  }

  auto *copyMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                tr("Kopieren"));
  copyMenu->setStyleSheet(TM().ssMenu());
  copyMenu->addAction(tr("Pfad kopieren"), this, [path]() {
    QGuiApplication::clipboard()->setText(path);
  });
  copyMenu->addAction(tr("Name kopieren"), this, [name]() {
    QGuiApplication::clipboard()->setText(name);
  });

  if (!isSolid && !isGdrive) {
    menu.addAction(
        QIcon::fromTheme(QStringLiteral("emblem-symbolic-link")),
        tr("Verknüpfung erstellen"), this, [this, path]() {
          const QString desktop =
              QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
          const QUrl url = QUrl::fromUserInput(path);
          const QString dirName = url.isLocalFile()
                                      ? QDir(url.toLocalFile()).dirName()
                                      : url.fileName();
          const QByteArray content =
              QStringLiteral(
                  "[Desktop Entry]\nType=Link\nName=%1\nURL=%2\nIcon=folder\n")
                  .arg(dirName, path)
                  .toUtf8();
          const QUrl dest =
              QUrl::fromLocalFile(desktop + "/" + dirName + ".desktop");
          auto *job = KIO::storedPut(content, dest, -1, KIO::Overwrite);
          job->setUiDelegate(KIO::createDefaultJobUiDelegate(
              KJobUiDelegate::AutoHandlingEnabled, this));
          job->start();
        });
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                   tr("Eigenschaften"), this, [path]() {
                     auto *dlg = new KPropertiesDialog(
                         QUrl::fromLocalFile(path), nullptr);
                     dlg->setAttribute(Qt::WA_DeleteOnClose);
                     dlg->show();
                   });
  }

  menu.exec(m_driveList->mapToGlobal(pos));
}

// --- Sidebar::onNewGroupDialog ---

void Sidebar::addNetworkPlace(const QString &path, const QString &name) {
  if (path.isEmpty())
    return;
  auto s = Config::group("NetworkPlaces");
  QStringList saved = s.readEntry("places", QStringList());
  const QString npath = mw_normalizePath(path);
  if (!saved.contains(npath)) {
    saved << npath;
    s.writeEntry("places", saved);
    s.writeEntry("name_" + QString(npath).replace("/", "_").replace(":", "_"),
                 name);
    s.config()->sync();
  }
  DriveManager::instance()->refreshAll();
  emit drivesChanged();
}

// --- Sidebar::showPlaceContextMenu ---
void Sidebar::showPlaceContextMenu(QListWidgetItem *item, QListWidget *list,
                                   const QPoint &pos,
                                   const QString &groupName) {
  const QString path = item->data(Qt::UserRole).toString();
  const QString name = item->text();
  const QString icon = item->data(Qt::UserRole + 2).toString();

  QMenu menu(this);
  menu.setStyleSheet(TM().ssMenu());

  auto *openIn = menu.addMenu(QIcon::fromTheme(QStringLiteral("folder-open")),
                              tr("Öffnen in"));
  openIn->setStyleSheet(TM().ssMenu());
  openIn->addAction(tr("Linke Pane"), this,
                    [this, path]() { emit driveClicked(path); });
  openIn->addAction(tr("Rechte Pane"), this,
                    [this, path]() { emit driveClickedRight(path); });

  auto saveList = [list, groupName]() {
    if (!groupName.isEmpty()) {
      auto gs = Config::group("CustomGroups");
      KConfigGroup(gs.config(), gs.name() + "/group_" + groupName)
          .deleteGroup();
      KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
      gNew.writeEntry("size", list->count());

      for (int i = 0; i < list->count(); ++i) {
        KConfigGroup itemG(gNew.config(),
                           gNew.name() + "/" + QString::number(i + 1));
        itemG.writeEntry("path", list->item(i)->data(Qt::UserRole).toString());
        itemG.writeEntry("name", list->item(i)->text());
        itemG.writeEntry("icon",
                         list->item(i)->data(Qt::UserRole + 2).toString());
      }
      gs.config()->sync();
    }
  };

  sc_buildPlaceMenu(
      menu, path, name, icon,
      [list, item, saveList]() {
        delete list->takeItem(list->row(item));
        adjustListHeight(list);
        saveList();
      },
      [item, saveList](const QString &newName, const QString &newPath,
                       const QString &newIcon) {
        item->setText(newName);
        item->setData(Qt::UserRole, newPath);
        item->setData(Qt::UserRole + 2, newIcon);

        // Icon aktualisieren
        if (!newIcon.isEmpty()) {
          item->setIcon(QIcon::fromTheme(newIcon));
        } else {
          item->setIcon(QIcon::fromTheme(
              KIO::iconNameForUrl(QUrl::fromLocalFile(newPath))));
        }
        saveList();
      });

  menu.exec(list->mapToGlobal(pos));
}

// --- Sidebar::setupTags / addTagItem / saveTags ---

void Sidebar::onTrashChanged() {
  if (!m_trashLister)
    return;
  const bool isEmpty = m_trashLister->items().isEmpty();
  const QString iconName = isEmpty ? QStringLiteral("user-trash")
                                   : QStringLiteral("user-trash-full");
  QIcon trashIcon = QIcon::fromTheme(iconName);
  if (trashIcon.isNull())
    trashIcon = QIcon::fromTheme(isEmpty ? QStringLiteral("trash-empty")
                                         : QStringLiteral("trash-full"));

  // Alle QListWidgets in der Sidebar durchlaufen
  QList<QListWidget *> lists;
  if (m_driveList)
    lists << m_driveList;
  if (m_favList)
    lists << m_favList;
  if (m_netList)
    lists << m_netList;

  // Benutzerdefinierte Gruppen finden
  if (m_contentLayout) {
    for (int i = 0; i < m_contentLayout->count(); ++i) {
      if (auto *box = m_contentLayout->itemAt(i)->widget()) {
        if (auto *lw = box->findChild<QListWidget *>()) {
          if (!lists.contains(lw))
            lists << lw;
        }
      }
    }
  }

  for (auto *list : lists) {
    for (int i = 0; i < list->count(); ++i) {
      auto *item = list->item(i);
      const QString path = item->data(Qt::UserRole).toString();
      if (QUrl(path).scheme() == QStringLiteral("trash")) {
        item->setIcon(trashIcon);
      }
    }
  }
}

// --- Sidebar::saveToUserPlaces ---
// Schreibt einen neuen Eintrag in user-places.xbel (KDE-Places)
// damit er auch in Dolphin, Gwenview etc. erscheint
void Sidebar::saveToUserPlaces(const QString &url, const QString &name) {
  const QString xbelPath =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/user-places.xbel");

  // XBEL lesen
  QFile f(xbelPath);
  QByteArray existing;
  if (f.open(QIODevice::ReadOnly)) {
    existing = f.readAll();
    f.close();
  }

  // Bereits vorhanden?
  if (existing.contains(url.toUtf8()))
    return;

  // Neuen Bookmark-Eintrag einfügen vor </xbel>
  const QString entry = QStringLiteral("  <bookmark href=\"%1\">\n"
                                       "    <title>%2</title>\n"
                                       "  </bookmark>\n")
                            .arg(url, name);

  if (existing.contains("</xbel>")) {
    existing.replace("</xbel>", (entry + QStringLiteral("</xbel>")).toUtf8());
  } else if (existing.isEmpty()) {
    const QString tmpl =
        QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE xbel>\n"
            "<xbel "
            "xmlns:bookmark=\"http://www.freedesktop.org/standards/"
            "desktop-bookmarks\""
            " xmlns:kdepriv=\"http://www.kde.org/kdepriv\" version=\"1.0\">\n"
            "%1"
            "</xbel>\n")
            .arg(entry);
    existing = tmpl.toUtf8();
  } else {
    return; // Ungültiges Format — nicht anfassen
  }

  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(existing);
}
