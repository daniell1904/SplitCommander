#pragma once
#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QListWidget>
#include <QVariantAnimation>
#include <QMap>

class HoverFader : public QObject {
    Q_OBJECT
public:
    explicit HoverFader(QListWidget *view, QObject *parent = nullptr)
        : QObject(parent), m_view(view) {
        m_view->viewport()->installEventFilter(this);
        m_view->viewport()->setAttribute(Qt::WA_Hover);
        m_anim = new QVariantAnimation(this);
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->setDuration(16); // 60fps tick
        m_anim->setLoopCount(-1);
        connect(m_anim, &QVariantAnimation::valueChanged, this, &HoverFader::tick);
    }

    double opacity(int row) const { return m_opacities.value(row, 0.0); }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj == m_view->viewport()) {
            if (ev->type() == QEvent::HoverMove || ev->type() == QEvent::MouseMove) {
                QPoint pos = static_cast<QMouseEvent*>(ev)->pos();
                QModelIndex idx = m_view->indexAt(pos);
                int row = idx.isValid() ? idx.row() : -1;
                if (row != m_hoveredRow) {
                    m_hoveredRow = row;
                    if (m_anim->state() != QAbstractAnimation::Running) m_anim->start();
                }
            } else if (ev->type() == QEvent::HoverLeave || ev->type() == QEvent::Leave) {
                m_hoveredRow = -1;
                if (m_anim->state() != QAbstractAnimation::Running) m_anim->start();
            }
        }
        return QObject::eventFilter(obj, ev);
    }

private:
    void tick() {
        bool needsUpdate = false;
        bool anyAnimating = false;
        
        for (int row = 0; row < m_view->count(); ++row) {
            double current = m_opacities.value(row, 0.0);
            double target = (row == m_hoveredRow) ? 1.0 : 0.0;
            
            if (qAbs(current - target) > 0.01) {
                current += (target > current) ? 0.15 : -0.10;
                current = qBound(0.0, current, 1.0);
                m_opacities[row] = current;
                needsUpdate = true;
                anyAnimating = true;
            } else if (current != target) {
                m_opacities[row] = target;
                needsUpdate = true;
            }
        }
        
        if (needsUpdate) {
            m_view->viewport()->update();
        }
        if (!anyAnimating) {
            m_anim->stop();
        }
    }

    QListWidget *m_view;
    QVariantAnimation *m_anim;
    int m_hoveredRow = -1;
    QMap<int, double> m_opacities;
};
