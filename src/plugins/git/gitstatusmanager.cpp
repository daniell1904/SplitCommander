#include "gitstatusmanager.h"
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QRegularExpression>

GitStatusManager &GitStatusManager::instance()
{
    static GitStatusManager s_inst;
    return s_inst;
}

GitStatusManager::GitStatusManager(QObject *parent) : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    Q_ASSERT(m_watcher != nullptr);
    
    m_periodicTimer = new QTimer(this);
    Q_ASSERT(m_periodicTimer != nullptr);
    m_periodicTimer->setSingleShot(false);
    
    connect(m_periodicTimer, &QTimer::timeout, this, &GitStatusManager::refreshAllRepos);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path)
    {
        // Finde Repo zu dem dieser Pfad gehört
        for (const auto &r : Config::gitRepos())
        {
            if (!r.localDir.isEmpty() && path.startsWith(r.localDir))
            {
                // Debounce: in 1s refreshen
                QTimer::singleShot(1000, this, [this, repo = r.localDir]()
                {
                    refreshRepo(repo);
                });
                break;
            }
        }
    });
    reloadConfig();
}

void GitStatusManager::reloadConfig()
{
    setupWatchers();
    setupTimer();
    refreshAllRepos();
}

void GitStatusManager::setupWatchers()
{
    Q_ASSERT(m_watcher != nullptr);
    // Alle bisherigen Watches entfernen
    if (!m_watcher->directories().isEmpty())
    {
        m_watcher->removePaths(m_watcher->directories());
    }

    const QString mode = Config::gitRefreshMode();
    if (mode != QStringLiteral("onchange"))
    {
        return;
    }

    // Repo-Ordner rekursiv beobachten (nur Top-Level + 1-2 Ebenen wegen Limits)
    for (const auto &r : Config::gitRepos())
    {
        if (r.localDir.isEmpty() || !QFileInfo::exists(r.localDir))
        {
            continue;
        }
        m_watcher->addPath(r.localDir);
        // Unterordner (1 Ebene) auch beobachten
        QDir d(r.localDir);
        const auto subs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &s : subs)
        {
            if (s == QStringLiteral(".git"))
            {
                continue;
            }
            m_watcher->addPath(d.absoluteFilePath(s));
        }
    }
}

void GitStatusManager::setupTimer()
{
    Q_ASSERT(m_periodicTimer != nullptr);
    const QString mode = Config::gitRefreshMode();
    if (mode == QStringLiteral("periodic"))
    {
        const int min = qMax(1, Config::gitRefreshIntervalMinutes());
        m_periodicTimer->start(min * 60 * 1000);
    }
    else
    {
        m_periodicTimer->stop();
    }
}

void GitStatusManager::refreshAllRepos()
{
    for (const auto &r : Config::gitRepos())
    {
        if (!r.localDir.isEmpty())
        {
            refreshRepo(r.localDir);
        }
    }
}

void GitStatusManager::refreshRepo(const QString &repoPath)
{
    if (repoPath.isEmpty() || !QFileInfo::exists(repoPath))
    {
        return;
    }
    runStatus(repoPath);
    runFetchThenDiff(repoPath);
}

void GitStatusManager::runStatus(const QString &repoPath)
{
    auto *proc = new QProcess(this);
    Q_ASSERT(proc != nullptr);
    proc->setWorkingDirectory(repoPath);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, proc, repoPath](int, QProcess::ExitStatus)
    {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        // Lokal geänderte Dateien als rot markieren — alle anderen vorher tracken als grün
        auto &repoMap = m_status[repoPath];
        // Erst alle aktuell als grün markierten Lokal-Änderungen zurücksetzen
        for (auto it = repoMap.begin(); it != repoMap.end(); )
        {
            if (it.value() == GitFileStatus::LocalChange)
            {
                it = repoMap.erase(it);
            }
            else
            {
                ++it;
            }
        }
        const auto lines = out.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            if (line.size() < 4)
            {
                continue;
            }
            const QString rel = line.mid(3).trimmed();
            repoMap[rel] = GitFileStatus::LocalChange;
        }
        emit statusUpdated(repoPath);
        proc->deleteLater();
    });
    proc->start(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")});
}

void GitStatusManager::runFetchThenDiff(const QString &repoPath)
{
    auto *fetch = new QProcess(this);
    Q_ASSERT(fetch != nullptr);
    fetch->setWorkingDirectory(repoPath);
    connect(fetch, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, fetch, repoPath](int, QProcess::ExitStatus)
    {
        fetch->deleteLater();
        // Jetzt diff gegen Upstream
        auto *diff = new QProcess(this);
        Q_ASSERT(diff != nullptr);
        diff->setWorkingDirectory(repoPath);
        connect(diff, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, diff, repoPath](int code, QProcess::ExitStatus)
        {
            const QString out = QString::fromUtf8(diff->readAllStandardOutput());
            auto &repoMap = m_status[repoPath];
            // Vorher RemoteAhead entfernen
            for (auto it = repoMap.begin(); it != repoMap.end(); )
            {
                if (it.value() == GitFileStatus::RemoteAhead)
                {
                    it = repoMap.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            if (code == 0)
            {
                const auto files = out.split('\n', Qt::SkipEmptyParts);
                for (const QString &rel : files)
                {
                    // Nur als RemoteAhead markieren wenn nicht schon lokal geändert
                    if (!repoMap.contains(rel))
                    {
                        repoMap[rel] = GitFileStatus::RemoteAhead;
                    }
                }
            }
            emit statusUpdated(repoPath);
            diff->deleteLater();
        });
        diff->start(QStringLiteral("git"), {QStringLiteral("diff"), QStringLiteral("--name-only"), QStringLiteral("HEAD..@{u}")});
    });
    fetch->start(QStringLiteral("git"), {QStringLiteral("fetch"), QStringLiteral("--quiet")});
}

GitFileStatus GitStatusManager::statusFor(const QString &repoPath, const QString &filePath) const
{
    auto it = m_status.find(repoPath);
    if (it == m_status.end())
    {
        return GitFileStatus::Unchanged;
    }
    QString rel = filePath;
    if (rel.startsWith(repoPath))
    {
        rel = rel.mid(repoPath.size()).remove(QRegularExpression(QStringLiteral("^/+")));
    }
    auto fileIt = it->find(rel);
    if (fileIt == it->end())
    {
        return GitFileStatus::Unchanged;
    }
    return fileIt.value();
}

QStringList GitStatusManager::trackedFiles(const QString &repoPath) const
{
    auto it = m_status.find(repoPath);
    if (it == m_status.end())
    {
        return {};
    }
    return it->keys();
}
