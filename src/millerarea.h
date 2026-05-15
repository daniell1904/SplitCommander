#pragma once
#include "millercolumn.h"
// --- MillerArea ---
class MillerArea : public QWidget {
    Q_OBJECT
public:
    explicit MillerArea(QWidget *parent = nullptr);
    void init();
    void refreshDrives();
    void navigateTo(const QString &path, bool clearForward = true);
    void refresh();
    QString activePath() const;
    QList<QUrl> selectedUrls() const;
    void setFocused(bool f);
    const QList<MillerColumn*>& cols() const { return m_cols; }
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
protected:
    void resizeEvent(QResizeEvent *e) override;
private:
    void appendColumn(const QString &path);
    void updateVisibleColumns();
    void trimAfter(MillerColumn *col);
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
};
