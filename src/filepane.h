#ifndef FILEPANE_H
#define FILEPANE_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QListView>
#include <QStackedWidget>
#include <QPainter>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QFileIconProvider>
#include <QAction>
#include <QMenu>

#include <QScrollBar>

// Alle verfügbaren Spalten
enum FPCol {
    FP_NAME=0,
    FP_TYP,
    FP_ALTER,
    FP_DATUM,        // Geändert
    FP_ERSTELLT,
    FP_ZUGRIFF,      // Letzter Zugriff
    FP_GROESSE,
    FP_RECHTE,
    FP_EIGENTUEMER,
    FP_GRUPPE,
    FP_PFAD,
    FP_ERWEITERUNG,
    FP_TAGS,
    // Bild
    FP_IMG_DATUM,
    FP_IMG_ABMESS,
    FP_IMG_BREITE,
    FP_IMG_HOEHE,
    FP_IMG_AUSRICHT,
    // Audio
    FP_AUD_KUENSTLER,
    FP_AUD_GENRE,
    FP_AUD_ALBUM,
    FP_AUD_DAUER,
    FP_AUD_BITRATE,
    FP_AUD_STUECK,
    // Video
    FP_VID_SEITENVERH,
    FP_VID_FRAMERATE,
    FP_VID_DAUER,
    // Dokument
    FP_DOC_TITEL,
    FP_DOC_AUTOR,
    FP_DOC_HERAUSGEBER,
    FP_DOC_SEITEN,
    FP_DOC_WOERTER,
    FP_DOC_ZEILEN,
    FP_COUNT
};

struct FPColDef {
    FPCol   id;
    QString label;
    QString group;   // "" = Standard, "Bild", "Audio", "Video", "Dokument", "Weitere"
    bool    defaultVisible;
    int     defaultWidth;
};

class FilePaneDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    bool focused = true;
    explicit FilePaneDelegate(QObject *par=nullptr);
    void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
private:
    QColor ageColor(qint64) const;
    QString formatAge(qint64) const;
};

class FilePane : public QWidget {
    Q_OBJECT
public:
    explicit FilePane(QWidget *parent=nullptr);
    void setRootPath(const QString &path);
    void setNameFilter(const QString &pattern);
    void setFoldersFirst(bool on);
    bool hasFocus() const;
    QString currentPath() const;
    QTreeView *view() { return m_view; }

signals:
    void fileActivated(const QString &path);
    void fileSelected(const QString &path);
    void columnsChanged(int colId, bool visible);
public:
    void setColumnVisible(int colId, bool visible);
    void setViewMode(int mode);
    void showTaggedFiles(const QString &tagName);

private slots:
    void onItemActivated(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);
    void showHeaderMenu(const QPoint &pos);
    void reload();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void populate(const QString &path);
    void buildRow(const QFileInfo &fi, QList<QStandardItem*> &items);
    void fetchMetaAsync(const QFileInfo &fi, int row);
    void setupColumns();
    void openWithApp(const QString &entry, const QString &path);

    QStackedWidget        *m_stack;
    QTreeView             *m_view;
    QListView             *m_iconView;
    QScrollBar            *m_overlayBar;
    QStandardItemModel    *m_model;
    QSortFilterProxyModel *m_proxy; // FolderFirstProxy zur Laufzeit
    QFileSystemWatcher    *m_watcher;
    QFileIconProvider      m_iconProv;
    QString                m_currentPath;
    QString                m_currentTagFilter; // leer = normaler Ordner-Modus
    QString                m_filter;
    bool                   m_foldersFirst = true;
    QList<bool>            m_colVisible;  // Sichtbarkeit pro FPCol

    static const QList<FPColDef>& colDefs();
};
#endif
