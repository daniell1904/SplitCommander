#pragma once

#include <QWidget>
#include <QScrollBar>
#include <QScrollArea>
#include <QResizeEvent>
#include <QListWidget>
#include <QTreeWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <memory>

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);
    ~Sidebar() override = default;

    void showLayoutMenu(QWidget *anchor = nullptr);

public slots:
    void updateDrives();
    void applyIconSizes();
#ifdef SC_PLUGIN_GIT
    void refreshGitSection();
#endif
    void renameNetworkPlace(const QString &path, const QString &newName);
    const QStringList& gdriveAccounts() const { return m_gdriveAccounts; }
    void addPlace(const QString &path);
    void addToGroup(const QString &groupName, QListWidget *list, const QString &path);
    QStringList groupNames() const;
    void addPathToGroup(const QString &groupName, const QString &path);
    void addNetworkPlace(const QString &path, const QString &name);
    void setupTags();

signals:
    void driveClicked(const QString &path);
    void driveClickedRight(const QString &path);
    void driveClickedLeft(const QString &path);
    void drivesChanged();
    void removeFromPlacesRequested(const QString &url);
    void unmountRequested(const QString &path);
    void teardownRequested(const QString &udi);
    void addCurrentPathToPlaces();
    void requestActivePath(QString *outPath);
    void layoutChangeRequested(int mode);
    void tagClicked(const QString &tagName);
    void settingsChanged();
    void hiddenFilesChanged(bool show);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    // --- UI-Aufbau ---
    void buildLogo(QVBoxLayout *parent);
    void buildDrivesSection(QVBoxLayout *parent);
    void buildGroupsSection(QVBoxLayout *parent);
    void buildTagsSection(QVBoxLayout *parent);
    void buildNewGroupFixedSection(QVBoxLayout *parent);
    void showDriveContextMenu(QListWidgetItem *item, const QPoint &pos);
    void buildFooter(QVBoxLayout *parent);

    // --- UI-Aufbau Hilfsfunktionen (NASA Rule 4) ---
    void setupLogoIcon(QHBoxLayout *lay);
    void setupLayoutMenuModes(class QButtonGroup *grp, QDialog *popup, int current);
    void setupDrivesList(QVBoxLayout *listLay);
    void setupNetList(QVBoxLayout *netWLay);
    void showNetListContextMenu(const QPoint &pos);
    void showDrivesMenu(QPushButton *menuBtn, QLabel *lbl);
    void handleDrivesMenuAction(const QString &actionName, QPushButton *menuBtn, QLabel *lbl);
    
    // --- Drive Kontextmenü Hilfsfunktionen ---
    void showDriveContextMenuSolid(QMenu &menu, const QString &path, const QString &udi);
    void showDriveContextMenuNonSolid(QMenu &menu, const QString &path);
    void showDriveContextMenuPinned(QMenu &menu, const QString &path, const QString &name);
    void showDriveContextMenuShortcut(QMenu &menu, const QString &path, const QString &);

    // --- Gruppen ---
    void onNewGroupDialog();
    void loadCustomGroups();
    void saveGroupOrder();
    QListWidget *createGroupWidget(const QString &name, QWidget *beforeWidget);
#ifdef SC_PLUGIN_GIT
    void createGitGroupWidget(const QString &name);
#endif

    // --- Gruppen Hilfsfunktionen (NASA Rule 4) ---
    void handleNewGroupDialogAccepted(int checkedId, const QString &grpName, class QButtonGroup *btnGrp);
    void setupGroupWidgetHeader(QWidget *headerRow, QHBoxLayout *hLay, const QString &name, QPushButton *menuBtn, QPushButton *addBtn);
    void setupGroupWidgetConnections(QListWidget *list, std::shared_ptr<QString> sharedName, QPushButton *toggleBtn, QPushButton *addBtn, QPushButton *menuBtn, QLabel *lbl, QWidget *outerBox, QWidget *wrapper);
    void handleGroupMenuRename(std::shared_ptr<QString> sharedName, QLabel *lbl);
    void handleGroupMenuDelete(QWidget *wrapper, std::shared_ptr<QString> sharedName);
#ifdef SC_PLUGIN_GIT
    void setupGitGroupWidgetHeader(QWidget *headerRow, QHBoxLayout *hLay, const QString &name, QPushButton *menuBtn);
    void setupGitGroupWidgetConnections(QTreeWidget *tree, std::shared_ptr<QString> sharedName, QPushButton *toggleBtn, QPushButton *menuBtn, QLabel *lbl, QWidget *wrapper);
#endif

    // --- Laufwerke ---
    void setupDriveContextMenu();
    void loadUserPlaces();
    void saveToUserPlaces(const QString &url, const QString &name);
    void connectDriveList();

    // --- Orte / Kontextmenü ---
    void showPlaceContextMenu(QListWidgetItem *item, QListWidget *list, const QPoint &pos, const QString &groupName = {});

    // --- Tags ---
    void addTagItem(const QString &name, const QString &color, const QString &fontFamily = {});
    void showTagContextMenu(QListWidgetItem *item, const QPoint &pos);
    void saveTags();

    // --- Hilfsfunktionen ---
    static void adjustListHeight(QListWidget *list);
    void onTrashChanged();

    // --- Member-Variablen ---
    QListWidget *m_driveList     = nullptr;
    QStringList  m_gdriveAccounts;

    QScrollArea *m_scrollArea    = nullptr;
    QScrollBar  *m_overlayBar    = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QWidget     *m_newGroupBox   = nullptr;
    QListWidget *m_favList       = nullptr;
    QListWidget *m_netList       = nullptr;
    QWidget     *m_netBox        = nullptr;

    QListWidget *m_tagList       = nullptr;
    QWidget     *m_tagsWrap      = nullptr;
    QWidget     *m_tagsBox       = nullptr;
    
    class KDirLister *m_trashLister = nullptr;
};
