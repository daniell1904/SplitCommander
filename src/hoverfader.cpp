#include "hoverfader.h"

HoverFader::HoverFader(QListWidget *view, QObject *parent)
    : QObject(parent)
    , m_view(view)
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(view->viewport() != nullptr);

    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setAttribute(Qt::WA_Hover);
    
    m_anim = new QVariantAnimation(this);
    Q_ASSERT(m_anim != nullptr);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setDuration(16);
    m_anim->setLoopCount(-1);
    
    connect(m_anim, &QVariantAnimation::valueChanged, this, &HoverFader::tick);
}

double HoverFader::opacity(int row) const
{
    Q_ASSERT(row >= -1);
    Q_ASSERT(m_view != nullptr);
    return m_opacities.value(row, 0.0);
}

bool HoverFader::eventFilter(QObject *obj, QEvent *ev)
{
    Q_ASSERT(obj != nullptr);
    Q_ASSERT(ev != nullptr);
    Q_ASSERT(m_view != nullptr);

    if (obj == m_view->viewport())
    {
        if (ev->type() == QEvent::HoverMove || ev->type() == QEvent::MouseMove)
        {
            auto *me = static_cast<QMouseEvent*>(ev);
            Q_ASSERT(me != nullptr);
            const QPoint pos = me->pos();
            const QModelIndex idx = m_view->indexAt(pos);
            const int row = idx.isValid() ? idx.row() : -1;
            if (row != m_hoveredRow)
            {
                m_hoveredRow = row;
                if (m_anim != nullptr && m_anim->state() != QAbstractAnimation::Running)
                {
                    m_anim->start();
                }
            }
        }
        else if (ev->type() == QEvent::HoverLeave || ev->type() == QEvent::Leave)
        {
            m_hoveredRow = -1;
            if (m_anim != nullptr && m_anim->state() != QAbstractAnimation::Running)
            {
                m_anim->start();
            }
        }
    }
    return QObject::eventFilter(obj, ev);
}

void HoverFader::tick()
{
    Q_ASSERT(m_view != nullptr);
    Q_ASSERT(m_anim != nullptr);

    bool needsUpdate = false;
    bool anyAnimating = false;
    
    for (int row = 0; row < m_view->count(); ++row)
    {
        const double current = m_opacities.value(row, 0.0);
        const double target = (row == m_hoveredRow) ? 1.0 : 0.0;
        
        if (qAbs(current - target) > 0.01)
        {
            double nextVal = current + ((target > current) ? 0.15 : -0.10);
            nextVal = qBound(0.0, nextVal, 1.0);
            m_opacities[row] = nextVal;
            needsUpdate = true;
            anyAnimating = true;
        }
        else if (current != target)
        {
            m_opacities[row] = target;
            needsUpdate = true;
        }
    }
    
    if (needsUpdate)
    {
        m_view->viewport()->update();
    }
    if (!anyAnimating)
    {
        m_anim->stop();
    }
}
