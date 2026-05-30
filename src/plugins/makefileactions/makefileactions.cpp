#include "makefileactions.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <KTerminalLauncherJob>

namespace MakefileActions
{

enum BuildSystem
{
    None,
    Make,
    CMake
};

[[nodiscard]] static BuildSystem detectBuildSystem(const QString &dirPath)
{
    if (QFileInfo::exists(dirPath + QStringLiteral("/CMakeCache.txt")) || QFileInfo::exists(dirPath + QStringLiteral("/build.ninja")))
    {
        return CMake;
    }
    if (QFileInfo::exists(dirPath + QStringLiteral("/CMakeLists.txt")))
    {
        return CMake;
    }
    if (QFileInfo::exists(dirPath + QStringLiteral("/Makefile")) || QFileInfo::exists(dirPath + QStringLiteral("/makefile")) || QFileInfo::exists(dirPath + QStringLiteral("/GNUmakefile")))
    {
        return Make;
    }
    return None;
}

bool hasMakefile(const QString &dirPath)
{
    return detectBuildSystem(dirPath) != None;
}

QStringList listTargets(const QString &dirPath)
{
    const BuildSystem bs = detectBuildSystem(dirPath);
    if (bs == CMake)
    {
        return {QStringLiteral("all"), QStringLiteral("clean"), QStringLiteral("install"), QStringLiteral("test")};
    }

    QProcess proc;
    proc.setWorkingDirectory(dirPath);
    proc.start(QStringLiteral("make"), {QStringLiteral("-pRr"), QStringLiteral(":")});
    proc.waitForFinished(10000);
    QSet<QString> targetSet;
    bool nonTarget = false;
    const auto lines = QString::fromUtf8(proc.readAllStandardOutput()).split('\n');
    for (const QString &line : lines)
    {
        if (nonTarget)
        {
            nonTarget = false;
            continue;
        }
        if (line.contains(QStringLiteral("Not a target")))
        {
            nonTarget = true;
            continue;
        }
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(' ') || line.contains(QStringLiteral(" := ")) || line.contains(QStringLiteral(" = ")) || line.contains('%'))
        {
            continue;
        }
        const QString target = line.section(':', 0, 0).trimmed();
        if (!target.isEmpty() && !target.contains(' '))
        {
            targetSet.insert(target);
        }
    }
    QStringList result = targetSet.values();
    result.sort();
    const QStringList priorities = {QStringLiteral("all"), QStringLiteral("install"), QStringLiteral("clean"), QStringLiteral("distclean"), QStringLiteral("test")};
    for (const QString &prio : priorities)
    {
        if (result.removeOne(prio))
        {
            result.prepend(prio);
        }
    }
    return result;
}

void runTarget(const QString &dirPath, const QString &target, QWidget *parent)
{
    Q_UNUSED(parent)
    const BuildSystem bs = detectBuildSystem(dirPath);
    QString cmd;
    if (bs == CMake)
    {
        cmd = QStringLiteral("cmake --build %1 --target %2 -- -j$(nproc)")
              .arg(QDir::toNativeSeparators(dirPath), target);
    }
    else
    {
        cmd = QStringLiteral("make -C %1 %2 -j$(nproc)")
              .arg(QDir::toNativeSeparators(dirPath), target);
    }

    auto *job = new KTerminalLauncherJob(cmd);
    Q_ASSERT(job != nullptr);
    job->setWorkingDirectory(dirPath);
    job->start();
}

} // namespace MakefileActions
