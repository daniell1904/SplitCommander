#pragma once

#include <QObject>
#include <QStringList>

class FileOperationWorker : public QObject {
    Q_OBJECT
public:
    enum Type { Copy, Move, Delete };
    FileOperationWorker(Type type, const QStringList &sources, const QString &dest, QObject *parent = nullptr);

public slots:
    void process();

signals:
    void statusUpdate(const QString &fileName, int progress);
    void finished();

private:
    bool copyRecursively(const QString &srcPath, const QString &dstPath);
    Type m_type;
    QStringList m_sources;
    QString m_dest;
};

