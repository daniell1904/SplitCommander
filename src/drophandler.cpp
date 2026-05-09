#include "drophandler.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <KIO/DropJob>
#include <KJobWidgets>
#include <QWidget>

DropHandler::DropHandler(QAbstractItemView *view, UrlResolver resolver, QObject *parent)
    : QObject(parent), m_view(view), m_resolver(std::move(resolver))
{
}

bool DropHandler::eventFilter(QObject *obj, QEvent *e)
{
    if (!m_view || obj != m_view->viewport()) return QObject::eventFilter(obj, e);

    if (e->type() == QEvent::DragEnter) {
        auto *de = static_cast<QDragEnterEvent*>(e);
        if (de->mimeData() && de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::DragMove) {
        auto *dm = static_cast<QDragMoveEvent*>(e);
        if (dm->mimeData() && dm->mimeData()->hasUrls()) {
            dm->acceptProposedAction();
            return true;
        }
    }
    if (e->type() == QEvent::Drop) {
        auto *de = static_cast<QDropEvent*>(e);
        if (!de->mimeData() || !de->mimeData()->hasUrls()) return false;

        QModelIndex idx = m_view->indexAt(de->position().toPoint());
        QUrl destUrl = m_resolver(idx);

        if (destUrl.isValid()) {
            auto *job = KIO::drop(de, destUrl);
            if (job) KJobWidgets::setWindow(job, m_view->window());
            return true;
        }
    }
    return QObject::eventFilter(obj, e);
}
