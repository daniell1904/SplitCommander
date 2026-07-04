#include "sidebar.h"
#include "config.h"
#include "drivedelegate.h"
#include "thememanager.h"
#include "tagmanager.h"
#include <KIO/CopyJob>
#include <KIO/Global>
#include <KIO/OpenUrlJob>
#include <KIO/JobUiDelegateFactory>
#include <QDir>
#include <QButtonGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QInputDialog>
#include <QTreeWidget>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>
#include <QVBoxLayout>

static QString sc_getText(QWidget *parent, const QString &title, const QString &label, const QString &defaultText = QString())
{
    QDialog dlg(parent);
    dlg.setAttribute(Qt::WA_StyledBackground, true);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(TM().ssDialog());
    auto *lay = new QVBoxLayout(&dlg);
    Q_ASSERT(lay != nullptr);
    lay->setSpacing(4);
    lay->setContentsMargins(12, 10, 12, 10);
    auto *lbl = new QLabel(label, &dlg);
    Q_ASSERT(lbl != nullptr);
    auto *edit = new QLineEdit(defaultText, &dlg);
    Q_ASSERT(edit != nullptr);
    auto *btnRow = new QHBoxLayout();
    Q_ASSERT(btnRow != nullptr);
    auto *ok = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-ok")), QCoreApplication::translate("SplitCommander", "OK"), &dlg);
    auto *can = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QCoreApplication::translate("SplitCommander", "Abbrechen"), &dlg);
    Q_ASSERT(ok != nullptr && can != nullptr);
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
    if (parent != nullptr)
    {
        QPoint center = parent->mapToGlobal(parent->rect().center());
        dlg.move(center - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    if (dlg.exec() != QDialog::Accepted)
    {
        return {};
    }
    return edit->text();
}

void Sidebar::handleNewGroupDialogAccepted(int checkedId, const QString &grpName, QButtonGroup *btnGrp)
{
    Q_ASSERT(btnGrp != nullptr);
    auto s = Config::group("CustomGroups");
    QStringList groups = s.readEntry("groups", QStringList());
    if (!groups.contains(grpName))
    {
        groups << grpName;
        s.writeEntry("groups", groups);
        s.config()->sync();
    }

#ifdef SC_PLUGIN_GIT
    if (checkedId == 2)
    {
        KConfigGroup g(s.config(), s.name() + "/group_" + grpName);
        g.writeEntry("type", QStringLiteral("git"));
        g.config()->sync();
        createGitGroupWidget(grpName);
        saveGroupOrder();
        refreshGitSection();
        if (m_scrollArea != nullptr && m_scrollArea->widget() != nullptr)
        {
            m_scrollArea->widget()->adjustSize();
        }
        return;
    }
#endif

    QListWidget *list = createGroupWidget(grpName, m_newGroupBox);
    saveGroupOrder();

    if (m_scrollArea != nullptr && m_scrollArea->widget() != nullptr)
    {
        m_scrollArea->widget()->adjustSize();
    }

    if (checkedId == 1)
    {
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
        for (const QString &path : xdgPaths)
        {
            const QUrl url(path);
            const bool isKio = !url.scheme().isEmpty() && url.scheme() != QStringLiteral("file");
            if (!path.isEmpty() && (isKio || QDir(path).exists()))
            {
                addToGroup(grpName, list, path);
            }
        }
    }
}

static void sc_buildNewGroupDialogContentButtons(QVBoxLayout *vl, QButtonGroup *&btnGrp)
{
    btnGrp = new QButtonGroup(vl->parentWidget());
    Q_ASSERT(btnGrp != nullptr);
    auto *emptyBtn = new QPushButton(QObject::tr("Leere Gruppe"));
    auto *homeBtn = new QPushButton(QObject::tr("Home-Favoriten"));
    Q_ASSERT(emptyBtn != nullptr && homeBtn != nullptr);
    
#ifdef SC_PLUGIN_GIT
    auto *gitBtn = new QPushButton(QObject::tr("Git Repositories"));
    Q_ASSERT(gitBtn != nullptr);
    for (auto *b : {emptyBtn, homeBtn, gitBtn})
    {
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
#else
    for (auto *b : {emptyBtn, homeBtn})
    {
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
#endif
    emptyBtn->setChecked(true);
    btnGrp->addButton(emptyBtn, 0);
    btnGrp->addButton(homeBtn, 1);
#ifdef SC_PLUGIN_GIT
    btnGrp->addButton(gitBtn, 2);
    auto s = Config::group("CustomGroups");
    const QStringList existingGroups = s.readEntry("groups", QStringList());
    for (const QString &gn : existingGroups)
    {
        KConfigGroup g(s.config(), s.name() + "/group_" + gn);
        if (g.readEntry("type", QString()) == QStringLiteral("git"))
        {
            gitBtn->setEnabled(false);
            gitBtn->setToolTip(QObject::tr("Es existiert bereits eine Git-Box"));
            break;
        }
    }
#endif

    auto *optRow = new QHBoxLayout();
    Q_ASSERT(optRow != nullptr);
    optRow->setSpacing(6);
    optRow->addWidget(emptyBtn);
    optRow->addWidget(homeBtn);
#ifdef SC_PLUGIN_GIT
    optRow->addWidget(gitBtn);
#endif
    vl->addLayout(optRow);
}

static void sc_buildNewGroupDialogUI(QDialog *dlg, QLineEdit *&nameEdit, QButtonGroup *&btnGrp)
{
    auto *vl = new QVBoxLayout(dlg);
    Q_ASSERT(vl != nullptr);
    vl->setSpacing(10);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->addWidget(new QLabel(QObject::tr("Gruppenname:")));
    nameEdit = new QLineEdit(dlg);
    Q_ASSERT(nameEdit != nullptr);
    nameEdit->setPlaceholderText(QObject::tr("Mein Ordner..."));
    vl->addWidget(nameEdit);

    vl->addWidget(new QLabel(QObject::tr("Inhalt:")));
    sc_buildNewGroupDialogContentButtons(vl, btnGrp);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    Q_ASSERT(btns != nullptr);
    QObject::connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vl->addWidget(btns);

    nameEdit->setFocus();
}

void Sidebar::onNewGroupDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Neue Gruppe"));
    dlg.setMinimumWidth(480);
    dlg.setStyleSheet(TM().ssDialog());

    QLineEdit *nameEdit = nullptr;
    QButtonGroup *btnGrp = nullptr;
    sc_buildNewGroupDialogUI(&dlg, nameEdit, btnGrp);
    dlg.adjustSize();
    if (dlg.exec() == QDialog::Accepted)
    {
        const QString grpName = nameEdit->text().trimmed();
        if (!grpName.isEmpty())
        {
            handleNewGroupDialogAccepted(btnGrp->checkedId(), grpName, btnGrp);
        }
    }
}


void Sidebar::setupGroupWidgetHeader(QWidget *headerRow, QHBoxLayout *hLay, const QString &name, QPushButton *menuBtn, QPushButton *addBtn)
{
    Q_ASSERT(headerRow != nullptr && hLay != nullptr && menuBtn != nullptr && addBtn != nullptr);
    headerRow->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);

    QString displayName = name;
    if (name == QStringLiteral("Orte")) displayName = tr("Orte");
    else if (name == QStringLiteral("Favoriten")) displayName = tr("Favoriten");
    else if (name == QStringLiteral("Repos")) displayName = tr("Repos");
    else if (name == QStringLiteral("Repo")) displayName = tr("Repo");

    auto *lbl = new QLabel(displayName);
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QStringLiteral("font-size:14px;font-weight:normal;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    if (menuBtn->icon().isNull())
    {
        menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    }
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(QStringLiteral("QPushButton { background: transparent; border: none; padding: 2px; }"
                                         "QPushButton:hover { background: %1; border-radius: 4px; }")
                              .arg(TM().colors().bgHover));
    hLay->addWidget(menuBtn);

    addBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    if (addBtn->icon().isNull())
    {
        addBtn->setIcon(QIcon::fromTheme(QStringLiteral("add-subtitle-symbolic")));
    }
    addBtn->setFixedSize(26, 22);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QStringLiteral("QPushButton { background:transparent; border:none; padding:2px; }"
                                         "QPushButton:hover { background:%1; border-radius:4px; }")
                             .arg(TM().colors().bgHover));
    hLay->addWidget(addBtn);
}

void Sidebar::handleGroupMenuRename(std::shared_ptr<QString> sharedName, QLabel *lbl)
{
    Q_ASSERT(sharedName != nullptr && lbl != nullptr);
    bool ok;
    QString newName = sc_getText(this, tr("Gruppe umbenennen"), tr("Neuer Name:"), *sharedName);
    ok = !newName.isNull();
    if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == *sharedName)
    {
        return;
    }
    const QString oldName = *sharedName;
    *sharedName = newName.trimmed();
    lbl->setText(*sharedName);

    auto gs = Config::group("CustomGroups");
    QStringList grps = gs.readEntry("groups", QStringList());
    int idx = grps.indexOf(oldName);
    if (idx != -1)
    {
        grps[idx] = *sharedName;
        gs.writeEntry("groups", grps);
    }
 
    KConfigGroup oldGrp(gs.config(), gs.name() + "/group_" + oldName);
    KConfigGroup newGrp(gs.config(), gs.name() + "/group_" + *sharedName);
    int cnt = oldGrp.readEntry("size", 0);
    newGrp.writeEntry("size", cnt);
    for (int j = 1; j <= cnt; ++j)
    {
        KConfigGroup oldItem(oldGrp.config(), oldGrp.name() + "/" + QString::number(j));
        KConfigGroup newItem(newGrp.config(), newGrp.name() + "/" + QString::number(j));
        newItem.writeEntry("path", oldItem.readEntry("path", QString()));
        newItem.writeEntry("name", oldItem.readEntry("name", QString()));
    }
    oldGrp.deleteGroup();
    gs.writeEntry("pinned_" + *sharedName, gs.readEntry("pinned_" + oldName, false));
    gs.deleteEntry("pinned_" + oldName);
    gs.config()->sync();
}

void Sidebar::handleGroupMenuDelete(QWidget *wrapper, std::shared_ptr<QString> sharedName)
{
    Q_ASSERT(wrapper != nullptr && sharedName != nullptr && m_contentLayout != nullptr);
    m_contentLayout->removeWidget(wrapper);
    delete wrapper;
    if (m_scrollArea != nullptr && m_scrollArea->widget() != nullptr)
    {
        m_scrollArea->widget()->adjustSize();
    }
    auto gs = Config::group("CustomGroups");
    KConfigGroup(gs.config(), gs.name() + "/group_" + *sharedName).deleteGroup();
    QStringList grps = gs.readEntry("groups", QStringList());
    grps.removeAll(*sharedName);
    gs.writeEntry("groups", grps);
    gs.config()->sync();
    saveGroupOrder();
}

void Sidebar::setupGroupWidgetConnections(QListWidget *list, std::shared_ptr<QString> sharedName, QPushButton *toggleBtn, QPushButton *addBtn, QPushButton *menuBtn, QLabel *lbl, QWidget *outerBox, QWidget *wrapper)
{
    Q_ASSERT(list != nullptr && sharedName != nullptr && toggleBtn != nullptr && addBtn != nullptr && menuBtn != nullptr && lbl != nullptr && outerBox != nullptr && wrapper != nullptr);

    connect(addBtn, &QPushButton::clicked, this, [this, list, sharedName]()
    {
        QString activePath;
        emit requestActivePath(&activePath);
        addToGroup(*sharedName, list, activePath);
    });

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it)
    {
        Q_ASSERT(it != nullptr);
        emit driveClicked(it->data(Qt::UserRole).toString());
    });

    connect(list, &QListWidget::customContextMenuRequested, this, [this, list, sharedName](const QPoint &pos)
    {
        auto *item = list->itemAt(pos);
        if (item != nullptr)
        {
            showPlaceContextMenu(item, list, pos, *sharedName);
        }
    });

    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl, sharedName, outerBox, wrapper]()
    {
        handleGroupMenuClicked(menuBtn, lbl, sharedName, outerBox, wrapper);
    });
}

void Sidebar::handleGroupMenuClicked(QPushButton *menuBtn, QLabel *lbl, std::shared_ptr<QString> sharedName, QWidget *outerBox, QWidget *wrapper)
{
    auto *m = new QMenu(this);
    Q_ASSERT(m != nullptr);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->setStyleSheet(TM().ssMenu());

    connect(m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Gruppe umbenennen")), &QAction::triggered, this, [this, sharedName, lbl]()
    {
        handleGroupMenuRename(sharedName, lbl);
    });

    m->addSeparator();

    const bool isPinned = outerBox->property("pinned").toBool();
    setupGroupMenuPinAction(m, outerBox, sharedName, isPinned);

    m->addSeparator();

    setupGroupMenuMoveActions(m, wrapper, isPinned);

    m->addSeparator();
    auto *delAct = m->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Gruppe löschen"));
    Q_ASSERT(delAct != nullptr);

    connect(delAct, &QAction::triggered, this, [this, wrapper, sharedName]()
    {
        handleGroupMenuDelete(wrapper, sharedName);
    });

    m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
}

void Sidebar::setupGroupMenuPinAction(QMenu *m, QWidget *outerBox, std::shared_ptr<QString> sharedName, bool isPinned)
{
    connect(m->addAction(QIcon::fromTheme(isPinned ? QStringLiteral("window-unpin") : QStringLiteral("window-pin")),
                         isPinned ? tr("Lösen") : tr("An Position verankern")), &QAction::triggered, this, [outerBox, sharedName]()
    {
        const bool nowPinned = !outerBox->property("pinned").toBool();
        outerBox->setProperty("pinned", nowPinned);
        auto gs = Config::group("CustomGroups");
        gs.writeEntry("pinned_" + *sharedName, nowPinned);
        gs.config()->sync();
    });
}

void Sidebar::setupGroupMenuMoveActions(QMenu *m, QWidget *wrapper, bool isPinned)
{
    auto *upAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-up")), tr("Nach oben"));
    auto *downAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-down")), tr("Nach unten"));
    Q_ASSERT(upAct != nullptr && downAct != nullptr);
    if (isPinned)
    {
        upAct->setEnabled(false);
        downAct->setEnabled(false);
    }

    connect(upAct, &QAction::triggered, this, [this, wrapper]()
    {
        Q_ASSERT(m_contentLayout != nullptr);
        int idx = m_contentLayout->indexOf(wrapper);
        if (idx > 0)
        {
            m_contentLayout->removeWidget(wrapper);
            m_contentLayout->insertWidget(idx - 1, wrapper);
            saveGroupOrder();
        }
    });
    connect(downAct, &QAction::triggered, this, [this, wrapper]()
    {
        Q_ASSERT(m_contentLayout != nullptr);
        int idx = m_contentLayout->indexOf(wrapper);
        if (idx >= 0 && idx < m_contentLayout->count() - 2)
        {
            m_contentLayout->removeWidget(wrapper);
            m_contentLayout->insertWidget(idx + 1, wrapper);
            saveGroupOrder();
        }
    });
}

QListWidget *Sidebar::createGroupWidget(const QString &name, QWidget *beforeWidget)
{
    auto *outerBox = new QWidget();
    Q_ASSERT(outerBox != nullptr);
    outerBox->setObjectName(QStringLiteral("groupBox"));
    outerBox->setStyleSheet(TM().ssBox());
    outerBox->setProperty("groupName", name);

    {
        auto pinSettings = Config::group("CustomGroups");
        outerBox->setProperty("pinned", pinSettings.readEntry("pinned_" + name, false));
    }

    auto *vbox = new QVBoxLayout(outerBox);
    Q_ASSERT(vbox != nullptr);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *headerRow = new QWidget(outerBox);
    Q_ASSERT(headerRow != nullptr);
    auto *hLay = new QHBoxLayout(headerRow);
    Q_ASSERT(hLay != nullptr);
    auto *lbl = new QLabel(name);
    Q_ASSERT(lbl != nullptr);

    auto *menuBtn = new QPushButton();
    auto *addBtn = new QPushButton();
    Q_ASSERT(menuBtn != nullptr && addBtn != nullptr);
    setupGroupWidgetHeader(headerRow, hLay, name, menuBtn, addBtn);
    vbox->addWidget(headerRow);

    QWidget *listCont = nullptr;
    auto *list = buildGroupListAndContainer(listCont);
    vbox->addWidget(listCont);

    auto *toggleBtn = buildGroupToggleBtn(listCont);
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    auto *wrapper = new QWidget();
    Q_ASSERT(wrapper != nullptr);
    wrapper->setObjectName(QStringLiteral("groupWrapper"));
    wrapper->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    Q_ASSERT(wLay != nullptr);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);
    wLay->addWidget(outerBox);

    insertGroupWrapper(wrapper, beforeWidget);

    auto sharedName = std::make_shared<QString>(name);
    setupGroupWidgetConnections(list, sharedName, toggleBtn, addBtn, menuBtn, lbl, outerBox, wrapper);

    return list;
}

QPushButton *Sidebar::buildGroupToggleBtn(QWidget *listCont)
{
    auto *toggleBtn = new QPushButton();
    Q_ASSERT(toggleBtn != nullptr);
    toggleBtn->setCheckable(true);
    toggleBtn->setFixedHeight(16);
    toggleBtn->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    toggleBtn->setIconSize(QSize(10, 10));
    toggleBtn->setStyleSheet(QStringLiteral("QPushButton{background:transparent !important; border:none;}"));

    connect(toggleBtn, &QPushButton::toggled, this, [listCont, toggleBtn](bool on)
    {
        listCont->setVisible(!on);
        toggleBtn->setIcon(QIcon::fromTheme(on ? QStringLiteral("go-down") : QStringLiteral("go-up")));
    });

    return toggleBtn;
}

QListWidget *Sidebar::buildGroupListAndContainer(QWidget *&listCont)
{
    listCont = new QWidget();
    Q_ASSERT(listCont != nullptr);
    listCont->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *listLay = new QVBoxLayout(listCont);
    Q_ASSERT(listLay != nullptr);

    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSpacing(0);
    listLay->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *list = new QListWidget();
    Q_ASSERT(list != nullptr);
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

    listLay->addWidget(list);
    adjustListHeight(list);
    return list;
}

void Sidebar::insertGroupWrapper(QWidget *wrapper, QWidget *beforeWidget)
{
    int insertIdx = -1;
    if (beforeWidget != nullptr)
    {
        Q_ASSERT(m_contentLayout != nullptr);
        insertIdx = m_contentLayout->indexOf(beforeWidget);
    }
    if (insertIdx == -1)
    {
        Q_ASSERT(m_contentLayout != nullptr);
        const int total = m_contentLayout->count();
        if (total == 0)
        {
            insertIdx = 0;
        }
        else
        {
            auto *lastItem = m_contentLayout->itemAt(total - 1);
            if (lastItem != nullptr && lastItem->spacerItem() != nullptr)
            {
                insertIdx = total - 1;
            }
            else
            {
                insertIdx = total;
            }
        }
    }
    m_contentLayout->insertWidget(insertIdx, wrapper);
}

#ifdef SC_PLUGIN_GIT
void Sidebar::setupGitGroupWidgetHeader(QWidget *headerRow, QHBoxLayout *hLay, const QString &name, QPushButton *menuBtn)
{
    Q_ASSERT(headerRow != nullptr && hLay != nullptr && menuBtn != nullptr);
    headerRow->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    hLay->setContentsMargins(12, 10, 8, 6);
    hLay->setSpacing(4);

    QString displayName = name;
    if (name == QStringLiteral("Orte")) displayName = tr("Orte");
    else if (name == QStringLiteral("Favoriten")) displayName = tr("Favoriten");
    else if (name == QStringLiteral("Repos")) displayName = tr("Repos");
    else if (name == QStringLiteral("Repo")) displayName = tr("Repo");

    auto *lbl = new QLabel(displayName);
    Q_ASSERT(lbl != nullptr);
    lbl->setStyleSheet(QStringLiteral("font-size:14px;font-weight:normal;background:transparent;color:%1;").arg(TM().colors().textAccent));
    hLay->addWidget(lbl, 1);

    menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    if (menuBtn->icon().isNull())
    {
        menuBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
    }
    menuBtn->setFixedSize(26, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setStyleSheet(QStringLiteral("QPushButton { background: transparent; border: none; padding: 2px; }"
                                         "QPushButton:hover { background: %1; border-radius: 4px; }")
                              .arg(TM().colors().bgHover));
    hLay->addWidget(menuBtn);
}

void Sidebar::setupGitGroupWidgetConnections(QTreeWidget *tree, std::shared_ptr<QString> sharedName, QPushButton *toggleBtn, QPushButton *menuBtn, QLabel *lbl, QWidget *wrapper)
{
    Q_ASSERT(tree != nullptr && sharedName != nullptr && toggleBtn != nullptr && menuBtn != nullptr && lbl != nullptr && wrapper != nullptr);

    connect(tree, &QTreeWidget::itemClicked, this, [this, tree](QTreeWidgetItem *it, int)
    {
        if (it == nullptr)
        {
            return;
        }
        if (it->parent() == nullptr)
        {
            it->setExpanded(!it->isExpanded());
            adjustGitTreeHeight(tree);
            return;
        }
        const QString p = it->data(0, Qt::UserRole).toString();
        if (!p.isEmpty())
        {
            emit driveClicked(p);
        }
    });
    connect(tree, &QTreeWidget::itemExpanded, this, [this, tree](){ adjustGitTreeHeight(tree); });
    connect(tree, &QTreeWidget::itemCollapsed, this, [this, tree](){ adjustGitTreeHeight(tree); });

    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn, lbl, sharedName, wrapper]()
    {
        handleGitGroupMenuClicked(menuBtn, lbl, sharedName, wrapper);
    });
}

void Sidebar::handleGitGroupMenuClicked(QPushButton *menuBtn, QLabel *lbl, std::shared_ptr<QString> sharedName, QWidget *wrapper)
{
    auto *m = new QMenu(this);
    Q_ASSERT(m != nullptr);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->setStyleSheet(TM().ssMenu());

    connect(m->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Gruppe umbenennen")), &QAction::triggered, this, [this, lbl, sharedName]()
    {
        handleGitGroupMenuRename(sharedName, lbl);
    });

    m->addSeparator();

    auto *upAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-up")), tr("Nach oben"));
    auto *downAct = m->addAction(QIcon::fromTheme(QStringLiteral("go-down")), tr("Nach unten"));
    Q_ASSERT(upAct != nullptr && downAct != nullptr);

    m->addSeparator();
    auto *delAct = m->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Gruppe löschen"));
    Q_ASSERT(delAct != nullptr);

    connect(upAct, &QAction::triggered, this, [this, wrapper]()
    {
        Q_ASSERT(m_contentLayout != nullptr);
        int idx = m_contentLayout->indexOf(wrapper);
        if (idx > 0)
        {
            m_contentLayout->removeWidget(wrapper);
            m_contentLayout->insertWidget(idx - 1, wrapper);
            saveGroupOrder();
        }
    });
    connect(downAct, &QAction::triggered, this, [this, wrapper]()
    {
        Q_ASSERT(m_contentLayout != nullptr);
        int idx = m_contentLayout->indexOf(wrapper);
        if (idx >= 0 && idx < m_contentLayout->count() - 2)
        {
            m_contentLayout->removeWidget(wrapper);
            m_contentLayout->insertWidget(idx + 1, wrapper);
            saveGroupOrder();
        }
    });
    connect(delAct, &QAction::triggered, this, [this, wrapper, sharedName]()
    {
        handleGroupMenuDelete(wrapper, sharedName);
    });

    m->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
}

void Sidebar::handleGitGroupMenuRename(std::shared_ptr<QString> sharedName, QLabel *lbl)
{
    bool ok;
    QString newName = sc_getText(this, tr("Gruppe umbenennen"), tr("Neuer Name:"), *sharedName);
    ok = !newName.isNull();
    if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == *sharedName)
    {
        return;
    }
    const QString oldName = *sharedName;
    *sharedName = newName.trimmed();
    lbl->setText(*sharedName);

    auto gs = Config::group("CustomGroups");
    QStringList grps = gs.readEntry("groups", QStringList());
    int idx = grps.indexOf(oldName);
    if (idx != -1)
    {
        grps[idx] = *sharedName;
        gs.writeEntry("groups", grps);
    }

    KConfigGroup oldGrp(gs.config(), gs.name() + "/group_" + oldName);
    KConfigGroup newGrp(gs.config(), gs.name() + "/group_" + *sharedName);
    newGrp.writeEntry("type", QStringLiteral("git"));
    oldGrp.deleteGroup();
    gs.config()->sync();
}

void Sidebar::createGitGroupWidget(const QString &name)
{
    auto *outerBox = new QWidget();
    Q_ASSERT(outerBox != nullptr);
    outerBox->setObjectName(QStringLiteral("groupBox"));
    outerBox->setStyleSheet(TM().ssBox());
    outerBox->setProperty("groupName", name);
    outerBox->setProperty("groupType", QStringLiteral("git"));

    auto *vbox = new QVBoxLayout(outerBox);
    Q_ASSERT(vbox != nullptr);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto *headerRow = new QWidget(outerBox);
    Q_ASSERT(headerRow != nullptr);
    auto *hLay = new QHBoxLayout(headerRow);
    Q_ASSERT(hLay != nullptr);
    auto *lbl = new QLabel(name);
    Q_ASSERT(lbl != nullptr);

    auto *menuBtn = new QPushButton();
    Q_ASSERT(menuBtn != nullptr);
    setupGitGroupWidgetHeader(headerRow, hLay, name, menuBtn);
    vbox->addWidget(headerRow);

    QWidget *listCont = nullptr;
    auto *tree = buildGitGroupTreeAndContainer(listCont);
    vbox->addWidget(listCont);

    auto *toggleBtn = buildGroupToggleBtn(listCont);
    vbox->addWidget(toggleBtn, 0, Qt::AlignCenter);

    auto *wrapper = new QWidget();
    Q_ASSERT(wrapper != nullptr);
    wrapper->setObjectName(QStringLiteral("groupWrapper"));
    wrapper->setStyleSheet(QStringLiteral("background:%1;").arg(TM().colors().bgMain));
    auto *wLay = new QVBoxLayout(wrapper);
    Q_ASSERT(wLay != nullptr);
    wLay->setContentsMargins(10, 2, 6, 2);
    wLay->setSpacing(0);
    wLay->addWidget(outerBox);

    auto sharedName = std::make_shared<QString>(name);
    setupGitGroupWidgetConnections(tree, sharedName, toggleBtn, menuBtn, lbl, wrapper);

    insertGroupWrapper(wrapper, nullptr);
}

QTreeWidget *Sidebar::buildGitGroupTreeAndContainer(QWidget *&listCont)
{
    listCont = new QWidget();
    Q_ASSERT(listCont != nullptr);
    listCont->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    auto *listLay = new QVBoxLayout(listCont);
    Q_ASSERT(listLay != nullptr);
    listLay->setContentsMargins(6, 0, 6, 0);
    listLay->setSpacing(0);

    auto *tree = new QTreeWidget();
    Q_ASSERT(tree != nullptr);
    tree->setObjectName(QStringLiteral("gitTree"));
    tree->setHeaderHidden(true);
    tree->setFrameShape(QFrame::NoFrame);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setIndentation(14);
    tree->setRootIsDecorated(true);
    tree->setItemsExpandable(true);
    tree->setExpandsOnDoubleClick(false);
    tree->setDragEnabled(false);
    tree->setDragDropMode(QAbstractItemView::NoDragDrop);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setFocusPolicy(Qt::StrongFocus);
    tree->setIconSize(QSize(Config::sidebarIconSize() + 16, Config::sidebarIconSize()));
    tree->setAnimated(false);
    tree->setUniformRowHeights(true);
    tree->setStyleSheet(
        QStringLiteral("QTreeWidget { background:transparent; outline:none; border:none; }"
                       "QTreeWidget::item { padding: 3px; border-radius:4px; font-size:13px; }"
                       "QTreeWidget::item:hover { background:%1; }"
                       "QTreeWidget::item:selected { background:%2; }")
            .arg(TM().colors().bgHover, TM().colors().bgSelect));
    listLay->addWidget(tree);
    return tree;
}
#endif

void Sidebar::saveGroupOrder()
{
    Q_ASSERT(m_contentLayout != nullptr);
    QStringList order;
    for (int i = 0; i < m_contentLayout->count(); ++i)
    {
        auto *w = m_contentLayout->itemAt(i) ? m_contentLayout->itemAt(i)->widget() : nullptr;
        if (w == nullptr || w->objectName() != QStringLiteral("groupWrapper"))
        {
            continue;
        }
        auto *ob = w->findChild<QWidget *>(QStringLiteral("groupBox"));
        if (ob != nullptr)
        {
            order << ob->property("groupName").toString();
        }
    }
    auto gs = Config::group("CustomGroups");
    gs.writeEntry("groups", order);
    gs.config()->sync();
}

void Sidebar::loadCustomGroups()
{
    auto s = Config::group("CustomGroups");
    const QStringList groups = s.readEntry("groups", QStringList());
    for (const QString &groupName : groups)
    {
        KConfigGroup g(s.config(), s.name() + "/group_" + groupName);
        const QString type = g.readEntry("type", QString());

#ifdef SC_PLUGIN_GIT
        if (type == QStringLiteral("git"))
        {
            createGitGroupWidget(groupName);
            continue;
        }
#else
        if (type == QStringLiteral("git"))
        {
            continue;
        }
#endif

        QListWidget *list = createGroupWidget(groupName, m_newGroupBox);
        if (groupName == QStringLiteral("Favoriten"))
        {
            m_favList = list;
        }

        int cnt = g.readEntry("size", 0);
        loadGroupItems(g, list, cnt);
        adjustListHeight(list);
    }
#ifdef SC_PLUGIN_GIT
    refreshGitSection();
#endif
}

void Sidebar::loadGroupItems(const KConfigGroup &g, QListWidget *list, int cnt)
{
    for (int i = 1; i <= cnt; ++i)
    {
        KConfigGroup itemG(g.config(), g.name() + "/" + QString::number(i));
        const QString path = itemG.readEntry("path", QString());
        const QString itemName = itemG.readEntry("name", QString());
        const QString customIco = itemG.readEntry("icon", QString());
        if (path.isEmpty())
        {
            continue;
        }

        QIcon ico;
        if (!customIco.isEmpty())
        {
            ico = QIcon::fromTheme(customIco);
        }
        else if (!path.startsWith(QLatin1Char('/')))
        {
            const QString scheme = QUrl::fromUserInput(path).scheme().toLower();
            ico = QIcon::fromTheme(
                scheme == QStringLiteral("gdrive") ? QStringLiteral("folder-gdrive") :
                scheme == QStringLiteral("smb") ? QStringLiteral("network-workgroup") :
                (scheme == QStringLiteral("sftp") || scheme == QStringLiteral("ssh")) ? QStringLiteral("network-connect") :
                scheme == QStringLiteral("mtp") ? QStringLiteral("multimedia-player") :
                scheme == QStringLiteral("bluetooth") ? QStringLiteral("bluetooth") :
                QStringLiteral("network-server"));
        }
        else
        {
            ico = QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(path)));
        }
        if (ico.isNull())
        {
            ico = QIcon::fromTheme(QStringLiteral("folder"));
        }
        
        QString displayName = itemName;

        auto *it = new QListWidgetItem(ico, displayName, list);
        Q_ASSERT(it != nullptr);
        it->setData(Qt::UserRole, path);
        it->setData(Qt::UserRole + 2, customIco);
    }
}

void Sidebar::addToGroup(const QString &groupName, QListWidget *list, const QString &path)
{
    if (path.isEmpty() || list == nullptr)
    {
        return;
    }
    for (int i = 0; i < list->count(); ++i)
    {
        if (list->item(i)->data(Qt::UserRole).toString() == path)
        {
            return;
        }
    }

    QUrl url(path);
    const QString scheme = url.scheme().toLower();
    
    QString name = determineAddedGroupName(path, url);
    QIcon ico = determineAddedGroupIcon(path, scheme);

    auto *it = new QListWidgetItem(ico, name, list);
    Q_ASSERT(it != nullptr);
    it->setData(Qt::UserRole, path);
    adjustListHeight(list);

    auto gs = Config::group("CustomGroups");
    KConfigGroup(gs.config(), gs.name() + "/group_" + groupName).deleteGroup();
    KConfigGroup gNew(gs.config(), gs.name() + "/group_" + groupName);
    gNew.writeEntry(QStringLiteral("size"), list->count());

    for (int i = 0; i < list->count(); ++i)
    {
        KConfigGroup itemG(gNew.config(), gNew.name() + "/" + QString::number(i + 1));
        itemG.writeEntry(QStringLiteral("path"), list->item(i)->data(Qt::UserRole).toString());
        itemG.writeEntry(QStringLiteral("name"), list->item(i)->text());
    }
    gs.config()->sync();
}

QString Sidebar::determineAddedGroupName(const QString &path, const QUrl &url)
{
    QString name = url.isLocalFile() ? QDir(path).dirName() : url.fileName();
    if (name.isEmpty())
    {
        name = path;
    }

    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("trash"))
    {
        return tr("Papierkorb");
    }
    if (scheme == QStringLiteral("recentdocuments"))
    {
        return tr("Zuletzt verwendet");
    }
    if (scheme == QStringLiteral("remote"))
    {
        return tr("Netzwerk");
    }
    if (path == QDir::homePath())
    {
        return tr("Persönlicher Ordner");
    }
    return name;
}

QIcon Sidebar::determineAddedGroupIcon(const QString &path, const QString &scheme)
{
    QIcon ico;
    if (scheme == QStringLiteral("trash"))
    {
        ico = QIcon::fromTheme(QStringLiteral("user-trash"));
        if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("user-trash-full"));
        if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("trash-empty"));
        if (ico.isNull()) ico = QIcon::fromTheme(QStringLiteral("trash"));
    }
    else if (scheme == QStringLiteral("recentdocuments"))
    {
        ico = QIcon::fromTheme(QStringLiteral("document-open-recent"));
    }
    else if (!path.startsWith(QLatin1Char('/')))
    {
        ico = QIcon::fromTheme(
            scheme == QStringLiteral("gdrive") ? QStringLiteral("folder-gdrive") :
            scheme == QStringLiteral("smb") ? QStringLiteral("network-workgroup") :
            (scheme == QStringLiteral("sftp") || scheme == QStringLiteral("ssh")) ? QStringLiteral("network-connect") :
            scheme == QStringLiteral("mtp") ? QStringLiteral("multimedia-player") :
            scheme == QStringLiteral("bluetooth") ? QStringLiteral("bluetooth") :
            scheme == QStringLiteral("afc") ? QStringLiteral("phone") :
            QStringLiteral("network-server"));
    }
    else
    {
        ico = QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(path)));
    }
    if (ico.isNull())
    {
        ico = QIcon::fromTheme(QStringLiteral("folder"));
    }
    return ico;
}

void Sidebar::addPlace(const QString &path)
{
    if (path.isEmpty() || m_favList == nullptr)
    {
        return;
    }
    addToGroup(QStringLiteral("Favoriten"), m_favList, path);
}

QStringList Sidebar::groupNames() const
{
    QStringList names;
    if (m_contentLayout == nullptr)
    {
        return names;
    }
    for (int i = 0; i < m_contentLayout->count(); ++i)
    {
        auto *item = m_contentLayout->itemAt(i);
        if (item == nullptr)
        {
            continue;
        }
        auto *wrapper = item->widget();
        if (wrapper == nullptr)
        {
            continue;
        }
        auto *box = wrapper->findChild<QWidget*>(QStringLiteral("groupBox"));
        if (box != nullptr)
        {
            QString name = box->property("groupName").toString();
            if (!name.isEmpty())
            {
                names << name;
            }
        }
    }
    return names;
}

void Sidebar::addPathToGroup(const QString &groupName, const QString &path)
{
    if (m_contentLayout == nullptr)
    {
        return;
    }
    for (int i = 0; i < m_contentLayout->count(); ++i)
    {
        auto *item = m_contentLayout->itemAt(i);
        if (item == nullptr)
        {
            continue;
        }
        auto *wrapper = item->widget();
        if (wrapper == nullptr)
        {
            continue;
        }
        auto *box = wrapper->findChild<QWidget*>(QStringLiteral("groupBox"));
        if (box != nullptr && box->property("groupName").toString() == groupName)
        {
            auto *list = box->findChild<QListWidget*>();
            if (list != nullptr)
            {
                addToGroup(groupName, list, path);
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
    p.setBrush(QColor(color));
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 14, 14);
    p.end();

    Q_ASSERT(m_tagList != nullptr);
    auto *it = new QListWidgetItem(QIcon(pix), name, m_tagList);
    Q_ASSERT(it != nullptr);
    it->setSizeHint(QSize(0, Config::sidebarRowHeight()));
    it->setData(Qt::UserRole, color);
    it->setData(Qt::UserRole + 1, fontFamily);
    if (!fontFamily.isEmpty())
    {
        it->setFont(QFont(fontFamily));
    }
}

void Sidebar::setupTags()
{
    Q_ASSERT(m_tagList != nullptr);
    m_tagList->clear();
    for (const auto &t : TagManager::instance().tags())
    {
        QString tagName = t.first;
        if (tagName == QStringLiteral("Wichtig")) tagName = tr("Wichtig");
        else if (tagName == QStringLiteral("Arbeit")) tagName = tr("Arbeit");
        else if (tagName == QStringLiteral("Schule")) tagName = tr("Schule");
        addTagItem(tagName, t.second, QString());
    }

    adjustListHeight(m_tagList);
    if (m_tagsBox != nullptr)
    {
        m_tagsBox->updateGeometry();
    }
    if (m_tagsWrap != nullptr)
    {
        m_tagsWrap->updateGeometry();
    }

    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it)
    {
        Q_ASSERT(it != nullptr);
        emit tagClicked(it->text());
        QTimer::singleShot(150, m_tagList, [this]()
        {
            m_tagList->clearSelection();
        });
    });

    m_tagList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tagList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos)
    {
        auto *item = m_tagList->itemAt(pos);
        if (item != nullptr)
        {
            showTagContextMenu(item, pos);
        }
    });
}

void Sidebar::showTagContextMenu(QListWidgetItem *item, const QPoint &pos)
{
    Q_ASSERT(item != nullptr && m_tagList != nullptr);
    QMenu menu(this);
    menu.setStyleSheet(TM().ssMenu());

    menu.addAction(QIcon::fromTheme(QStringLiteral("color-picker")), tr("Farbe ändern …"), this, [this, item]()
    {
        handleTagColorChange(item);
    });

    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), tr("Umbenennen …"), this, [this, item]()
    {
        handleTagRename(item);
    });

    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Löschen"), this, [this, item]()
    {
        handleTagDelete(item);
    });

    menu.exec(m_tagList->mapToGlobal(pos));
}

void Sidebar::handleTagColorChange(QListWidgetItem *item)
{
    QColorDialog dlg(QColor(item->data(Qt::UserRole).toString()), this);
    dlg.setWindowTitle(tr("Farbe wählen"));
    dlg.setOptions(QColorDialog::DontUseNativeDialog);
    dlg.setStyleSheet(TM().ssDialog());
    if (dlg.exec() == QDialog::Accepted)
    {
        QColor col = dlg.currentColor();
        if (col.isValid())
        {
            TagManager::instance().updateTag(item->text(), item->text(), col.name());
        }
    }
}

void Sidebar::handleTagRename(QListWidgetItem *item)
{
    bool ok;
    QString name = sc_getText(this, tr("Tag umbenennen"), tr("Name:"), item->text());
    ok = !name.isNull();
    if (ok && !name.isEmpty() && name != item->text())
    {
        TagManager::instance().updateTag(item->text(), name, item->data(Qt::UserRole).toString());
    }
}

void Sidebar::handleTagDelete(QListWidgetItem *item)
{
    TagManager::instance().removeTag(item->text());
}

void Sidebar::saveTags()
{
    // Keine Aktion erforderlich, da TagManager die Tags verwaltet und speichert.
}
