#include "fileoperations.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

FileOperationWorker::FileOperationWorker(Type type, const QStringList &sources, const QString &dest, QObject *parent)
    : QObject(parent), m_type(type), m_sources(sources), m_dest(dest) {}

void FileOperationWorker::process() {
    int total = m_sources.size();
    if (total == 0) {
        emit finished();
        return;
    }

    for (int i = 0; i < total; ++i) {
        QString src = m_sources[i];
        QFileInfo fi(src);
        QString target = m_dest + "/" + fi.fileName();

        emit statusUpdate(fi.fileName(), (i * 100) / total);

        if (m_type == Copy) {
            if (fi.isDir()) copyRecursively(src, target);
            else QFile::copy(src, target);
        } else if (m_type == Move) {
            QFile::rename(src, target);
        } else if (m_type == Delete) {
            if (fi.isDir()) QDir(src).removeRecursively();
            else QFile::remove(src);
        }
    }
    emit finished();
}

bool FileOperationWorker::copyRecursively(const QString &srcPath, const QString &dstPath) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;
    QDir().mkpath(dstPath);
    for (const auto &f : srcDir.entryList(QDir::Files)) 
        QFile::copy(srcPath + "/" + f, dstPath + "/" + f);
    for (const auto &d : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        copyRecursively(srcPath + "/" + d, dstPath + "/" + d);
    return true;
}