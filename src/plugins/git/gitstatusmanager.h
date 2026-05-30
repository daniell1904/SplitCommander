#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QFileSystemWatcher>
#include "../../config.h"

// Status pro Datei
enum class GitFileStatus
{
    Unchanged,    // grün
    LocalChange,  // rot — uncommitted / lokal geändert
    RemoteAhead,  // gelb — Remote hat neuere Version
};

class GitStatusManager : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static GitStatusManager &instance();

    // Status für eine konkrete Datei (relativ oder absolut)
    [[nodiscard]] GitFileStatus statusFor(const QString &repoPath, const QString &filePath) const;
    // Liste aller Repo-Dateien für die der Manager Status hat
    [[nodiscard]] QStringList trackedFiles(const QString &repoPath) const;

    void reloadConfig();
    void refreshAllRepos();
    void refreshRepo(const QString &repoPath);

signals:
    void statusUpdated(const QString &repoPath);

private:
    explicit GitStatusManager(QObject *parent = nullptr);
    ~GitStatusManager() override = default;

    void setupWatchers();
    void setupTimer();
    void runStatus(const QString &repoPath);
    void runFetchThenDiff(const QString &repoPath);

    // repoPath -> (relPath -> Status)
    QHash<QString, QHash<QString, GitFileStatus>> m_status;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_periodicTimer = nullptr;
};
