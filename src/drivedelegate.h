#pragma once
#include <QStyledItemDelegate>

#include <QVariantAnimation>
#include <QWidget>
#include "hoverfader.h"

class DriveDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DriveDelegate(bool showBars = true, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_showBars(showBars) {
        m_anim = new QVariantAnimation(this);
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->setDuration(600);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this, parent](const QVariant& value) {
            m_animProgress = value.toDouble();
            if (auto* w = qobject_cast<QWidget*>(parent)) w->update();
        });
        m_anim->start(QAbstractAnimation::KeepWhenStopped);
    }

    void  paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;
    
    void setHoverFader(HoverFader *fader) { m_fader = fader; }
    void restartAnimation() {
        if (m_anim) m_anim->start(QAbstractAnimation::KeepWhenStopped);
    }

private:
    bool m_showBars;
    double m_animProgress = 0.0;
    HoverFader *m_fader = nullptr;
    QVariantAnimation *m_anim = nullptr;
};
