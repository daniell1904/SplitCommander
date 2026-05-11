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


TagManager::TagManager()
    : QObject(nullptr)
{
    m_config = KSharedConfig::openConfig("splitcommanderrc");
    load();
}

void TagManager::load()
{
    QMutexLocker lock(&m_mutex);
    auto s = m_config->group("Tags");
    auto tagsG = s.group("tags");
    int tcount = tagsG.readEntry("size", 0);
    if (tcount > 100) tcount = 100;

    m_tags.clear();
    if (tcount == 0) {
        m_tags = {{"Wichtig", "#bf616a"}, {"Arbeit", "#5e81ac"}, {"Privat", "#a3be8c"}};
    } else {
        for (int i = 1; i <= tcount; ++i) {
            auto tG = tagsG.group(QString::number(i));
            m_tags.append(qMakePair(tG.readEntry("name", QString()),
                                    tG.readEntry("color", QString())));
        }
    }


    m_fileTags.clear();
    auto fileTagsG = s.group("fileTags");
    for (const QString &key : fileTagsG.keyList())
        m_fileTags[decodeKey(key)] = fileTagsG.readEntry(key, QString());
}

void TagManager::save()
{
    QMutexLocker lock(&m_mutex);
    auto s = m_config->group("Tags");
    
    s.group("tags").deleteGroup();
    auto tagsG = s.group("tags");
    tagsG.writeEntry("size", m_tags.size());
    for (int i = 0; i < m_tags.size(); ++i) {
        auto tG = tagsG.group(QString::number(i + 1));
        tG.writeEntry("name",  m_tags[i].first);
        tG.writeEntry("color", m_tags[i].second);
    }
 
    s.group("fileTags").deleteGroup();
    auto fileTagsG = s.group("fileTags");
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
        fileTagsG.writeEntry(encodeKey(it.key()), it.value());
 
    m_config->sync();
}

void TagManager::addTag(const QString &name, const QString &color)
{
    {
        QMutexLocker lock(&m_mutex);
        for (auto &t : m_tags) if (t.first == name) return;
        m_tags.append(qMakePair(name, color));

    }
    save();
    emit tagsChanged();
}

void TagManager::removeTag(const QString &name)
{
    {
        QMutexLocker lock(&m_mutex);
        m_tags.removeIf([&](const QPair<QString,QString> &t){ return t.first == name; });
        for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ) {
            if (it.value() == name) it = m_fileTags.erase(it);
            else ++it;
        }
    }
    save();
    emit tagsChanged();
}

void TagManager::setFileTag(const QString &path, const QString &tag)
{
    {
        QMutexLocker lock(&m_mutex);
        m_fileTags[path] = tag;
    }
    save();
    emit fileTagChanged(path);
}

void TagManager::clearFileTag(const QString &path)
{
    {
        QMutexLocker lock(&m_mutex);
        m_fileTags.remove(path);
    }
    save();
    emit fileTagChanged(path);
}

QString TagManager::fileTag(const QString &path) const
{
    QMutexLocker lock(&m_mutex);
    return m_fileTags.value(path);
}

QString TagManager::tagColor(const QString &tagName) const
{
    QMutexLocker lock(&m_mutex);
    for (auto &t : m_tags)
        if (t.first == tagName) return t.second;
    return {};
}

QStringList TagManager::filesWithTag(const QString &tag) const
{
    QMutexLocker lock(&m_mutex);
    QStringList result;
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
        if (it.value() == tag) result << it.key();
    return result;
}

