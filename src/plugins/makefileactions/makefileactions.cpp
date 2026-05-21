#include "makefileactions.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <KTerminalLauncherJob>

namespace MakefileActions {

enum BuildSystem { None, Make, CMake };

static BuildSystem detectBuildSystem(const QString &dirPath)
{
    if (QFileInfo::exists(dirPath + "/CMakeCache.txt") ||
        QFileInfo::exists(dirPath + "/build.ninja"))
        return CMake;
    if (QFileInfo::exists(dirPath + "/CMakeLists.txt"))
        return CMake;
    if (QFileInfo::exists(dirPath + "/Makefile") ||
        QFileInfo::exists(dirPath + "/makefile") ||
        QFileInfo::exists(dirPath + "/GNUmakefile"))
        return Make;
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
        return {"all", "clean", "install", "test"};

    QProcess proc;
    proc.setWorkingDirectory(dirPath);
    proc.start("make", {"-pRr", ":"});
    proc.waitForFinished(10000);
    QSet<QString> targetSet;
    bool nonTarget = false;
    for (const QString &line : QString::fromUtf8(proc.readAllStandardOutput()).split('\n')) {
        if (nonTarget) { nonTarget = false; continue; }
        if (line.contains("Not a target")) { nonTarget = true; continue; }
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(' ')
            || line.contains(" := ") || line.contains(" = ")
            || line.contains('%')) continue;
        const QString target = line.section(':', 0, 0).trimmed();
        if (!target.isEmpty() && !target.contains(' '))
            targetSet.insert(target);
    }
    QStringList result = targetSet.values();
    result.sort();
    for (const QString prio : {"all", "install", "clean", "distclean", "test"})
        if (result.removeOne(prio)) result.prepend(prio);
    return result;
}

void runTarget(const QString &dirPath, const QString &target, QWidget *parent)
{
    Q_UNUSED(parent)
    const BuildSystem bs = detectBuildSystem(dirPath);
    QString cmd;
    if (bs == CMake)
        cmd = QString("cmake --build %1 --target %2 -- -j$(nproc)")
              .arg(QDir::toNativeSeparators(dirPath), target);
    else
        cmd = QString("make -C %1 %2 -j$(nproc)")
              .arg(QDir::toNativeSeparators(dirPath), target);

    auto *job = new KTerminalLauncherJob(cmd);
    job->setWorkingDirectory(dirPath);
    job->start();
}

} // namespace MakefileActions
