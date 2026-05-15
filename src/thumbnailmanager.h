#pragma once
#include <KIO/PreviewJob>
#include <QCache>
#include <QObject>
#include <QPixmap>
#include <QStringList>
#include <QUrl>

class ThumbnailManager : public QObject {
  Q_OBJECT
public:
  static ThumbnailManager &instance();

  // Liefert das Vorschaubild wenn vorhanden, sonst null
  QPixmap thumbnail(const QString &path, int size = 64);

  // Fordert ein Vorschaubild an (asynchron)
  void requestThumbnail(const QString &path, int size = 64);

signals:
  void thumbnailReady(const QString &path, const QPixmap &pix);

private:
  explicit ThumbnailManager(QObject *parent = nullptr);

  QCache<QString, QPixmap> m_cache;
  QSet<QString> m_pending;
};
