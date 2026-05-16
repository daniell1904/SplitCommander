#include "millercolumn.h"
#include "drivemanager.h"
#include "hoverfader.h"

#include <QPropertyAnimation>
#include <QEasingCurve>
#include "config.h"
#include "dialogutils.h"
#include "drophandler.h"
#include "panewidgets.h"
#include "thememanager.h"
#include "scglobal.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <KDirLister>
#include <KFileItem>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>
#include <QScrollBar>

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
            for (const KFileItem &item : items) {
              auto *it = new MillerWidgetItem(
                  QIcon::fromTheme(item.iconName(),
                                   QIcon::fromTheme(QStringLiteral("folder"))),
                  item.name(), item.isDir());
              it->setData(Qt::UserRole, item.url().toString());
              it->setData(Qt::UserRole + 3, item.isDir());
              m_list->addItem(it);
            }
          });
  connect(m_lister, &KDirLister::completed, this,
          [this]() { m_list->sortItems(); });

  m_header = new QPushButton();
  m_header->setObjectName("MillerHeader");
  m_header->setFlat(true);
  m_header->setFixedHeight(Config::millerHeaderHeight());
  m_header->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
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
    if (m_path == "__drives__" || m_path == "/" || m_path.isEmpty()) {
      emit headerClicked("__drives__");
    } else {
      emit headerClicked(m_path);
    }
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

  auto* millerDel = new MillerItemDelegate(m_list);
  millerDel->setHoverFader(new HoverFader(m_list, millerDel));
  m_list->setItemDelegate(millerDel);

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
                DriveManager::instance()->refreshAll(); // Neuladen der Spalte
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
                     [this, itemPath]() {
                         auto *job = new KTerminalLauncherJob(QString());
                         job->setWorkingDirectory(itemPath);
                         job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
                         job->start();
                     });
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
  m_list->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->verticalScrollBar()->hide();

  auto* dm = DriveManager::instance();

  // Local Drives
  for (const auto &info : dm->localDrives()) {
      auto *it = new QListWidgetItem(m_list);
      it->setData(Qt::DisplayRole, info.name);
      it->setData(Qt::DecorationRole, QIcon::fromTheme(info.iconName));
      it->setData(Qt::UserRole, info.path);
      it->setData(Qt::UserRole + 1, info.udi);
      it->setSizeHint(QSize(0, Config::millerDriveRowHeight()));
      it->setData(Qt::UserRole + 3, true); 
      it->setData(Qt::UserRole + 12, true); 

      it->setData(Qt::UserRole + 10, info.total);
      it->setData(Qt::UserRole + 11, info.free);

      if (!info.isMounted) {
          it->setForeground(QColor(TM().colors().textMuted));
      }
  }

  // Network Drives
  for (const auto &info : dm->networkDrives()) {
      auto *it = new QListWidgetItem(m_list);
      it->setData(Qt::DisplayRole, info.name);
      it->setData(Qt::DecorationRole, QIcon::fromTheme(info.iconName));
      it->setData(Qt::UserRole, info.path);
      it->setData(Qt::UserRole + 1, info.scheme);
      it->setSizeHint(QSize(0, Config::millerDriveRowHeight()));
      it->setData(Qt::UserRole + 3, true);
      it->setData(Qt::UserRole + 12, true);

      it->setData(Qt::UserRole + 10, info.total);
      it->setData(Qt::UserRole + 11, info.free);
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
  else if (url.scheme() == "gdrive" && url.fileName().isEmpty())
    name = tr("Google Drive");
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
  m_list->setIconSize(QSize(Config::millerIconSize(), Config::millerIconSize()));
  m_list->setItemDelegate(new MillerItemDelegate(m_list));

  if (path != "__drives__") {
    m_lister->stop();
    m_lister->openUrl(url, KDirLister::NoFlags);
  }
}

void MillerColumn::refreshStyle() {
  setActive(m_list->styleSheet() != TM().ssColInactive());
  const int sz = Config::millerIconSize();
  m_list->setIconSize(QSize(sz, sz));
  m_header->setIconSize(QSize(sz, sz));
  m_list->viewport()->update();
}

void MillerColumn::setActive(bool active) {
  m_list->setStyleSheet(
      m_path == "__drives__"
          ? TM().ssColDrives()
          : (active ? TM().ssColActive() : TM().ssColInactive()));
}

