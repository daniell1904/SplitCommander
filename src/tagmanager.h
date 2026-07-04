#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QPair>
#include <QList>
#include <QMutex>
#include <KSharedConfig>
#include <QUrl>

class TagManager : public QObject
{
    Q_OBJECT
public:
    [[nodiscard]] static TagManager &instance();

    [[nodiscard]] const QList<QPair<QString, QString>>& tags() const;
    void addTag(const QString &name, const QString &color);
    void removeTag(const QString &name);
    void updateTag(const QString &oldName, const QString &newName, const QString &newColor);

    void setFileTag(const QString &path, const QString &tag);
    void setFileTags(const QStringList &paths, const QString &tag);
    void clearFileTag(const QString &path);
    void clearFileTags(const QStringList &paths);
    [[nodiscard]] QString fileTag(const QString &path) const;

    void setFileTag(const QUrl &url, const QString &tag);
    void clearFileTag(const QUrl &url);
    [[nodiscard]] QString fileTag(const QUrl &url) const;
    [[nodiscard]] QList<QUrl> urlsWithTag(const QString &tag) const;
    [[nodiscard]] QString tagColor(const QString &tagName) const;

    [[nodiscard]] QStringList filesWithTag(const QString &tag) const;

signals:
    void tagsChanged();
    void fileTagChanged(const QString &path);

private:
    TagManager();
    void load();
    void save();

    QList<QPair<QString, QString>> m_tags;
    QMap<QString, QString>         m_fileTags;
    KSharedConfigPtr              m_config;
    mutable QMutex                m_mutex;
};
