// --- mainwindow.cpp — SplitCommander Hauptfenster ---

#include "mainwindow.h"
#include "agebadgedialog.h"
#include "config.h"
#include "filepane.h"
#include "joboverlay.h"
#include "panewidgets.h"
#include "terminalutils.h"
#include "thememanager.h"
#include <KActionCollection>
#include <KShortcutsDialog>
#include <KStandardShortcut>

#include "drophandler.h"
#include <KFileItem>
#include <KFormat>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>
#include <KPropertiesDialog>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include "dialogutils.h"
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>

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
#include <QFileSystemWatcher>
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

static QString mw_normalizePath(QString s) {
  if (s.length() > 1 && s.endsWith('/'))
    s.chop(1);
  return s;
}

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
  m_emptyTrashBtn = mk("trash-empty", tr("Papierkorb leeren"),
                       &PaneToolbar::emptyTrashClicked);
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

  m_viewGroup = new QButtonGroup(this);
  m_viewGroup->setExclusive(true);
  connect(m_viewGroup, &QButtonGroup::idClicked, this,
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
    m_viewGroup->addButton(b, modeId++);
    r3->addWidget(b);
  }
  vlay->addLayout(r3);
}

void PaneToolbar::setViewMode(int mode) {
  if (!m_viewGroup)
    return;
  auto *b = m_viewGroup->button(mode);
  if (b) {
    b->blockSignals(true);
    b->setChecked(true);
    b->blockSignals(false);
  }
}

void PaneToolbar::setPath(const QString &path) {
  if (!m_pathLabel)
    return;

  QString name;
  if (path == "__drives__" || path == "remote:/") {
    name = tr("Dieser PC");
  } else {
    QUrl url = QUrl::fromUserInput(path);
    name = url.fileName();
    if (name.isEmpty()) {
      name = url.isLocalFile() ? url.toLocalFile() : path;
    }
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
    KFormat format;
    QString s = format.formatByteSize(totalBytes);
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

  // Lister für normale Verzeichnisse
  m_lister = new KDirLister(this);
  connect(m_lister, &KDirLister::newItems, this,
          [this](const KFileItemList &items) {
            if (m_path == "__drives__") {
              return;
            }
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
          .arg(TM().colors().bgBox, TM().colors().separator,
               TM().colors().textAccent, TM().colors().bgHover,
               TM().colors().textPrimary));
  auto *hlay = new QHBoxLayout();
  hlay->setContentsMargins(0, 0, 0, 0);
  hlay->setSpacing(0);
  hlay->addWidget(m_header, 1);
  hlay->addStretch(1);
  lay->addLayout(hlay);
  connect(m_header, &QPushButton::clicked, this, [this]() {
    emit headerClicked(m_path == "__drives__" ? QString() : m_path);
  });

  m_list = new QListWidget();
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setUniformItemSizes(true);
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
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
  m_list->viewport()->installEventFilter(
      new DropHandler(m_list, resolver, m_list));

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

  connect(m_list, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            emit activated(this);
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
        openInMenu->addAction(tr("Linke Ansicht"), this, [this, itemPath]() {
          emit openInLeft(itemPath);
        });
        openInMenu->addAction(tr("Rechte Ansicht"), this, [this, itemPath]() {
          emit openInRight(itemPath);
        });
        menu.addSeparator();
      }

      {
        auto netCheck = Config::group("NetworkPlaces");
        const QStringList netPlaces =
            netCheck.readEntry("places", QStringList());
        const QString npath = mw_normalizePath(itemPath);
        bool alreadyIn = false;
        for (const QString &p : netPlaces) {
          if (mw_normalizePath(p) == npath) {
            alreadyIn = true;
            break;
          }
        }

        if (alreadyIn) {
          menu.addAction(
              QIcon::fromTheme("edit-rename"), tr("Umbenennen"), this,
              [this, itemPath, it]() {
                bool ok;
                const QString newName =
                    DialogUtils::getText(this, tr("Umbenennen"),
                                         tr("Anzeigename:"), it->text(), &ok);
                if (!ok || newName.trimmed().isEmpty())
                  return;

                auto s = Config::group("NetworkPlaces");

                s.writeEntry(
                    "name_" +
                        QString(itemPath).replace("/", "_").replace(":", "_"),
                    newName.trimmed());
                s.config()->sync();
                populateDrives(); // Neuladen der Spalte
              });
          menu.addAction(
              QIcon::fromTheme("list-remove"), tr("Aus Laufwerken entfernen"),
              this, [this, itemPath]() {
                auto s = Config::group("NetworkPlaces");
                QStringList saved = s.readEntry("places", QStringList());
                const QString npath = mw_normalizePath(itemPath);
                saved.removeAll(npath);
                QString otherVersion = npath.endsWith('/')
                                           ? npath.left(npath.length() - 1)
                                           : npath + "/";
                if (otherVersion != "/")
                  saved.removeAll(otherVersion);

                s.writeEntry("places", saved);
                s.deleteEntry(
                    "name_" +
                    QString(npath).replace("/", "_").replace(":", "_"));
                s.deleteEntry(
                    "name_" +
                    QString(otherVersion).replace("/", "_").replace(":", "_"));
                s.config()->sync();
                emit removeFromPlacesRequested(itemPath);
              });
        }
        menu.addSeparator();
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
            menu.addAction(QIcon::fromTheme("drive-harddisk"), tr("Einhängen"),
                           this, [this, udi]() { emit setupRequested(udi); });
          menu.addSeparator();
        }
      }

      menu.addAction(
          QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"), this,
          [itemPath]() { QGuiApplication::clipboard()->setText(itemPath); });
    } else {
      // --- Ordner-Menü ---
      mw_applyMenuShadow(&menu);
      menu.addAction(QIcon::fromTheme("folder-open"), tr("Öffnen"), this,
                     [this, itemPath]() { emit entryClicked(itemPath, this); });

      auto *openInMenu =
          menu.addMenu(QIcon::fromTheme("folder-open"), tr("Öffnen in"));
      openInMenu->setStyleSheet(TM().ssMenu());
      openInMenu->addAction(tr("Linke Ansicht"), this,
                            [this, itemPath]() { emit openInLeft(itemPath); });
      openInMenu->addAction(tr("Rechte Ansicht"), this,
                            [this, itemPath]() { emit openInRight(itemPath); });

      menu.addSeparator();
      menu.addAction(QIcon::fromTheme("utilities-terminal"),
                     tr("Im Terminal öffnen"), this,
                     [itemPath]() { sc_openTerminal(itemPath); });
      menu.addSeparator();
      menu.addAction(
          QIcon::fromTheme("edit-copy"), tr("Pfad kopieren"), this,
          [itemPath]() { QGuiApplication::clipboard()->setText(itemPath); });
      menu.addSeparator();
      menu.addAction(
          QIcon::fromTheme("document-properties"), tr("Eigenschaften"), this,
          [this, itemPath]() { emit propertiesRequested(itemPath); });
    }

    menu.exec(m_list->mapToGlobal(pos));
      });
}

void MillerColumn::populateDrives() {
  m_path = "__drives__";
  m_lister->stop();
  m_header->setText(tr("Dieser PC"));
  m_header->setIcon(QIcon::fromTheme("computer"));
  m_list->clear();
  m_list->setStyleSheet(TM().ssColDrives());
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->verticalScrollBar()->hide();

  // --- Eingehängte Laufwerke ---
  QSet<QString> shownUdis;
  QSet<QString> shownPaths;
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
    if (mounted)
      shownPaths.insert(mw_normalizePath(p));

    QString driveName =
        mounted && (p == "/") ? sc_rootVolumeName() : dev.description();
    if (driveName.isEmpty())
      driveName = mounted ? QDir(p).dirName() : dev.udi().section('/', -1);
    if (driveName.isEmpty())
      continue;

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

  // --- Nicht gemountete Block-Geräte ---
  const auto vdevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
  for (const Solid::Device &device : vdevices) {
    // Bereits in Schleife 1 gezeigt — überspringen
    if (shownUdis.contains(device.udi())) continue;

    const auto *vol = device.as<Solid::StorageVolume>();
    if (!vol) continue;
    if (vol->usage() != Solid::StorageVolume::FileSystem
        && vol->usage() != Solid::StorageVolume::Other) continue;

    const QString label  = vol->label();
    const QString lbl    = label.toUpper();
    const QString fsType = vol->fsType().toLower();
    // Entfernt: if (label.isEmpty()) continue;
    if (lbl == "BOOT" || lbl == "EFI" || lbl == "EFI SYSTEM PARTITION" || lbl == "ESP") continue;
    if (fsType == "iso9660" || fsType == "udf") continue;

    const auto *access = device.as<Solid::StorageAccess>();
    if (access && access->isAccessible()) continue;

    QString name = label;
    if (name.isEmpty()) name = device.udi().section('/', -1);
    if (name.isEmpty()) continue;  // Fallback, falls UDI leer

    QString iconName = "drive-harddisk";
    if (const auto *drv = device.as<Solid::StorageDrive>()) {
      if (drv->driveType() == Solid::StorageDrive::CdromDrive) iconName = "drive-optical";
      else if (drv->isRemovable() || drv->isHotpluggable())    iconName = "drive-removable-media";
    } else if (!device.icon().isEmpty()) {
      iconName = device.icon();
    }

    auto *it = new QListWidgetItem(m_list);
    it->setData(Qt::DisplayRole, name);
    it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
    it->setData(Qt::UserRole, QString(QStringLiteral("solid:") + device.udi()));
    it->setData(Qt::UserRole + 1, device.udi());
    it->setSizeHint(QSize(0, 44));
    it->setForeground(QColor(TM().colors().textMuted));
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
    if (fs == "fuse.portal" || fs == "fusectl")
      continue;
    const QString path = storage.rootPath();
    const QString normalizedPath = mw_normalizePath(path);
    if (shownPaths.contains(normalizedPath))
      continue;
    shownPaths.insert(normalizedPath);
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
    auto netSettings = Config::group("NetworkPlaces");
    const QStringList savedPlaces =
        netSettings.readEntry("places", QStringList());
    for (const QString &p : savedPlaces) {
      if (p.isEmpty())
        continue;
      const QString normalizedP = mw_normalizePath(p);
      if (shownPaths.contains(normalizedP))
        continue;
      shownPaths.insert(normalizedP);
      const QUrl pUrl(p);
      const QString scheme = pUrl.scheme().toLower();
      const QString savedName = netSettings.readEntry(
          "name_" + QString(p).replace("/", "_").replace(":", "_"),
          scheme == "gdrive" ? "Google Drive" : pUrl.fileName());

      if (savedName.isEmpty())
        continue;

      QString iconName = scheme == "gdrive"      ? "folder-gdrive"
                         : scheme == "smb"       ? "network-workgroup"
                         : scheme == "sftp"      ? "network-connect"
                         : scheme == "mtp"       ? "multimedia-player"
                         : scheme == "bluetooth" ? "bluetooth"
                                                 : "network-server";
      auto *it = new QListWidgetItem(m_list);
      it->setData(Qt::DisplayRole, savedName);
      it->setData(Qt::DecorationRole, QIcon::fromTheme(iconName));
      QString url = pUrl.toString();
      if ((url.startsWith("gdrive:") || url.startsWith("mtp:")) &&
          !url.endsWith("/"))
        url += "/";
      it->setData(Qt::UserRole, url);
      it->setData(Qt::UserRole + 1, scheme);
      it->setSizeHint(QSize(0, 44));
    }
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
    s->hide();
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
      QString targetPath = m_cols[i]->path();
      while (m_cols.size() > i + 1) {
        trimAfter(m_cols[i]);
      }
      updateVisibleColumns();
      emit headerClicked(targetPath);
    });
  }

  // Alte Trenner entfernen und neu aufbauen
  for (auto *sep : m_colSeparators) {
    sep->hide();
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
  // Nur den Inhalt der ersten Spalte aktualisieren, ohne die Navigation zu
  // unterbrechen
  m_cols[0]->populateDrives();
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
            // Bei Laufwerken immer als Ordner behandeln
            QString p = path;
            if ((p.startsWith("gdrive:") || p.startsWith("mtp:")) &&
                !p.endsWith("/"))
              p += "/";
            appendColumn(p);
            emit pathChanged(p);
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
          &MillerArea::teardownRequested);
  connect(col, &MillerColumn::setupRequested, this, [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;
    connect(
        acc, &Solid::StorageAccess::setupDone, this,
        [this](Solid::ErrorType, QVariant, const QString &) {
          refreshDrives();
          emit drivesChanged(); // Sidebar aktualisieren
        },
        Qt::SingleShotConnection);
    acc->setup();
  });
  connect(col, &MillerColumn::removeFromPlacesRequested, this,
          &MillerArea::removeFromPlacesRequested);
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
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
            const QString sch2 = u2.scheme();
            const bool isKioDir =
                (sch2 == "gdrive" || sch2 == "mtp" || sch2 == "smb" ||
                 sch2 == "sftp" || sch2 == "ftp" || sch2 == "remote");
            if (isKioDir || KFileItem(u2).isDir()) {
              QString nav = p2;
              if (isKioDir && !nav.endsWith("/"))
                nav += "/";
              appendColumn(nav);
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
  connect(col, &MillerColumn::openInLeft, this,
          [this](const QString &p) { emit openInLeft(p); });
  connect(col, &MillerColumn::openInRight, this,
          [this](const QString &p) { emit openInRight(p); });
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
    last->hide();
    m_colLayout->removeWidget(last);
    last->deleteLater();

    // Zugehörigen Trenner ebenfalls entfernen
    if (!m_colSeparators.isEmpty()) {
      auto *sep = m_colSeparators.takeLast();
      sep->hide();
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

QList<QUrl> MillerArea::selectedUrls() const {
  return {}; // Löschen in Miller-Spalten deaktiviert
}

void MillerArea::navigateTo(const QString &path, bool clearForward) {
  (void)clearForward;
  if (path.isEmpty())
    return;

  if (path == "__drives__") {
    if (!m_cols.isEmpty()) {
      trimAfter(m_cols[0]);
      m_cols[0]->populateDrives();
      m_cols[0]->list()->clearSelection();
      m_cols[0]->setActive(true);
      m_activeCol = m_cols[0];
    }
    return;
  }

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
    const QString targetNorm = mw_normalizePath(normPath);
    for (int i = 0; i < driveList->count(); ++i) {
      QString dp = driveList->item(i)->data(Qt::UserRole).toString();
      QString normDp = dp;
      if (normDp.startsWith("file://"))
        normDp = QUrl(normDp).toLocalFile();
      const QString dpNorm = mw_normalizePath(normDp);

      if (!dpNorm.isEmpty() &&
          (targetNorm == dpNorm || targetNorm.startsWith(dpNorm + "/"))) {
        if (dpNorm.length() >= mw_normalizePath(drivePath).length()) {
          drivePath = dp;
          driveList->setCurrentRow(i);
          m_cols[0]->setActive(true);
        }
      }
    }
    // Fallback für KIO-Protokolle falls Discovery noch nicht fertig
    if (drivePath.isEmpty()) {
      if (startUrl.scheme() == "gdrive")
        drivePath = "gdrive:/";
      else if (startUrl.scheme() == "mtp")
        drivePath = "mtp:/";
      else if (startUrl.scheme() == "remote")
        drivePath = "remote:/";
    }
    trimAfter(m_cols[0]);
  }

  QStringList segments;
  QUrl cur = startUrl;
  while (cur.isValid()) {
    const QString curStr = cur.toString();
    segments.prepend(curStr);
    if (!drivePath.isEmpty() &&
        mw_normalizePath(curStr) == mw_normalizePath(drivePath))
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
      if (mw_normalizePath(segments[i]) == mw_normalizePath(drivePath)) {
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
                            sc_openTerminal(path.isEmpty() ? QDir::homePath()
                                                           : path);
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

  // 3. Terminal wählen … (Submenu)
  auto *menuTerminalSelect = menuConfigure->addMenu(
      QIcon::fromTheme("utilities-terminal"), tr("Terminal wählen…"));
  menuTerminalSelect->setStyleSheet(TM().ssMenu());
  connect(menuTerminalSelect, &QMenu::aboutToShow, this,
          [this, menuTerminalSelect]() {
            menuTerminalSelect->clear();
            const QStringList installed = sc_installedTerminals();
            const QString current = sc_detectTerminal();
            for (const QString &t : installed) {
              auto *act = menuTerminalSelect->addAction(t);
              act->setCheckable(true);
              act->setChecked(t == current);
              connect(act, &QAction::triggered, this,
                      [t]() { Config::setTerminalApp(t); });
            }
          });

  // 4. Altersbadges
  auto *actAgeBadge = menuConfigure->addAction(QIcon::fromTheme("chronometer"),
                                               tr("Altersbadges"));
  connect(actAgeBadge, &QAction::triggered, this,
          [this]() { (new AgeBadgeDialog(this))->open(); });

  hamburgerMenu->addSeparator();

  // Über SplitCommander
  auto *actAbout = hamburgerMenu->addAction(QIcon::fromTheme("help-about"),
                                            tr("Über SplitCommander"));
  connect(actAbout, &QAction::triggered, this, [this]() {
    const QString body =
        tr("<b>SplitCommander</b> &nbsp;<small>v%1</small><br>")
            .arg(QCoreApplication::applicationVersion()) +
        tr("<small>Ein nativer KDE-Dateimanager.</small><br><br><b>Stack:</b> "
           "Qt6 · KF6 · C++20<br><b>Autor:</b> D. Lange");
    DialogUtils::message(this, tr("Über SplitCommander"), body);
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
    if (ok && !name.isEmpty())
      QFile::link(target, dir + "/" + name);
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

void MainWindow::doDelete(PaneWidget *pane, bool permanent) {
  if (!pane)
    pane = activePane();
  const QList<QUrl> urls = pane->filePane()->selectedUrls();
  if (urls.isEmpty())
    return;

  // Exakt wie Dolphin: DefaultConfirmation + AutoWarningHandlingEnabled
  auto *job = new KIO::DeleteOrTrashJob(
      urls,
      permanent ? KIO::AskUserActionInterface::Delete
                : KIO::AskUserActionInterface::Trash,
      KIO::AskUserActionInterface::DefaultConfirmation,
      this);
  job->setUiDelegate(KIO::createDefaultJobUiDelegate(
      KJobUiDelegate::AutoWarningHandlingEnabled, this));
  registerJob(job, tr("Löschen..."));
  job->start();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("SplitCommander");
  {
    auto s = Config::group("UI");
    const QByteArray geo = s.readEntry("windowGeometry", QByteArray());
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
    auto s = Config::group("UI");
    const int sidebarW = s.readEntry("sidebarWidth", 250);
    const bool sidebarVis = s.readEntry("sidebarVisible", true);
    m_sidebar->setFixedWidth(sidebarW);
    m_sidebar->setVisible(sidebarVis);
  }

  rootLay->addWidget(m_sidebar);
  auto *sidebarHandle = new SidebarHandle(m_sidebar, central);
  {
    auto s = Config::group("UI");
    const bool sidebarVis = s.readEntry("sidebarVisible", true);
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
    connect(pane, &PaneWidget::extensionsToggled, this, [this](bool on) {
      Config::setShowFileExtensions(on);
      m_leftPane->filePane()->setRootPath(m_leftPane->currentPath());
      m_rightPane->filePane()->setRootPath(m_rightPane->currentPath());
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

    if (path == "remote:/") {
      PaneWidget *pane = leftPane ? m_leftPane : m_rightPane;
      pane->setViewMode(0);                // Details
      pane->navigateTo(path, true, false); // false = don't update miller
      pane->setFocused(true);
      if (leftPane)
        m_rightPane->setFocused(false);
      else
        m_leftPane->setFocused(false);
      return;
    }

    if (path.startsWith("solid:")) {
      Solid::Device dev(path.mid(6));
      auto *acc = dev.as<Solid::StorageAccess>();
      if (!acc)
        return;
      if (acc->isAccessible()) {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
        m_sidebar->updateDrives();
      } else {
        connect(
            acc, &Solid::StorageAccess::setupDone, this,
            [this, acc](Solid::ErrorType, QVariant, const QString &) {
              if (acc->isAccessible()) {
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
    if (!url.isEmpty()) {
      const QString normUrl = mw_normalizePath(url);
      for (auto *pane : {m_leftPane, m_rightPane}) {
        const QString current = mw_normalizePath(pane->currentPath());
        if (current == normUrl || current.startsWith(normUrl + "/")) {
          pane->navigateTo("__drives__");
        }
      }
    }
    m_sidebar->updateDrives();
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    emit m_sidebar->drivesChanged();
  };
  connect(m_leftPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(m_rightPane->miller(), &MillerArea::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(m_sidebar, &Sidebar::removeFromPlacesRequested, this,
          doRemoveFromPlaces);
  connect(
      m_sidebar, &Sidebar::unmountRequested, this, [this](const QString &path) {
        const QString normPath = mw_normalizePath(path);

        // Pfade VOR dem Aushängen erfassen
        const bool leftPaneOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_leftPane->currentPath()) == normPath ||
            mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + "/"));
        const bool rightPaneOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_rightPane->currentPath()) == normPath ||
            mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + "/"));
        const bool leftMillerOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_leftPane->miller()->activePath()) == normPath ||
            mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + "/"));
        const bool rightMillerOnMount = !normPath.isEmpty() && (
            mw_normalizePath(m_rightPane->miller()->activePath()) == normPath ||
            mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + "/"));

        auto *proc = new QProcess(this);
        connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, leftPaneOnMount, rightPaneOnMount,
                       leftMillerOnMount, rightMillerOnMount](int exitCode, QProcess::ExitStatus) {
                  if (exitCode == 0) {
                    if (leftPaneOnMount)  m_leftPane->navigateTo("__drives__");
                    if (rightPaneOnMount) m_rightPane->navigateTo("__drives__");
                    if (leftMillerOnMount)
                      m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
                    else
                      m_leftPane->miller()->refreshDrives();
                    if (rightMillerOnMount)
                      m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));
                    else
                      m_rightPane->miller()->refreshDrives();
                  }
                  m_sidebar->updateDrives();
                  proc->deleteLater();
                });
        proc->start("umount", {path});
      });

  auto doTeardown = [this](const QString &udi) {
    Solid::Device dev(udi);
    auto *acc = dev.as<Solid::StorageAccess>();
    if (!acc)
      return;

    const QString mountPoint = acc->filePath();
    const QString normPath = mw_normalizePath(mountPoint);

    // Pfade VOR dem Aushängen erfassen
    const bool leftPaneOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_leftPane->currentPath()) == normPath ||
        mw_normalizePath(m_leftPane->currentPath()).startsWith(normPath + "/"));
    const bool rightPaneOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_rightPane->currentPath()) == normPath ||
        mw_normalizePath(m_rightPane->currentPath()).startsWith(normPath + "/"));
    const bool leftMillerOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_leftPane->miller()->activePath()) == normPath ||
        mw_normalizePath(m_leftPane->miller()->activePath()).startsWith(normPath + "/"));
    const bool rightMillerOnMount = !normPath.isEmpty() && (
        mw_normalizePath(m_rightPane->miller()->activePath()) == normPath ||
        mw_normalizePath(m_rightPane->miller()->activePath()).startsWith(normPath + "/"));

    connect(
        acc, &Solid::StorageAccess::teardownDone, this,
        [this, leftPaneOnMount, rightPaneOnMount,
         leftMillerOnMount, rightMillerOnMount](Solid::ErrorType, QVariant, const QString &) {
          if (leftPaneOnMount)  m_leftPane->navigateTo(QDir::homePath());
          if (rightPaneOnMount) m_rightPane->navigateTo(QDir::homePath());
          if (leftMillerOnMount)
            m_leftPane->miller()->navigateTo(QStringLiteral("__drives__"));
          else
            m_leftPane->miller()->refreshDrives();
          if (rightMillerOnMount)
            m_rightPane->miller()->navigateTo(QStringLiteral("__drives__"));
          else
            m_rightPane->miller()->refreshDrives();
          m_sidebar->updateDrives();
        },
        Qt::SingleShotConnection);
    acc->teardown();
  };
  connect(m_leftPane->miller(), &MillerArea::teardownRequested, this,
          doTeardown);
  connect(m_rightPane->miller(), &MillerArea::teardownRequested, this,
          doTeardown);
  // Sidebar Aushängen ebenfalls über doTeardown koordinieren
  connect(m_sidebar, &Sidebar::teardownRequested, this, doTeardown);

  // Miller Ein-/Aushängen -> Sidebar synchronisieren
  connect(m_leftPane->miller(), &MillerArea::drivesChanged,
          m_sidebar, &Sidebar::updateDrives);
  connect(m_rightPane->miller(), &MillerArea::drivesChanged,
          m_sidebar, &Sidebar::updateDrives);
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

    m_leftPane->setFocused(activePane() == m_leftPane);
    m_rightPane->setFocused(activePane() == m_rightPane);

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

  // FileSystem Watcher für Mount-Punkte (zusätzlich zu Solid)
  m_fsWatcher = new QFileSystemWatcher(this);
  QString userMediaPath = QDir::homePath() + "/.local/share/Trash"; // Fallback
  if (QDir("/run/media").exists()) {
    QString userPath = "/run/media/" + QString::fromLocal8Bit(qgetenv("USER"));
    if (QDir(userPath).exists()) {
      m_fsWatcher->addPath(userPath);
    } else {
      m_fsWatcher->addPath("/run/media");
    }
  }
  if (QDir("/media").exists()) {
    m_fsWatcher->addPath("/media");
  }
  connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });
  connect(m_fsWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });

  // Fallback: Timer für regelmäßige Aktualisierung
  QTimer *driveTimer = new QTimer(this);
  connect(driveTimer, &QTimer::timeout, this, [this]() {
    m_leftPane->miller()->refreshDrives();
    m_rightPane->miller()->refreshDrives();
    m_sidebar->updateDrives();
  });
  driveTimer->start(5000); // Alle 5 Sekunden

  connect(m_leftPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_rightPane->filePane()->setColumnVisible(colId, visible);
          });
  connect(m_rightPane->filePane(), &FilePane::columnsChanged, this,
          [this](int colId, bool visible) {
            m_leftPane->filePane()->setColumnVisible(colId, visible);
          });

  // Delete-Taste direkt aus dem View abfangen
  connect(m_leftPane->filePane(), &FilePane::deleteRequested, this,
          [this](bool perm) { doDelete(m_leftPane, perm); });
  connect(m_rightPane->filePane(), &FilePane::deleteRequested, this,
          [this](bool perm) { doDelete(m_rightPane, perm); });

  // KIO-Einträge zu Laufwerken hinzufügen
  auto doAddToPlaces = [this](const QString &url, const QString &name) {
    auto s = Config::group("NetworkPlaces");
    QStringList saved = s.readEntry("places", QStringList());
    if (!saved.contains(url)) {
      saved << url;
      s.writeEntry("places", saved);
      s.writeEntry("name_" + QString(url).replace("/", "_").replace(":", "_"),
                   name);
      s.config()->sync();
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

  auto s = Config::group("UI");
  const QString mode = s.readEntry("startupPathMode", "last");

  QString leftPath, rightPath;
  const int behavior = Config::startupBehavior();

  if (behavior == 1) { // Letzte Sitzung
    leftPath = Config::lastLeftPath();
    rightPath = Config::lastRightPath();
    // Validierung lokaler Pfade
    if (leftPath.startsWith("/") && !QFileInfo::exists(leftPath))
      leftPath = QDir::homePath();
    if (rightPath.startsWith("/") && !QFileInfo::exists(rightPath))
      rightPath = QDir::homePath();
  } else if (behavior == 2) { // Dieser PC (Root)
    leftPath = "__drives__";
    rightPath = "__drives__";
  } else { // Home (0)
    leftPath = QDir::homePath();
    rightPath = QDir::homePath();
  }

  // Sonderwunsch: Miller oben bei Laufwerken, Liste unten bei Home
  m_leftPane->navigateTo("__drives__");
  m_rightPane->navigateTo("__drives__");

  // Jetzt die Liste unten auf Home setzen, aber OHNE Miller zu aktualisieren
  m_leftPane->navigateTo(QDir::homePath(), true, false);
  m_rightPane->navigateTo(QDir::homePath(), true, false);

  auto sUI = Config::group("UI");
  m_currentMode = sUI.readEntry("layoutMode", 1);
  applyLayout(m_currentMode);

  connect(m_panesSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
    auto ss = Config::group("UI");
    ss.writeEntry("panesSplitterState", m_panesSplitter->saveState());
    ss.config()->sync();
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
  // Beim ersten Aufruf: KActionCollection anlegen
  if (!m_actionCollection) {
    m_actionCollection =
        new KActionCollection(this, QStringLiteral("splitcommander"));
    m_actionCollection->setComponentDisplayName(tr("SplitCommander"));

    // --- Hilfsmakro: Aktion anlegen ---
    // Qt::ApplicationShortcut: greift immer, egal welches Widget den Fokus hat
    auto addAct = [this](const QString &id, const QString &label,
                         const QString &icon, const QKeySequence &defKey,
                         std::function<void()> fn,
                         const QKeySequence &altKey = {}) -> QAction * {
      auto *a = m_actionCollection->addAction(id);
      a->setText(label);
      if (!icon.isEmpty())
        a->setIcon(QIcon::fromTheme(icon));
      a->setShortcutContext(Qt::ApplicationShortcut);
      if (altKey.isEmpty()) {
        m_actionCollection->setDefaultShortcut(a, defKey);
      } else {
        m_actionCollection->setDefaultShortcuts(a, {defKey, altKey});
      }
      connect(a, &QAction::triggered, this, fn);
      return a;
    };

    // Navigation
    addAct("nav_back", tr("Zurück"), "go-previous", Qt::ALT | Qt::Key_Left,
           [this]() {
             auto *p = activePane();
             if (!p->histBack().isEmpty()) {
               p->histFwd().push(p->currentPath());
               p->navigateTo(p->histBack().pop(), false);
             }
           });
    addAct("nav_forward", tr("Vorwärts"), "go-next", Qt::ALT | Qt::Key_Right,
           [this]() {
             auto *p = activePane();
             if (!p->histFwd().isEmpty()) {
               p->histBack().push(p->currentPath());
               p->navigateTo(p->histFwd().pop(), false);
             }
           });
    addAct("nav_up", tr("Übergeordneter Ordner"), "go-up", Qt::ALT | Qt::Key_Up,
           [this]() {
             QDir d(activePane()->currentPath());
             if (d.cdUp())
               activePane()->navigateTo(d.absolutePath());
           });
    addAct("nav_home", tr("Home-Verzeichnis"), "go-home",
           Qt::ALT | Qt::Key_Home,
           [this]() { activePane()->navigateTo(QDir::homePath()); });
    addAct(
        "nav_reload", tr("Neu laden"), "view-refresh", Qt::CTRL | Qt::Key_R,
        [this]() {
          m_leftPane->miller()->refreshDrives();
          m_rightPane->miller()->refreshDrives();
          m_leftPane->navigateTo(m_leftPane->currentPath());
          m_rightPane->navigateTo(m_rightPane->currentPath());
        },
        Qt::Key_F5);

    // Pane-Fokus
    addAct("pane_focus_left", tr("Linke Pane fokussieren"), "go-first",
           Qt::CTRL | Qt::Key_Left, [this]() {
             m_leftPane->setFocused(true);
             m_rightPane->setFocused(false);
           });
    addAct("pane_focus_right", tr("Rechte Pane fokussieren"), "go-last",
           Qt::CTRL | Qt::Key_Right, [this]() {
             m_rightPane->setFocused(true);
             m_leftPane->setFocused(false);
           });
    addAct("pane_swap", tr("Panes tauschen"), "view-split-left-right",
           Qt::CTRL | Qt::Key_U, [this]() {
             const QString l = m_leftPane->currentPath();
             const QString r = m_rightPane->currentPath();
             m_leftPane->navigateTo(r);
             m_rightPane->navigateTo(l);
           });
    addAct("pane_sync", tr("Pfade synchronisieren"), "view-refresh",
           Qt::CTRL | Qt::SHIFT | Qt::Key_S,
           [this]() { m_rightPane->navigateTo(m_leftPane->currentPath()); });

    // Datei
    addAct(
        "file_rename", tr("Umbenennen"), "edit-rename", Qt::Key_F2, [this]() {
          const QList<QUrl> urls = activePane()->selectedUrls();
          if (urls.size() != 1)
            return;
          const QString path = urls.first().toLocalFile();
          bool ok;
          QString newName =
              DialogUtils::getText(this, tr("Umbenennen"), tr("Neuer Name:"),
                                   QFileInfo(path).fileName(), &ok);
          if (!ok || newName.isEmpty() || newName == QFileInfo(path).fileName())
            return;
          QUrl dest = QUrl::fromLocalFile(QFileInfo(path).dir().absolutePath() +
                                          "/" + newName);
          KIO::moveAs(urls.first(), dest, KIO::DefaultFlags);
        });
    // Wie Dolphin: zwei separate Actions für Trash und permanentes Löschen
    addAct("file_trash", tr("In den Papierkorb verschieben"), "user-trash",
           Qt::Key_Delete, [this]() {
             QWidget* fw = focusWidget();
             bool inMiller = false;
             while(fw) {
                 if (qobject_cast<MillerArea*>(fw)) { inMiller = true; break; }
                 fw = fw->parentWidget();
             }
             if (!inMiller)
                 doDelete(activePane(), false);
           });
    addAct("file_delete", tr("Löschen"), "edit-delete",
           QKeySequence(Qt::SHIFT | Qt::Key_Delete), [this]() {
             QWidget* fw = focusWidget();
             bool inMiller = false;
             while(fw) {
                 if (qobject_cast<MillerArea*>(fw)) { inMiller = true; break; }
                 fw = fw->parentWidget();
             }
             if (!inMiller)
                 doDelete(activePane(), true);
           });
    addAct("file_newfolder", tr("Neuer Ordner"), "folder-new", Qt::Key_F7,
           [this]() { emit activePane() -> newFolderRequested(); });
    addAct("file_copy", tr("Kopieren (Zwischenablage)"), "edit-copy",
           KStandardShortcut::copy().first(), [this]() {
             const QList<QUrl> urls = activePane()->selectedUrls();
             if (urls.isEmpty())
               return;
             auto *mime = new QMimeData();
             mime->setUrls(urls);
             mime->setData("x-kde-cut-selection", QByteArray("0"));
             QGuiApplication::clipboard()->setMimeData(mime);
           });
    addAct("file_move", tr("Ausschneiden (Zwischenablage)"), "edit-cut",
           KStandardShortcut::cut().first(), [this]() {
             const QList<QUrl> urls = activePane()->selectedUrls();
             if (urls.isEmpty())
               return;
             auto *mime = new QMimeData();
             mime->setUrls(urls);
             mime->setData("x-kde-cut-selection", QByteArray("1"));
             QGuiApplication::clipboard()->setMimeData(mime);
           });

    // Ansicht
    addAct("view_hidden", tr("Versteckte Dateien umschalten"), "view-hidden",
           Qt::CTRL | Qt::Key_H, [this]() {
             const bool cur = Config::showHiddenFiles();
             Config::setShowHiddenFiles(!cur);
             m_leftPane->navigateTo(m_leftPane->currentPath());
             m_rightPane->navigateTo(m_rightPane->currentPath());
             for (auto *col : m_leftPane->miller()->cols())
               col->populateDir(col->path());
             for (auto *col : m_rightPane->miller()->cols())
               col->populateDir(col->path());
           });

    addAct("view_layout", tr("Layout wechseln"), "view-choose",
           Qt::CTRL | Qt::Key_L, [this]() {
             int next = (m_currentMode + 1) % 3;
             auto gs = Config::group("UI");
             gs.writeEntry("layoutMode", next);
             gs.config()->sync();
             applyLayout(next);
           });

    // Einfügen
    addAct("file_paste", tr("Einfügen"), "edit-paste",
           KStandardShortcut::paste().first(), [this]() {
             const QMimeData *clip = QGuiApplication::clipboard()->mimeData();
             if (!clip || !clip->hasUrls())
               return;
             const bool isCut = clip->data("x-kde-cut-selection") == "1";
             const QList<QUrl> urls = clip->urls();
             const QUrl destUrl =
                 QUrl::fromLocalFile(activePane()->currentPath());
             if (isCut) {
               auto *job = KIO::move(urls, destUrl, KIO::DefaultFlags);
               job->uiDelegate()->setAutoErrorHandlingEnabled(true);
               registerJob(job, tr("Verschiebe Dateien..."));
               QGuiApplication::clipboard()->clear();
             } else {
               auto *job = KIO::copy(urls, destUrl, KIO::DefaultFlags);
               job->uiDelegate()->setAutoErrorHandlingEnabled(true);
               registerJob(job, tr("Kopiere Dateien..."));
             }
           });

    // Alles auswählen
    addAct("file_selectall", tr("Alles auswählen"), "edit-select-all",
           KStandardShortcut::selectAll().first(),
           [this]() { activePane()->filePane()->view()->selectAll(); });

    // Shortcuts aus KConfig laden (persistiert KShortcutsDialog-Änderungen)
    m_actionCollection->readSettings();

    // Alle Aktionen dem MainWindow zuweisen, damit sie feuern
    for (QAction *a : m_actionCollection->actions())
      addAction(a);
  } else {
    // Bereits initialisiert: nur Settings neu einlesen (z.B. nach
    // KShortcutsDialog)
    m_actionCollection->readSettings();
  }
}

void PaneWidget::setMillerVisible(bool visible) {
  if (m_millerToggle) {
    m_millerToggle->setChecked(visible);
  }
}

void PaneWidget::setViewMode(int mode) {
  m_toolbar->setViewMode(mode);
  m_filePane->setViewMode(mode);
}


void PaneWidget::saveState() const {
  if (m_settingsKey.isEmpty() || !m_vSplit)
    return;
  auto s = Config::group("UI");
  // Größe speichern — aber nur wenn nicht gerade eingeklappt
  if (!m_millerCollapsed)
    s.writeEntry(m_settingsKey + "/vSplitState", m_vSplit->saveState());
  s.writeEntry(m_settingsKey + "/millerCollapsed", m_millerCollapsed);
  s.writeEntry(m_settingsKey + "/currentPath", currentPath());
  s.config()->sync();
}

void MainWindow::saveWindowState() {
  auto s = Config::group("UI");

  // Fenster-Geometrie
  s.writeEntry("windowGeometry", saveGeometry());

  // Sidebar
  s.writeEntry("sidebarVisible", m_sidebar->isVisible());
  s.writeEntry("sidebarWidth", m_sidebar->width());

  // Pane-Splitter (links/rechts bzw. oben/unten)
  s.writeEntry("panesSplitterState", m_panesSplitter->saveState());

  // Beide Panes: Miller-Größe und collapsed-State
  m_leftPane->saveState();
  m_rightPane->saveState();

  s.config()->sync();
}

void MainWindow::closeEvent(QCloseEvent *e) {
  saveWindowState();

  // Session speichern falls aktiviert
  if (Config::startupBehavior() == 1) {
    Config::setLastPaths(m_leftPane->currentPath(), m_rightPane->currentPath());
  }

  QMainWindow::closeEvent(e);
}

void MainWindow::applyLayout(int mode) {
  m_currentMode = mode;
  auto s = Config::group("UI");
  s.writeEntry("layoutMode", mode);
  s.config()->sync();

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
    const QByteArray paneState =
        s.readEntry("panesSplitterState", QByteArray());
    if (!paneState.isEmpty())
      m_panesSplitter->restoreState(paneState);
  }
}
// EOF - Force IDE Refresh
