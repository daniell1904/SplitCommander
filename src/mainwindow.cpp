// --- mainwindow.cpp — SplitCommander Hauptfenster ---

#include "mainwindow.h"
#include "agebadgedialog.h"
#include "filepane.h"
#include "joboverlay.h"
#include "panewidgets.h"
#include "settingsdialog.h"
#include "shortcutdialog.h"
#include "terminalutils.h"
#include "thememanager.h"

#include <KFileItem>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>
#include <KPropertiesDialog>
#include "drophandler.h"
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include "dialogutils.h"
#include <KFileItem>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTreeWidget>
#include <QUrl>
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <functional>

static QString mw_fmtSize(qint64 sz) {
  if (sz < 1024)
    return QString("%1 B").arg(sz);
  if (sz < 1024 * 1024)
    return QString("%1 KB").arg(sz / 1024);
  if (sz < 1024LL * 1024 * 1024)
    return QString("%1 MB").arg(sz / (1024 * 1024));
  return QString("%1 GB").arg(sz / (1024LL * 1024 * 1024));
}

// ---  ---
// StyleSheet-Konstanten

static QString sc_rootVolumeName() {
  const QString name = QStorageInfo(QStringLiteral("/")).name();
  return name.isEmpty() ? QObject::tr("System") : name;
}

static void mw_applyMenuShadow(QMenu *menu) {
  if (!menu)
    return;
  auto *shadow = new QGraphicsDropShadowEffect(menu);
  shadow->setBlurRadius(20);
  shadow->setOffset(0, 4);
  shadow->setColor(QColor(0, 0, 0, 140));
  menu->setGraphicsEffect(shadow);
}

// --- PaneToolbar ---
PaneToolbar::PaneToolbar(QWidget *parent) : QWidget(parent) {
  setFixedHeight(96);
  setAttribute(Qt::WA_StyledBackground, true);

  setStyleSheet(TM().ssToolbar());

  auto *vlay = new QVBoxLayout(this);
  vlay->setContentsMargins(12, 10, 12, 10);
  vlay->setSpacing(6);

  auto mk = [&](const QString &icon, const QString &tip,
                auto sig) -> QToolButton * {
    auto *b = new QToolButton();
    b->setIcon(QIcon::fromTheme(icon));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(28, 28);
    b->setToolTip(tip);
    b->setStyleSheet(TM().ssActionBtn());
    connect(b, &QToolButton::clicked, this, sig);
    return b;
  };

  // Row 1: Pfad | Aktionen
  auto *r1 = new QHBoxLayout();
  r1->setContentsMargins(0, 0, 0, 0);
  r1->setSpacing(2);
  m_pathLabel = new QLabel();
  m_pathLabel->setStyleSheet(
      QString("color:%1;font-size:18px;font-weight:300;background:transparent;")
          .arg(TM().colors().textAccent));
  r1->addWidget(m_pathLabel);
  r1->addStretch(1);
  r1->addWidget(
      mk("view-sort-ascending", tr("Sortieren"), &PaneToolbar::sortClicked));
  m_newFolderBtn = mk("folder-new", tr("Neu"), &PaneToolbar::newFolderClicked);
  r1->addWidget(m_newFolderBtn);
  m_copyBtn = mk("edit-copy", tr("Kopieren"), &PaneToolbar::copyClicked);
  r1->addWidget(m_copyBtn);
  m_emptyTrashBtn = mk("trash-empty", tr("Papierkorb leeren"), &PaneToolbar::emptyTrashClicked);
  m_emptyTrashBtn->hide();
  r1->addWidget(m_emptyTrashBtn);
  vlay->addLayout(r1);

  // Row 2: Anzahl | Größe
  auto *r2 = new QHBoxLayout();
  r2->setContentsMargins(0, 0, 0, 0);
  r2->setSpacing(0);
  m_countLabel = new QLabel();
  m_countLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().textPrimary));
  m_selectedLabel = new QLabel();
  m_selectedLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().accent));
  m_selectedLabel->hide();
  m_sizeLabel = new QLabel();
  m_sizeLabel->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().textPrimary));
  m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  r2->addWidget(m_countLabel);
  r2->addWidget(m_selectedLabel);
  r2->addWidget(m_sizeLabel);
  r2->addStretch(1);
  vlay->addLayout(r2);

  // Row 3: Navigation | Ansichtsmodi
  auto *r3 = new QHBoxLayout();
  r3->setContentsMargins(0, 0, 0, 0);
  r3->setSpacing(2);
  r3->addWidget(mk("go-previous", tr("Zurück"), &PaneToolbar::backClicked));
  r3->addWidget(mk("go-next", tr("Vorwärts"), &PaneToolbar::forwardClicked));
  r3->addWidget(mk("go-up", tr("Hoch"), &PaneToolbar::upClicked));

  auto *foldersFirstBtn = new QToolButton();
  foldersFirstBtn->setIcon(QIcon::fromTheme("go-parent-folder"));
  foldersFirstBtn->setIconSize(QSize(16, 16));
  foldersFirstBtn->setFixedSize(28, 28);
  foldersFirstBtn->setCheckable(true);
  foldersFirstBtn->setChecked(true);
  foldersFirstBtn->setToolTip(tr("Ordner zuerst"));
  foldersFirstBtn->setStyleSheet(TM().ssActionBtn());
  connect(foldersFirstBtn, &QToolButton::toggled, this,
          &PaneToolbar::foldersFirstToggled);
  r3->addWidget(foldersFirstBtn);
  r3->addStretch(1);

  auto *viewGroup = new QButtonGroup(this);
  viewGroup->setExclusive(true);
  connect(viewGroup, &QButtonGroup::idClicked, this,
          &PaneToolbar::viewModeChanged);

  int modeId = 0;
  for (auto &v :
       {std::pair<const char *, const char *>{"view-list-tree", "Details"},
        {"view-list-details", "Kompakt"},
        {"view-list-icons", "Symbole"}}) {
    auto *b = new QToolButton();
    b->setIcon(QIcon::fromTheme(v.first));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(28, 28);
    b->setToolTip(v.second);
    b->setStyleSheet(TM().ssActionBtn());
    b->setCheckable(true);
    if (modeId == 0)
      b->setChecked(true);
    viewGroup->addButton(b, modeId++);
    r3->addWidget(b);
  }
  vlay->addLayout(r3);
}

void PaneToolbar::setPath(const QString &path) {
  if (!m_pathLabel)
    return;
  QUrl url(path);
  QString name = url.fileName();
  if (name.isEmpty()) {
    name = url.isLocalFile() ? url.toLocalFile() : path;
  }
  m_pathLabel->setText(name);
  const bool isTrash = (path == "trash:/" || path.startsWith("trash:"));
  m_emptyTrashBtn->setVisible(isTrash);
  m_newFolderBtn->setVisible(!isTrash);
  m_copyBtn->setVisible(!isTrash);
}

void PaneToolbar::setCount(int count, qint64 totalBytes) {
  if (m_countLabel)
    m_countLabel->setText(tr("%1 Elemente").arg(count));
  if (m_sizeLabel) {
    QString s;
    if (totalBytes < 1024)
      s = QString("%1 B").arg(totalBytes);
    else if (totalBytes < 1024 * 1024)
      s = QString("%1 KB").arg(totalBytes / 1024);
    else
      s = QString("%1 MB").arg(totalBytes / (1024 * 1024));
    m_sizeLabel->setText(" | " + s);
  }
}

void PaneToolbar::setSelected(int count) {
  if (!m_selectedLabel)
    return;
  if (count > 0) {
    m_selectedLabel->setText(tr(" | %1 ausgewählt").arg(count));
    m_selectedLabel->show();
  } else
    m_selectedLabel->hide();
}

// --- MillerWidgetItem für Ordner-Sortierung ---
class MillerWidgetItem : public QListWidgetItem {
public:
  MillerWidgetItem(const QIcon &icon, const QString &text, bool isDir,
                   QListWidget *parent = nullptr)
      : QListWidgetItem(icon, text, parent), m_isDir(isDir) {}

  bool operator<(const QListWidgetItem &other) const override {
    const auto *otherM = dynamic_cast<const MillerWidgetItem *>(&other);
    if (otherM) {
      if (m_isDir != otherM->m_isDir)
        return m_isDir; // Ordner vor Dateien
    }
    return text().localeAwareCompare(other.text()) < 0;
  }

private:
  bool m_isDir;
};

// --- MillerColumn ---
MillerColumn::MillerColumn(QWidget *parent) : QWidget(parent) {
  setMinimumWidth(120);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  m_lister = new KDirLister(this);
  // Wir zeigen sowohl Ordner als auch Dateien in den Spalten an
  connect(m_lister, &KDirLister::newItems, this,
          [this](const KFileItemList &items) {
            QFileIconProvider iconProv;
            for (const KFileItem &item : items) {
              auto *it = new MillerWidgetItem(
                  QIcon::fromTheme(item.iconName(),
                                   iconProv.icon(QFileIconProvider::Folder)),
                  item.name(), item.isDir());
              it->setData(Qt::UserRole, item.url().toString());
              m_list->addItem(it);
            }
          });
  connect(m_lister, &KDirLister::completed, this,
          [this]() { m_list->sortItems(); });

  static const QStringList discoverProtocols = {"gdrive:/", "mtp:/",
                                                "remote:/"};
  for (const QString &proto : discoverProtocols) {
    Q_UNUSED(proto);
    auto *dl = new KDirLister(this);
    dl->setAutoErrorHandlingEnabled(false);
    m_discoveryListers.append(dl);
    connect(dl, &KDirLister::newItems, this,
            [this](const KFileItemList &items) {
              if (m_path != "__drives__")
                return;
              for (const KFileItem &item : items) {
                const QString url = item.url().toString();
                // Duplikate prüfen
                bool exists = false;
                for (int i = 0; i < m_list->count(); ++i) {
                  if (m_list->item(i)->data(Qt::UserRole).toString() == url) {
                    exists = true;
                    break;
                  }
                }
                if (!exists) {
                  auto *it =
                      new MillerWidgetItem(QIcon::fromTheme(item.iconName()),
                                           item.name(), item.isDir(), m_list);
                  it->setData(Qt::UserRole, url);
                  it->setSizeHint(QSize(0, 44));
                }
              }
            });
  }

  m_header = new QPushButton();
  m_header->setObjectName("MillerHeader");
  m_header->setFlat(true);
  m_header->setFixedHeight(40);
  m_header->setIconSize(QSize(22, 22));
  m_header->setStyleSheet(
      QString(
          "QPushButton#MillerHeader { background:%1; border:none; "
          "border-right:1px solid %2; color:%3; "
          "font-family:'Segoe UI Semilight', 'Roboto Light', sans-serif; "
          "font-weight:300; font-size:22px; padding:4px 12px; text-align:left; "
          "border-top-right-radius:8px; border-top-left-radius:0px; "
          "border-bottom-right-radius:0px; border-bottom-left-radius:0px; } "
          "QPushButton#MillerHeader:hover { background:%4; color:%5; "
          "border-top-right-radius:8px; }")
          .arg(TM().colors().bgPanel, TM().colors().separator,
               TM().colors().textAccent, TM().colors().bgHover,
               TM().colors().textPrimary));
  lay->addWidget(m_header);
  connect(m_header, &QPushButton::clicked, this, [this]() {
    emit headerClicked(m_path == "__drives__" ? QString() : m_path);
  });

  m_list = new QListWidget();
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->verticalScrollBar()->hide();
  m_list->setStyleSheet(TM().ssColInactive());
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_list->setDragEnabled(true);
  m_list->setAcceptDrops(true);
  m_list->setDropIndicatorShown(true);
  m_list->setDragDropMode(QAbstractItemView::DragDrop);

  auto resolver = [this](const QModelIndex &idx) -> QUrl {
      QUrl destUrl = QUrl::fromUserInput(m_path);
      if (idx.isValid()) {
          QListWidgetItem *it = m_list->item(idx.row());
          if (it) {
              QString itemPath = it->data(Qt::UserRole).toString();
              if (!itemPath.isEmpty() && !itemPath.startsWith("solid:")) {
                  destUrl = QUrl::fromUserInput(itemPath);
              }
          }
      }
      return destUrl;
  };
  m_list->viewport()->installEventFilter(new DropHandler(m_list, resolver, m_list));

  lay->addWidget(m_list);

  connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
    emit activated(this);
    const QString p = it->data(Qt::UserRole).toString();
    if (p.isEmpty())
      return;
    if (p.startsWith("solid:")) {
      // Nicht eingehängtes Laufwerk – einhängen
      const QString udi = it->data(Qt::UserRole + 1).toString();
      if (!udi.isEmpty())
        emit setupRequested(udi);
    } else {
      emit entryClicked(p, this);
    }
  });

  connect(
      m_list, &QListWidget::customContextMenuRequested, this,
      [this](const QPoint &pos) {
        QListWidgetItem *it = m_list->itemAt(pos);
        if (!it)
          return;

        const QString itemPath = it->data(Qt::UserRole).toString();
        QMenu menu(this);
        menu.setStyleSheet(TM().ssMenu());

        if (m_path == QLatin1String("__drives__")) {
          // --- Laufwerk-Menü ---
          const QString udi = it->data(Qt::UserRole + 1).toString();
          mw_applyMenuShadow(&menu);

          // Öffnen + Öffnen in für alle navigierbaren Einträge
          if (!itemPath.isEmpty() && !itemPath.startsWith("solid:")) {
            menu.addAction(
                QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
                [this, itemPath]() { emit entryClicked(itemPath, this); });
            auto *openInMenu =
                menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
            openInMenu->setStyleSheet(TM().ssMenu());
            openInMenu->addAction(
                tr("Linke Ansicht"), this,
                [this, itemPath]() { emit openInLeft(itemPath); });
            openInMenu->addAction(
                tr("Rechte Ansicht"), this,
                [this, itemPath]() { emit openInRight(itemPath); });
            menu.addSeparator();
          }

          // NetworkPlaces: umbenennen + aus Liste entfernen
          {
            QSettings netCheck("SplitCommander", "NetworkPlaces");
            if (netCheck.value("places").toStringList().contains(itemPath)) {
              menu.addAction(
                  QIcon::fromTheme("edit-rename"), tr("Umbenennen"), this,
                  [this, itemPath, it]() {
                    bool ok;
                    const QString newName = DialogUtils::getText(
                        this, tr("Umbenennen"), tr("Anzeigename:"), it->text(),
                        &ok);
                    if (!ok || newName.trimmed().isEmpty())
                      return;

                    QSettings s("SplitCommander", "NetworkPlaces");
                    s.setValue("name_" +
                                   QString(itemPath).replace("/", "_").replace(
                                       ":", "_"),
                               newName.trimmed());
                    s.sync();
                    populateDrives(); // Neuladen der Spalte
                  });
              menu.addAction(
                  QIcon::fromTheme("list-remove"),
                  tr("Aus Laufwerken entfernen"), this, [this, itemPath]() {
                    QSettings s("SplitCommander", "NetworkPlaces");
                    QStringList saved = s.value("places").toStringList();
                    saved.removeAll(itemPath);
                    s.setValue("places", saved);
                    s.remove(
                        "name_" +
                        QString(itemPath).replace("/", "_").replace(":", "_"));
                    s.sync();
                    emit removeFromPlacesRequested(itemPath);
                  });
              menu.addSeparator();
            }
          }

          auto *copyMenu =
              menu.addMenu(QIcon::fromTheme("edit-copy"), tr("Kopieren"));
          copyMenu->setStyleSheet(TM().ssMenu());
          copyMenu->addAction(tr("Pfad kopieren"), [itemPath]() {
            QGuiApplication::clipboard()->setText(itemPath);
          });
          copyMenu->addAction(tr("Name kopieren"), [it]() {
            QGuiApplication::clipboard()->setText(it->text());
          });
          menu.addSeparator();

          // Solid: Aushängen/Einhängen
          if (!udi.isEmpty()) {
            Solid::Device dev(udi);
            const auto *acc = dev.as<Solid::StorageAccess>();
            if (acc) {
              if (acc->isAccessible())
                menu.addAction(QIcon::fromTheme("media-eject"), tr("Aushängen"),
                               this,
                               [this, udi]() { emit teardownRequested(udi); });
              else
                menu.addAction(QIcon::fromTheme("drive-harddisk"),
                               tr("Einhängen"), this,
                               [this, udi]() { emit setupRequested(udi); });
              menu.addSeparator();
            }
          }

          menu.addAction(QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"),
                         this, [itemPath]() {
                           QGuiApplication::clipboard()->setText(itemPath);
                         });
        } else {
          // --- Ordner-Menü ---
          mw_applyMenuShadow(&menu);
          menu.addAction(
              QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
              [this, itemPath]() { emit entryClicked(itemPath, this); });

          auto *openInMenu =
              menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
          openInMenu->setStyleSheet(TM().ssMenu());
          openInMenu->addAction(tr("Linke Ansicht"), this, [this, itemPath]() {
            emit openInLeft(itemPath);
          });
          openInMenu->addAction(tr("Rechte Ansicht"), this, [this, itemPath]() {
            emit openInRight(itemPath);
          });

          menu.addSeparator();
          menu.addAction(QIcon::fromTheme("utilities-terminal"),
                         tr("Im Terminal öffnen"), this,
                         [itemPath]() { sc_openTerminal(itemPath); });
          menu.addSeparator();
          menu.addAction(QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"),
                         this, [itemPath]() {
                           QGuiApplication::clipboard()->setText(itemPath);
                         });
          menu.addSeparator();
          menu.addAction(
              QIcon::fromTheme("document-properties"), tr("Eigenschaften"),
              this, [this, itemPath]() { emit propertiesRequested(itemPath); });
        }

        menu.exec(m_list->mapToGlobal(pos));
      });
}

void MillerColumn::populateDrives() {
  m_path = "__drives__";
  m_header->setText(tr("Dieser PC"));
  m_header->setIcon(QIcon::fromTheme("computer"));
  m_list->clear();
  m_list->setStyleSheet(TM().ssColDrives());
  m_list->setItemDelegate(new DriveDelegate(true, m_list));

  // --- Eingehängte Laufwerke ---
  QSet<QString> shownUdis;
  for (const Solid::Device &dev :
       Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess)) {
    const auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      continue;

    // Bereits gezeigtes UDI überspringen
    if (shownUdis.contains(dev.udi()))
      continue;

    const bool mounted = acc->isAccessible();
    const QString p = acc->filePath();

    // Systempartitionen überspringen
    if (mounted) {
      if (p.isEmpty())
        continue;
      if (p.startsWith("/boot") || p.startsWith("/efi") ||
          p.startsWith("/snap"))
        continue;
    }

    // Nicht-gemountete Geräte: nur anzeigen wenn echtes Filesystem-Volume
    if (!mounted) {
      const auto *vol = dev.as<Solid::StorageVolume>();
      if (!vol)
        continue;
      if (vol->usage() != Solid::StorageVolume::FileSystem &&
          vol->usage() != Solid::StorageVolume::Other)
        continue;
      const QString lbl = vol->label().toUpper();
      const QString fs = vol->fsType().toLower();
      if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" ||
          lbl == "ESP")
        continue;
      if (fs == "iso9660" || fs == "udf")
        continue;
    }

    shownUdis.insert(dev.udi());

    QString driveName =
        mounted && (p == "/") ? sc_rootVolumeName() : dev.description();
    if (driveName.isEmpty())
      driveName = mounted ? QDir(p).dirName() : dev.udi().section('/', -1);

    QString iconName = dev.icon().isEmpty() ? "drive-harddisk" : dev.icon();
    if (const auto *drv = dev.as<Solid::StorageDrive>()) {
      if (drv->driveType() == Solid::StorageDrive::CdromDrive)
        iconName = "drive-optical";
      else if (drv->isRemovable() || drv->isHotpluggable() ||
               (mounted && p.startsWith("/run/media/")))
        iconName = dev.icon().isEmpty() ? "drive-removable-media" : dev.icon();
    }

    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, driveName);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
    it->setData(Qt::UserRole, mounted ? p : QString("solid:") + dev.udi());
    it->setData(Qt::UserRole + 1, dev.udi());
    it->setSizeHint(QSize(0, 44));

    if (!mounted) {
      it->setForeground(QColor(TM().colors().textMuted));
    }
  }

  // --- Netzwerklaufwerke (FUSE-gemountet) ---
  for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
    if (!storage.isValid() || !storage.isReady())
      continue;
    const QString fs = storage.fileSystemType();
    if (fs != "cifs" && fs != "smb3" && fs != "nfs" && fs != "nfs4" &&
        fs != "sshfs" && fs != "fuse.sshfs" && fs != "davfs" &&
        fs != "fuse.davfs2" && !fs.startsWith("fuse."))
      continue;
    const QString path = storage.rootPath();
    QString name = storage.name().isEmpty()
                       ? QUrl::fromLocalFile(path).fileName()
                       : storage.name();
    if (name.isEmpty())
      name = path;
    QString icon = (fs == "cifs" || fs == "smb3")          ? "network-workgroup"
                   : (fs == "sshfs" || fs == "fuse.sshfs") ? "network-connect"
                                                           : "network-server";
    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, name);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(icon));
    it->setData(Qt::UserRole, path);
    it->setData(Qt::UserRole + 1, fs);
    it->setSizeHint(QSize(0, 44));
  }

  // --- Gespeicherte Netzwerkplätze (NetworkPlaces) ---
  {
    QSettings netSettings("SplitCommander", "NetworkPlaces");
    const QStringList savedPlaces = netSettings.value("places").toStringList();
    for (const QString &p : savedPlaces) {
      const QString scheme = QUrl::fromUserInput(p).scheme().toLower();
      const QString savedName =
          netSettings
              .value("name_" + QString(p).replace("/", "_").replace(":", "_"),
                     scheme == "gdrive" ? "Google Drive"
                                        : QUrl::fromUserInput(p).fileName())
              .toString();
      QString iconName = scheme == "gdrive"      ? "folder-gdrive"
                         : scheme == "smb"       ? "network-workgroup"
                         : scheme == "sftp"      ? "network-connect"
                         : scheme == "mtp"       ? "multimedia-player"
                         : scheme == "bluetooth" ? "bluetooth"
                                                 : "network-server";
      auto *it = new QListWidgetItem(m_list);
      it->setData(Qt::DisplayRole, savedName);
      it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
      it->setData(Qt::UserRole, QUrl::fromUserInput(p).toString());
      it->setData(Qt::UserRole + 1, scheme);
      it->setSizeHint(QSize(0, 44));
    }
  }

  // --- Automatische Konto-Entdeckung (Parallel) ---
  for (auto *dl : m_discoveryListers) {
    const QString proto = m_discoveryListers.indexOf(dl) == 0   ? "gdrive:/"
                          : m_discoveryListers.indexOf(dl) == 1 ? "mtp:/"
                                                                : "remote:/";
    dl->openUrl(QUrl(proto), KDirLister::NoFlags);
  }
}

void MillerColumn::populateDir(const QString &path) {
  m_path = path;
  QUrl url(path);
  if (url.scheme().isEmpty() && !path.isEmpty())
    url = QUrl::fromUserInput(path);

  QString name;
  const QString local = url.toLocalFile();
  if (path == "__drives__")
    name = tr("Dieser PC");
  else if (local == "/" || (url.path() == "/" && url.scheme().isEmpty()))
    name = sc_rootVolumeName();
  else
    name = url.fileName().isEmpty() ? (local.isEmpty() ? path : local)
                                    : url.fileName();

  m_header->setText(name);
  if (path == "__drives__")
    m_header->setIcon(QIcon::fromTheme("computer"));
  else if (url.scheme() == "gdrive")
    m_header->setIcon(QIcon::fromTheme("folder-gdrive"));
  else if (url.scheme() == "mtp")
    m_header->setIcon(QIcon::fromTheme("multimedia-player"));
  else
    m_header->setIcon(QIcon::fromTheme("folder"));

  m_list->clear();
  m_list->setStyleSheet(TM().ssColInactive());
  m_list->setItemDelegate(new MillerItemDelegate(m_list));

  if (path != "__drives__") {
    m_lister->stop();
    m_lister->openUrl(url, KDirLister::NoFlags);
  }
}

void MillerColumn::refreshStyle() {
  setActive(m_list->styleSheet() != TM().ssColInactive());
}

void MillerColumn::setActive(bool active) {
  m_list->setStyleSheet(
      m_path == "__drives__"
          ? TM().ssColDrives()
          : (active ? TM().ssColActive() : TM().ssColInactive()));
}

// --- MillerArea ---
static constexpr int FULL_COLS = 3;

MillerArea::MillerArea(QWidget *parent) : QWidget(parent) {
  setStyleSheet(TM().ssPane() + "border-top:none;");
  auto *outerLay = new QVBoxLayout(this);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->setSpacing(0);

  m_rowWidget = new QWidget();
  m_rowWidget->setStyleSheet(TM().ssPane());
  m_rowLayout = new QHBoxLayout(m_rowWidget);
  m_rowLayout->setContentsMargins(0, 0, 0, 0);
  m_rowLayout->setSpacing(0);

  m_stripDivider = new QFrame();
  m_stripDivider->setFrameShape(QFrame::VLine);
  m_stripDivider->setStyleSheet(
      QString("background:%1;color:%1;").arg(TM().colors().colActive));
  m_stripDivider->setFixedWidth(2);
  m_stripDivider->setFrameShadow(QFrame::Sunken);
  m_stripDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  m_stripDivider->setVisible(false);
  m_rowLayout->addWidget(m_stripDivider);

  m_colContainer = new QWidget();
  m_colContainer->setStyleSheet(TM().ssPane());
  m_colLayout = new QHBoxLayout(m_colContainer);
  m_colLayout->setContentsMargins(0, 0, 0, 0);
  m_colLayout->setSpacing(0);
  m_rowLayout->addWidget(m_colContainer, 1);

  outerLay->addWidget(m_rowWidget, 1);
}

void MillerArea::updateVisibleColumns() {
  const int n = m_cols.size();
  const int stripCount = qMax(0, n - FULL_COLS);

  // 1. Alte Strips bereinigen
  for (auto *s : m_strips) {
    m_rowLayout->removeWidget(s);
    s->deleteLater();
  }
  m_strips.clear();

  // 2. Neue Strips für ausgeblendete Spalten
  for (int i = 0; i < stripCount; ++i) {
    QUrl u(m_cols[i]->path());
    QString label = (i == 0) ? QStringLiteral("This PC") : u.fileName();
    if (label.isEmpty())
      label = u.path();
    if (label.isEmpty())
      label = m_cols[i]->path();

    auto *strip = new MillerStrip(label, m_rowWidget);
    m_rowLayout->insertWidget(i, strip);
    m_strips.append(strip);

    connect(strip, &MillerStrip::clicked, this, [this, i]() {
      emit focusRequested();
      while (m_cols.size() > i + 1) {
        trimAfter(m_cols[i]);
      }
      updateVisibleColumns();
    });
  }

  // Alte Trenner entfernen und neu aufbauen
  for (auto *sep : m_colSeparators) {
    m_colLayout->removeWidget(sep);
    sep->deleteLater();
  }
  m_colSeparators.clear();

  m_stripDivider->setVisible(stripCount > 0);

  // Spalten ein-/ausblenden und Trenner neu aufbauen
  for (int i = 0; i < n; ++i) {
    const bool vis = (i >= n - FULL_COLS);
    m_cols[i]->setVisible(vis);
    m_colLayout->setStretchFactor(m_cols[i], vis ? 1 : 0);

    if (vis && i > stripCount) {
      QFrame *sep = new QFrame(m_colContainer);
      sep->setFixedWidth(1);
      sep->setStyleSheet(
          QString("background:%1;border:none;").arg(TM().colors().separator));

      int layoutIdx = m_colLayout->indexOf(m_cols[i]);
      m_colLayout->insertWidget(layoutIdx, sep);
      m_colSeparators.append(sep);
    }
  }
}

void MillerArea::refreshDrives() {
  if (m_cols.isEmpty())
    return;
  m_cols[0]->populateDrives();
  updateVisibleColumns();
}

void MillerArea::init() {
  auto *col = new MillerColumn();
  col->populateDrives();
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);
  m_activeCol = col;

  connect(col, &MillerColumn::entryClicked, this,
          [this](const QString &path, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;
            appendColumn(path);
            emit pathChanged(path);
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);

  connect(col, &MillerColumn::teardownRequested, this,
          [this](const QString &udi) {
            // Vor dem Aushängen zu homePath navigieren, damit das Laufwerk
            // nicht belegt ist
            navigateTo(QDir::homePath());

            Solid::Device dev(udi);
            auto *acc = dev.as<Solid::StorageAccess>();
            if (!acc)
              return;
            connect(
                acc, &Solid::StorageAccess::teardownDone, this,
                [this](Solid::ErrorType, QVariant, const QString &) {
                  refreshDrives();
                },
                Qt::SingleShotConnection);
            acc->teardown();
          });
  connect(col, &MillerColumn::setupRequested, this, [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;
    connect(
        acc, &Solid::StorageAccess::setupDone, this,
        [this](Solid::ErrorType, QVariant, const QString &) {
          refreshDrives();
        },
        Qt::SingleShotConnection);
    acc->setup();
  });
  connect(col, &MillerColumn::removeFromPlacesRequested, this,
          &MillerArea::removeFromPlacesRequested);
  connect(col, &MillerColumn::openInLeft, this, &MillerArea::openInLeft);
  connect(col, &MillerColumn::openInRight, this, &MillerArea::openInRight);
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
}

void MillerArea::refresh() {
  for (auto *col : m_cols) {
    if (col->path() == "__drives__")
      col->populateDrives();
    else
      col->populateDir(col->path());
  }
}

void MillerArea::appendColumn(const QString &path) {
  auto *col = new MillerColumn();
  col->populateDir(path);
  col->setActive(true);
  m_colLayout->addWidget(col, 1);
  m_cols.append(col);

  connect(col, &MillerColumn::entryClicked, this,
          [this](const QString &p2, MillerColumn *src) {
            emit focusRequested();
            trimAfter(src);
            for (auto *c : m_cols)
              c->setActive(false);
            src->setActive(true);
            m_activeCol = src;
            // Wenn es ein Ordner ist: neue Spalte öffnen. Wenn Datei: nur
            // signalisieren.
            QUrl u2(p2);
            if (u2.scheme().isEmpty())
              u2 = QUrl::fromUserInput(p2);
            if (KFileItem(u2).isDir()) {
              appendColumn(p2);
            }
            emit pathChanged(p2);
          });
  connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
    emit focusRequested();
    for (auto *c : m_cols)
      c->setActive(false);
    src->setActive(true);
    m_activeCol = src;
  });
  connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
  connect(col, &MillerColumn::openInLeft, this, &MillerArea::openInLeft);
  connect(col, &MillerColumn::openInRight, this, &MillerArea::openInRight);
  connect(col, &MillerColumn::propertiesRequested, this,
          &MillerArea::propertiesRequested);
  updateVisibleColumns();
}

void MillerArea::trimAfter(MillerColumn *col) {
  const int idx = m_cols.indexOf(col);
  if (idx < 0)
    return;
  while (m_cols.size() > idx + 1) {
    auto *last = m_cols.takeLast();
    m_colLayout->removeWidget(last);
    last->deleteLater();

    // Zugehörigen Trenner ebenfalls entfernen
    if (!m_colSeparators.isEmpty()) {
      auto *sep = m_colSeparators.takeLast();
      m_colLayout->removeWidget(sep);
      sep->deleteLater();
    }
  }
  updateVisibleColumns();
}

void MillerArea::redistributeWidths() {}
void MillerArea::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); }

QString MillerArea::activePath() const {
  return m_activeCol ? m_activeCol->path() : QString();
}

void MillerArea::navigateTo(const QString &path, bool clearForward) {
  (void)clearForward;
  if (path.isEmpty())
    return;

  QUrl startUrl(path);
  if (startUrl.scheme().isEmpty() && !path.isEmpty())
    startUrl = QUrl::fromUserInput(path);
  if (startUrl.isLocalFile() && !QFileInfo::exists(startUrl.toLocalFile()))
    return;

  QString drivePath;
  if (!m_cols.isEmpty()) {
    QListWidget *driveList = m_cols[0]->list();
    const QString normPath =
        startUrl.isLocalFile() ? startUrl.toLocalFile() : startUrl.toString();
    for (int i = 0; i < driveList->count(); ++i) {
      QString dp = driveList->item(i)->data(Qt::UserRole).toString();
      QString normDp = dp;
      if (normDp.startsWith("file://"))
        normDp = QUrl(normDp).toLocalFile();
      if (!normDp.isEmpty() && normPath.startsWith(normDp)) {
        if (normDp.length() >
            drivePath.length()) { // Längsten Match nehmen (z.B. gdrive:/account
                                  // vs gdrive:/)
          drivePath = dp;
          driveList->setCurrentRow(i);
          m_cols[0]->setActive(true);
        }
      }
    }
    trimAfter(m_cols[0]);
  }

  QStringList segments;
  QUrl cur = startUrl;
  while (cur.isValid()) {
    const QString curStr = cur.toString();
    segments.prepend(curStr);
    if (!drivePath.isEmpty() && curStr == drivePath)
      break; // Stop bei Laufwerks-Wurzel

    QUrl up = cur.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    if (up == cur ||
        (up.path().isEmpty() && up.host().isEmpty() && up.scheme() != "file"))
      break;
    cur = up;
  }

  const QString targetDir = startUrl.toString();

  // Segmente überspringen die VOR dem drivePath liegen
  int startIdx = 0;
  if (!drivePath.isEmpty()) {
    for (int i = 0; i < segments.size(); ++i) {
      if (segments[i] == drivePath) {
        startIdx = i;
        break;
      }
    }
  }

  for (int i = startIdx + 1; i < segments.size(); ++i) {
    const QString seg = segments[i - 1];
    appendColumn(seg);
    if (!m_cols.isEmpty()) {
      MillerColumn *col = m_cols.last();
      const QString next = segments[i];
      for (int r = 0; r < col->list()->count(); ++r) {
        if (col->list()->item(r)->data(Qt::UserRole).toString() == next) {
          col->list()->setCurrentRow(r);
          break;
        }
      }
    }
  }

  if (m_cols.isEmpty() || m_cols.last()->path() != targetDir)
    appendColumn(targetDir);

  updateVisibleColumns();
}

void MillerArea::setFocused(bool f) {
  m_focused = f;
  setStyleSheet(TM().ssPane());
}

// --- PaneWidget ---
PaneWidget::PaneWidget(const QString &settingsKey, QWidget *parent)
    : QWidget(parent), m_settingsKey(settingsKey) {
  setStyleSheet(QString("background:%1;").arg(TM().colors().bgDeep));
  auto *rootLay = new QVBoxLayout(this);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  // --- Tab-Leiste mit Breadcrumb ---
  auto *tabBar = new QWidget();
  tabBar->setFixedHeight(47);
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

  auto *millerToggle = new QToolButton();
  millerToggle->setFixedSize(24, 24);
  millerToggle->setCheckable(true);
  millerToggle->setChecked(true);
  millerToggle->setIcon(QIcon::fromTheme("go-up"));
  millerToggle->setIconSize(QSize(14, 14));
  millerToggle->setToolTip(tr("Miller-Columns ein-/ausklappen"));
  millerToggle->setStyleSheet(TM().ssToolBtn());

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
  // actNewLinkUrl entfernt, da unbenutzt

  hamburgerMenu->addSeparator();

  // Toggle-Aktionen
  auto *actHidden = hamburgerMenu->addAction(QIcon::fromTheme("view-hidden"),
                                             tr("Versteckte Dateien anzeigen"));
  actHidden->setCheckable(true);
  actHidden->setChecked(SettingsDialog::showHiddenFiles());

  auto *actSingleClick = hamburgerMenu->addAction(
      QIcon::fromTheme("input-mouse"), tr("Einfachklick zum Öffnen"));
  actSingleClick->setCheckable(true);
  actSingleClick->setChecked(SettingsDialog::singleClickOpen());

  auto *actExtensions = hamburgerMenu->addAction(
      QIcon::fromTheme("text-x-generic"), tr("Dateiendungen anzeigen"));
  actExtensions->setCheckable(true);
  actExtensions->setChecked(SettingsDialog::showFileExtensions());

  auto *menuTerminal = hamburgerMenu->addMenu(
      QIcon::fromTheme("utilities-terminal"), tr("Terminal"));
  menuTerminal->setStyleSheet(TM().ssMenu());

  menuTerminal->addAction(QIcon::fromTheme("utilities-terminal"),
                          tr("Im Terminal öffnen"), this, [this]() {
                            const QString path = this->currentPath();
                            sc_openTerminal(path.isEmpty() ? QDir::homePath()
                                                           : path);
                          });

  menuTerminal->addSeparator();

  menuTerminal->addAction(
      QIcon::fromTheme("preferences-system"), tr("Terminal wählen…"), this,
      [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Terminal wählen"));
        dlg.setMinimumWidth(340);
        dlg.setStyleSheet(TM().ssDialog());

        auto *vl = new QVBoxLayout(&dlg);
        vl->setSpacing(10);
        vl->setContentsMargins(16, 16, 16, 16);

        vl->addWidget(new QLabel(tr("Installierte Terminals:")));

        const QStringList installed = sc_installedTerminals();
        const QString current = sc_detectTerminal();

        auto *grp = new QButtonGroup(&dlg);
        for (const QString &t : installed) {
          auto *btn = new QPushButton(t, &dlg);
          btn->setCheckable(true);
          btn->setChecked(t == current);
          btn->setIcon(QIcon::fromTheme("utilities-terminal"));
          grp->addButton(btn);
          vl->addWidget(btn);
        }

        vl->addWidget(new QLabel(tr("Oder eigenen Befehl eingeben:")));
        auto *customEdit = new QLineEdit(&dlg);
        customEdit->setPlaceholderText(tr("z.B. /usr/bin/kitty"));
        if (!installed.contains(current))
          customEdit->setText(current);
        vl->addWidget(customEdit);

        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(btns);

        if (dlg.exec() != QDialog::Accepted)
          return;

        QString chosen;
        if (auto *checked = grp->checkedButton())
          chosen = checked->text();
        if (!customEdit->text().trimmed().isEmpty())
          chosen = customEdit->text().trimmed();

        if (!chosen.isEmpty()) {
          QSettings s("SplitCommander", "General");
          s.setValue("terminalApp", chosen);
          s.sync();
        }
      });

  hamburgerMenu->addSeparator();

  // Theme-Untermenü
  auto *menuTheme = hamburgerMenu->addMenu(
      QIcon::fromTheme("preferences-desktop-color"), tr("Theme"));
  menuTheme->setStyleSheet(TM().ssMenu());

  // Widget-Inhalt des Untermenüs
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

  // KDE Global Checkbox
  auto *twSysCheck =
      new QCheckBox(tr("KDE Global Theme verwenden"), themeWidget);
  twSysCheck->setChecked(SettingsDialog::useSystemTheme());
  auto *twHint =
      new QLabel(tr("Übernimmt Farben und Stil des aktiven KDE Global Themes."),
                 themeWidget);
  twHint->setObjectName("hint");
  twHint->setWordWrap(true);
  twLay->addWidget(twSysCheck);
  twLay->addWidget(twHint);

  // Trennlinie
  auto *twSep = new QFrame(themeWidget);
  twSep->setFrameShape(QFrame::HLine);
  twSep->setStyleSheet(QString("background:%1;").arg(TM().colors().separator));
  twSep->setFixedHeight(1);
  twLay->addSpacing(4);
  twLay->addWidget(twSep);
  twLay->addSpacing(4);

  // Radio-Buttons für eigene Themes dynamisch beim Öffnen laden
  auto *twThemeGroup = new QButtonGroup(themeWidget);
  auto *twThemesContainer = new QWidget(themeWidget);
  auto *twThemesLay = new QVBoxLayout(twThemesContainer);
  twThemesLay->setContentsMargins(0, 0, 0, 0);
  twThemesLay->setSpacing(2);
  twLay->addWidget(twThemesContainer);

  auto refreshThemeList = [twThemesLay, twThemeGroup, twThemesContainer,
                           themeWidget]() {
    // Alte Buttons löschen
    QLayoutItem *child;
    while ((child = twThemesLay->takeAt(0)) != nullptr) {
      if (child->widget())
        child->widget()->deleteLater();
      delete child;
    }

    const QString curTheme = SettingsDialog::selectedTheme();
    const auto allThemes = TM().allThemes();
    const auto &colors = TM().colors();
    themeWidget->setStyleSheet(
        QString("QWidget{background:%1;color:%2;font-size:11px;}")
            .arg(colors.bgList, colors.textPrimary));

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

      rb->setChecked(!SettingsDialog::useSystemTheme() && t.name == curTheme);
      rb->setEnabled(!SettingsDialog::useSystemTheme());
      twThemeGroup->addButton(rb, i);
      twThemesLay->addWidget(rb);
    }
    themeWidget->adjustSize();
  };

  connect(menuTheme, &QMenu::aboutToShow, themeWidget, refreshThemeList);
  refreshThemeList();

  // "Themes neu laden" Button ganz unten
  auto *btnReload = new QPushButton(QIcon::fromTheme("view-refresh"),
                                    tr("Designs neu laden"), themeWidget);
  btnReload->setStyleSheet(
      TM().ssToolBtn() +
      "QPushButton{margin-top:8px; font-size:10px; padding:4px;}");
  twLay->addWidget(btnReload);

  connect(btnReload, &QPushButton::clicked, themeWidget, [refreshThemeList]() {
    TM().loadExternalThemes();
    refreshThemeList();
  });

  auto *btnGuide =
      new QPushButton(QIcon::fromTheme("help-about"),
                      tr("Anleitung / Vorlage öffnen"), themeWidget);
  btnGuide->setStyleSheet(TM().ssToolBtn() +
                          "QPushButton{font-size:10px; padding:4px;}");
  twLay->addWidget(btnGuide);

  connect(btnGuide, &QPushButton::clicked, themeWidget, []() {
    QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/themes/00_ANLEITUNG_THEME_ERSTELLEN.json";
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  });

  connect(twSysCheck, &QCheckBox::toggled, themeWidget,
          [twThemeGroup](bool checked) {
            for (auto *btn : twThemeGroup->buttons())
              btn->setEnabled(!checked);
          });

  // Übernehmen / Abbrechen
  twLay->addSpacing(8);
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
  twBtnLay->addWidget(twCancel);
  twBtnLay->addWidget(twApply);
  twLay->addWidget(twBtnRow);
  twLay->addSpacing(6);

  auto *twAction = new QWidgetAction(menuTheme);
  twAction->setDefaultWidget(themeWidget);
  menuTheme->addAction(twAction);

  connect(twCancel, &QPushButton::clicked, menuTheme, &QMenu::close);
  connect(twApply, &QPushButton::clicked, this,
          [this, twSysCheck, twThemeGroup, menuTheme, hamburgerMenu]() {
            QSettings s("SplitCommander", "Appearance");
            s.setValue("useSystemTheme", twSysCheck->isChecked());
            if (!twSysCheck->isChecked()) {
              int idx = twThemeGroup->checkedId();
              // Hier nutzen wir direkt die aktuelle Liste vom Manager
              const auto allThemes = TM().allThemes();
              if (idx >= 0 && idx < allThemes.size())
                s.setValue("theme", allThemes.at(idx).name);
            }
            s.sync();
            menuTheme->close();
            hamburgerMenu->close();
            DialogUtils::message(this, tr("Neustart erforderlich"),
                                 tr("Das Theme wird nach einem Neustart von "
                                    "SplitCommander vollständig angewendet."));
            QProcess::startDetached(QApplication::applicationFilePath(),
                                    QApplication::arguments());
            QApplication::quit();
          });

  // Age-Badges → öffnet SettingsDialog auf Design-Tab
  auto *actAgeBadge = hamburgerMenu->addAction(QIcon::fromTheme("chronometer"),
                                               tr("Altersbadges"));

  // Shortcuts → öffnet SettingsDialog auf Shortcuts-Tab
  auto *actShortcuts = hamburgerMenu->addAction(
      QIcon::fromTheme("configure-shortcuts"), tr("Shortcuts"));

  hamburgerMenu->addSeparator();

  // Zeilenhöhe — nur für Detailliste, wird per aboutToShow ein/ausgeblendet
  auto *rhWidget = new QWidget();
  rhWidget->setStyleSheet(
      QString("QWidget{background:%1;border:none;}").arg(TM().colors().bgList));
  auto *rhLay = new QHBoxLayout(rhWidget);
  rhLay->setContentsMargins(12, 6, 12, 6);
  rhLay->setSpacing(8);
  auto *rhLbl = new QLabel(tr("Zeilenhöhe:"));
  rhLbl->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;")
          .arg(TM().colors().textPrimary));
  auto *rhSlider = new QSlider(Qt::Horizontal);
  rhSlider->setRange(18, 52);
  rhSlider->setValue(
      QSettings().value(m_settingsKey + "/rowHeight", 26).toInt());
  rhSlider->setFixedWidth(100);
  rhSlider->setStyleSheet(
      QString("QSlider::groove:horizontal{height:4px;background:%1;border-"
              "radius:2px;}"
              "QSlider::handle:horizontal{width:12px;height:12px;margin:-4px "
              "0;border-radius:6px;background:%2;}"
              "QSlider::sub-page:horizontal{background:%2;border-radius:2px;}")
          .arg(TM().colors().borderAlt, TM().colors().accent));
  auto *rhValLbl = new QLabel(QString::number(rhSlider->value()));
  rhValLbl->setStyleSheet(
      QString("color:%1;font-size:11px;background:transparent;min-width:20px;")
          .arg(TM().colors().textAccent));
  rhLay->addWidget(rhLbl);
  rhLay->addWidget(rhSlider);
  rhLay->addWidget(rhValLbl);
  auto *rhAction = new QWidgetAction(hamburgerMenu);
  rhAction->setDefaultWidget(rhWidget);
  hamburgerMenu->addAction(rhAction);
  connect(hamburgerMenu, &QMenu::aboutToShow, this, [this, rhWidget]() {
    rhWidget->setVisible(m_filePane && m_filePane->viewMode() == 0);
  });
  connect(rhSlider, &QSlider::valueChanged, this, [this, rhValLbl](int val) {
    rhValLbl->setText(QString::number(val));
    if (m_filePane)
      m_filePane->setRowHeight(val);
  });

  hamburgerMenu->addSeparator();

  // Über
  auto *actAbout = hamburgerMenu->addAction(QIcon::fromTheme("help-about"),
                                            tr("Über SplitCommander"));

  hamburgerBtn->setMenu(hamburgerMenu);

  tabLay->addWidget(millerToggle);
  tabLay->addWidget(pathStack, 1);
  tabLay->addWidget(searchBtn);
  tabLay->addWidget(layoutBtn);
  tabLay->addWidget(hamburgerBtn);
  rootLay->addWidget(tabBar);

  // --- Layout-Button Connect ---
  connect(layoutBtn, &QToolButton::clicked, this, [this, layoutBtn]() {
    auto *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(
        TM().ssDialog() +
        "QPushButton { background:#23283a; border:1px solid #2c3245; "
        "color:#ccd4e8;"
        " border-radius:4px; padding:8px; font-size:10px; }"
        "QPushButton:hover { background:#3b4252; border-color:#5e81ac; }"
        "QPushButton:checked { background:#3b4252; border:2px solid #5e81ac; "
        "color:#88c0d0; }");

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
    QSettings s("SplitCommander", "UI");
    int current = s.value("layoutMode", 1).toInt();

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
        QSettings ss("SplitCommander", "UI");
        ss.setValue("layoutMode", entry.mode);
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
    if (ok && !name.isEmpty())
      QFile::link(target, dir + "/" + name);
  });

  connect(actHidden, &QAction::toggled, this, [this](bool checked) {
    QSettings s("SplitCommander", "General");
    s.setValue("showHidden", checked);
    s.sync();
    emit hiddenFilesToggled(checked);
  });

  connect(actSingleClick, &QAction::toggled, this, [](bool checked) {
    QSettings s("SplitCommander", "General");
    s.setValue("singleClick", checked);
    s.sync();
  });

  connect(actExtensions, &QAction::toggled, this, [this](bool checked) {
    QSettings s("SplitCommander", "General");
    s.setValue("showExtensions", checked);
    s.sync();
    emit extensionsToggled(checked);
  });

  connect(actAgeBadge, &QAction::triggered, this, [this]() {
    auto *dlg = new AgeBadgeDialog(this);
    dlg->open();
  });

  connect(actShortcuts, &QAction::triggered, this, [this]() {
    MainWindow *mw = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets())
      if ((mw = qobject_cast<MainWindow *>(w)))
        break;
    auto *dlg = new ShortcutDialog(mw ? static_cast<QWidget *>(mw)
                                      : static_cast<QWidget *>(this));
    if (mw)
      QObject::connect(dlg, &ShortcutDialog::shortcutsChanged, mw,
                       &MainWindow::registerShortcuts);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
  });

  connect(actAbout, &QAction::triggered, this, [this]() {
    const QString body =
        tr("<b>SplitCommander</b> &nbsp;<small>v%1</small><br>")
            .arg(QCoreApplication::applicationVersion()) +
        QString("<small>Ein nativer KDE-Dateimanager für den täglichen "
                "Einsatz.</small>"
                "<br><br>"
                "<b>Stack:</b> Qt6 · KF6 · C++20 · Solid · KIO<br>"
                "<b>Lizenz:</b> GPL-3.0<br>"
                "<b>Autor:</b> D. Lange<br>"
                "<br>"
                "<b>Features:</b> Miller-Columns · Dual-Pane · Tags · "
                "Batch-Rename · "
                "Vorschau · Themes · Hot-Plug USB<br>"
                "<br>"
                "<small>Build: ") +
        QString(__DATE__) + " · " + QString(__TIME__) + "</small>";
    DialogUtils::message(this, tr("Über SplitCommander"), body);
  });

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

        if (m_searchProc) {
          m_searchProc->disconnect();
          m_searchProc->kill();
          m_searchProc->deleteLater();
          m_searchProc = nullptr;
        }
        auto *proc = new QProcess(this);
        m_searchProc = proc;
        proc->setProgram("baloosearch6");
        proc->setArguments({term});
        connect(
            proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, searchResults](int, QProcess::ExitStatus) {
              if (proc != m_searchProc) {
                proc->deleteLater();
                return;
              }
              m_searchProc = nullptr;
              const QString out = proc->readAllStandardOutput().trimmed();
              proc->deleteLater();
              searchResults->clear();
              if (out.isEmpty()) {
                auto *empty = new QTreeWidgetItem(searchResults);
                empty->setText(0, QObject::tr("Keine Ergebnisse"));
                return;
              }
              QFileIconProvider ip;
              for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
                const QString path = line.trimmed();
                if (!QFileInfo::exists(path))
                  continue;
                const QFileInfo fi(path);
                auto *it = new QTreeWidgetItem(searchResults);
                it->setIcon(0, ip.icon(fi));
                it->setText(0, fi.fileName());
                it->setText(
                    1, QString("~/%1").arg(
                           QDir::home().relativeFilePath(fi.absolutePath())));
                it->setText(2, fi.lastModified().toString("dd.MM.yy"));
                it->setData(0, Qt::UserRole, path);
              }
              if (searchResults->topLevelItemCount() == 0) {
                auto *empty = new QTreeWidgetItem(searchResults);
                empty->setText(0, QObject::tr("Keine Ergebnisse"));
              }
            });
        proc->start();
      });
  connect(searchResults, &QTreeWidget::itemClicked, this,
          [this, searchBtn, searchOverlay, searchEdit](QTreeWidgetItem *it, int) {
            const QString path = it->data(0, Qt::UserRole).toString();
            if (path.isEmpty())
              return;
            searchEdit->clear();
            m_filePane->setNameFilter(QString());
            navigateTo(QFileInfo(path).isDir()
                           ? path
                           : QFileInfo(path).absolutePath());
            searchOverlay->hide();
            searchBtn->setChecked(false);
          });

  // --- Vertikaler Splitter: Miller | Dateiliste ---
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
  lowerLay->addWidget(m_toolbar);
  lowerLay->addWidget(m_filePane, 1);
  m_vSplit->addWidget(lowerWidget);
  m_vSplit->setSizes({200, 450});
  m_vSplit->setStretchFactor(0, 0);
  m_vSplit->setStretchFactor(1, 1);

  // Gespeicherte Position wiederherstellen (pane-spezifischer Key)
  {
    QSettings s("SplitCommander", "UI");
    const QString key = m_settingsKey + "/vSplitState";
    const QByteArray state = s.value(key).toByteArray();
    if (!state.isEmpty())
      m_vSplit->restoreState(state);
  }

  // Position beim Verschieben speichern — nur als Backup bei Drag
  connect(m_vSplit, &QSplitter::splitterMoved, this, [](int, int) {
    // m_millerCollapsed nicht über splitterMoved setzen — nur über Toggle
    // State wird beim App-Beenden in saveState() gespeichert
  });

  connect(millerToggle, &QToolButton::toggled, this,
          [this, millerToggle](bool checked) {
            if (checked) {
              // Aufklappen: gespeicherte Größe wiederherstellen
              QSettings s("SplitCommander", "UI");
              const QByteArray saved =
                  s.value(m_settingsKey + "/vSplitState").toByteArray();
              if (!saved.isEmpty()) {
                m_vSplit->restoreState(saved);
                if (m_vSplit->sizes().value(0) == 0)
                  m_vSplit->setSizes({200, 450});
              } else {
                m_vSplit->setSizes({200, 450});
              }
              millerToggle->setIcon(QIcon::fromTheme("go-up"));
              millerToggle->setToolTip("Miller-Columns ausklappen");
              m_millerCollapsed = false;
            } else {
              // Einklappen: aktuelle Größe vorher sichern
              if (m_vSplit->sizes().value(0) > 0) {
                QSettings s("SplitCommander", "UI");
                s.setValue(m_settingsKey + "/vSplitState",
                           m_vSplit->saveState());
                s.sync();
              }
              m_vSplit->setSizes({0, 1});
              millerToggle->setIcon(QIcon::fromTheme("go-down"));
              millerToggle->setToolTip("Miller-Columns einblenden");
              m_millerCollapsed = true;
            }
          });

  // Miller-Toggle-State beim Start setzen
  {
    QSettings s("SplitCommander", "UI");
    if (s.value(m_settingsKey + "/millerCollapsed", false).toBool()) {
      m_millerCollapsed = true;
      millerToggle->blockSignals(true);
      millerToggle->setChecked(false);
      millerToggle->setIcon(QIcon::fromTheme("go-down"));
      millerToggle->setToolTip("Miller-Columns einblenden");
      millerToggle->blockSignals(false);
      m_vSplit->setSizes({0, 1});
    }
  }

  // Verbindungen
  connect(m_miller, &MillerArea::pathChanged, this,
          [this](const QString &path) { navigateTo(path); });
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
            m_pathEdit->setText(path.isEmpty() ? currentPath() : path);
            m_pathEdit->selectAll();
            pathStack->setCurrentIndex(1);
            m_pathEdit->setFocus();
          });
  connect(m_filePane, &FilePane::fileSelected, this,
          [this](const QString &path) {
            emit focusRequested();
            updateFooter(path);
          });
  connect(m_filePane, &FilePane::fileActivated, this,
          [this](const QString &path) {
            emit focusRequested();
            // KIO-URL oder lokales Verzeichnis
            const bool isKio = !path.startsWith("/") && path.contains(":/");
            if (isKio || QFileInfo(path).isDir()) {
              navigateTo(path);
            } else {
              QDesktopServices::openUrl(QUrl::fromUserInput(path));
            }
          });

  // Toolbar-Verbindungen
  connect(m_toolbar, &PaneToolbar::newFolderClicked, this, [this]() {
    bool ok;
    QString name = DialogUtils::getText(
        this, tr("Neuer Ordner"), tr("Ordnername:"), tr("Neuer Ordner"), &ok);
    if (!ok || name.isEmpty())
      return;
    if (!QDir(currentPath()).mkdir(name))
      DialogUtils::message(this, tr("Fehler"),
                           tr("Ordner konnte nicht erstellt werden."));
  });
  connect(m_toolbar, &PaneToolbar::deleteClicked, this, [this]() {
    const QList<QUrl> urls = m_filePane->selectedUrls();
    if (urls.isEmpty())
      return;

    QString msg;
    if (urls.size() == 1) {
      msg = tr("'%1' in den Papierkorb?").arg(urls.first().fileName());
    } else {
      msg = tr("%1 Elemente in den Papierkorb?").arg(urls.size());
    }

    if (!DialogUtils::question(this, tr("Löschen"), msg))
      return;

    auto *job = new KIO::DeleteOrTrashJob(
        urls, KIO::AskUserActionInterface::Trash,
        KIO::AskUserActionInterface::DefaultConfirmation, this);
    auto *mw = qobject_cast<MainWindow *>(window());
    if (mw)
      mw->registerJob(job, tr("In den Papierkorb verschieben"));
    job->start();
  });

  connect(m_toolbar, &PaneToolbar::emptyTrashClicked, this, [this]() {
    if (!DialogUtils::question(this, tr("Papierkorb leeren"), tr("Möchten Sie den Papierkorb wirklich leeren?"))) return;
    auto *job = KIO::emptyTrash();
    job->start();
    if (job->uiDelegate()) job->uiDelegate()->setAutoErrorHandlingEnabled(true);
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

  m_miller->init();
  const QString home = QDir::homePath();
  m_filePane->setRootPath(home);
  m_pathEdit->setText(home);
  m_toolbar->setPath(home);
  buildFooter(rootLay);
}

void PaneWidget::setFocused(bool f) {
  m_focused = f;
  m_miller->setFocused(f);
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
  return m_pathEdit ? m_pathEdit->text() : QDir::homePath();
}

void PaneWidget::navigateTo(const QString &path, bool clearForward) {
  const bool isKio = !path.startsWith("/") && path.contains(":/");
  if (path.isEmpty() || path == "__drives__")
    return;
  // Lokale Pfade die nicht existieren überspringen; KIO-URLs immer durchlassen
  if (!isKio && !QFileInfo::exists(path))
    return;

  const QString cur = currentPath();
  if (!cur.isEmpty() && cur != path)
    m_histBack.push(cur);
  if (clearForward)
    m_histFwd.clear();
  m_filePane->setRootPath(path);
  QUrl url(path);
  m_pathEdit->setText(url.isLocalFile() ? url.toLocalFile() : path);
  m_toolbar->setPath(path);
  if (!m_miller->cols().isEmpty())
    m_miller->navigateTo(path);

  if (!isKio) {
    const QDir dir(path);
    int cnt = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count();
    qint64 sz = 0;
    for (const QFileInfo &fi :
         dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot))
      sz += fi.size();
    m_toolbar->setCount(cnt, sz);
  } else {
    m_toolbar->setCount(0, 0);
  }
  emit pathUpdated(path);
  updateFooter(path);
}

void PaneWidget::buildFooter(QVBoxLayout *rootLay) {
  auto *fw = new FooterWidget(this);
  fw->onHeightChanged = [this]() {
    positionFooterPanel();
    const QString p =
        m_lastPreviewPath.isEmpty() ? currentPath() : m_lastPreviewPath;
    updateFooter(p);
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

void PaneWidget::updateFooter(const QString &path) {
  if (!m_footerCount)
    return;
  const QUrl url(path);
  const bool isLocal = url.isLocalFile() || url.scheme().isEmpty();

  m_lastPreviewPath = path;

  // 1. Wenn der Pfad dem aktuellen Verzeichnis des Panes entspricht:
  // Model-Daten nutzen
  if (path == currentPath()) {
    const int count = m_filePane->view()->model()->rowCount();
    m_footerCount->setText(tr("%1 Elemente").arg(count));
    m_footerSize->setText(QString());
    if (m_footerSelected)
      m_footerSelected->hide();
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
        m_footerSelected->setText(tr("1 ausgewählt"));
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
            QFileIconProvider().icon(fi).pixmap(iconSize, iconSize));
      }
    } else {
      m_previewIcon->setPixmap(
          QFileIconProvider().icon(fi).pixmap(iconSize, iconSize));
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

    addRow("Name", fi.fileName().toHtmlEscaped());
    addRow("Typ", fi.isDir() ? tr("Ordner")
                  : fi.suffix().isEmpty()
                      ? tr("Datei")
                      : fi.suffix().toUpper() + tr("-Datei"));
    addRow("Erstellt", fi.birthTime().toString("yyyy-MM-dd  hh:mm"));
    addRow("Geändert", fi.lastModified().toString("yyyy-MM-dd  hh:mm"));

    const qint64 days = fi.lastModified().daysTo(QDateTime::currentDateTime());
    addRow("Alter", days == 0    ? tr("Heute")
                    : days == 1  ? tr("Gestern")
                    : days < 30  ? tr("%1 t").arg(days)
                    : days < 365 ? tr("%1 m").arg(days / 30)
                                 : tr("%1 j").arg(days / 365));

    if (!fi.isDir()) {
      addRow("Größe:", mw_fmtSize(fi.size()));
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
    addRow("Attribute", perm);

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

void PaneWidget::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  positionFooterPanel();
}

// --- MainWindow ---
MainWindow::~MainWindow() {}

void MainWindow::registerJob(KJob *job, const QString &title) {
  if (m_jobOverlay) {
    m_jobOverlay->addJob(job, title);
  }
}

PaneWidget *MainWindow::activePane() const {
  if (m_leftPane && m_leftPane->isFocused())
    return m_leftPane;
  return m_rightPane;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("SplitCommander");
  {
    QSettings s("SplitCommander", "UI");
    const QByteArray geo = s.value("windowGeometry").toByteArray();
    if (!geo.isEmpty())
      restoreGeometry(geo);
    else
      resize(1280, 900);
  }

  auto *central = new QWidget(this);
  setCentralWidget(central);
  auto *rootLay = new QHBoxLayout(central);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  m_sidebar = new Sidebar(this);
  m_jobOverlay = new JobOverlay(this);
  {
    QSettings s("SplitCommander", "UI");
    const int sidebarW = s.value("sidebarWidth", 250).toInt();
    const bool sidebarVis = s.value("sidebarVisible", true).toBool();
    m_sidebar->setFixedWidth(sidebarW);
    m_sidebar->setVisible(sidebarVis);
  }
  rootLay->addWidget(m_sidebar);
  auto *sidebarHandle = new SidebarHandle(m_sidebar, central);
  {
    QSettings s("SplitCommander", "UI");
    const bool sidebarVis = s.value("sidebarVisible", true).toBool();
    sidebarHandle->setFixedWidth(sidebarVis ? 10 : 32);
    if (!sidebarVis)
      sidebarHandle->setCursor(Qt::PointingHandCursor);
  }
  rootLay->addWidget(sidebarHandle);

  m_panesSplitter = new PaneSplitter(Qt::Horizontal, central);
  m_panesSplitter->setHandleWidth(12);
  m_panesSplitter->setChildrenCollapsible(true);
  m_panesSplitter->setStyleSheet(TM().ssSplitter());

  m_leftPane = new PaneWidget("leftPane");
  m_rightPane = new PaneWidget("rightPane");
  m_panesSplitter->addWidget(m_leftPane);
  m_panesSplitter->addWidget(m_rightPane);
  rootLay->addWidget(m_panesSplitter, 1);

  connect(m_leftPane, &PaneWidget::focusRequested, this, [this]() {
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
  });
  connect(m_rightPane, &PaneWidget::focusRequested, this, [this]() {
    m_rightPane->setFocused(true);
    m_leftPane->setFocused(false);
  });

  auto connectHamburger = [this](PaneWidget *pane) {
    connect(pane, &PaneWidget::newFolderRequested, this, [this, pane]() {
      bool ok;
      QString name = DialogUtils::getText(
          this, tr("Neuer Ordner"), tr("Ordnername:"), tr("Neuer Ordner"), &ok);
      if (!ok || name.isEmpty())
        return;
      if (!QDir(pane->currentPath()).mkdir(name))
        DialogUtils::message(this, tr("Fehler"),
                             tr("Ordner konnte nicht erstellt werden."));
    });
    connect(pane, &PaneWidget::hiddenFilesToggled, this, [this](bool show) {
      emit m_sidebar->hiddenFilesChanged(show);
      m_leftPane->filePane()->setShowHiddenFiles(show);
      m_rightPane->filePane()->setShowHiddenFiles(show);
      m_leftPane->miller()->refresh();
      m_rightPane->miller()->refresh();
    });
    connect(pane, &PaneWidget::extensionsToggled, this, [this](bool) {
      m_leftPane->filePane()->setRootPath(m_leftPane->currentPath());
      m_rightPane->filePane()->setRootPath(m_rightPane->currentPath());
    });
    connect(pane, &PaneWidget::openSettingsRequested, this, [this](int page) {
      auto *dlg = new SettingsDialog(this);
      dlg->setInitialPage(page);
      connect(dlg, &SettingsDialog::shortcutsChanged, this,
              [this]() { emit m_sidebar->settingsChanged(); });
      connect(dlg, &SettingsDialog::hiddenFilesChanged, this,
              [this](bool show) {
                emit m_sidebar->hiddenFilesChanged(show);
                m_leftPane->filePane()->setShowHiddenFiles(show);
                m_rightPane->filePane()->setShowHiddenFiles(show);
                m_leftPane->miller()->refresh();
                m_rightPane->miller()->refresh();
              });
      connect(dlg, &SettingsDialog::singleClickChanged, this,
              [this]() { emit m_sidebar->settingsChanged(); });
      dlg->open();
    });
  };
  connectHamburger(m_leftPane);
  connectHamburger(m_rightPane);

  auto mountAndNavigate = [this](const QString &path, bool leftPane) {
    auto navigate = [this, leftPane](const QString &p) {
      if (leftPane) {
        m_leftPane->navigateTo(p);
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
      } else {
        m_rightPane->navigateTo(p);
        m_rightPane->setFocused(true);
        m_leftPane->setFocused(false);
      }
    };
    if (path.startsWith("solid:")) {
      Solid::Device dev(path.mid(6));
      auto *acc = dev.as<Solid::StorageAccess>();
      if (!acc)
        return;
      if (acc->isAccessible()) {
        navigate(acc->filePath());
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
        m_sidebar->updateDrives();
      } else {
        connect(
            acc, &Solid::StorageAccess::setupDone, this,
            [this, navigate, acc](Solid::ErrorType, QVariant, const QString &) {
              if (acc->isAccessible()) {
                navigate(acc->filePath());
                m_leftPane->miller()->refreshDrives();
                m_rightPane->miller()->refreshDrives();
                m_sidebar->updateDrives();
              }
            },
            Qt::SingleShotConnection);
        acc->setup();
      }
    } else {
      navigate(path);
    }
  };

  connect(m_sidebar, &Sidebar::driveClicked, this,
          [this, mountAndNavigate](const QString &path) {
            mountAndNavigate(path, !m_rightPane->isFocused());
          });
  connect(m_sidebar, &Sidebar::driveClickedLeft, this,
          [mountAndNavigate](const QString &path) {
            mountAndNavigate(path, true); // immer links
          });
  connect(m_sidebar, &Sidebar::driveClickedRight, this,
          [mountAndNavigate](const QString &path) {
            mountAndNavigate(path, false); // immer rechts
          });
  // Öffnen in Linke/Rechte Ansicht – fix, unabhängig von der Quell-Pane
  for (auto *pane : {m_leftPane, m_rightPane}) {
    connect(pane, &PaneWidget::openInLeftRequested, this,
            [this](const QString &path) {
              m_leftPane->navigateTo(path);
              m_leftPane->setFocused(true);
              m_rightPane->setFocused(false);
            });
    connect(pane, &PaneWidget::openInRightRequested, this,
            [this](const QString &path) {
              m_rightPane->navigateTo(path);
              m_rightPane->setFocused(true);
              m_leftPane->setFocused(false);
            });
  }
  connect(m_sidebar, &Sidebar::addCurrentPathToPlaces, this,
          [this]() { m_sidebar->addPlace(m_leftPane->currentPath()); });
  connect(m_sidebar, &Sidebar::requestActivePath, this, [this](QString *out) {
    if (out)
      *out = m_leftPane->currentPath();
  });
  connect(m_sidebar, &Sidebar::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_leftPane, &PaneWidget::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_rightPane, &PaneWidget::layoutChangeRequested, this,
          &MainWindow::applyLayout);
  connect(m_sidebar, &Sidebar::tagClicked, this,
          [this](const QString &tagName) {
            auto *pane = activePane();
            const QString cur = pane->currentPath();
            const bool isKio = !cur.startsWith("/") && cur.contains(":/");
            if (isKio) {
              // KIO-Modus verlassen ohne History-Eintrag
              pane->filePane()->setRootPath(QDir::homePath());
            }
            pane->filePane()->showTaggedFiles(tagName);
          });
  connect(m_sidebar, &Sidebar::drivesChanged, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
  });

  // NetworkPlace aus Millers entfernen — sofort refreshen
  auto doRemoveFromPlaces = [this](const QString &url) {
    (void)url;
    m_sidebar->updateDrives();
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    emit m_sidebar->drivesChanged();
  };
  connect(m_leftPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(m_rightPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  QTimer::singleShot(2000, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
  });

  // --- Settings-Änderungen live anwenden ---
  connect(m_sidebar, &Sidebar::hiddenFilesChanged, this, [this](bool) {
    // Beide Panes neu laden — populate() liest showHidden selbst aus Settings
    m_leftPane->navigateTo(m_leftPane->currentPath());
    m_rightPane->navigateTo(m_rightPane->currentPath());
    // Miller-Columns ebenfalls aktualisieren
    for (auto *col : m_leftPane->miller()->cols())
      col->populateDir(col->path());
    for (auto *col : m_rightPane->miller()->cols())
      col->populateDir(col->path());
  });
  connect(m_sidebar, &Sidebar::settingsChanged, this, [this]() {
    // ThemeManager neu anwenden — setzt qApp->setStyleSheet und emittiert
    // themeChanged
    TM().apply();
    // Shortcuts neu registrieren falls geändert
    registerShortcuts();
  });
  connect(&TM(), &ThemeManager::themeChanged, this, [this]() {
    // Miller-Spalten neu stylen
    for (auto *col : m_leftPane->miller()->cols())
      col->refreshStyle();
    for (auto *col : m_rightPane->miller()->cols())
      col->refreshStyle();
    m_leftPane->filePane()->view()->viewport()->update();
    m_rightPane->filePane()->view()->viewport()->update();
    for (QWidget *w : QApplication::topLevelWidgets()) {
      w->style()->unpolish(w);
      w->style()->polish(w);
      w->update();
    }
  });

  // Hot-Plug
  connect(Solid::DeviceNotifier::instance(),
          &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &) {
            m_leftPane->miller()->refreshDrives();
            m_rightPane->miller()->refreshDrives();
            m_sidebar->updateDrives();
          });
  connect(Solid::DeviceNotifier::instance(),
          &Solid::DeviceNotifier::deviceRemoved, this, [this](const QString &) {
            m_leftPane->miller()->refreshDrives();
            m_rightPane->miller()->refreshDrives();
            m_sidebar->updateDrives();
          });

  connect(m_leftPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_rightPane->filePane()->setColumnVisible(colId, visible);
          });
  connect(m_rightPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_leftPane->filePane()->setColumnVisible(colId, visible);
          });

  // Delete-Taste direkt aus dem View abfangen
  auto doDelete = [this]() {
    const QList<QUrl> urls = activePane()->filePane()->selectedUrls();
    if (urls.isEmpty())
      return;
    const bool shift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
    auto *job = new KIO::DeleteOrTrashJob(
        urls,
        shift ? KIO::AskUserActionInterface::Delete
              : KIO::AskUserActionInterface::Trash,
        KIO::AskUserActionInterface::DefaultConfirmation, this);
    job->start();
  };
  connect(m_leftPane->filePane(), &FilePane::deleteRequested, this, doDelete);
  connect(m_rightPane->filePane(), &FilePane::deleteRequested, this, doDelete);

  // KIO-Einträge zu Laufwerken hinzufügen
  auto doAddToPlaces = [this](const QString &url, const QString &name) {
    QSettings s("SplitCommander", "NetworkPlaces");
    QStringList saved = s.value("places").toStringList();
    if (!saved.contains(url)) {
      saved << url;
      s.setValue("places", saved);
      s.setValue("name_" + QString(url).replace("/", "_").replace(":", "_"),
                 name);
      s.sync();
    }
    m_sidebar->updateDrives();
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    emit m_sidebar->drivesChanged();
  };
  connect(m_leftPane->filePane(), &FilePane::addToPlacesRequested, this,
          doAddToPlaces);
  connect(m_rightPane->filePane(), &FilePane::addToPlacesRequested, this,
          doAddToPlaces);

  QSettings s("SplitCommander", "UI");
  m_leftPane->navigateTo(s.value("leftPane/currentPath", QDir::homePath()).toString());
  m_rightPane->navigateTo(s.value("rightPane/currentPath", QDir::homePath()).toString());

  m_currentMode = s.value("layoutMode", 1).toInt();
  applyLayout(m_currentMode);

  connect(m_panesSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
    QSettings ss("SplitCommander", "UI");
    ss.setValue("panesSplitterState", m_panesSplitter->saveState());
    ss.sync();
  });

  m_leftPane->setFocused(true);
  m_rightPane->setFocused(false);

  QTimer::singleShot(100, this, [this]() {
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
  });

  registerShortcuts();
}

void MainWindow::registerShortcuts() {
  // Alte Shortcuts löschen
  for (auto *sc : m_shortcuts)
    sc->deleteLater();
  m_shortcuts.clear();

  // Hilfslambda: Shortcut registrieren und in m_shortcuts speichern
  auto add = [this](const QString &id, std::function<void()> fn) {
    const QString seq = ShortcutDialog::shortcut(id);
    if (seq.isEmpty())
      return;
    auto *sc = new QShortcut(QKeySequence(seq), this);
    sc->setContext(Qt::ApplicationShortcut);
    connect(sc, &QShortcut::activated, this, fn);
    m_shortcuts.append(sc);
  };

  // Navigation
  add("nav_up", [this]() {
    QDir d(activePane()->currentPath());
    if (d.cdUp())
      activePane()->navigateTo(d.absolutePath());
  });
  add("nav_home", [this]() { activePane()->navigateTo(QDir::homePath()); });
  add("nav_reload", [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_leftPane->navigateTo(m_leftPane->currentPath());
    m_rightPane->navigateTo(m_rightPane->currentPath());
  });

  // Pane-Fokus
  add("pane_focus_left", [this]() {
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);
  });
  add("pane_focus_right", [this]() {
    m_rightPane->setFocused(true);
    m_leftPane->setFocused(false);
  });

  // Panes tauschen
  add("pane_swap", [this]() {
    const QString l = m_leftPane->currentPath();
    const QString r = m_rightPane->currentPath();
    m_leftPane->navigateTo(r);
    m_rightPane->navigateTo(l);
  });

  // Pfade synchronisieren
  add("pane_sync",
      [this]() { m_rightPane->navigateTo(m_leftPane->currentPath()); });

  // Versteckte Dateien umschalten
  add("view_hidden", [this]() {
    QSettings gs("SplitCommander", "General");
    const bool cur = gs.value("showHidden", false).toBool();
    gs.setValue("showHidden", !cur);
    gs.sync();
    m_leftPane->navigateTo(m_leftPane->currentPath());
    m_rightPane->navigateTo(m_rightPane->currentPath());
    for (auto *col : m_leftPane->miller()->cols())
      col->populateDir(col->path());
    for (auto *col : m_rightPane->miller()->cols())
      col->populateDir(col->path());
  });

  // Layout wechseln
  add("view_layout", [this]() {
    int next = (m_currentMode + 1) % 3;
    QSettings gs("SplitCommander", "UI");
    gs.setValue("layoutMode", next);
    gs.sync();
    applyLayout(next);
  });

  // Navigation zurück/vorwärts
  add("nav_back", [this]() {
    auto *p = activePane();
    if (!p->histBack().isEmpty()) {
      p->histFwd().push(p->currentPath());
      p->navigateTo(p->histBack().pop(), false);
    }
  });
  add("nav_forward", [this]() {
    auto *p = activePane();
    if (!p->histFwd().isEmpty()) {
      p->histBack().push(p->currentPath());
      p->navigateTo(p->histFwd().pop(), false);
    }
  });

  // Datei löschen (Entf = Papierkorb, Shift+Entf = permanent)
  add("file_delete", [this]() {
    const QList<QUrl> urls = activePane()->filePane()->selectedUrls();
    if (urls.isEmpty())
      return;
    const bool shift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
    auto *job = new KIO::DeleteOrTrashJob(
        urls,
        shift ? KIO::AskUserActionInterface::Delete
              : KIO::AskUserActionInterface::Trash,
        KIO::AskUserActionInterface::DefaultConfirmation, this);
    job->start();
  });

  // Umbenennen
  add("file_rename", [this]() {
    const QList<QUrl> urls = activePane()->filePane()->selectedUrls();
    if (urls.size() != 1)
      return;
    const QString path = urls.first().toLocalFile();
    bool ok;
    QString newName =
        DialogUtils::getText(this, tr("Umbenennen"), tr("Neuer Name:"),
                             QFileInfo(path).fileName(), &ok);
    if (!ok || newName.isEmpty() || newName == QFileInfo(path).fileName())
      return;
    QUrl dest = QUrl::fromLocalFile(QFileInfo(path).dir().absolutePath() + "/" +
                                    newName);
    KIO::moveAs(urls.first(), dest, KIO::DefaultFlags);
  });

  // Kopieren in Zwischenablage
  add("file_copy", [this]() {
    const QList<QUrl> urls = activePane()->filePane()->selectedUrls();
    if (urls.isEmpty())
      return;
    auto *mime = new QMimeData();
    mime->setUrls(urls);
    mime->setData("x-kde-cut-selection", QByteArray("0"));
    QGuiApplication::clipboard()->setMimeData(mime);
  });

  add("file_move", [this]() {
    const QList<QUrl> urls = activePane()->filePane()->selectedUrls();
    if (urls.isEmpty())
      return;
    auto *mime = new QMimeData();
    mime->setUrls(urls);
    mime->setData("x-kde-cut-selection", QByteArray("1"));
    QGuiApplication::clipboard()->setMimeData(mime);
  });

  // Neuer Ordner
  add("file_newfolder",
      [this]() { emit activePane() -> newFolderRequested(); });
}

void PaneWidget::saveState() const {
  if (m_settingsKey.isEmpty() || !m_vSplit)
    return;
  QSettings s("SplitCommander", "UI");
  // Größe speichern — aber nur wenn nicht gerade eingeklappt
  if (!m_millerCollapsed)
    s.setValue(m_settingsKey + "/vSplitState", m_vSplit->saveState());
  s.setValue(m_settingsKey + "/millerCollapsed", m_millerCollapsed);
  s.setValue(m_settingsKey + "/currentPath", currentPath());
  s.sync();
}

void MainWindow::saveWindowState() {
  QSettings s("SplitCommander", "UI");

  // Fenster-Geometrie
  s.setValue("windowGeometry", saveGeometry());

  // Sidebar
  s.setValue("sidebarVisible", m_sidebar->isVisible());
  s.setValue("sidebarWidth", m_sidebar->width());

  // Pane-Splitter (links/rechts bzw. oben/unten)
  s.setValue("panesSplitterState", m_panesSplitter->saveState());

  // Beide Panes: Miller-Größe und collapsed-State
  m_leftPane->saveState();
  m_rightPane->saveState();

  s.sync();
}

void MainWindow::closeEvent(QCloseEvent *e) {
  saveWindowState();
  QMainWindow::closeEvent(e);
}

void MainWindow::applyLayout(int mode) {
  m_currentMode = mode;
  QSettings s("SplitCommander", "UI");
  s.setValue("layoutMode", mode);

  const int total =
      m_panesSplitter->width() > 10 ? m_panesSplitter->width() : 1000;

  switch (mode) {
  case 0: // Klassisch
    m_panesSplitter->setOrientation(Qt::Horizontal);
    m_rightPane->hide();
    m_leftPane->show();
    m_panesSplitter->setSizes({total, 0});
    break;
  case 1: // Standard Dual
    m_panesSplitter->setOrientation(Qt::Horizontal);
    m_leftPane->show();
    m_rightPane->show();
    m_panesSplitter->setSizes({total / 2, total / 2});
    break;
  case 2: // Spalten Dual
    m_panesSplitter->setOrientation(Qt::Vertical);
    m_leftPane->show();
    m_rightPane->show();
    {
      const int h =
          m_panesSplitter->height() > 10 ? m_panesSplitter->height() : 800;
      m_panesSplitter->setSizes({h / 2, h / 2});
    }
    break;
  }

  // Gespeicherten State wiederherstellen — nur beim initialen Aufruf
  if (!m_panesSplitterRestored) {
    m_panesSplitterRestored = true;
    const QByteArray paneState = s.value("panesSplitterState").toByteArray();
    if (!paneState.isEmpty())
      m_panesSplitter->restoreState(paneState);
  }
}
