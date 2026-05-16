// --- sidebargroups.cpp — Gruppen- und Tag-Logik der Sidebar ---
#include "sidebar.h"
#include "config.h"
#include "drivedelegate.h"

#include "thememanager.h"
#include "tagmanager.h"
#include "scglobal.h"
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <QButtonGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// sc_getText helper
static QString sc_getText(QWidget *parent, const QString &title, const QString &label,
                          const QString &defaultText = QString())
{
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
    auto *ok  = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok")),     QCoreApplication::translate("SplitCommander", "OK"),         &dlg);
    auto *can = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QCoreApplication::translate("SplitCommander", "Abbrechen"),  &dlg);
    ok->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    btnRow->addWidget(can);
    lay->addWidget(lbl);
    lay->addWidget(edit);
    lay->addLayout(btnRow);
    QObject::connect(ok,  &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(can, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.adjustSize();
    if (parent) {
        QPoint center = parent->mapToGlobal(parent->rect().center());
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    if (dlg.exec() != QDialog::Accepted) return {};
    return edit->text();
}

void Sidebar::onNewGroupDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Neue Gruppe"));
    dlg.setMinimumWidth(360);
    dlg.setStyleSheet(TM().ssDialog());

    auto *vl       = new QVBoxLayout(&dlg);
    vl->setSpacing(10);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->addWidget(new QLabel(tr("Gruppenname:")));
    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Mein Ordner..."));
    vl->addWidget(nameEdit);

    vl->addWidget(new QLabel(tr("Inhalt:")));
    auto *btnGrp   = new QButtonGroup(&dlg);
    auto *emptyBtn = new QPushButton(tr("Leere Gruppe"));
    auto *homeBtn  = new QPushButton(tr("Home-Favoriten"));
    for (auto *b : {emptyBtn, homeBtn}) { b->setCheckable(true); b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); }
    emptyBtn->setChecked(true);
    btnGrp->addButton(emptyBtn, 0);
    btnGrp->addButton(homeBtn,  1);
    auto *optRow = new QHBoxLayout(); optRow->setSpacing(6);
    optRow->addWidget(emptyBtn); optRow->addWidget(homeBtn);
    vl->addLayout(optRow);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(btns);

    nameEdit->setFocus();
    if (dlg.exec() != QDialog::Accepted) return;

    const QString grpName = nameEdit->text().trimmed();
    if (grpName.isEmpty()) return;

    auto s = Config::group("CustomGroups");
    QStringList groups = s.readEntry("groups", QStringList());
    if (!groups.contains(grpName)) {
        groups << grpName;
        s.writeEntry("groups", groups);
        s.config()->sync();
    }


    QListWidget *list = createGroupWidget(grpName, m_newGroupBox);
    saveGroupOrder();

    if (m_scrollArea && m_scrollArea->widget())
        m_scrollArea->widget()->adjustSize();

    if (btnGrp->checkedId() == 1) {
        const QStringList xdgPaths = {
            QDir::homePath(),
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
            QStringLiteral("trash:/"),
        };
        for (const QString &path : xdgPaths) {
            const QUrl url(path);
            const bool isKio = !url.scheme().isEmpty() && url.scheme() != "file";
            if (!path.isEmpty() && (isKio || QDir(path).exists()))
                addToGroup(grpName, list, path);
        }
    }
}

// --- Sidebar::createGroupWidget ---
QListWidget *Sidebar::createGroupWidget(const QString &name, QWidget *beforeWidget)
{
    // Äußere Box
    auto *outerBox = new QWidget();
    outerBox->setObjectName(QStringLiteral("groupBox"));
    outerBox->setStyleSheet(TM().ssBox());
    outerBox->setProperty("groupName", name);

    {
        auto pinSettings = Config::group("CustomGroups");
        outerBox->setProperty("pinned", pinSettings.readEntry("pinned_" + name, false));
    }


    auto *vbox = new QVBoxLayout(outerBox);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // Header (einfaches Widget ohne Drag-Funktion)
    auto *headerRow = new QWidget(outerBox);
    headerRow->setStyleSheet("background:transparent; border:none;");
    auto *hLay = new QHBoxLayout(headerRow);
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);

    auto *lbl = new QLabel(name.toUpper());
    lbl->setStyleSheet(QString("font-size:13px;font-weight:bold;text-transform:uppercase;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    // Menü-Button (KDE Style)
    auto *menuBtn = new QPushButton();
    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    if (menuBtn->icon().isNull()) menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(
        QString("QPushButton { background: transparent; border: none; padding: 2px; }"
                "QPushButton:hover { background: %1; border-radius: 4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(menuBtn);

    // Plus-Button (KDE Style)
    auto *addBtn = new QPushButton();
    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    if (addBtn->icon().isNull()) addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QString(
        "QPushButton { background:transparent; border:none; padding:2px; }"
        "QPushButton:hover { background:%1; border-radius:4px; }")
        .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
    vbox->addWidget(headerRow);

    // --- Liste in einen Container einbetten (für die Margins) ---
    auto *listCont = new QWidget();
    listCont->setStyleSheet("background:transparent; border:none;");
    auto *listLay = new QVBoxLayout(listCont);

    // Hier werden die Abstände gesetzt: 6px links/rechts, passend zur Geräte-Box
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSpacing(0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *list = new QListWidget();
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setFrameShape(QFrame::NoFrame);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSpacing(0);
    list->setIconSize(QSize(Config::sidebarIconSize(), Config::sidebarIconSize()));
    list->setStyleSheet(TM().ssListWidget());
    list->setItemDelegate(new DriveDelegate(false, this));
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Liste zum Container-Layout hinzufügen
    listLay->addWidget(list);

    adjustListHeight(list);
    vbox->addWidget(listCont);

    // Toggle für Orte (Favoriten)
    auto *toggleBtn = new QPushButton(); // KEIN "▲" im Konstruktor
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16); // Etwas höher für bessere Klickbarkeit
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    toggleBtn->setIconSize(QSize(10, 10));

    // font-size:7px entfernen, da Icons nicht auf Schriftgröße reagieren
    toggleBtn->setStyleSheet("QPushButton{background:transparent !important; border:none;}");

    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    connect(toggleBtn, &QPushButton::toggled, this, [listCont, toggleBtn](bool on) {
        listCont->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    // Wrapper mit dunklem Hintergrund (wie GERÄTE-Box)
    auto *wrapper = new QWidget();
    wrapper->setObjectName(QStringLiteral("groupWrapper"));
    wrapper->setStyleSheet(QString("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);
    wLay->addWidget(outerBox);

    // In Layout einfügen: immer am Ende der Gruppenliste, aber vor dem Stretch-Element.
    int insertIdx = -1;
    if (beforeWidget)
        insertIdx = m_contentLayout->indexOf(beforeWidget);
    if (insertIdx == -1) {
        const int total = m_contentLayout->count();
        if (total == 0) {
            insertIdx = 0;
        } else {
            auto *lastItem = m_contentLayout->itemAt(total - 1);
            if (lastItem && lastItem->spacerItem())
                insertIdx = total - 1;
            else
                insertIdx = total;
        }
    }
    m_contentLayout->insertWidget(insertIdx, wrapper);

    // Signals
    auto sharedName = std::make_shared<QString>(name);

    connect(toggleBtn, &QPushButton::toggled, list, [list, toggleBtn](bool on) {
        list->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? "go-down" : "go-up"));
    });

    connect(addBtn, &QPushButton::clicked, this, [this, list, sharedName]() {
        QString activePath;
        emit requestActivePath(&activePath);
        addToGroup(*sharedName, list, activePath);
    });

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit driveClicked(it->data(Qt::UserRole).toString());
    });

    connect(list, &QListWidget::customContextMenuRequested, this,
            [this, list, sharedName](const QPoint &pos) {
        auto *item = list->itemAt(pos);
        if (!item) return;
        showPlaceContextMenu(item, list, pos, *sharedName);
    });

    connect(menuBtn, &QPushButton::clicked, this,
            [this, menuBtn, lbl, sharedName, outerBox, wrapper]() {
        auto *m = new QMenu(this);
        m->setAttribute(Qt::WA_DeleteOnClose);
        m->setStyleSheet(TM().ssMenu());

        // Umbenennen
        connect(m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Gruppe umbenennen")),
                &QAction::triggered, this, [this, lbl, sharedName]() {
            bool ok;
            QString newName = sc_getText(this, tr("Gruppe umbenennen"), tr("Neuer Name:"), *sharedName);
            ok = !newName.isNull();
            if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == *sharedName) return;
            const QString oldName = *sharedName;
            *sharedName = newName.trimmed();
            lbl->setText(sharedName->toUpper());

            auto gs = Config::group("CustomGroups");
            QStringList grps = gs.readEntry("groups", QStringList());
            int idx = grps.indexOf(oldName);
            if (idx != -1) { grps[idx] = *sharedName; gs.writeEntry("groups", grps); }
 
            KConfigGroup oldGrp(gs.config(), gs.name() + "/group_" + oldName);
            KConfigGroup newGrp(gs.config(), gs.name() + "/group_" + *sharedName);
            int cnt = oldGrp.readEntry("size", 0);
            newGrp.writeEntry("size", cnt);
            for (int j = 1; j <= cnt; ++j) {
                KConfigGroup oldItem(oldGrp.config(), oldGrp.name() + "/" + QString::number(j));
                KConfigGroup newItem(newGrp.config(), newGrp.name() + "/" + QString::number(j));
                newItem.writeEntry("path", oldItem.readEntry("path", QString()));
                newItem.writeEntry("name", oldItem.readEntry("name", QString()));
            }
            oldGrp.deleteGroup();
            gs.writeEntry("pinned_" + *sharedName, gs.readEntry("pinned_" + oldName, false));
            gs.deleteEntry("pinned_" + oldName);
            gs.config()->sync();

        });

        m->addSeparator();

        // Pin/Unpin
        const bool isPinned = outerBox->property("pinned").toBool();
        connect(m->addAction(QIcon::fromTheme(isPinned ? "window-unpin" : "window-pin"),
                             isPinned ? tr("Lösen") : tr("An Position verankern")),
                &QAction::triggered, this, [outerBox, sharedName]() {

            const bool nowPinned = !outerBox->property("pinned").toBool();
            outerBox->setProperty("pinned", nowPinned);
            auto gs = Config::group("CustomGroups");
            gs.writeEntry("pinned_" + *sharedName, nowPinned);
            gs.config()->sync();
        });

        m->addSeparator();

        auto *upAct   = m->addAction(QIcon::fromTheme(QStringLiteral("go-up")),   tr("Nach oben"));
        auto *downAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-down")), tr("Nach unten"));
        if (isPinned) { upAct->setEnabled(false); downAct->setEnabled(false); }

        m->addSeparator();
        auto *delAct = m->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Gruppe löschen"));

        connect(upAct, &QAction::triggered, this, [this, wrapper]() {
            int idx = m_contentLayout->indexOf(wrapper);
            if (idx > 0) { m_contentLayout->removeWidget(wrapper); m_contentLayout->insertWidget(idx - 1, wrapper); saveGroupOrder(); }
        });
        connect(downAct, &QAction::triggered, this, [this, wrapper]() {
            int idx = m_contentLayout->indexOf(wrapper);
            if (idx >= 0 && idx < m_contentLayout->count() - 2) { m_contentLayout->removeWidget(wrapper); m_contentLayout->insertWidget(idx + 1, wrapper); saveGroupOrder(); }
        });
        connect(delAct, &QAction::triggered, this, [this, wrapper, sharedName]() {
            m_contentLayout->removeWidget(wrapper);
            delete wrapper;
            if (m_scrollArea && m_scrollArea->widget())
                m_scrollArea->widget()->adjustSize();
            auto gs = Config::group("CustomGroups");
            KConfigGroup(gs.config(), gs.name() + "/group_" + *sharedName).deleteGroup();
            gs.config()->sync();
            saveGroupOrder();
        });


        m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    return list;
}

// --- Sidebar::saveGroupOrder ---
void Sidebar::saveGroupOrder()
{
    QStringList order;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *w = m_contentLayout->itemAt(i) ? m_contentLayout->itemAt(i)->widget() : nullptr;
        if (!w || w->objectName() != "groupWrapper") continue;
        auto *ob = w->findChild<QWidget *>("groupBox");
        if (ob) order << ob->property("groupName").toString();
    }
    auto gs = Config::group("CustomGroups");
    gs.writeEntry("groups", order);
    gs.config()->sync();
}

void Sidebar::loadCustomGroups()
{
    auto s = Config::group("CustomGroups");
    const QStringList groups = s.readEntry("groups", QStringList());
    for (const QString &groupName : groups) {
        QListWidget *list = createGroupWidget(groupName, m_newGroupBox);
        if (groupName == "Favoriten") m_favList = list;

        KConfigGroup g(s.config(), s.name() + "/group_" + groupName);
        int cnt = g.readEntry("size", 0);
        for (int i = 1; i <= cnt; ++i) {
            KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
            const QString path      = itemG.readEntry("path", QString());
            const QString itemName  = itemG.readEntry("name", QString());
            const QString customIco = itemG.readEntry("icon", QString());
            if (path.isEmpty()) continue;

            QIcon ico;
            if (!customIco.isEmpty()) {
                ico = QIcon::fromTheme(customIco);
            } else if (!path.startsWith("/")) {
                const QString scheme = QUrl::fromUserInput(path).scheme().toLower();
                ico = QIcon::fromTheme(
                    scheme == "gdrive"    ? "folder-gdrive"     :
                    scheme == "smb"       ? "network-workgroup"  :
                    scheme == "sftp" || scheme == "ssh" ? "network-connect" :
                    scheme == "mtp"       ? "multimedia-player"  :
                    scheme == "bluetooth" ? "bluetooth"          :
                    "network-server");
            } else {
                ico = QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(path)));
            }
            if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("folder"));
            auto *it = new QListWidgetItem(ico, itemName, list);
            it->setData(Qt::UserRole, path);
            it->setData(Qt::UserRole + 2, customIco);
        }
        adjustListHeight(list);
    }
}


// --- Sidebar::addToGroup ---
void Sidebar::addToGroup(const QString &groupName, QListWidget *list, const QString &path)
{
    if (path.isEmpty() || !list) return;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == path) return;

    QUrl url(path);
    QString name = url.isLocalFile() ? QDir(path).dirName() : url.fileName();
    if (name.isEmpty()) name = path;

    // Spezielle Namen für bekannte KIO-Pfade
    const QString scheme = url.scheme().toLower();
    if (scheme == "trash")             name = tr("Papierkorb");
    else if (scheme == "recentdocuments") name = tr("Zuletzt verwendet");
    else if (scheme == "remote")       name = tr("Netzwerk");
    else if (path == QDir::homePath()) name = tr("Persönlicher Ordner");

    // Icon immer aus System-Theme
    QIcon ico;
    if (scheme == "trash") {
        ico = QIcon::fromTheme("user-trash");
        if (ico.isNull()) ico = QIcon::fromTheme("user-trash-full");
        if (ico.isNull()) ico = QIcon::fromTheme("trash-empty");
        if (ico.isNull()) ico = QIcon::fromTheme("trash");
    } else if (scheme == "recentdocuments") {
        ico = QIcon::fromTheme("document-open-recent");
    } else if (!path.startsWith("/")) {
        ico = QIcon::fromTheme(
            scheme == "gdrive"    ? "folder-gdrive"      :
            scheme == "smb"       ? "network-workgroup"   :
            scheme == "sftp" || scheme == "ssh" ? "network-connect" :
            scheme == "mtp"       ? "multimedia-player"   :
            scheme == "bluetooth" ? "bluetooth"           :
            scheme == "afc"       ? "phone"               :
            "network-server");
    } else {
        ico = QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(path)));
    }
    if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("folder"));

    auto *it = new QListWidgetItem(ico, name, list);
    it->setData(Qt::UserRole, path);
    adjustListHeight(list);

    auto gs = Config::group("CustomGroups");
    KConfigGroup(gs.config(), gs.name() + "/group_" + groupName).deleteGroup();
    KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
    gNew.writeEntry("size", list->count());

    for (int i = 0; i < list->count(); ++i) {
        KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));

        itemG.writeEntry("path", list->item(i)->data(Qt::UserRole).toString());
        itemG.writeEntry("name", list->item(i)->text());
    }
    gs.config()->sync();

}

// --- Sidebar::addPlace ---
void Sidebar::addPlace(const QString &path)
{
    if (path.isEmpty() || !m_favList) return;
    addToGroup("Favoriten", m_favList, path);
}

QStringList Sidebar::groupNames() const
{
    QStringList names;
    if (!m_contentLayout) return names;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *item = m_contentLayout->itemAt(i);
        if (!item) continue;
        auto *wrapper = item->widget();
        if (!wrapper) continue;
        auto *box = wrapper->findChild<QWidget*>("groupBox");
        if (box) {
            QString name = box->property("groupName").toString();
            if (!name.isEmpty()) names << name;
        }
    }
    return names;
}

void Sidebar::addPathToGroup(const QString &groupName, const QString &path)
{
    if (!m_contentLayout) return;
    for (int i = 0; i < m_contentLayout->count(); ++i) {
        auto *item = m_contentLayout->itemAt(i);
        if (!item) continue;
        auto *wrapper = item->widget();
        if (!wrapper) continue;
        auto *box = wrapper->findChild<QWidget*>("groupBox");
        if (box && box->property("groupName").toString() == groupName) {
            auto *list = box->findChild<QListWidget*>();
            if (list) {
                addToGroup(groupName, list, path);
                return;
            }
        }
    }
}
void Sidebar::addTagItem(const QString &name, const QString &color, const QString &fontFamily)
{
    QPixmap pix(14, 14);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(color)); p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 14, 14);

    auto *it = new QListWidgetItem(QIcon(pix), name, m_tagList);
    it->setData(Qt::UserRole,     color);
    it->setData(Qt::UserRole + 1, fontFamily);
    if (!fontFamily.isEmpty()) it->setFont(QFont(fontFamily));
}

void Sidebar::setupTags()
{
    m_tagList->clear();
    for (const auto &t : TagManager::instance().tags()) {
        addTagItem(t.first, t.second, QString());
    }

 
    adjustListHeight(m_tagList);
    if (m_tagsBox)  m_tagsBox->updateGeometry();
    if (m_tagsWrap) m_tagsWrap->updateGeometry();
 
    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit tagClicked(it->text());
        QTimer::singleShot(150, m_tagList, [this]() {
            m_tagList->clearSelection();
        });
    });



    m_tagList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tagList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_tagList->itemAt(pos);
        if (!item) return;
        QMenu menu(this);
        menu.setStyleSheet(TM().ssMenu());

        menu.addAction(QIcon::fromTheme(QStringLiteral("color-picker")), tr("Farbe ändern …"), this,
            [this, item]() {
                QColorDialog dlg(QColor(item->data(Qt::UserRole).toString()), this);
                dlg.setWindowTitle(tr("Farbe wählen"));
                dlg.setOptions(QColorDialog::DontUseNativeDialog);
                dlg.setStyleSheet(TM().ssDialog());
                if (dlg.exec() != QDialog::Accepted) return;
                QColor col = dlg.currentColor();
                if (!col.isValid()) return;
                item->setData(Qt::UserRole, col.name());
                QPixmap pix(14, 14); pix.fill(Qt::transparent);
                QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(col); p.setPen(Qt::NoPen); p.drawEllipse(0, 0, 14, 14);
                item->setIcon(QIcon(pix));
                saveTags();
            });
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen …"), this,
            [this, item]() {
                bool ok;
                QString name = sc_getText(this, tr("Tag umbenennen"), tr("Name:"), item->text());
                ok = !name.isNull();
                if (ok && !name.isEmpty()) { item->setText(name); saveTags(); }
            });
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Löschen"), this,
            [this, item]() {
                delete m_tagList->takeItem(m_tagList->row(item));
                adjustListHeight(m_tagList);
                if (m_tagsBox)  m_tagsBox->updateGeometry();
                if (m_tagsWrap) m_tagsWrap->updateGeometry();
                saveTags();
            });

        menu.exec(m_tagList->mapToGlobal(pos));
    });
}

void Sidebar::saveTags()
{
    auto s = Config::group("Tags");
    KConfigGroup(s.config(), s.name() + "/tags").deleteGroup();
    KConfigGroup tagsG(s.config(), s.name() + "/tags");
    tagsG.writeEntry("size", m_tagList->count());
    for (int i = 0; i < m_tagList->count(); ++i) {
        KConfigGroup tG(tagsG.config(), tagsG.name() + "/" + QString::number(i + 1));
        tG.writeEntry("name",  m_tagList->item(i)->text());
        tG.writeEntry("color", m_tagList->item(i)->data(Qt::UserRole).toString());
        tG.writeEntry("font",  m_tagList->item(i)->data(Qt::UserRole + 1).toString());
    }
    s.config()->sync();
}
