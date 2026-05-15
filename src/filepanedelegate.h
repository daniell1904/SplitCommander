#pragma once
#include <QStyledItemDelegate>
#include <QColor>
#include <QIcon>

class FilePaneDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    bool focused   = true;
    int  rowHeight = 36;
    int  fontSize  = 13;

    explicit FilePaneDelegate(QObject *par = nullptr);
    void  paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;

    static QColor   ageColor(qint64 secs);
    static QString  formatAge(qint64 secs);
};

class ScaledIconDelegate : public QStyledItemDelegate {
public:
    explicit ScaledIconDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
    void initStyleOption(QStyleOptionViewItem *opt, const QModelIndex &idx) const override;
};
