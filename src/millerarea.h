#pragma once
#include "millercolumn.h"
#include <Solid/StorageAccess>

class MillerArea : public QWidget
{
    Q_OBJECT
public:
    explicit MillerArea(QWidget *parent = nullptr);
    ~MillerArea() override = default;

    void init();
    void refreshDrives();
    void navigateTo(const QString &path, bool clearForward = true);
    void refresh();
    void setCollapsed(bool collapsed, const QString &fullPath = QString());
    
    [[nodiscard]] QString activePath() const;
    [[nodiscard]] QList<QUrl> selectedUrls() const;
    
    void setFocused(bool f);
    
    [[nodiscard]] const QList<MillerColumn*>& cols() const;

signals:
    void pathChanged(const QString &path);
    void focusRequested();
    void headerClicked(const QString &path);
    void kioPathRequested(const QString &path);
    void openInLeft(const QString &path);
    void openInRight(const QString &path);
    void propertiesRequested(const QString &path);
    void teardownRequested(const QString &udi);
    void removeFromPlacesRequested(const QString &url);
    void drivesChanged();
    void editPathRequested();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void appendColumn(const QString &path);
    void updateVisibleColumns();
    void trimAfter(MillerColumn *col);

    void initColumnSignals(MillerColumn *col);
    void connectEntryClicked(MillerColumn *col);
    void connectActivationAndHeaders(MillerColumn *col);
    void connectActionSignals(MillerColumn *col);
    
    void handleDeviceSetup(Solid::StorageAccess *acc);
    void selectAndNavigateDrive(const QUrl &startUrl, QString &drivePath);
    void buildAndAppendSegments(const QStringList &segments, int startIdx, const QString &targetDir);
    
    void clearStrips();
    void buildStrips(int stripCount);
    void clearSeparators();
    void applyColumnVisibility(int n, int stripCount);
    
    bool navigateToDrives(const QString &path);
    QUrl prepareNavigateUrl(const QString &path);
    void computeNavigateSegments(const QUrl &startUrl, const QString &drivePath, QStringList &segments, int &startIdx);

    QList<MillerColumn*>  m_cols;
    MillerColumn         *m_activeCol     = nullptr;
    QHBoxLayout          *m_rowLayout     = nullptr;
    QWidget              *m_rowWidget     = nullptr;
    QHBoxLayout          *m_colLayout     = nullptr;
    QWidget              *m_colContainer  = nullptr;
    QList<QFrame*>        m_colSeparators;
    QList<QWidget*>       m_strips;
    QFrame               *m_stripDivider  = nullptr;
    bool                  m_focused       = false;
    bool                  m_collapsed     = false;
};
