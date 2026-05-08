// --- panewidgets.h — Interne Widget-Klassen für MainWindow ---
#pragma once

#include <QWidget>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QList>
#include <QIcon>
#include <QPoint>
#include <functional>

// --- MillerStrip — schmaler Streifen für zurückliegende Miller-Spalten ---
class MillerStrip : public QWidget {
    Q_OBJECT
public:
    explicit MillerStrip(const QString &label, QWidget *parent = nullptr);
    void setLabel(const QString &label);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QString m_label;
    bool    m_hovered = false;
};

// --- PaneSplitterHandle — custom Handle mit Grip-Strichen und Collapse-Pfeilen ---
class PaneSplitterHandle : public QSplitterHandle {
    Q_OBJECT
public:
    PaneSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
    int collapseState() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QPoint     m_pressGlobal;
    QList<int> m_sizesAtPress;
    bool       m_hovered = false;
};

// --- PaneSplitter — QSplitter mit custom Handle ---
class PaneSplitter : public QSplitter {
    Q_OBJECT
public:
    explicit PaneSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    QSplitterHandle *createHandle() override;
};

// --- SidebarHandle — Leiste zum Ein-/Ausklappen und Verbreitern der Sidebar ---
class SidebarHandle : public QWidget {
    Q_OBJECT
public:
    explicit SidebarHandle(QWidget *sidebar, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QWidget      *m_sidebar;
    QList<QIcon>  m_icons;
    bool          m_hov      = false;
    bool          m_dragging = false;
    int           m_hovIcon  = -1;
    QPoint        m_pressGlobal;
    int           m_pressWidth = 0;
};

// --- FooterWidget — zusammenklappbare Vorschau-/Info-Leiste am Pane-Rand ---
class FooterWidget : public QWidget {
public:
    QLabel *countLbl    = nullptr;
    QLabel *selectedLbl = nullptr;
    QLabel *sizeLbl     = nullptr;
    QLabel *previewIcon = nullptr;
    QLabel *previewInfo = nullptr;

    std::function<void()> onHeightChanged;

    explicit FooterWidget(QWidget *parent);
    void setExpanded(bool on);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    enum { CLOSED_H = 22, OPEN_H = 160 };
    QVBoxLayout *m_mainLay      = nullptr;
    QWidget     *m_barRow       = nullptr;
    QWidget     *m_content      = nullptr;
    bool         m_expanded     = false;
    bool         m_dragging     = false;
    bool         m_clickOnArrow = false;
    bool         m_arrowHov     = false;
    int          m_pressY       = 0;
    int          m_pressH       = 0;
};

// --- MillerItemDelegate — Age-Badge Indikator in Miller-Spalten ---
class MillerItemDelegate : public QStyledItemDelegate {
public:
    explicit MillerItemDelegate(QObject *parent = nullptr);
    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override;
};
