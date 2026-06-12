// --- filepane_menus.cpp -----------------------------------------------------
// Kontextmenüs und Header-Menü für FilePane.
// ---------------------------------------------------------------------------

#include "filepane.h"

#ifdef SC_PLUGIN_PAPERLESS
#include "plugins/paperless/paperlessmanager.h"
#endif
#ifdef SC_PLUGIN_MOUNTISO
#include "plugins/mountiso/mountiso.h"
#endif
#ifdef SC_PLUGIN_MAKEFILEACTIONS
#include "plugins/makefileactions/makefileactions.h"
#endif

#include <QApplication>
#include <KActionCollection>
#include <KFormat>
#include <QPointer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include "config.h"
#include "tagmanager.h"
#include <KJob>

#include <QAction>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>

#include <QFileDialog>
#include <QKeyEvent>
#include <QScrollBar>

#include <QStandardPaths>

#include <QDir>
#include <QResizeEvent>
#include <QTimer>

#include <QClipboard>
#include <KIO/OpenUrlJob>
#include <KDesktopFile>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QToolTip>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>

#include <KTerminalLauncherJob>
#include <KDialogJobUiDelegate>
#include <KApplicationTrader>
#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFileUtils>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/EmptyTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/Job>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/PasteJob>
#include <KIO/RenameDialog>
#include <KIO/RestoreJob>
#include <KJobWidgets>
#include <KNewFileMenu>
#include <KPropertiesDialog>
#include <KService>

#include "filepane_helpers.h"
#include "scremoveaction.h"

// Öffnet Terminal im angegebenen Verzeichnis, immer in einem neuen Fenster.
static void openTerminalHere(const QString &dir, QWidget *parent)
{
    static const QList<QPair<QString,QStringList>> candidates = {
        {"konsole",  {"--new-window", "--workdir"}},
        {"gnome-terminal", {"--working-directory"}},
        {"xfce4-terminal", {"--working-directory"}},
        {"tilix",    {"--working-directory"}},
        {"alacritty",{"--working-directory"}},
        {"kitty",    {"--directory"}},
        {"foot",     {"--working-directory"}},
    };
    for (const auto &c : candidates) {
        const QString bin = QStandardPaths::findExecutable(c.first);
        if (!bin.isEmpty()) {
            QStringList args = c.second;
            args << dir;
            QProcess::startDetached(bin, args);
            return;
        }
    }
    auto *job = new KTerminalLauncherJob(QString(), parent);
    job->setWorkingDirectory(dir);
    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, parent));
    job->start();
}



void FilePane::showHeaderMenu(const QPoint &pos) {
  QMenu menu;
  menu.setStyleSheet("QMenu::item{padding:5px 20px 5px 8px;}"
                     "QMenu::indicator{width:14px;height:14px;}");
  fp_applyMenuShadow(&menu);

  QMap<QString, QMenu *> subMenus;

  for (const auto &d : colDefs()) {
    QMenu *target = &menu;
    if (!d.group.isEmpty()) {
      if (!subMenus.contains(d.group)) {
        auto *sub = menu.addMenu(d.group);
        sub->setStyleSheet(menu.styleSheet());
        subMenus[d.group] = sub;
      }
      target = subMenus[d.group];
    }
    auto *act = target->addAction(d.label);
    act->setCheckable(true);
    act->setChecked(m_colVisible[d.id]);
    connect(act, &QAction::toggled, this,
            [this, id = d.id](bool v) { setColumnVisible(id, v); });
  }
  menu.exec(m_view->header()->mapToGlobal(pos));
}

// --- showContextMenu — volles Menü mit KFileItemActions + KNewFileMenu ---

FilePane::ContextMenuState FilePane::buildContextMenuState(const QPoint &pos) {
  ContextMenuState ctx;
  ctx.view = qobject_cast<QAbstractItemView *>(sender());
  if (!ctx.view)
    ctx.view = m_view;

  const QModelIndex proxyIndex = ctx.view->indexAt(pos);
  ctx.hasItem = proxyIndex.isValid();

  if (ctx.hasItem) {
    if (!ctx.view->selectionModel()->isSelected(proxyIndex)) {
      ctx.view->selectionModel()->select(proxyIndex,
                                         QItemSelectionModel::ClearAndSelect |
                                             QItemSelectionModel::Rows);
    }
    ctx.view->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::NoUpdate);
  } else {
    ctx.view->selectionModel()->clearSelection();
  }

  {

    const QModelIndexList sel = ctx.view->selectionModel()->selectedIndexes();
    QSet<int> seenRows;
    for (const auto &idx : sel) {
      if (idx.column() != 0) continue;
      if (seenRows.contains(idx.row())) continue;
      seenRows.insert(idx.row());
      KFileItem it = m_proxy->fileItem(idx);
      if (!it.isNull())
        ctx.selectedItems << it;
    }
  }

  if (ctx.hasItem) {
    ctx.item = m_proxy->fileItem(proxyIndex);
    if (ctx.item.isNull()) {
      ctx.hasItem = false;
    } else {
      ctx.path = ctx.item.localPath().isEmpty() ? ctx.item.url().toString() : ctx.item.localPath();
      ctx.itemUrl = ctx.item.url();
    }
  }

  ctx.isKioPath = m_kioMode || (!m_currentPath.startsWith("/") &&
                                 m_currentPath.contains(QStringLiteral(":/")));
  ctx.dirUrl = ctx.isKioPath
      ? (m_currentUrl.isValid() ? m_currentUrl : QUrl(m_currentPath))
      : QUrl::fromLocalFile(m_currentPath.isEmpty()
                                ? QFileInfo(ctx.path).absolutePath()
                                : m_currentPath);
  ctx.isTrash = ctx.dirUrl.scheme() == QStringLiteral("trash");

  ctx.items = ctx.hasItem ? ctx.selectedItems : KFileItemList{KFileItem(ctx.dirUrl)};
  if (ctx.hasItem && ctx.items.isEmpty()) ctx.items << ctx.item;

  return ctx;
}

void FilePane::applyMenuStyling(QMenu &menu) {
  auto applyStyle = [](QMenu *m, auto &self) -> void {
    if (!m) return;
    m->setStyleSheet(fp_menuStyle());
    fp_applyMenuShadow(m);
    auto styleChildren = [m, &self]() {
      for (auto *sub : m->findChildren<QMenu *>())
        self(sub, self);
      for (auto *act : m->actions()) {
        if (auto *sub = act->menu())
          self(sub, self);
      }
    };
    QObject::connect(m, &QMenu::aboutToShow, m, styleChildren);
    styleChildren();
  };
  applyStyle(&menu, applyStyle);
}

void FilePane::showContextMenu(const QPoint &pos) {
  emit focusRequested();

  ContextMenuState ctx = buildContextMenuState(pos);

  QString oldCwd = QDir::currentPath();
  if (!ctx.isKioPath && !m_currentPath.isEmpty())
    QDir::setCurrent(m_currentPath);

  QMenu menu(this);
  fp_applyMenuShadow(&menu);
  menu.setStyleSheet(fp_menuStyle());

  KFileItemActions actions(&menu);
  KFileItemListProperties props(ctx.items);
  actions.setItemListProperties(props);
  actions.setParentWidget(this);

  if (ctx.isTrash)
    populateTrashMenu(menu, ctx, actions, props);
  else if (ctx.hasItem)
    populateItemMenu(menu, ctx, actions, props);
  else
    populateBackgroundMenu(menu, ctx, actions, props);

  const QUrl propsUrl = ctx.hasItem ? ctx.itemUrl : ctx.dirUrl;
  menu.addSeparator();
  menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                 tr("Eigenschaften"), this, [propsUrl]() {
                   auto *dlg = new KPropertiesDialog(propsUrl, nullptr);
                   dlg->setAttribute(Qt::WA_DeleteOnClose);
                   dlg->show();
                 });

  applyMenuStyling(menu);

  menu.exec(ctx.view->viewport()->mapToGlobal(pos));
  QDir::setCurrent(oldCwd);
}


void FilePane::populateTrashMenu(QMenu &menu, const ContextMenuState &ctx,
                                 KFileItemActions &actions, KFileItemListProperties &props) {
  Q_UNUSED(actions); Q_UNUSED(props);
  const bool hasItem        = ctx.hasItem;
  const KFileItemList &items = ctx.items;
  QAbstractItemView *view   = ctx.view;
    if (hasItem) {
      // --- PAPIERKORB ITEM (Bild 1) ---
      menu.addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
                     tr("An ursprünglichem Ort wiederherstellen"), this,
                     [items]() {
                       QList<QUrl> urls;
                       urls.reserve(items.count());
                       // Wie Dolphin: item.url() für restoreFromTrash
                       for (const KFileItem &it : items)
                         urls << it.url();
                       if (urls.isEmpty()) return;
                       auto *job = KIO::restoreFromTrash(urls);
                       job->uiDelegate()->setAutoErrorHandlingEnabled(true);
                     });
      menu.addSeparator();

      if (m_actionCollection) {
        if (auto *a = m_actionCollection->action(QStringLiteral("file_move")))
          menu.addAction(a);
        if (auto *a = m_actionCollection->action(QStringLiteral("file_copy")))
          menu.addAction(a);
      }
      menu.addSeparator();

      if (m_actionCollection) {
        if (auto *a = m_actionCollection->action(QStringLiteral("file_delete"))) {
          a->setText(tr("Löschen"));
          a->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
          a->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
          menu.addAction(a);
        }
      }
    } else {
      // --- PAPIERKORB HINTERGRUND (Bild 2) ---
      auto *sortMenu = menu.addMenu(
          QIcon::fromTheme(QStringLiteral("view-sort")), tr("Sortieren nach"));
      if (auto *tree = qobject_cast<QTreeView *>(view)) {
        auto *hdr = tree->header();
        for (int i = 0; i < hdr->count(); ++i) {
          if (hdr->isSectionHidden(i))
            continue;
          QString label =
              tree->model()->headerData(i, Qt::Horizontal).toString();
          QAction *a = sortMenu->addAction(label);
          a->setCheckable(true);
          a->setChecked(hdr->sortIndicatorSection() == i);
          connect(a, &QAction::triggered, this,
                  [i, tree]() { tree->sortByColumn(i, Qt::AscendingOrder); });
        }
      }

      auto *modeMenu =
          menu.addMenu(QIcon::fromTheme(QStringLiteral("view-mode")),
                       tr("Ansichtsmodus ändern"));
      auto addMode = [&](const QString &label, const QString &icon, int m) {
        QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
        a->setCheckable(true);
        a->setChecked(m_viewMode == m);
        connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
      };
      addMode(tr("Details"), "view-list-details", 0);
      addMode(tr("Symbole"), "view-list-icons", 1);
      menu.addSeparator();

      menu.addAction(QIcon::fromTheme(QStringLiteral("trash-empty")),
                     tr("Papierkorb leeren"), this, []() {
                       auto *job = KIO::emptyTrash();
                       job->uiDelegate()->setAutoErrorHandlingEnabled(true);
                     });
    }
}

void FilePane::populateItemMenu(QMenu &menu, const ContextMenuState &ctx,
                                KFileItemActions &actions, KFileItemListProperties &props) {
  Q_UNUSED(props);
  const QUrl &dirUrl         = ctx.dirUrl;
  const bool isKioPath       = ctx.isKioPath;
  QAbstractItemView *view    = ctx.view;
    // --- 1. NEU / ÖFFNEN-BLOCK (Viewport) ---
    if (m_newFileMenu && dirUrl.scheme() != "trash") {
      m_newFileMenu->setWorkingDirectory(dirUrl);
      m_newFileMenu->checkUpToDate();
      auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-new")),
                                   tr("Neu erstellen"));
      for (QAction *act : m_newFileMenu->menu()->actions())
        newMenu->addAction(act);
    }
    actions.insertOpenWithActionsTo(
        nullptr, &menu, QStringList{QCoreApplication::applicationName()});
    menu.addSeparator();

    // --- 2. BEARBEITEN / EINFÜGEN ---
    if (m_actionCollection) {
      if (auto *a = m_actionCollection->action(QStringLiteral("file_move")))
        menu.addAction(a);
      if (auto *a = m_actionCollection->action(QStringLiteral("file_copy")))
        menu.addAction(a);
      if (auto *a = m_actionCollection->action(QStringLiteral("file_paste")))
        menu.addAction(a);
      menu.addSeparator();
      if (auto *a = m_actionCollection->action(QStringLiteral("file_rename")))
        menu.addAction(a);

      auto *trashDeleteAction = new SCRemoveAction(m_actionCollection, &menu);
      menu.addAction(trashDeleteAction);
    }

#ifdef SC_PLUGIN_PAPERLESS
    if (ctx.hasItem) {
        menu.addSeparator();
        QStringList selectedPaths;
        for (const auto &item : ctx.items)
            if (item.url().isLocalFile())
                selectedPaths << item.url().toLocalFile();
        if (!selectedPaths.isEmpty()) {
            auto *plAct = menu.addAction(
                QIcon::fromTheme("document-send"),
                tr("Zu Paperless hochladen"));
            connect(plAct, &QAction::triggered, this, [selectedPaths, this]() {
                PaperlessManagerDialog::uploadFiles(selectedPaths, this);
            });
        }
    }
#endif

#ifdef SC_PLUGIN_MOUNTISO
    if (ctx.hasItem && ctx.items.size() == 1 && ctx.items.first().url().isLocalFile()) {
        const QString isoPath = ctx.items.first().url().toLocalFile();
        if (MountIso::isMountable(isoPath)) {
            menu.addSeparator();
            if (MountIso::isMounted(isoPath)) {
                auto *act = menu.addAction(
                    QIcon::fromTheme("media-eject"), tr("ISO aushängen"));
                connect(act, &QAction::triggered, this, [isoPath, this]() {
                    MountIso::unmount(isoPath, this);
                });
            } else {
                auto *act = menu.addAction(
                    QIcon::fromTheme("media-mount"), tr("ISO einbinden"));
                connect(act, &QAction::triggered, this, [isoPath, this]() {
                    MountIso::mount(isoPath, this);
                });
            }
        }
    }
#endif
    // Zu Laufwerken hinzufügen (Dolphin: add_to_places)
    if (isKioPath) {
      // Immer m_currentUrl verwenden wenn KIO-Modus — enthält User/Auth-Info
      // z.B. smb://root@192.168.0.152/Daten statt smb://192.168.0.152/Daten
      QUrl resolvedUrl = (ctx.hasItem && ctx.itemUrl.isValid())
                             ? ctx.itemUrl
                             : (m_kioMode && m_currentUrl.isValid()
                                    ? m_currentUrl
                                    : QUrl::fromUserInput(m_currentPath));

      // Sicherstellen dass User-Info erhalten bleibt
      if (!m_currentUrl.userInfo().isEmpty() && resolvedUrl.userInfo().isEmpty()
          && resolvedUrl.host() == m_currentUrl.host()) {
        resolvedUrl.setUserInfo(m_currentUrl.userInfo());
      }

      const QString placeUrl = resolvedUrl.toString();
      const QUrl placeQUrl(placeUrl);
      auto netCheck = Config::group("NetworkPlaces");
      if (!netCheck.readEntry("places", QStringList()).contains(placeUrl)) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                       tr("Zu Laufwerken hinzufügen"), this, [this, placeUrl, placeQUrl]() {
                         const QString scheme = placeQUrl.scheme().toLower();
                         QString name;
                         if (scheme == QStringLiteral("gdrive"))
                           name = placeQUrl.path().section('/', 1, 1); // "google18"
                         if (name.isEmpty() && !placeQUrl.fileName().isEmpty())
                           name = placeQUrl.fileName(); // "Daten" für smb://.../Daten
                         if (name.isEmpty() && !placeQUrl.host().isEmpty())
                           name = placeQUrl.host();
                         if (name.isEmpty())
                           name = placeUrl;
                         emit addToPlacesRequested(placeUrl, name);
                       });
      }
    }
    menu.addSeparator();

    // --- 3. ANSICHT-BLOCK ---
    auto *sortMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-sort-ascending")),
                                  tr("Sortieren nach"));

    // Festes Sort-Set wie Dolphin
    struct SortEntry { QString label; int col; QString icon; };
    const QList<SortEntry> sortEntries = {
        { tr("Name"),            0, "sort-name"             },
        { tr("Größe"),           2, "sort-size"             },
        { tr("Geändert"),        3, "sort-time"             },
        { tr("Erstellt"),        3, ""                      },
        { tr("Letzter Zugriff"), 3, ""                      },
        { tr("Typ"),             1, "sort-file-type"        },
        { tr("Bewertung"),      -1, "rating"                },
    };
    auto *tree = qobject_cast<QTreeView *>(view);
    for (const SortEntry &e : sortEntries) {
        if (e.col < 0) continue; // Baloo-Felder — nur wenn vorhanden
        QAction *a = sortMenu->addAction(e.icon.isEmpty()
            ? QIcon() : QIcon::fromTheme(e.icon), e.label);
        a->setCheckable(true);
        if (tree) a->setChecked(tree->header()->sortIndicatorSection() == e.col
                                && e.label == tr("Name") ? tree->header()->sortIndicatorSection() == 0
                                : false);
        const int col = e.col;
        connect(a, &QAction::triggered, this, [col, tree]() {
            if (tree) tree->sortByColumn(col, Qt::AscendingOrder);
        });
    }
    sortMenu->addSeparator();
    // A-Z / Z-A
    auto *azAct = sortMenu->addAction(tr("A-Z"));
    azAct->setCheckable(true);
    auto *zaAct = sortMenu->addAction(tr("Z-A"));
    zaAct->setCheckable(true);
    if (tree) {
        azAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::AscendingOrder);
        zaAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::DescendingOrder);
    }
    connect(azAct, &QAction::triggered, this, [tree]() {
        if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::AscendingOrder);
    });
    connect(zaAct, &QAction::triggered, this, [tree]() {
        if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::DescendingOrder);
    });
    sortMenu->addSeparator();
    // Ordner zuerst
    auto *folderFirstAct = sortMenu->addAction(tr("Ordner zuerst"));
    folderFirstAct->setCheckable(true);
    folderFirstAct->setChecked(m_foldersFirst);
    connect(folderFirstAct, &QAction::triggered, this, [this](bool checked) {
        setFoldersFirst(checked);
    });
    // Versteckte Dateien zuletzt
    auto *hiddenLastAct = sortMenu->addAction(tr("Versteckte Dateien zuletzt"));
    hiddenLastAct->setCheckable(true);
    connect(hiddenLastAct, &QAction::triggered, this, [](bool) {});

    auto *modeMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-list-details")),
                                  tr("Ansichtsmodus ändern"));
    auto addMode = [&](const QString &label, const QString &icon, int m) {
      QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
      a->setCheckable(true);
      a->setChecked(m_viewMode == m);
      connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
    };
    addMode(tr("Details"), "view-list-details", 0);
    addMode(tr("Symbole"), "view-list-icons", 1);

    // --- 4. KIO-AKTIONEN (Aktionen, Stichwörter, Komprimieren, Aktivitäten) ---
    menu.addSeparator();

    // Aktionen in temporäres Menü sammeln
    QMenu tempMenu;
    actions.addActionsTo(&tempMenu, KFileItemActions::MenuActionSource::All, {});

    // Bereits vorhandene Einträge nach Text sammeln, um Duplikate zu vermeiden
    QSet<QString> existingActionTexts;
    for (QAction *act : menu.actions()) {
      if (!act->isSeparator() && !act->text().isEmpty())
        existingActionTexts.insert(act->text().trimmed());
    }

    // Submenüs nach Titel aus tempMenu holen
    auto findSubMenu = [&](const QString &title) -> QAction* {
        for (QAction *act : tempMenu.actions()) {
            if (!act->isSeparator() && act->text().contains(title, Qt::CaseInsensitive))
                return act;
        }
        return nullptr;
    };

    // Aktionen-Submenü bauen — Terminal + restliche Einträge
    auto *actMenu = new QMenu(tr("Aktionen"), &menu);
    actMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));


    // Terminal hier öffnen als ersten Eintrag
    auto *termAct = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                                tr("Terminal hier öffnen"), actMenu);
    termAct->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F4));
    connect(termAct, &QAction::triggered, this, [this, dirUrl]() {
        openTerminalHere(dirUrl.toLocalFile(), this);
    });
    actMenu->addAction(termAct);

    // Restliche KIO-Aktionen die nicht manuell im Menü hinzugefügt werden sollen
    static const QStringList skipTitles = {
        tr("Stichwörter zuweisen"), tr("Komprimieren"), tr("Aktivitäten"),
        tr("In den Papierkorb verschieben"), tr("Löschen"), tr("Umbenennen"),
        tr("Kopieren"), tr("Ausschneiden"), tr("Einfügen"),
        tr("Entpacken"), tr("Teilen"), tr("Extract"), tr("Share")
    };
    for (QAction *act : tempMenu.actions()) {
        if (act->isSeparator()) continue;
        // Farbige Ordner-Widget-Aktionen (haben kein Text oder sind Widgets) überspringen
        const QString actText = act->text().trimmed();
        if (actText.isEmpty()) continue;
        if (existingActionTexts.contains(actText)) continue;
        bool skip = false;
        for (const QString &t : skipTitles)
            if (actText.contains(t, Qt::CaseInsensitive)) { skip = true; break; }
        if (!skip) actMenu->addAction(act);
    }

    // Reihenfolge wie Dolphin: Aktionen, Stichwörter, Komprimieren, Aktivitäten
    menu.addMenu(actMenu);
    if (auto *a = findSubMenu(tr("Stichwörter zuweisen"))) menu.addAction(a);
    if (auto *a = findSubMenu(tr("Komprimieren")))          menu.addAction(a);

    // --- Entpacken-Submenü (wie Dolphin via Ark) ---
    buildExtractionMenu(menu, ctx);

    if (auto *a = findSubMenu(tr("Aktivitäten")))           menu.addAction(a);
    if (auto *a = findSubMenu(tr("Teilen")))                menu.addAction(a);

    // --- 5. EIGENE TAGS ---
    if (!TagManager::instance().tags().isEmpty()) {
      auto *tagMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("tag")), tr("Tag setzen"));
      for (const auto &tag : TagManager::instance().tags()) {
        const QString &tagName = tag.first;
        const QString &tagColor = tag.second;
        QPixmap pix(12, 12);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(tagColor));
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 12, 12);
        QAction *a = tagMenu->addAction(QIcon(pix), tagName);
        connect(a, &QAction::triggered, this, [this, ctx, tagName]() {
          if (!m_proxy) return;
          QStringList paths;
          for (const KFileItem &item : ctx.selectedItems) {
            QString path = item.localPath();
            if (!path.isEmpty()) paths << path;
          }
          if (!paths.isEmpty()) {
            TagManager::instance().setFileTags(paths, tagName);
          }
        });
      }
      tagMenu->addSeparator();
      QAction *clearAct = tagMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-clear")), tr("Tag entfernen"));
      connect(clearAct, &QAction::triggered, this, [this, ctx]() {
        if (!m_proxy) return;
        QStringList paths;
        for (const KFileItem &item : ctx.selectedItems) {
          QString path = item.localPath();
          if (!path.isEmpty()) paths << path;
        }
        if (!paths.isEmpty()) {
          TagManager::instance().clearFileTags(paths);
        }
      });
    }

    // --- 6. CHECKSUMMEN ---
    {
      QStringList localPaths;
      for (const KFileItem &item : ctx.selectedItems)
        if (item.url().isLocalFile() && !item.isDir())
          localPaths << item.url().toLocalFile();

      if (!localPaths.isEmpty()) {
        menu.addSeparator();
        auto *csMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-edit-verify")),
                                    tr("Prüfsumme"));
        for (const auto &pair : {
               std::pair<QString, QCryptographicHash::Algorithm>{QStringLiteral("MD5"),    QCryptographicHash::Md5},
               std::pair<QString, QCryptographicHash::Algorithm>{QStringLiteral("SHA-256"), QCryptographicHash::Sha256},
               std::pair<QString, QCryptographicHash::Algorithm>{QStringLiteral("SHA-1"),   QCryptographicHash::Sha1},
             }) {
          const QString algName = pair.first;
          const auto alg = pair.second;
          csMenu->addAction(algName, this, [localPaths, algName, alg, this]() {
            QStringList results;
            for (const QString &path : localPaths) {
              QFile f(path);
              if (!f.open(QIODevice::ReadOnly)) continue;
              QCryptographicHash hash(alg);
              if (!hash.addData(&f)) continue;
              const QString sum = hash.result().toHex();
              results << (localPaths.size() > 1
                          ? QFileInfo(path).fileName() + QStringLiteral(": ") + sum
                          : sum);
            }
            if (results.isEmpty()) return;
            const QString text = results.join(QLatin1Char('\n'));
            QApplication::clipboard()->setText(text);
            // Kurze visuelle Bestätigung via Tooltip-ähnlichem Mechanismus
            QToolTip::showText(QCursor::pos(),
                               tr("%1 kopiert").arg(algName), this, {}, 2000);
          });
        }
      }
    }

    menu.addSeparator();
  }

void FilePane::buildExtractionMenu(QMenu &menu, const ContextMenuState &ctx)
{
    static const QStringList kArchiveExts = {
      QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"),
      QStringLiteral("tgz"), QStringLiteral("bz2"), QStringLiteral("tbz2"),
      QStringLiteral("xz"), QStringLiteral("txz"), QStringLiteral("7z"),
      QStringLiteral("rar"), QStringLiteral("lzma"), QStringLiteral("lz"),
      QStringLiteral("lzo"), QStringLiteral("lha"), QStringLiteral("lzh"),
      QStringLiteral("cab"), QStringLiteral("ar"), QStringLiteral("cpio"),
      QStringLiteral("iso"), QStringLiteral("deb"), QStringLiteral("rpm"),
      QStringLiteral("jar"), QStringLiteral("war"), QStringLiteral("Z"),
    };
    bool allArchives = !ctx.items.isEmpty();
    QStringList archivePaths;
    for (const auto &it : ctx.items) {
      const QString path = it.localPath();
      const QString suffix = QFileInfo(path).suffix().toLower();
      if (!kArchiveExts.contains(suffix)) { allArchives = false; break; }
      archivePaths << path;
    }
    if (allArchives) {
      auto *extractMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("archive-extract")),
                                        tr("Entpacken"));
      setupExtractionMenuActions(extractMenu, archivePaths, m_currentPath);
    }
}

void FilePane::setupExtractionMenuActions(QMenu* extractMenu, const QStringList& archivePaths, const QString& workDir)
{
    extractMenu->addAction(QIcon::fromTheme(QStringLiteral("archive-extract")),
                           tr("Hierher entpacken"), this, [archivePaths, workDir]() {
      for (const QString &p : archivePaths)
        QProcess::startDetached(QStringLiteral("ark"),
                                 {QStringLiteral("--batch"),
                                  QStringLiteral("--autodestination"),
                                  QStringLiteral("--destination"), workDir, p});
    });
    extractMenu->addAction(QIcon::fromTheme(QStringLiteral("archive-extract")),
                           tr("Entpacken und Archiv in den Papierkorb verschieben"),
                           this, [archivePaths, workDir]() {
      QList<QUrl> trashUrls;
      for (const QString &p : archivePaths) trashUrls << QUrl::fromLocalFile(p);
      auto *proc = new QProcess();
      QStringList args{QStringLiteral("--batch"),
                       QStringLiteral("--autodestination"),
                       QStringLiteral("--destination"), workDir};
      args.append(archivePaths);
      connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
              [proc, trashUrls](int code, QProcess::ExitStatus) {
        if (code == 0) KIO::trash(trashUrls);
        proc->deleteLater();
      });
      proc->start(QStringLiteral("ark"), args);
    });
    extractMenu->addAction(QIcon::fromTheme(QStringLiteral("archive-extract")),
                           tr("Entpacken nach ..."), this, [this, archivePaths, workDir]() {
      const QString dest = QFileDialog::getExistingDirectory(this, tr("Zielordner wählen"), workDir);
      if (dest.isEmpty()) return;
      for (const QString &p : archivePaths)
        QProcess::startDetached(QStringLiteral("ark"),
                                 {QStringLiteral("--batch"),
                                  QStringLiteral("--autodestination"),
                                  QStringLiteral("--destination"), dest, p});
    });
}


void FilePane::populateBackgroundMenu(QMenu &menu, const ContextMenuState &ctx,
                                       KFileItemActions &actions,
                                       KFileItemListProperties &props) {
  const QUrl &dirUrl      = ctx.dirUrl;
  QAbstractItemView *view = ctx.view;
  Q_UNUSED(props);

  // --- 1. NEU ERSTELLEN ---
  if (m_newFileMenu && dirUrl.scheme() != "trash") {
    m_newFileMenu->setWorkingDirectory(dirUrl);
    m_newFileMenu->checkUpToDate();
    auto *newMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("document-new")),
                                 tr("Neu erstellen"));
    for (QAction *act : m_newFileMenu->menu()->actions())
      newMenu->addAction(act);
  }

  // --- 2. ORDNER ÖFFNEN MIT ---
  actions.insertOpenWithActionsTo(nullptr, &menu,
                                  QStringList{QCoreApplication::applicationName()});
  menu.addSeparator();

  // --- 3. EINFÜGEN ---
  if (m_actionCollection) {
    if (auto *a = m_actionCollection->action(QStringLiteral("file_paste")))
      menu.addAction(a);
  }
  menu.addSeparator();

  // --- 4. ANSICHT ---
  auto *sortMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-sort-ascending")),
                                tr("Sortieren nach"));
  auto *tree = qobject_cast<QTreeView *>(view);
  if (tree) {
    auto *hdr = tree->header();
    for (int i = 0; i < hdr->count(); ++i) {
      if (hdr->isSectionHidden(i))
        continue;
      QString label = tree->model()->headerData(i, Qt::Horizontal).toString();
      QAction *a = sortMenu->addAction(label);
      a->setCheckable(true);
      a->setChecked(hdr->sortIndicatorSection() == i);
      connect(a, &QAction::triggered, this,
              [i, tree]() { tree->sortByColumn(i, Qt::AscendingOrder); });
    }
  }
  sortMenu->addSeparator();
  auto *azAct = sortMenu->addAction(tr("A-Z"));
  azAct->setCheckable(true);
  auto *zaAct = sortMenu->addAction(tr("Z-A"));
  zaAct->setCheckable(true);
  if (tree) {
    azAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::AscendingOrder);
    zaAct->setChecked(tree->header()->sortIndicatorOrder() == Qt::DescendingOrder);
  }
  connect(azAct, &QAction::triggered, this, [tree]() {
    if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::AscendingOrder);
  });
  connect(zaAct, &QAction::triggered, this, [tree]() {
    if (tree) tree->sortByColumn(tree->header()->sortIndicatorSection(), Qt::DescendingOrder);
  });
  sortMenu->addSeparator();
  auto *folderFirstAct = sortMenu->addAction(tr("Ordner zuerst"));
  folderFirstAct->setCheckable(true);
  folderFirstAct->setChecked(m_foldersFirst);
  connect(folderFirstAct, &QAction::triggered, this, [this](bool checked) {
    setFoldersFirst(checked);
  });

  auto *modeMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("view-list-details")),
                                tr("Ansichtsmodus ändern"));
  auto addMode = [&](const QString &label, const QString &icon, int m) {
    QAction *a = modeMenu->addAction(QIcon::fromTheme(icon), label);
    a->setCheckable(true);
    a->setChecked(m_viewMode == m);
    connect(a, &QAction::triggered, this, [this, m]() { setViewMode(m); });
  };
  addMode(tr("Details"), "view-list-details", 0);
  addMode(tr("Symbole"), "view-list-icons", 1);
  menu.addSeparator();

  // --- 5. KIO-AKTIONEN ---
  QMenu tempMenu;
  actions.addActionsTo(&tempMenu, KFileItemActions::MenuActionSource::All, {});

  auto findAction = [&](const QString &text) -> QAction* {
    for (QAction *act : tempMenu.actions()) {
      if (act->isSeparator())
        continue;
      if (act->text().contains(text, Qt::CaseInsensitive))
        return act;
    }
    return nullptr;
  };

  auto *actMenu = new QMenu(tr("Aktionen"), &menu);
  actMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
  auto *termAct = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                              tr("Terminal hier öffnen"), actMenu);
  termAct->setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F4));
  connect(termAct, &QAction::triggered, this, [this, dirUrl]() {
    openTerminalHere(dirUrl.toLocalFile(), this);
  });
  actMenu->addAction(termAct);

  static const QStringList skipTitles = {
    tr("Stichwörter zuweisen"), tr("Komprimieren"), tr("Aktivitäten"),
    tr("Entpacken"), tr("Teilen"), tr("Extract"), tr("Share")
  };
  for (QAction *act : tempMenu.actions()) {
    if (act->isSeparator())
      continue;
    const QString actText = act->text().trimmed();
    if (actText.isEmpty())
      continue;
    bool skip = false;
    for (const QString &skipText : skipTitles) {
      if (actText.contains(skipText, Qt::CaseInsensitive)) {
        skip = true;
        break;
      }
    }
    if (!skip)
      actMenu->addAction(act);
  }

  menu.addMenu(actMenu);
  if (auto *action = findAction(tr("Stichwörter zuweisen")))
    menu.addAction(action);
  if (auto *action = findAction(tr("Komprimieren")))
    menu.addAction(action);
  if (auto *action = findAction(tr("Aktivitäten")))
    menu.addAction(action);
  menu.addSeparator();

#ifdef SC_PLUGIN_MAKEFILEACTIONS
  {
      const QString makePath = ctx.dirUrl.isLocalFile() ? ctx.dirUrl.toLocalFile() : m_currentPath;
      if (!makePath.isEmpty() && MakefileActions::hasMakefile(makePath)) {
          QStringList targets = MakefileActions::listTargets(makePath);
          if (!targets.isEmpty()) {
              auto *makeMenu = menu.addMenu(
                  QIcon::fromTheme("run-build"), tr("Make"));
              for (const QString &target : targets) {
                  auto *act = makeMenu->addAction(target);
                  connect(act, &QAction::triggered, this, [makePath, target, this]() {
                      MakefileActions::runTarget(makePath, target, this);
                  });
              }
          }
      }
  }
#endif
}
