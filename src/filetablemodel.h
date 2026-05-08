#pragma once

#include <QAbstractTableModel>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMap>
#include <QMutex>
#include "tagmanager.h"

class FileTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { IconCol=0, NameCol, TypeCol, AgeCol, DateCol, SizeCol, PermsCol, TagCol, ColCount };

    explicit FileTableModel(QObject *parent = nullptr);

    void    setDirectory(const QString &path);
    void    setShowHidden(bool show);
    bool    showHidden() const { return m_showHidden; }
    void    setTagFilter(const QString &tag);
    const QString& tagFilter() const { return m_tagFilter; }
    const QString& currentPath() const { return m_path; }
    QFileSystemModel *fsModel() const { return m_fs; }

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    void     sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    QFileInfo fileInfo(int row) const;
    bool      isDir(int row)    const;
    QString   filePath(int row) const;

signals:
    void directoryLoaded();

public:
    void reload();
private:

    QFileSystemModel *m_fs;
    QString           m_path;
    QFileInfoList     m_entries;
    bool              m_showHidden = false;
    mutable QMap<QString,qint64> m_sizeCache;
    mutable QMutex    m_sizeCacheMutex;
    QString           m_tagFilter;
};
