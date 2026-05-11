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

// --- GroupDragHandle --- (Hilfsklasse für das Umordnen von Gruppen per Drag & Drop)
class GroupDragHandle : public QWidget {
    Q_OBJECT
public:
    explicit GroupDragHandle(QWidget *outerBox, QWidget *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void         showIndicator(int index);
    void         hideIndicator();
    QVBoxLayout *parentLayout() const;
    int          layoutIndex() const;

    QWidget *m_outerBox     = nullptr;
    QWidget *m_indicator    = nullptr;
    bool     m_dragging     = false;
    int      m_startY       = 0;
    int      m_dragIndex    = 0;
    int      m_currentIndex = 0;
};

// --- DriveDelegate --- (Zeichnet Laufwerke und Netzwerkordner (inklusive Speicherplatz-Balken))
class DriveDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DriveDelegate(bool showBars = true, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_showBars(showBars) {}

    void  paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;

private:
    bool m_showBars;
};

// --- Sidebar --- (Linke Navigationsleiste (Laufwerke, Gruppen, Tags))
class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);

public slots:
    void updateDrives();
    void renameNetworkPlace(const QString &path, const QString &newName);
    const QStringList& gdriveAccounts() const { return m_gdriveAccounts; }
    void setupPlaces();
    void addPlace(const QString &path);
    void addToGroup(const QString &groupName, QListWidget *list, const QString &path);
    void setupRemotes();
    void setupTags();

signals:
    void driveClicked(const QString &path);
    void driveClickedRight(const QString &path);
    void driveClickedLeft(const QString &path);
    void drivesChanged();
    void removeFromPlacesRequested(const QString &url);
    void unmountRequested(const QString &path);
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
    void buildFooter(QVBoxLayout *parent);

    // --- Laufwerke ---
    void setupDriveContextMenu();
    void loadGDriveAccountsAsync();
    void loadUserPlaces();
    void connectDriveList();

    // --- Gruppen ---
    void onNewGroupDialog();
    void loadCustomGroups();
    void saveGroupOrder();
    QListWidget *createGroupWidget(const QString &name, QWidget *beforeWidget);

    // --- Orte / Kontextmenü ---
    void showPlaceContextMenu(QListWidgetItem *item, QListWidget *list,
                               const QPoint &pos, const QString &groupName = {});
    void savePlaces(QListWidget **list);

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
    class KDirLister *m_trashLister = nullptr;
};
