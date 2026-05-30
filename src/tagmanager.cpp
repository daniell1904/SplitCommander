#include "tagmanager.h"
#include <KConfigGroup>

static QString encodeKey(const QString &path)
{
    return QString::fromLatin1(path.toUtf8().toHex());
}

static QString decodeKey(const QString &key)
{
    return QString::fromUtf8(QByteArray::fromHex(key.toLatin1()));
}

TagManager &TagManager::instance()
{
    static TagManager inst;
    Q_ASSERT(inst.m_config != nullptr);
    return inst;
}

const QList<QPair<QString, QString>>& TagManager::tags() const
{
    return m_tags;
}

TagManager::TagManager()
    : QObject(nullptr)
{
    m_config = KSharedConfig::openConfig(QStringLiteral("splitcommanderrc"));
    Q_ASSERT(m_config != nullptr);
    load();
}

void TagManager::load()
{
    QMutexLocker lock(&m_mutex);
    Q_ASSERT(m_config != nullptr);
    
    auto s = m_config->group(QStringLiteral("Tags"));
    auto tagsG = s.group(QStringLiteral("tags"));
    int tcount = tagsG.readEntry(QStringLiteral("size"), 0);
    if (tcount > 100)
    {
        tcount = 100;
    }

    m_tags.clear();
    if (tcount == 0)
    {
        m_tags = {{QStringLiteral("Wichtig"), QStringLiteral("#bf616a")}, 
                  {QStringLiteral("Arbeit"), QStringLiteral("#5e81ac")}, 
                  {QStringLiteral("Privat"), QStringLiteral("#a3be8c")}};
    }
    else
    {
        for (int i = 1; i <= tcount; ++i)
        {
            auto tG = tagsG.group(QString::number(i));
            m_tags.append(qMakePair(tG.readEntry(QStringLiteral("name"), QString()),
                                    tG.readEntry(QStringLiteral("color"), QString())));
        }
    }

    m_fileTags.clear();
    auto fileTagsG = s.group(QStringLiteral("fileTags"));
    for (const QString &key : fileTagsG.keyList())
    {
        m_fileTags[decodeKey(key)] = fileTagsG.readEntry(key, QString());
    }
}

void TagManager::save()
{
    QMutexLocker lock(&m_mutex);
    Q_ASSERT(m_config != nullptr);
    
    auto s = m_config->group(QStringLiteral("Tags"));
    
    s.group(QStringLiteral("tags")).deleteGroup();
    auto tagsG = s.group(QStringLiteral("tags"));
    tagsG.writeEntry(QStringLiteral("size"), m_tags.size());
    for (int i = 0; i < m_tags.size(); ++i)
    {
        auto tG = tagsG.group(QString::number(i + 1));
        tG.writeEntry(QStringLiteral("name"),  m_tags[i].first);
        tG.writeEntry(QStringLiteral("color"), m_tags[i].second);
    }
 
    s.group(QStringLiteral("fileTags")).deleteGroup();
    auto fileTagsG = s.group(QStringLiteral("fileTags"));
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
    {
        fileTagsG.writeEntry(encodeKey(it.key()), it.value());
    }
 
    m_config->sync();
}

void TagManager::addTag(const QString &name, const QString &color)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!color.isEmpty());

    {
        QMutexLocker lock(&m_mutex);
        for (const auto &t : m_tags)
        {
            if (t.first == name)
            {
                return;
            }
        }
        m_tags.append(qMakePair(name, color));
    }
    save();
    emit tagsChanged();
}

void TagManager::removeTag(const QString &name)
{
    Q_ASSERT(!name.isEmpty());

    {
        QMutexLocker lock(&m_mutex);
        m_tags.removeIf([&](const QPair<QString, QString> &t)
        {
            return t.first == name;
        });
        for (auto it = m_fileTags.begin(); it != m_fileTags.end(); )
        {
            if (it.value() == name)
            {
                it = m_fileTags.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    save();
    emit tagsChanged();
}

void TagManager::clearFileTag(const QString &path)
{
    Q_ASSERT(!path.isEmpty());
    clearFileTags({path});
}

void TagManager::setFileTag(const QString &path, const QString &tag)
{
    Q_ASSERT(!path.isEmpty());
    Q_ASSERT(!tag.isEmpty());
    setFileTags({path}, tag);
}

void TagManager::setFileTags(const QStringList &paths, const QString &tag)
{
    Q_ASSERT(!tag.isEmpty());
    {
        QMutexLocker lock(&m_mutex);
        for (const QString &path : paths)
        {
            Q_ASSERT(!path.isEmpty());
            m_fileTags[path] = tag;
        }
    }
    save();
    for (const QString &path : paths)
    {
        emit fileTagChanged(path);
    }
}

void TagManager::clearFileTags(const QStringList &paths)
{
    {
        QMutexLocker lock(&m_mutex);
        for (const QString &path : paths)
        {
            Q_ASSERT(!path.isEmpty());
            m_fileTags.remove(path);
        }
    }
    save();
    for (const QString &path : paths)
    {
        emit fileTagChanged(path);
    }
}

QString TagManager::fileTag(const QString &path) const
{
    Q_ASSERT(!path.isEmpty());
    QMutexLocker lock(&m_mutex);
    return m_fileTags.value(path);
}

QString TagManager::tagColor(const QString &tagName) const
{
    Q_ASSERT(!tagName.isEmpty());
    QMutexLocker lock(&m_mutex);
    for (const auto &t : m_tags)
    {
        if (t.first == tagName)
        {
            return t.second;
        }
    }
    return QString();
}

QStringList TagManager::filesWithTag(const QString &tag) const
{
    Q_ASSERT(!tag.isEmpty());
    QMutexLocker lock(&m_mutex);
    QStringList result;
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
    {
        if (it.value() == tag)
        {
            result << it.key();
        }
    }
    return result;
}

static QString keyForUrl(const QUrl &url)
{
    if (url.isLocalFile())
    {
        return url.toLocalFile();
    }
    return url.toString(QUrl::NormalizePathSegments);
}

void TagManager::setFileTag(const QUrl &url, const QString &tag)
{
    Q_ASSERT(url.isValid());
    Q_ASSERT(!tag.isEmpty());
    setFileTag(keyForUrl(url), tag);
}

void TagManager::clearFileTag(const QUrl &url)
{
    Q_ASSERT(url.isValid());
    clearFileTag(keyForUrl(url));
}

QString TagManager::fileTag(const QUrl &url) const
{
    Q_ASSERT(url.isValid());
    return fileTag(keyForUrl(url));
}

QList<QUrl> TagManager::urlsWithTag(const QString &tag) const
{
    Q_ASSERT(!tag.isEmpty());
    QMutexLocker lock(&m_mutex);
    QList<QUrl> result;
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
    {
        if (it.value() == tag)
        {
            const QString &key = it.key();
            if (key.contains(QStringLiteral("://")))
            {
                result << QUrl(key);
            }
            else
            {
                result << QUrl::fromLocalFile(key);
            }
        }
    }
    return result;
}
