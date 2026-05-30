#include "drophandler.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <KIO/DropJob>
#include <KJobWidgets>
#include <QWidget>

DropHandler::DropHandler(QAbstractItemView *view, UrlResolver resolver, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_resolver(std::move(resolver))
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(m_resolver != nullptr);
}

bool DropHandler::eventFilter(QObject *obj, QEvent *e)
{
    Q_ASSERT(obj != nullptr);
    Q_ASSERT(e != nullptr);

    if (m_view == nullptr || obj != m_view->viewport())
    {
        return QObject::eventFilter(obj, e);
    }

    if (e->type() == QEvent::DragEnter)
    {
        auto *de = static_cast<QDragEnterEvent*>(e);
        Q_ASSERT(de != nullptr);
        if (de->mimeData() != nullptr && de->mimeData()->hasUrls())
        {
            de->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::DragMove)
    {
        auto *dm = static_cast<QDragMoveEvent*>(e);
        Q_ASSERT(dm != nullptr);
        if (dm->mimeData() != nullptr && dm->mimeData()->hasUrls())
        {
            dm->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::Drop)
    {
        auto *de = static_cast<QDropEvent*>(e);
        Q_ASSERT(de != nullptr);
        if (de->mimeData() == nullptr || !de->mimeData()->hasUrls())
        {
            return false;
        }

        const QModelIndex idx = m_view->indexAt(de->position().toPoint());
        const QUrl destUrl = m_resolver(idx);

        if (destUrl.isValid())
        {
            auto *job = KIO::drop(de, destUrl);
            if (job != nullptr)
            {
                KJobWidgets::setWindow(job, m_view->window());
            }
            return true;
        }
    }
    return QObject::eventFilter(obj, e);
}
