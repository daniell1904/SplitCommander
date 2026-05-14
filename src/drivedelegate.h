#pragma once
#include <QStyledItemDelegate>

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
