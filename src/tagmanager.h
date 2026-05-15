#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include <QPair>
#include <QList>
#include <QMutex>
#include <KSharedConfig>


// --- TagManager --- (Verwaltet die Datei-Tags (Markierungen))
class TagManager : public QObject
{
    Q_OBJECT
public:
    static TagManager &instance() {
        static TagManager inst;
        return inst;
    }

    // Verfügbare Tags: Name → Farbe
    const QList<QPair<QString,QString>>& tags() const { return m_tags; }
    void addTag(const QString &name, const QString &color);
    void removeTag(const QString &name);

    // Tag einer Datei setzen/lesen
    void    setFileTag(const QString &path, const QString &tag);
    void    setFileTags(const QStringList &paths, const QString &tag);
    void    clearFileTag(const QString &path);
    void    clearFileTags(const QStringList &paths);
    QString fileTag(const QString &path) const;
    QString tagColor(const QString &tagName) const;

    // Alle Dateien mit einem bestimmten Tag
    QStringList filesWithTag(const QString &tag) const;

signals:
    void tagsChanged();
    void fileTagChanged(const QString &path);

private:
    TagManager();
    void load();
    void save();

    QList<QPair<QString,QString>> m_tags;
    QMap<QString,QString>         m_fileTags; // path → tagName
    KSharedConfigPtr              m_config;
    mutable QMutex                m_mutex;
};


