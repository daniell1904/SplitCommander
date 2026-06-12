#pragma once
#include <KDirLister>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

class MillerColumn : public QWidget
{
    Q_OBJECT
public:
    explicit MillerColumn(QWidget *parent = nullptr);
    ~MillerColumn() override = default;

    void populateDrives();
    void populateDir(const QString &path);
    void setActive(bool active);
    void refreshStyle();
    
    [[nodiscard]] const QString& path() const;
    [[nodiscard]] QListWidget* list();
    
    void addHeaderWidget(QWidget *w);

signals:
    void entryClicked(const QString &path, MillerColumn *self);
    void activated(MillerColumn *self);
    void headerClicked(const QString &path);
    void editPathRequested();
    void teardownRequested(const QString &udi);
    void setupRequested(const QString &udi);
    void removeFromPlacesRequested(const QString &url);
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);

private slots:
    void showContextMenu(const QPoint &pos);

private:
    void initLister();
    void initHeader(class QVBoxLayout *lay);
    void initListWidget(class QVBoxLayout *lay);
    void setupEventFiltersAndDelegates(class QVBoxLayout *lay);
    void connectListSignals();

    void addDrivesOpenActions(class QMenu &menu, const QString &itemPath);
    void addDrivesNetworkPlacesActions(class QMenu &menu, const QString &itemPath, QListWidgetItem *it);
    void addDrivesDeviceActions(class QMenu &menu, const QString &udi);

    void handleDrivesContextMenu(QListWidgetItem *it, const QString &itemPath, const QPoint &pos);
    void handleDirContextMenu(const QString &itemPath, const QPoint &pos);

    QListWidget *m_list = nullptr;
    QPushButton *m_colLabel = nullptr;
    QHBoxLayout *m_headerLay = nullptr;
    bool         m_active    = false;
    QString m_path;
    KDirLister *m_lister = nullptr;
};
