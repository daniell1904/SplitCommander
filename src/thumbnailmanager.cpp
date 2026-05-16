#include "thumbnailmanager.h"
#include <KIO/PreviewJob>
#include <QBuffer>
#include <QMimeDatabase>
#include <QFileInfo>
#include "config.h"

ThumbnailManager &ThumbnailManager::instance() {
  static ThumbnailManager inst;
  return inst;
}

ThumbnailManager::ThumbnailManager(QObject *parent) : QObject(parent) {
  m_cache.setMaxCost(1000);
}

QPixmap ThumbnailManager::thumbnail(const QString &path, int size) {
  QString key = QString::number(size) + path;
  if (m_cache.contains(key))
    return *m_cache.object(key);
  return QPixmap();
}

void ThumbnailManager::requestThumbnail(const QString &path, int size) {
  if (path.isEmpty() || !Config::useThumbnails())
    return;

  QFileInfo fi(path);
  if (fi.size() > (qint64)Config::maxThumbnailSize() * 1024 * 1024)
    return;

  QString key = QString::number(size) + path;
  if (m_cache.contains(key))
    return;
  if (m_pending.contains(key))
    return;

  m_pending.insert(key);

  KFileItem item(QUrl::fromLocalFile(path));
  KFileItemList items;
  items << item;

  QStringList plugins = KIO::PreviewJob::availablePlugins();
  auto *job = KIO::filePreview(items, QSize(size, size), &plugins);
  job->setIgnoreMaximumSize(true);

  connect(job, &KIO::PreviewJob::gotPreview, this,
          [this, path, key](const KFileItem &, const QPixmap &pix) {
            m_cache.insert(key, new QPixmap(pix));
            m_pending.remove(key);
            emit thumbnailReady(path, pix);
          });

  connect(job, &KIO::PreviewJob::failed, this,
          [this, key](const KFileItem &) { m_pending.remove(key); });
}
