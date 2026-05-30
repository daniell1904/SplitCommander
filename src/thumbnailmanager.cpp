#include "thumbnailmanager.h"
#include <KIO/PreviewJob>
#include <QBuffer>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QThread>
#include <memory>
#include "config.h"

ThumbnailManager &ThumbnailManager::instance()
{
    static ThumbnailManager inst;
    Q_ASSERT(inst.m_cache.maxCost() == 1000);
    Q_ASSERT(inst.thread() == QThread::currentThread());
    return inst;
}

ThumbnailManager::ThumbnailManager(QObject *parent)
    : QObject(parent)
{
    m_cache.setMaxCost(1000);
}

QPixmap ThumbnailManager::thumbnail(const QString &path, int size)
{
    if (path.isEmpty() || size <= 0)
    {
        return QPixmap();
    }

    const QString key = QString::number(size) + path;
    if (m_cache.contains(key))
    {
        auto *obj = m_cache.object(key);
        Q_ASSERT(obj != nullptr);
        return *obj;
    }
    return QPixmap();
}

void ThumbnailManager::requestThumbnail(const QString &path, int size)
{
    if (path.isEmpty() || size <= 0)
    {
        return;
    }

    if (!Config::useThumbnails())
    {
        return;
    }

    const QFileInfo fi(path);
    if (fi.size() > static_cast<qint64>(Config::maxThumbnailSize()) * 1024 * 1024)
    {
        return;
    }

    const QString key = QString::number(size) + path;
    if (m_cache.contains(key) || m_pending.contains(key))
    {
        return;
    }

    m_pending.insert(key);

    const KFileItem item(QUrl::fromLocalFile(path));
    KFileItemList items;
    items << item;

    const QStringList plugins = KIO::PreviewJob::availablePlugins();
    auto *job = KIO::filePreview(items, QSize(size, size), &plugins);
    Q_ASSERT(job != nullptr);
    job->setIgnoreMaximumSize(true);

    connect(job, &KIO::PreviewJob::gotPreview, this,
            [this, path, key](const KFileItem &, const QPixmap &pix)
            {
                auto pixPtr = std::make_unique<QPixmap>(pix);
                Q_ASSERT(pixPtr != nullptr);
                m_cache.insert(key, pixPtr.release());
                m_pending.remove(key);
                emit thumbnailReady(path, pix);
            });

    connect(job, &KIO::PreviewJob::failed, this,
            [this, key](const KFileItem &)
            {
                m_pending.remove(key);
            });
}
