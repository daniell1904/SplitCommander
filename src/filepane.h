#pragma once

#include <QUrl>
#include <QWidget>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QListView>
#include <QStackedWidget>
#include <QDateTime>
#include <QFileIconProvider>
#include <QAction>
#include <QMenu>
#include <QScrollBar>

#include <KDirModel>
#include <KDirLister>
#include <KDirSortFilterProxyModel>
#include <KFileItem>
#include <KNewFileMenu>

// --- FPCol --- (Definition der Spalten-IDs (Name, Größe, Datum, etc.))
enum FPCol {
    FP_NAME=0,
    FP_TYP,
    FP_ALTER,
    FP_DATUM,
    FP_ERSTELLT,
    FP_ZUGRIFF,
    FP_GROESSE,
    FP_RECHTE,
    FP_EIGENTUEMER,
    FP_GRUPPE,
    FP_PFAD,
    FP_ERWEITERUNG,
    FP_TAGS,
    FP_IMG_DATUM,
    FP_IMG_ABMESS,
    FP_IMG_BREITE,
    FP_IMG_HOEHE,
    FP_IMG_AUSRICHT,
    FP_AUD_KUENSTLER,
    FP_AUD_GENRE,
    FP_AUD_ALBUM,
    FP_AUD_DAUER,
    FP_AUD_BITRATE,
    FP_AUD_STUECK,
    FP_VID_SEITENVERH,
    FP_VID_FRAMERATE,
    FP_VID_DAUER,
    FP_DOC_TITEL,
    FP_DOC_AUTOR,
    FP_DOC_HERAUSGEBER,
    FP_DOC_SEITEN,
    FP_DOC_WOERTER,
    FP_DOC_ZEILEN,
    FP_COUNT
};

// --- FPColDef --- (Definition der Eigenschaften einer einzelnen Spalte)
struct FPColDef {
    FPCol   id;
    QString label;
    QString group;
    bool    defaultVisible;
    int     defaultWidth;
};

// --- FPColumnsProxy --- (Proxy-Model zur Verwaltung der Spaltenreihenfolge und Sichtbarkeit)
class FPColumnsProxy : public QAbstractProxyModel {
    Q_OBJECT
public:
        explicit FPColumnsProxy(QObject *parent = nullptr);

    void setVisibleCols(const QList<FPCol> &cols);
    const QList<FPCol>& visibleCols() const { return m_visCols; }
    void setTagFilter(const QString &tag);

    void setSourceModel(QAbstractItemModel *model) override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    QMimeData* mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    KFileItem fileItem(const QModelIndex &proxyIdx) const;

private:
    bool acceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    QVariant extraData(const KFileItem &item, FPCol col, int role) const;
    int kdirColumn(FPCol col) const;

    QList<FPCol>              m_visCols;
    QString                   m_tagFilter;
    KDirSortFilterProxyModel *m_sortProxy = nullptr;
    KDirModel                *m_kdirModel = nullptr;
};

// --- FilePaneDelegate --- (Zeichnet die Einträge in der Dateiliste (Farben, Icons, Text))
class FilePaneDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    bool focused   = true;
    int  rowHeight = 26;
    int  fontSize  = 11;

        explicit FilePaneDelegate(QObject *par = nullptr);
    void  paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;

    static QColor   ageColor(qint64 secs);
    static QString  formatAge(qint64 secs);
};

// --- FilePane --- (Haupt-Widget für eine Dateiliste (enthält Liste, Icons, Modelle))
class FilePane : public QWidget {
    Q_OBJECT
public:
        explicit FilePane(QWidget *parent = nullptr, const QString &settingsKey = QStringLiteral("default"));

    void setRootPath(const QString &path);
    void setRootUrl(const QUrl &url);
    void setNameFilter(const QString &pattern);
    void setFoldersFirst(bool on);
    void setShowHiddenFiles(bool show);
    [[nodiscard]] bool    hasFocus()    const;
    [[nodiscard]] const QString& currentPath() const;
    QTreeView *view()     { return m_view; }
    [[nodiscard]] QList<QUrl> selectedUrls() const;
    [[nodiscard]] qint64 currentTotalSize() const;

signals:
    void directoryLoaded();
    void modelUpdated();
    void fileActivated(const QString &path);
    void fileSelected(const QString &path);
    void columnsChanged(int colId, bool visible);
    void deleteRequested();
    void addToPlacesRequested(const QString &url, const QString &name);

public:
    void setColumnVisible(int colId, bool visible);
    void setViewMode(int mode);
    int  viewMode() const { return m_viewMode; }
    void setRowHeight(int height);
    void showTaggedFiles(const QString &tagName);
    [[nodiscard]] static const QList<FPColDef>& colDefs();

private slots:
    void onItemActivated(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);
    void showHeaderMenu(const QPoint &pos);
    void reload();
    void onNewFileCreated(const QUrl &url);

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void setupColumns();
    void openWithApp(const QString &entry, const QString &path);

    QStackedWidget           *m_stack;
    QTreeView                *m_view;
    FilePaneDelegate         *m_delegate   = nullptr;
    QListView                *m_iconView;
    QScrollBar               *m_overlayBar;
    QScrollBar               *m_overlayHBar = nullptr;

    // KDE model stack
    KDirLister               *m_lister;
    KDirModel                *m_dirModel;
    KDirSortFilterProxyModel *m_sortProxy;
    FPColumnsProxy           *m_proxy;

    KNewFileMenu             *m_newFileMenu = nullptr;

    QString  m_currentPath;
    QString  m_settingsKey; // pane-spezifischer Settings-Key
    QUrl     m_currentUrl;
    bool     m_kioMode    = false;
    int      m_viewMode   = 0;
    QString  m_filter;
    bool     m_foldersFirst = true;
    QString  m_currentTagFilter;
    QList<bool> m_colVisible;
};
