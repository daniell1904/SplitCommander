#pragma once

#include <QWidget>
#include <QScrollBar>
#include <QScrollArea>
#include <QResizeEvent>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPushButton>
#include <QMenu>

// --- Sidebar --- (Linke Navigationsleiste (Laufwerke, Gruppen, Tags))
class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

public slots:
    void updateDrives();
    void applyIconSizes();
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
    void buildGitSection(QVBoxLayout *parent);
    void buildTagsSection(QVBoxLayout *parent);
    void buildNewGroupFixedSection(QVBoxLayout *parent);
    void showDriveContextMenu(QListWidgetItem *item, const QPoint &pos);
    void buildFooter(QVBoxLayout *parent);

    // --- Laufwerke ---
    void setupDriveContextMenu();
    void loadUserPlaces();
    void saveToUserPlaces(const QString &url, const QString &name);
    void connectDriveList();

    // --- Gruppen ---
    void onNewGroupDialog();
    void loadCustomGroups();
    void saveGroupOrder();
    QListWidget *createGroupWidget(const QString &name, QWidget *beforeWidget);

    // --- Orte / Kontextmenü ---
    void showPlaceContextMenu(QListWidgetItem *item, QListWidget *list,
                               const QPoint &pos, const QString &groupName = {});

    // --- Tags ---
    void addTagItem(const QString &name, const QString &color, const QString &fontFamily = {});
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
    
    QListWidget *m_gitList       = nullptr;
    QWidget     *m_gitWrap       = nullptr;
    QWidget     *m_gitBox        = nullptr;
    
    class KDirLister *m_trashLister = nullptr;
};
