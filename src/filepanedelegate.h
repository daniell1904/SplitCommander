#pragma once
#include <QStyledItemDelegate>
#include <QColor>
#include <QIcon>

class FilePaneDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    bool focused   = true;
    int  rowHeight = 26;
    int  fontSize  = 11;

    explicit FilePaneDelegate(QObject *par = nullptr);
    void  paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;

    static QColor   ageColor(qint64 secs);
    static QString  formatAge(qint64 secs);
};

class ScaledIconDelegate : public QStyledItemDelegate {
public:
    explicit ScaledIconDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
    void initStyleOption(QStyleOptionViewItem *opt, const QModelIndex &idx) const override {
        QStyledItemDelegate::initStyleOption(opt, idx);
        QIcon ico = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
        if (!ico.isNull()) {
            int sz = opt->decorationSize.width();
            QPixmap pm = ico.pixmap(QSize(sz, sz));
            if (!pm.isNull())
                opt->icon = QIcon(pm);
        }
    }
};
