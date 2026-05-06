#include "tagmanager.h"

static QString encodeKey(const QString &path)
{
    return QString::fromLatin1(path.toUtf8().toBase64());
}

static QString decodeKey(const QString &key)
{
    return QString::fromUtf8(QByteArray::fromBase64(key.toLatin1()));
}

TagManager::TagManager()
    : QObject(nullptr)
    , m_settings(new QSettings("SplitCommander", "Tags", this))
{
    load();
}

void TagManager::load()
{
    // Standard-Tags laden
    int tcount = m_settings->beginReadArray("tags");
    if (tcount == 0) {
        m_settings->endArray();
        // Defaults
        m_tags = {{"Wichtig", "#bf616a"}, {"Arbeit", "#5e81ac"}, {"Privat", "#a3be8c"}};
    } else {
        for (int i = 0; i < tcount; ++i) {
            m_settings->setArrayIndex(i);
            m_tags.append({m_settings->value("name").toString(),
                           m_settings->value("color").toString()});
        }
        m_settings->endArray();
    }

    // Datei-Tags laden
    m_settings->beginGroup("fileTags");
    for (const QString &key : m_settings->childKeys())
        m_fileTags[decodeKey(key)] = m_settings->value(key).toString();
    m_settings->endGroup();
}

void TagManager::save()
{
    m_settings->beginWriteArray("tags");
    for (int i = 0; i < m_tags.size(); ++i) {
        m_settings->setArrayIndex(i);
        m_settings->setValue("name",  m_tags[i].first);
        m_settings->setValue("color", m_tags[i].second);
    }
    m_settings->endArray();

    m_settings->beginGroup("fileTags");
    m_settings->remove("");
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
        m_settings->setValue(encodeKey(it.key()), it.value());
    m_settings->endGroup();
}

void TagManager::addTag(const QString &name, const QString &color)
{
    for (auto &t : m_tags) if (t.first == name) return;
    m_tags.append({name, color});
    save();
    emit tagsChanged();
}

void TagManager::removeTag(const QString &name)
{
    m_tags.removeIf([&](const QPair<QString,QString> &t){ return t.first == name; });
    // Datei-Tags mit diesem Namen löschen
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ) {
        if (it.value() == name) it = m_fileTags.erase(it);
        else ++it;
    }
    save();
    emit tagsChanged();
}

void TagManager::setFileTag(const QString &path, const QString &tag)
{
    m_fileTags[path] = tag;
    save();
    emit fileTagChanged(path);
}

void TagManager::clearFileTag(const QString &path)
{
    m_fileTags.remove(path);
    save();
    emit fileTagChanged(path);
}

QString TagManager::fileTag(const QString &path) const
{
    return m_fileTags.value(path);
}

QString TagManager::tagColor(const QString &tagName) const
{
    for (auto &t : m_tags)
        if (t.first == tagName) return t.second;
    return {};
}

QStringList TagManager::filesWithTag(const QString &tag) const
{
    QStringList result;
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it)
        if (it.value() == tag) result << it.key();
    return result;
}
