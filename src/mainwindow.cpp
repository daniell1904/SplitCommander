// ─────────────────────────────────────────────────────────────────────────────
// mainwindow.cpp — SplitCommander Hauptfenster
// ─────────────────────────────────────────────────────────────────────────────

#include "mainwindow.h"
#include "settingsdialog.h"

#include <KIO/DeleteOrTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QDirIterator>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStorageInfo>
#include <QTreeWidget>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// StyleSheet-Konstanten
// ─────────────────────────────────────────────────────────────────────────────
static const char *SS_TOOL_BTN =
    "QToolButton{color:#4c566a;background:transparent;border:none;border-radius:3px;padding:3px;}"
    "QToolButton:hover{color:#ccd4e8;background:#2e3440;}";

static const char *SS_ACTION_BTN =
    "QToolButton{color:#d8dee9;background:transparent;border:1px solid #2c3245;border-radius:4px;padding:4px;margin:0 1px;}"
    "QToolButton:hover{background:#2e3440;border-color:#434c5e;}"
    "QToolButton:checked{background:#3b4252;border-color:#5e81ac;color:#88c0d0;}";

static const char *SS_COL_ACTIVE =
    "QListWidget{background:#2e3440;border:none;color:#ccd4e8;outline:none;}"
    "QListWidget::item{padding:3px 8px;border-bottom:1px solid #1e2330;}"
    "QListWidget::item:selected{background:#3b4252;color:#88c0d0;}"
    "QListWidget::item:hover{background:#2e3440;}"
    "QListWidget QScrollBar:vertical{width:4px;background:transparent;border:none;}"
    "QListWidget QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-radius:2px;min-height:20px;}"
    "QListWidget:hover QScrollBar::handle:vertical{background:rgba(255,255,255,40);}"
    "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
    "QListWidget QScrollBar:horizontal{height:0;}";

static const char *SS_COL_INACTIVE =
    "QListWidget{background:#2e3440;border:none;color:#8a94a8;outline:none;}"
    "QListWidget::item{padding:3px 8px;border-bottom:1px solid #161a22;}"
    "QListWidget::item:selected{background:#2e3440;color:#ccd4e8;}"
    "QListWidget::item:hover{background:#1e2330;}"
    "QListWidget QScrollBar:vertical{width:4px;background:transparent;border:none;}"
    "QListWidget QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-radius:2px;min-height:20px;}"
    "QListWidget:hover QScrollBar::handle:vertical{background:rgba(255,255,255,25);}"
    "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
    "QListWidget QScrollBar:horizontal{height:0;}";

static const char *SS_COL_DRIVES =
    "QListWidget{background:#2e3440;border:none;color:#ccd4e8;outline:none;}"
    "QListWidget::item{padding:5px 8px;border-bottom:1px solid #0f1218;}"
    "QListWidget::item:selected{background:#3b4252;color:#88c0d0;}"
    "QListWidget::item:hover{background:#1e2330;}"
    "QListWidget QScrollBar:vertical{width:4px;background:transparent;border:none;}"
    "QListWidget QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-radius:2px;min-height:20px;}"
    "QListWidget:hover QScrollBar::handle:vertical{background:rgba(255,255,255,25);}"
    "QListWidget QScrollBar::add-line:vertical,QListWidget QScrollBar::sub-line:vertical{height:0;}"
    "QListWidget QScrollBar:horizontal{height:0;}";

static void mw_applyMenuShadow(QMenu *menu)
{
    if (!menu) return;
    auto *shadow = new QGraphicsDropShadowEffect(menu);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 140));
    menu->setGraphicsEffect(shadow);
}

// ─────────────────────────────────────────────────────────────────────────────
// MillerStrip — schmaler Streifen für zurückliegende Miller-Spalten
// ─────────────────────────────────────────────────────────────────────────────
class MillerStrip : public QWidget {
    Q_OBJECT
public:
    explicit MillerStrip(const QString &label, QWidget *parent = nullptr)
        : QWidget(parent), m_label(label) {
        setFixedWidth(22);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setCursor(Qt::PointingHandCursor);
        setToolTip(label);
    }
    void setLabel(const QString &l) { m_label = l; update(); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), m_hovered ? QColor("#1a1f29") : QColor("#161a22"));
        p.setPen(QColor("#2c3245"));
        p.drawLine(width() - 1, 0, width() - 1, height());
        p.save();
        p.translate(width() / 2 + 4, height() - 8);
        p.rotate(-90);
        p.setPen(m_hovered ? QColor("#d8dee9") : QColor("#88c0d0"));
        QFont f = p.font(); f.setPointSize(8); p.setFont(f);
        p.drawText(0, 0, p.fontMetrics().elidedText(m_label, Qt::ElideRight, height() - 16));
        p.restore();
    }
    void mousePressEvent(QMouseEvent *) override { emit clicked(); }
    void enterEvent(QEnterEvent *)       override { m_hovered = true;  update(); }
    void leaveEvent(QEvent *)            override { m_hovered = false; update(); }

private:
    QString m_label;
    bool    m_hovered = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// PaneSplitterHandle — custom Handle mit Grip-Strichen und Collapse-Pfeilen
// ─────────────────────────────────────────────────────────────────────────────
class PaneSplitterHandle : public QSplitterHandle {
    Q_OBJECT
public:
    PaneSplitterHandle(Qt::Orientation o, QSplitter *parent)
        : QSplitterHandle(o, parent) {
        setStyleSheet("background:#0a0d14;");
        connect(parent, &QSplitter::splitterMoved, this, [this](int, int) { update(); });
    }

    // 0 = beide offen, 1 = links eingeklappt, 2 = rechts eingeklappt
    int collapseState() const {
        QList<int> sz = splitter()->sizes();
        if (sz.size() < 2) return 0;
        if (sz[0] == 0)    return 1;
        if (sz[1] == 0)    return 2;
        return 0;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#0a0d14"));

        const int cx    = width() / 2;
        const int cy    = height() / 2;
        const int state = collapseState();

        if (state == 0) {
            // Grip-Striche
            p.setPen(QPen(QColor(255, 255, 255, m_hovered ? 140 : 50), 1));
            for (int i = -3; i <= 4; ++i) {
                const int y = cy + i * 4 - 2;
                p.drawLine(cx - 2, y, cx + 2, y);
            }
            // Pfeil oben (<)
            p.setPen(QPen(QColor(255, 255, 255, m_hovered ? 220 : 100), 1.5,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const int topY = cy - 34;
            p.drawLine(cx + 2, topY - 4, cx - 2, topY);
            p.drawLine(cx - 2, topY,     cx + 2, topY + 4);
            // Pfeil unten (>)
            const int botY = cy + 34;
            p.drawLine(cx - 2, botY - 4, cx + 2, botY);
            p.drawLine(cx + 2, botY,     cx - 2, botY + 4);
        } else {
            p.setPen(QPen(QColor(255, 255, 255, m_hovered ? 240 : 150), 2.0,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (state == 1) {
                p.drawLine(cx - 3, cy - 6, cx + 3, cy);
                p.drawLine(cx + 3, cy,     cx - 3, cy + 6);
            } else {
                p.drawLine(cx + 3, cy - 6, cx - 3, cy);
                p.drawLine(cx - 3, cy,     cx + 3, cy + 6);
            }
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        m_pressGlobal  = e->globalPosition().toPoint();
        m_sizesAtPress = splitter()->sizes();
        QSplitterHandle::mousePressEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) {
            QSplitterHandle::mouseReleaseEvent(e); return;
        }
        const int moved = qAbs(e->globalPosition().toPoint().x() - m_pressGlobal.x());
        if (moved < 4) {
            QList<int> sz    = splitter()->sizes();
            if (sz.size() < 2) return;
            const int total  = sz[0] + sz[1];
            const int state  = collapseState();
            if (state != 0) {
                splitter()->setSizes({total / 2, total / 2});
            } else {
                if (e->pos().y() < height() / 2)
                    splitter()->setSizes({0, total});
                else
                    splitter()->setSizes({total, 0});
            }
        }
        QSplitterHandle::mouseReleaseEvent(e);
    }

    void enterEvent(QEnterEvent *) override { m_hovered = true;  update(); }
    void leaveEvent(QEvent *)      override { m_hovered = false; update(); }

private:
    QPoint      m_pressGlobal;
    QList<int>  m_sizesAtPress;
    bool        m_hovered = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// PaneSplitter — QSplitter mit custom Handle
// ─────────────────────────────────────────────────────────────────────────────
class PaneSplitter : public QSplitter {
    Q_OBJECT
public:
    PaneSplitter(Qt::Orientation o, QWidget *parent = nullptr)
        : QSplitter(o, parent) {}
protected:
    QSplitterHandle *createHandle() override {
        return new PaneSplitterHandle(orientation(), this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SidebarHandle — Leiste zum Ein-/Ausklappen und Verbreitern der Sidebar
// ─────────────────────────────────────────────────────────────────────────────
class SidebarHandle : public QWidget {
    Q_OBJECT
public:
    explicit SidebarHandle(QWidget *sidebar, QWidget *parent = nullptr)
        : QWidget(parent), m_sidebar(sidebar) {
        setFixedWidth(10);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setMouseTracking(true);
        setToolTip("Sidebar ein-/ausklappen / ziehen");

        m_icons << QIcon::fromTheme("preferences-system")
                << QIcon::fromTheme("dialog-information")
                << QIcon::fromTheme("view-split-left-right")
                << QIcon::fromTheme("system-search")
                << QIcon::fromTheme("document-print")
                << QIcon::fromTheme("mail-message-new");
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#0a0d14"));

        const int cx = width() / 2;
        const int cy = height() / 2;

        if (m_sidebar->isVisible()) {
            // ── Ausgeklappt: Grip-Striche + Pfeil links ──
            p.setPen(QPen(QColor(255, 255, 255, 30), 1));
            p.drawLine(width() - 1, 0, width() - 1, height());

            p.setPen(QPen(QColor(255, 255, 255, m_hov ? 140 : 50), 1));
            for (int i = -3; i <= 4; ++i) {
                const int y = cy + 10 + i * 4;
                p.drawLine(cx - 2, y, cx + 2, y);
            }
            p.setPen(QPen(QColor(255, 255, 255, m_hov ? 200 : 80), 1.5,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const int topY = cy - 34;
            p.drawLine(cx + 2, topY - 4, cx - 2, topY);
            p.drawLine(cx - 2, topY,     cx + 2, topY + 4);
        } else {
            // ── Eingeklappt: Text + Grip-Striche + Pfeil rechts + Icons ──

            // "SplitCommander" vertikal
            p.save();
            p.translate(cx + 4, 200);
            p.rotate(-90);
            QFont f = p.font();
            f.setPointSize(9);
            f.setWeight(QFont::Light);
            p.setFont(f);
            p.setPen(QColor("#ccd4e8"));
            p.drawText(0, 0, "Split");
            const int x1 = p.fontMetrics().horizontalAdvance("Split");
            f.setWeight(QFont::Medium);
            p.setFont(f);
            p.setPen(QColor("#88c0d0"));
            p.drawText(x1, 0, "Commander");
            const int x2 = x1 + p.fontMetrics().horizontalAdvance("Commander");
            f.setPointSize(7); f.setWeight(QFont::Light);
            p.setFont(f);
            p.setPen(QColor("#4c566a"));
            p.drawText(x2 + 4, 0, "| Dateimanager");
            p.restore();

            // Grip-Striche
            p.setPen(QPen(QColor(255, 255, 255, m_hov ? 140 : 50), 1));
            for (int i = -3; i <= 4; ++i) {
                const int y = cy + 10 + i * 4;
                p.drawLine(cx - 2, y, cx + 2, y);
            }

            // Pfeil rechts
            p.setPen(QPen(QColor(255, 255, 255, m_hov ? 200 : 80), 1.5,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const int botY = cy + 44;
            p.drawLine(cx - 2, botY - 4, cx + 2, botY);
            p.drawLine(cx + 2, botY,     cx - 2, botY + 4);

            // Footer-Icons
            const int iconSize = 16;
            const int spacing  = 8;
            const int total    = m_icons.size() * (iconSize + spacing) - spacing;
            const int startY   = height() - total - 14;
            for (int i = 0; i < m_icons.size(); ++i) {
                const int iy   = startY + i * (iconSize + spacing);
                const int ix   = cx - iconSize / 2;
                const bool hov = m_hovIcon == i;
                if (hov) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor("#2e3440"));
                    p.drawRoundedRect(ix - 4, iy - 3, iconSize + 8, iconSize + 6, 4, 4);
                }
                p.setOpacity(hov ? 1.0 : (m_hov ? 0.7 : 0.4));
                p.drawPixmap(ix, iy, m_icons[i].pixmap(iconSize, iconSize));
                p.setOpacity(1.0);
            }
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        m_pressGlobal = e->globalPosition().toPoint();
        m_pressWidth  = m_sidebar->isVisible() ? m_sidebar->width() : 0;
        m_dragging    = false;
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        if (!(e->buttons() & Qt::LeftButton)) {
            // Hover-Erkennung für Icons (eingeklappt)
            if (!m_sidebar->isVisible()) {
                const int iconSize = 16, spacing = 8;
                const int total    = m_icons.size() * (iconSize + spacing) - spacing;
                const int startY   = height() - total - 14;
                const int cx       = width() / 2;
                const int prev     = m_hovIcon;
                m_hovIcon = -1;
                for (int i = 0; i < m_icons.size(); ++i) {
                    QRect r(cx - iconSize / 2 - 4, startY + i * (iconSize + spacing) - 3,
                            iconSize + 8, iconSize + 6);
                    if (r.contains(e->pos())) { m_hovIcon = i; break; }
                }
                if (m_hovIcon != prev) update();
            }
            return;
        }

        const int dx = e->globalPosition().toPoint().x() - m_pressGlobal.x();
        if (qAbs(dx) > 3) m_dragging = true;
        if (!m_dragging) return;

        if (!m_sidebar->isVisible()) {
            m_sidebar->setVisible(true);
            setFixedWidth(10);
            setCursor(Qt::SizeHorCursor);
        }

        const int newW = qBound(32, m_pressWidth + dx, 350);
        if (newW < 150) {
            m_sidebar->setVisible(false);
            setFixedWidth(newW);
        } else {
            m_sidebar->setVisible(true);
            m_sidebar->setFixedWidth(newW);
            setFixedWidth(10);
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        if (m_dragging && !m_sidebar->isVisible()) {
            setFixedWidth(32);
            setCursor(Qt::PointingHandCursor);
            m_dragging = false;
            update(); return;
        }
        if (!m_dragging) {
            const bool show = !m_sidebar->isVisible();
            m_sidebar->setVisible(show);
            if (show) m_sidebar->setFixedWidth(250);
            setFixedWidth(show ? 10 : 32);
            setCursor(show ? Qt::SizeHorCursor : Qt::PointingHandCursor);
        }
        m_dragging = false;
        update();
    }

    void enterEvent(QEnterEvent *) override {
        m_hov = true;
        setCursor(m_sidebar->isVisible() ? Qt::SizeHorCursor : Qt::PointingHandCursor);
        update();
    }
    void leaveEvent(QEvent *) override {
        m_hov     = false;
        m_hovIcon = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }

private:
    QWidget      *m_sidebar;
    QList<QIcon>  m_icons;
    bool          m_hov      = false;
    bool          m_dragging = false;
    int           m_hovIcon  = -1;
    QPoint        m_pressGlobal;
    int           m_pressWidth = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// FooterWidget — zusammenklappbare Vorschau-/Info-Leiste am Pane-Rand
// ─────────────────────────────────────────────────────────────────────────────
class FooterWidget : public QWidget {
public:
    QLabel *countLbl    = nullptr;
    QLabel *selectedLbl = nullptr;
    QLabel *sizeLbl     = nullptr;
    QLabel *previewIcon = nullptr;
    QLabel *previewInfo = nullptr;

    std::function<void()> onHeightChanged;

    explicit FooterWidget(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background:#2e3440; border-top:1px solid #12151b;");
        setFixedHeight(CLOSED_H);

        m_mainLay = new QVBoxLayout(this);
        m_mainLay->setContentsMargins(0, 0, 0, 0);
        m_mainLay->setSpacing(0);

        // ── Leisten-Zeile ──
        m_barRow = new QFrame();
        m_barRow->setFixedHeight(CLOSED_H);
        m_barRow->setStyleSheet(
            "QFrame { border:none; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 rgba(10,13,20,100), stop:0.4 #2e3440, stop:1 #2e3440); }");
        auto *barLay = new QHBoxLayout(m_barRow);
        barLay->setContentsMargins(8, 0, 8, 0);
        barLay->setSpacing(4);

        countLbl = new QLabel("0 Elemente");
        countLbl->setStyleSheet("color:#d8dee9;font-size:10px;background:transparent;");
        selectedLbl = new QLabel();
        selectedLbl->setStyleSheet("color:#88c0d0;font-size:10px;background:transparent;");
        selectedLbl->hide();
        sizeLbl = new QLabel();
        sizeLbl->setStyleSheet("color:#d8dee9;font-size:10px;background:transparent;");

        barLay->addWidget(countLbl);
        barLay->addWidget(selectedLbl);
        barLay->addStretch(1);
        barLay->addSpacing(90);
        barLay->addWidget(sizeLbl);
        m_mainLay->addWidget(m_barRow);
        m_barRow->setMouseTracking(true);
        m_barRow->installEventFilter(this);

        // ── Inhalt (ausgeklappt) ──
        m_content = new QWidget();
        m_content->hide();
        auto *cLay = new QHBoxLayout(m_content);
        cLay->setContentsMargins(0, 0, 0, 0);
        cLay->setSpacing(0);

        auto *pvSide = new QWidget();
        pvSide->setStyleSheet("background:#2e3440;");
        auto *pvLay = new QVBoxLayout(pvSide);
        pvLay->setContentsMargins(8, 8, 8, 8);
        pvLay->setSpacing(0);
        previewIcon = new QLabel();
        previewIcon->setFixedSize(80, 80);
        previewIcon->setAlignment(Qt::AlignCenter);
        previewIcon->setStyleSheet("background:transparent; border:none;");
        previewIcon->setFrameShape(QFrame::NoFrame);
        previewIcon->setScaledContents(true);
        pvLay->addStretch();
        pvLay->addWidget(previewIcon, 0, Qt::AlignCenter);
        pvLay->addStretch();

        auto *divContainer = new QWidget();
        auto *divLay = new QVBoxLayout(divContainer);
        divLay->setContentsMargins(0, 12, 0, 12);
        auto *div = new QWidget(); div->setFixedWidth(1); div->setStyleSheet("background:#161a22;");
        divLay->addWidget(div);

        auto *infoSide = new QWidget();
        infoSide->setStyleSheet("background:#2e3440;");
        auto *infoLay = new QVBoxLayout(infoSide);
        infoLay->setContentsMargins(8, 8, 8, 8);
        infoLay->setSpacing(4);
        previewInfo = new QLabel();
        previewInfo->setStyleSheet("color:#ccd4e8;font-size:10px;background:transparent;border:none;");
        previewInfo->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        previewInfo->setWordWrap(true);
        previewInfo->setFrameShape(QFrame::NoFrame);
        previewInfo->setTextFormat(Qt::RichText);
        infoLay->addWidget(previewInfo);
        infoLay->addStretch();

        cLay->addWidget(pvSide, 1);
        cLay->addWidget(divContainer);
        cLay->addWidget(infoSide, 1);
        m_mainLay->addWidget(m_content, 1);
    }

    void setExpanded(bool on) {
        if (m_expanded == on) return;
        m_expanded = on;
        if (on) { m_content->show(); setFixedHeight(OPEN_H); }
        else    { m_content->hide(); setFixedHeight(CLOSED_H); }
        update();
        if (onHeightChanged) onHeightChanged();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj != m_barRow) return false;

        if (ev->type() == QEvent::Paint) {
            QPainter p(m_barRow);
            p.setRenderHint(QPainter::Antialiasing);
            const int cx = m_barRow->width() / 2;
            const int cy = m_barRow->height() / 2;

            p.setPen(QPen(QColor(255, 255, 255, m_arrowHov ? 140 : 50), 1));
            for (int i = -3; i <= 4; ++i) {
                const int x = cx + i * 4 - 2;
                p.drawLine(x, cy - 2, x, cy + 2);
            }
            p.setPen(QPen(QColor(255, 255, 255, m_arrowHov ? 220 : 100), 1.5,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const int arrowX = cx + 24;
            if (!m_expanded) {
                p.drawLine(arrowX - 4, cy + 2, arrowX, cy - 2);
                p.drawLine(arrowX,     cy - 2, arrowX + 4, cy + 2);
            } else {
                p.drawLine(arrowX - 4, cy - 2, arrowX, cy + 2);
                p.drawLine(arrowX,     cy + 2, arrowX + 4, cy - 2);
            }
        }
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *e = static_cast<QMouseEvent *>(ev);
            if (e->button() != Qt::LeftButton) return false;
            m_pressY = e->globalPosition().toPoint().y();
            m_pressH = height();
            m_dragging = false;
            const int cx = m_barRow->width() / 2;
            const int cy = m_barRow->height() / 2;
            m_clickOnArrow = QRect(cx + 16, cy - 8, 20, 16).contains(e->pos());
            m_barRow->grabMouse();
        }
        if (ev->type() == QEvent::MouseMove) {
            auto *e = static_cast<QMouseEvent *>(ev);
            if (!(e->buttons() & Qt::LeftButton)) {
                const int cx = m_barRow->width() / 2;
                const int cy = m_barRow->height() / 2;
                const bool ah = QRect(cx - 20, cy - 10, 60, 20).contains(e->pos());
                if (ah != m_arrowHov) { m_arrowHov = ah; m_barRow->update(); }
                return false;
            }
            if (m_clickOnArrow) return false;
            const int dy = m_pressY - e->globalPosition().toPoint().y();
            if (qAbs(dy) > 3) {
                m_dragging = true;
                const int newH = qBound((int)CLOSED_H, m_pressH + dy, 320);
                if (newH > (int)CLOSED_H + 10 && !m_expanded) { m_expanded = true; m_content->show(); }
                setFixedHeight(newH);
                m_barRow->update();
                if (onHeightChanged) onHeightChanged();
            }
        }
        if (ev->type() == QEvent::MouseButtonRelease) {
            auto *e = static_cast<QMouseEvent *>(ev);
            if (e->button() != Qt::LeftButton) return false;
            m_barRow->releaseMouse();
            if (m_clickOnArrow && !m_dragging) setExpanded(!m_expanded);
            if (m_dragging && height() < (int)CLOSED_H + 20) setExpanded(false);
            m_dragging = m_clickOnArrow = false;
        }
        return false;
    }

private:
    enum { CLOSED_H = 22, OPEN_H = 160 };
    QVBoxLayout *m_mainLay     = nullptr;
    QWidget     *m_barRow      = nullptr;
    QWidget     *m_content     = nullptr;
    bool         m_expanded    = false;
    bool         m_dragging    = false;
    bool         m_clickOnArrow = false;
    bool         m_arrowHov    = false;
    int          m_pressY      = 0;
    int          m_pressH      = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// PaneToolbar
// ─────────────────────────────────────────────────────────────────────────────
PaneToolbar::PaneToolbar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(96);
    setAttribute(Qt::WA_StyledBackground, true);

    // KORREKTUR: Hintergrundfarbe auf #202530 gesetzt (Sidebar-Box-Farbe)
    setStyleSheet("PaneToolbar { background:#202530; border-bottom:1px solid #1e2330; }");

    auto *vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(12, 10, 12, 10);
    vlay->setSpacing(6);

    auto mk = [&](const QString &icon, const QString &tip, auto sig) -> QToolButton * {
        auto *b = new QToolButton();
        b->setIcon(QIcon::fromTheme(icon));
        b->setIconSize(QSize(16, 16));
        b->setFixedSize(28, 28);
        b->setToolTip(tip);
        b->setStyleSheet(SS_ACTION_BTN);
        connect(b, &QToolButton::clicked, this, sig);
        return b;
    };

    // Row 1: Pfad | Aktionen
    auto *r1 = new QHBoxLayout();
    r1->setContentsMargins(0, 0, 0, 0); r1->setSpacing(2);
    m_pathLabel = new QLabel();
    m_pathLabel->setStyleSheet("color:#88c0d0;font-size:18px;font-weight:300;background:transparent;");
    r1->addWidget(m_pathLabel);
    r1->addStretch(1);
    r1->addWidget(mk("view-sort-ascending", tr("Sortieren"),  &PaneToolbar::sortClicked));
    r1->addWidget(mk("view-list-details",   tr("Ansicht"),    &PaneToolbar::sortClicked));
    r1->addWidget(mk("folder-new",          tr("Neu"),        &PaneToolbar::newFolderClicked));
    r1->addWidget(mk("edit-copy",           tr("Kopieren"),   &PaneToolbar::sortClicked));
    r1->addWidget(mk("system-run",          tr("Aktionen"),   &PaneToolbar::actionsClicked));
    vlay->addLayout(r1);

    // Row 2: Anzahl | Größe
    auto *r2 = new QHBoxLayout();
    r2->setContentsMargins(0, 0, 0, 0); r2->setSpacing(8);
    m_countLabel = new QLabel();
    m_countLabel->setStyleSheet("color:#d8dee9;font-size:11px;background:transparent;");
    m_selectedLabel = new QLabel();
    m_selectedLabel->setStyleSheet("color:#5e81ac;font-size:11px;background:transparent;");
    m_selectedLabel->hide();
    m_sizeLabel = new QLabel();
    m_sizeLabel->setStyleSheet("color:#d8dee9;font-size:11px;background:transparent;");
    m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    r2->addWidget(m_countLabel); r2->addWidget(m_selectedLabel);
    r2->addStretch(1); r2->addWidget(m_sizeLabel);
    vlay->addLayout(r2);

    // Row 3: Navigation | Ansichtsmodi
    auto *r3 = new QHBoxLayout();
    r3->setContentsMargins(0, 0, 0, 0); r3->setSpacing(2);
    r3->addWidget(mk("go-previous", tr("Zurück"),   &PaneToolbar::backClicked));
    r3->addWidget(mk("go-next",     tr("Vorwärts"), &PaneToolbar::forwardClicked));
    r3->addWidget(mk("go-up",       tr("Hoch"),     &PaneToolbar::upClicked));

    auto *foldersFirstBtn = new QToolButton();
    foldersFirstBtn->setIcon(QIcon::fromTheme("go-parent-folder"));
    foldersFirstBtn->setIconSize(QSize(16, 16));
    foldersFirstBtn->setFixedSize(28, 28);
    foldersFirstBtn->setCheckable(true); foldersFirstBtn->setChecked(true);
    foldersFirstBtn->setToolTip(tr("Ordner zuerst"));
    foldersFirstBtn->setStyleSheet(SS_ACTION_BTN);
    connect(foldersFirstBtn, &QToolButton::toggled, this, &PaneToolbar::foldersFirstToggled);
    r3->addWidget(foldersFirstBtn);
    r3->addStretch(1);

    auto *viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    connect(viewGroup, &QButtonGroup::idClicked, this, &PaneToolbar::viewModeChanged);

    int modeId = 0;
    for (auto &v : {std::pair<const char *, const char *>
            {"view-list-tree",    "Details"},
            {"view-list-details", "Kompakt"},
            {"view-list-icons",   "Liste"},
            {"view-grid",         "Grid"}}) {
        auto *b = new QToolButton();
        b->setIcon(QIcon::fromTheme(v.first));
        b->setIconSize(QSize(16, 16)); b->setFixedSize(28, 28);
        b->setToolTip(v.second); b->setStyleSheet(SS_ACTION_BTN);
        b->setCheckable(true);
        if (modeId == 0) b->setChecked(true);
        viewGroup->addButton(b, modeId++);
        r3->addWidget(b);
    }
    vlay->addLayout(r3);
}

void PaneToolbar::setPath(const QString &path)
{
    if (!m_pathLabel) return;
    QString name = QDir(path).dirName();
    if (name.isEmpty()) name = path;
    m_pathLabel->setText(name);
}

void PaneToolbar::setCount(int count, qint64 totalBytes)
{
    if (m_countLabel) m_countLabel->setText(tr("%1 Elemente").arg(count));
    if (m_sizeLabel) {
        if      (totalBytes < 1024)        m_sizeLabel->setText(QString("%1 B").arg(totalBytes));
        else if (totalBytes < 1024 * 1024) m_sizeLabel->setText(QString("%1 KB").arg(totalBytes / 1024));
        else                               m_sizeLabel->setText(QString("%1 MB").arg(totalBytes / (1024 * 1024)));
    }
}

void PaneToolbar::setSelected(int count)
{
    if (!m_selectedLabel) return;
    if (count > 0) { m_selectedLabel->setText(tr("%1 ausgewählt").arg(count)); m_selectedLabel->show(); }
    else             m_selectedLabel->hide();
}

// ─────────────────────────────────────────────────────────────────────────────
// MillerColumn
// ─────────────────────────────────────────────────────────────────────────────
MillerColumn::MillerColumn(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(120);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_header = new QPushButton();
    m_header->setFlat(true);
    m_header->setFixedHeight(26);
    m_header->setStyleSheet(
        "QPushButton{background:#2e3440;border:none;border-bottom:1px solid #222733;"
        "color:#88c0d0;font-weight:bold;font-size:11px;padding:0 8px;text-align:left;}"
        "QPushButton:hover{background:#3b4252;color:#d8dee9;}");
    lay->addWidget(m_header);
    connect(m_header, &QPushButton::clicked, this, [this]() {
        emit headerClicked(m_path == "__drives__" ? QString() : m_path);
    });

    m_list = new QListWidget();
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setStyleSheet(SS_COL_INACTIVE);
    lay->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        emit activated(this);
        const QString p = it->data(Qt::UserRole).toString();
        if (!p.isEmpty()) emit entryClicked(p, this);
    });
}

void MillerColumn::populateDrives()
{
    m_path = "__drives__";
    m_header->setText("This PC");
    m_list->clear();
    m_list->setStyleSheet(SS_COL_DRIVES);

    QSet<QString> shown;
    for (const Solid::Device &dev : Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess)) {
        const auto *acc = dev.as<Solid::StorageAccess>();
        if (!acc || !acc->isAccessible()) continue;
        const QString p = acc->filePath();
        if (p.isEmpty() || shown.contains(p)) continue;
        if (p.startsWith("/boot") || p.startsWith("/efi") || p.startsWith("/snap")) continue;
        shown.insert(p);

        QString driveName = (p == "/") ? QStringLiteral("Fedora") : dev.description();
        if (driveName.isEmpty()) driveName = QDir(p).dirName();

        QString iconName = "drive-harddisk";
        if (const auto *drv = dev.as<Solid::StorageDrive>()) {
            if (drv->driveType() == Solid::StorageDrive::CdromDrive)
                iconName = "drive-optical";
            else if (drv->isRemovable() || p.startsWith("/run/media/"))
                iconName = "drive-removable-media";
        }

        QStorageInfo si(p);
        QString sizeText;
        if (si.isValid() && si.bytesTotal() > 0) {
            double totalGB = si.bytesTotal() / (1024.0 * 1024 * 1024);
            double freeGB  = si.bytesFree()  / (1024.0 * 1024 * 1024);
            double usedGB  = totalGB - freeGB;

            // KORREKTUR: usedGB ist Weiß (#ffffff)
            // totalGB bekommt die Farbe der Ordner-Header: #88c0d0
            sizeText = QString("   <span style='color:#ffffff;'>%1 / </span>"
            "<span style='color:#88c0d0;'>%2 GB</span>")
            .arg(usedGB, 0, 'f', 0)
            .arg(totalGB, 0, 'f', 0);
        }

        auto *it = new QListWidgetItem(m_list);
        it->setData(Qt::UserRole, p);
        it->setSizeHint(QSize(0, 30));

        QLabel *label = new QLabel(m_list);
        QString fullHtml = QString("<html><div style='color:#ffffff;'>%1 %2</div></html>")
        .arg(driveName)
        .arg(sizeText);

        label->setText(fullHtml);
        label->setStyleSheet("background:transparent; padding-left: 5px;");

        it->setIcon(QIcon::fromTheme(iconName));
        m_list->setItemWidget(it, label);
    }
}

void MillerColumn::populateDir(const QString &path)
{
    m_path = path;

    // KORREKTUR: Wenn der Pfad "/" ist (Wurzelverzeichnis), setze den Namen auf "Fedora"
    QString name = QDir(path).dirName();
    if (name.isEmpty() && path == "/") {
        name = QStringLiteral("Fedora");
    }
    m_header->setText(name);

    m_list->clear();
    m_list->setStyleSheet(SS_COL_INACTIVE);

    QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;
    {
        QSettings gs("SplitCommander", "General");
        if (gs.value("showHidden", false).toBool()) filters |= QDir::Hidden;
    }
    QFileIconProvider iconProv;
    for (const QFileInfo &fi : QDir(path).entryInfoList(filters, QDir::Name | QDir::DirsFirst)) {
        auto *it = new QListWidgetItem(iconProv.icon(fi), fi.fileName());
        it->setData(Qt::UserRole, fi.absoluteFilePath());
        m_list->addItem(it);
    }
}

// Diese Funktion fehlte beim letzten Build und verursachte den Linker-Fehler
void MillerColumn::setActive(bool active)
{
    m_list->setStyleSheet(
        m_path == "__drives__" ? SS_COL_DRIVES
        : (active ? SS_COL_ACTIVE : SS_COL_INACTIVE));
}

// ─────────────────────────────────────────────────────────────────────────────
// MillerArea
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int FULL_COLS = 3;

MillerArea::MillerArea(QWidget *parent) : QWidget(parent)
{
    setStyleSheet("background:#2e3440;");
    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    m_rowWidget = new QWidget();
    m_rowWidget->setStyleSheet("background:#2e3440;");
    m_rowLayout = new QHBoxLayout(m_rowWidget);
    m_rowLayout->setContentsMargins(0, 0, 0, 0);
    m_rowLayout->setSpacing(0);

    m_stripDivider = new QFrame();
    m_stripDivider->setFrameShape(QFrame::VLine);
    m_stripDivider->setStyleSheet("background:#4c566a; color:#4c566a;");
    m_stripDivider->setFixedWidth(2);
    m_stripDivider->setFrameShadow(QFrame::Sunken);
    m_stripDivider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_stripDivider->setVisible(false);
    m_rowLayout->addWidget(m_stripDivider);

    m_colContainer = new QWidget();
    m_colContainer->setStyleSheet("background:#2e3440;");
    m_colLayout = new QHBoxLayout(m_colContainer);
    m_colLayout->setContentsMargins(0, 0, 0, 0);
    m_colLayout->setSpacing(0);
    m_rowLayout->addWidget(m_colContainer, 1);

    outerLay->addWidget(m_rowWidget, 1);
}

void MillerArea::updateVisibleColumns()
{
    const int n          = m_cols.size();
    const int stripCount = qMax(0, n - FULL_COLS);

    // 1. Alte Strips bereinigen
    for (auto *s : m_strips) {
        m_rowLayout->removeWidget(s);
        s->deleteLater();
    }
    m_strips.clear();

    // 2. Neue Strips für ausgeblendete Spalten
    for (int i = 0; i < stripCount; ++i) {
        QString label = (i == 0) ? QStringLiteral("This PC") : QDir(m_cols[i]->path()).dirName();
        if (label.isEmpty()) label = m_cols[i]->path();

        auto *strip = new MillerStrip(label, m_rowWidget);
        m_rowLayout->insertWidget(i, strip);
        m_strips.append(strip);

        connect(strip, &MillerStrip::clicked, this, [this, i]() {
            emit focusRequested();
            while (m_cols.size() > i + 1) {
                trimAfter(m_cols[i]);
            }
            updateVisibleColumns();
        });
    }

    // 3. RADIKALE LÖSUNG: Alle alten Trenner physisch löschen
    for (auto *sep : m_colSeparators) {
        m_colLayout->removeWidget(sep);
        sep->deleteLater();
    }
    m_colSeparators.clear();

    m_stripDivider->setVisible(stripCount > 0);

    // 4. Spalten zeigen und Trenner FRISCH einfügen
    for (int i = 0; i < n; ++i) {
        const bool vis = (i >= n - FULL_COLS);
        m_cols[i]->setVisible(vis);
        m_colLayout->setStretchFactor(m_cols[i], vis ? 1 : 0);

        // Trenner nur erzeugen, wenn die Spalte sichtbar ist UND nicht die erste sichtbare ist
        if (vis && i > stripCount) {
            QFrame *sep = new QFrame(m_colContainer);
            sep->setFixedWidth(1);
            sep->setStyleSheet("background: #222733; border: none;");

            // Trenner im Layout direkt vor der Spalte platzieren
            int layoutIdx = m_colLayout->indexOf(m_cols[i]);
            m_colLayout->insertWidget(layoutIdx, sep);
            m_colSeparators.append(sep);
        }
    }
}

void MillerArea::refreshDrives()
{
    if (m_cols.isEmpty()) return;
    m_cols[0]->populateDrives();
    updateVisibleColumns();
}

void MillerArea::init()
{
    auto *col = new MillerColumn();
    col->populateDrives();
    m_colLayout->addWidget(col, 1);
    m_cols.append(col);
    m_activeCol = col;

    connect(col, &MillerColumn::entryClicked, this, [this, col](const QString &path, MillerColumn *src) {
        emit focusRequested();
        trimAfter(src);
        for (auto *c : m_cols) c->setActive(false);
        src->setActive(true); m_activeCol = src;
        appendColumn(path); emit pathChanged(path);
    });
    connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
        emit focusRequested();
        for (auto *c : m_cols) c->setActive(false);
        src->setActive(true); m_activeCol = src;
    });
    connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
}

void MillerArea::appendColumn(const QString &path)
{
    // DER BLOCK FÜR DEN TRENNER (QFrame) WURDE HIER ENTFERNT

    auto *col = new MillerColumn();
    col->populateDir(path); col->setActive(true);
    m_colLayout->addWidget(col, 1);
    m_cols.append(col);

    connect(col, &MillerColumn::entryClicked, this, [this, col](const QString &p2, MillerColumn *src) {
        emit focusRequested();
        trimAfter(src);
        for (auto *c : m_cols) c->setActive(false);
        src->setActive(true); m_activeCol = src;
        if (QFileInfo(p2).isDir()) appendColumn(p2);
        emit pathChanged(p2);
    });
    connect(col, &MillerColumn::activated, this, [this](MillerColumn *src) {
        emit focusRequested();
        for (auto *c : m_cols) c->setActive(false);
        src->setActive(true); m_activeCol = src;
    });
    connect(col, &MillerColumn::headerClicked, this, &MillerArea::headerClicked);
    updateVisibleColumns();
}

void MillerArea::trimAfter(MillerColumn *col)
{
    const int idx = m_cols.indexOf(col);
    if (idx < 0) return;
    while (m_cols.size() > idx + 1) {
        auto *last = m_cols.takeLast();
        m_colLayout->removeWidget(last);
        last->deleteLater();

        // Zugehörigen Trenner ebenfalls entfernen
        if (!m_colSeparators.isEmpty()) {
            auto *sep = m_colSeparators.takeLast();
            m_colLayout->removeWidget(sep);
            sep->deleteLater();
        }
    }
    updateVisibleColumns();
}

void MillerArea::redistributeWidths() {}
void MillerArea::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); }

QString MillerArea::activePath() const
{
    return m_activeCol ? m_activeCol->path() : QString();
}

void MillerArea::navigateTo(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    while (m_cols.size() > 1) {
        auto *last = m_cols.takeLast();
        m_colLayout->removeWidget(last);
        last->deleteLater();
    }

    // Pfad-Segmente aufbauen
    QStringList segments;
    QString cur = path;
    while (true) {
        segments.prepend(cur);
        QDir parent(cur);
        if (!parent.cdUp()) break;
        const QString up = parent.absolutePath();
        if (up == cur) break;
        cur = up;
    }

    // Laufwerk selektieren
    if (!m_cols.isEmpty()) {
        QListWidget *driveList = m_cols[0]->list();
        for (int i = 0; i < driveList->count(); ++i) {
            const QString drivePath = driveList->item(i)->data(Qt::UserRole).toString();
            if (!drivePath.isEmpty() && path.startsWith(drivePath)) {
                driveList->setCurrentRow(i);
                m_cols[0]->setActive(true);
                break;
            }
        }
    }

    const QString targetDir = QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath();

    for (int i = 1; i < segments.size(); ++i) {
        const QString seg = segments[i - 1];
        if (!QFileInfo(seg).isDir()) continue;
        appendColumn(seg);
        if (!m_cols.isEmpty()) {
            MillerColumn *col  = m_cols.last();
            const QString next = segments[i];
            for (int r = 0; r < col->list()->count(); ++r) {
                if (col->list()->item(r)->data(Qt::UserRole).toString() == next) {
                    col->list()->setCurrentRow(r); break;
                }
            }
        }
    }

    if (QFileInfo(targetDir).isDir() && (m_cols.isEmpty() || m_cols.last()->path() != targetDir))
        appendColumn(targetDir);

    updateVisibleColumns();
}

void MillerArea::setFocused(bool f)
{
    m_focused = f;
    setStyleSheet("background:#2e3440;");
}

// ─────────────────────────────────────────────────────────────────────────────
// PaneWidget
// ─────────────────────────────────────────────────────────────────────────────
PaneWidget::PaneWidget(QWidget *parent) : QWidget(parent)
{
    setStyleSheet("background:#181c26;");
    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ── Tab-Leiste mit Breadcrumb ──
    auto *tabBar = new QWidget();
    tabBar->setFixedHeight(36);
    tabBar->setStyleSheet("background:#161a22; border-bottom:1px solid #222733;");
    auto *tabLay = new QHBoxLayout(tabBar);
    tabLay->setContentsMargins(4, 0, 4, 0);
    tabLay->setSpacing(2);

    auto *pathStack = new QStackedWidget();
    pathStack->setStyleSheet("background:transparent;");
    pathStack->setFixedHeight(26);

    m_pathEdit = new QLineEdit(QDir::homePath());
    m_pathEdit->setStyleSheet(
        "QLineEdit{background:#0f1218;border:1px solid #5e81ac;color:#ccd4e8;"
        "font-size:11px;padding:2px 6px;border-radius:2px;}");

    auto *breadcrumbBtn = new QPushButton(QDir::homePath());
    breadcrumbBtn->setFlat(true);
    breadcrumbBtn->setStyleSheet(
        "QPushButton{background:#0f1218;color:#ccd4e8;font-size:11px;"
        "padding:2px 6px;border-radius:2px;text-align:left;border:none;}"
        "QPushButton:hover{background:#1e2330;}");
    pathStack->addWidget(breadcrumbBtn);
    pathStack->addWidget(m_pathEdit);
    pathStack->setCurrentIndex(0);

    connect(breadcrumbBtn, &QPushButton::clicked, this, [pathStack, this]() {
        m_pathEdit->setText(currentPath());
        m_pathEdit->selectAll();
        pathStack->setCurrentIndex(1);
        m_pathEdit->setFocus();
    });

    auto commitPath = [pathStack, breadcrumbBtn, this]() {
        const QString p = m_pathEdit->text().trimmed();
        if (!p.isEmpty() && QFileInfo::exists(p)) navigateTo(p);
        breadcrumbBtn->setText(currentPath());
        pathStack->setCurrentIndex(0);
    };
    connect(m_pathEdit, &QLineEdit::returnPressed,   this, commitPath);
    connect(m_pathEdit, &QLineEdit::editingFinished, this, [pathStack, commitPath]() {
        if (pathStack->currentIndex() == 1) commitPath();
    });
    connect(this, &PaneWidget::pathUpdated, this, [breadcrumbBtn](const QString &p) {
        breadcrumbBtn->setText(p);
    });

    auto *millerToggle = new QToolButton();
    millerToggle->setFixedSize(24, 24);
    millerToggle->setCheckable(true); millerToggle->setChecked(true);
    millerToggle->setIcon(QIcon::fromTheme("go-up"));
    millerToggle->setIconSize(QSize(14, 14));
    millerToggle->setToolTip(tr("Miller-Columns ein-/ausklappen"));
    millerToggle->setStyleSheet(SS_TOOL_BTN);

    auto *searchBtn = new QToolButton();
    searchBtn->setFixedSize(24, 24);
    searchBtn->setIcon(QIcon::fromTheme("system-search"));
    searchBtn->setIconSize(QSize(14, 14));
    searchBtn->setToolTip(tr("Suchen"));
    searchBtn->setCheckable(true);
    searchBtn->setStyleSheet(SS_TOOL_BTN);

    tabLay->addWidget(millerToggle);
    tabLay->addWidget(pathStack, 1);
    tabLay->addWidget(searchBtn);
    rootLay->addWidget(tabBar);

    // ── Such-Panel ──
    auto *searchPanel = new QWidget();
    searchPanel->setStyleSheet("background:#161a22;");
    searchPanel->hide();
    auto *spVLay = new QVBoxLayout(searchPanel);
    spVLay->setContentsMargins(0, 0, 0, 0);
    spVLay->setSpacing(0);

    auto *spTopRow = new QWidget();
    spTopRow->setFixedHeight(36);
    spTopRow->setStyleSheet("background:#161a22; border-bottom:1px solid #222733;");
    auto *spLay = new QHBoxLayout(spTopRow);
    spLay->setContentsMargins(6, 4, 6, 4);
    spLay->setSpacing(4);

    auto *searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText(tr("Suchen ..."));
    searchEdit->setStyleSheet(
        "QLineEdit{background:#0f1218;border:1px solid #5e81ac;color:#ccd4e8;"
        "font-size:11px;padding:2px 6px;border-radius:2px;}");
    searchEdit->setClearButtonEnabled(true);

    auto *filterBtn = new QToolButton();
    filterBtn->setText(tr("Filtern"));
    filterBtn->setIcon(QIcon::fromTheme("view-filter"));
    filterBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    filterBtn->setIconSize(QSize(14, 14));
    filterBtn->setPopupMode(QToolButton::MenuButtonPopup);
    filterBtn->setStyleSheet(
        "QToolButton{background:#1e2330;border:1px solid #2c3245;color:#ccd4e8;"
        "font-size:11px;padding:2px 6px;border-radius:2px;}"
        "QToolButton:hover{border-color:#5e81ac;}"
        "QToolButton::menu-indicator{image:none;}"
        "QToolButton::menu-button{border-left:1px solid #2c3245;width:14px;}");

    auto *filterMenu = new QMenu(filterBtn);
    mw_applyMenuShadow(filterMenu);
    filterMenu->setStyleSheet(
        "QMenu{background:#2e3440;border:1px solid #434c5e;color:#eceff4;font-size:11px;}"
        "QMenu::item{padding:6px 16px;} QMenu::item:selected{background:#434c5e;}"
        "QMenu::separator{background:rgba(236,239,244,120);height:1px;margin:4px 8px;}");
    auto *actNames   = filterMenu->addAction(tr("Dateinamen"));
    auto *actContent = filterMenu->addAction(tr("Dateiinhalt"));
    actNames->setCheckable(true);   actNames->setChecked(true);
    actContent->setCheckable(true);
    auto *filterGroup = new QActionGroup(filterMenu);
    filterGroup->addAction(actNames); filterGroup->addAction(actContent);
    filterGroup->setExclusive(true);
    filterMenu->addSeparator();
    filterMenu->addAction(QIcon::fromTheme("system-search"), tr("KFind öffnen"),
                          this, []() { QProcess::startDetached("kfind", {}); });
    filterMenu->addAction(QIcon::fromTheme("configure"), tr("Sucheinstellungen"),
                          this, []() { QProcess::startDetached("kcmshell6", {"kcm_baloofile"}); });
    filterBtn->setMenu(filterMenu);

    auto *searchByName = new bool(true);
    connect(actNames,   &QAction::toggled, this, [searchByName](bool on) { *searchByName =  on; });
    connect(actContent, &QAction::toggled, this, [searchByName](bool on) { *searchByName = !on; });

    auto *searchCloseBtn = new QToolButton();
    searchCloseBtn->setIcon(QIcon::fromTheme("window-close"));
    searchCloseBtn->setIconSize(QSize(12, 12)); searchCloseBtn->setFixedSize(20, 20);
    searchCloseBtn->setStyleSheet(
        "QToolButton{background:transparent;border:none;color:#bf616a;border-radius:10px;}"
        "QToolButton:hover{background:#bf616a;color:#eceff4;}");

    spLay->addWidget(searchEdit, 1); spLay->addWidget(filterBtn); spLay->addWidget(searchCloseBtn);
    spVLay->addWidget(spTopRow);

    auto *spTabRow = new QWidget();
    spTabRow->setFixedHeight(28);
    spTabRow->setStyleSheet("background:#161a22; border-bottom:1px solid #222733;");
    spTabRow->hide();
    auto *spTabLay = new QHBoxLayout(spTabRow);
    spTabLay->setContentsMargins(6, 0, 6, 0); spTabLay->setSpacing(0);
    auto mkTab = [](const QString &lbl) {
        auto *b = new QToolButton(); b->setText(lbl); b->setCheckable(true);
        b->setStyleSheet(
            "QToolButton{background:transparent;border:none;color:#4c566a;"
            "font-size:11px;padding:2px 10px;border-bottom:2px solid transparent;}"
            "QToolButton:checked{color:#88c0d0;border-bottom:2px solid #5e81ac;}"
            "QToolButton:hover{color:#ccd4e8;}");
        return b;
    };
    auto *tabHere    = mkTab(tr("Ab hier"));
    auto *tabOverall = mkTab(tr("Überall"));
    tabOverall->setChecked(true);
    auto *tabGrp = new QButtonGroup(spTabRow);
    tabGrp->addButton(tabHere); tabGrp->addButton(tabOverall); tabGrp->setExclusive(true);
    spTabLay->addWidget(tabHere); spTabLay->addWidget(tabOverall); spTabLay->addStretch();
    spVLay->addWidget(spTabRow);
    rootLay->addWidget(searchPanel);

    // ── Suchergebnis-Overlay ──
    auto *searchOverlay = new QWidget(this);
    searchOverlay->hide();
    searchOverlay->setStyleSheet("background:#0f1218; border:1px solid #222733; border-top:none;");
    auto *ovLay = new QVBoxLayout(searchOverlay);
    ovLay->setContentsMargins(0, 0, 0, 0); ovLay->setSpacing(0);

    auto *searchResults = new QTreeWidget(searchOverlay);
    searchResults->setHeaderLabels({tr("Name"), tr("Pfad"), tr("Geändert")});
    searchResults->setRootIsDecorated(false);
    searchResults->setStyleSheet(
        "QTreeWidget{background:#0f1218;border:none;color:#ccd4e8;font-size:11px;outline:none;}"
        "QTreeWidget::item{padding:4px 4px;border-bottom:1px solid #161a22;}"
        "QTreeWidget::item:selected{background:#3b4252;color:#88c0d0;}"
        "QTreeWidget::item:hover{background:#1e2330;}"
        "QHeaderView::section{background:#161a22;color:#4c566a;border:none;"
        "border-bottom:1px solid #222733;padding:3px 8px;font-size:10px;}"
        "QTreeWidget QScrollBar:vertical{width:4px;background:transparent;border:none;}"
        "QTreeWidget QScrollBar::handle:vertical{background:rgba(255,255,255,0);border-radius:2px;min-height:20px;}"
        "QTreeWidget:hover QScrollBar::handle:vertical{background:rgba(255,255,255,40);}"
        "QTreeWidget QScrollBar::add-line:vertical,QTreeWidget QScrollBar::sub-line:vertical{height:0;}"
        "QTreeWidget QScrollBar:horizontal{height:4px;background:transparent;border:none;}"
        "QTreeWidget QScrollBar::handle:horizontal{background:rgba(255,255,255,0);border-radius:2px;min-width:20px;}"
        "QTreeWidget:hover QScrollBar::handle:horizontal{background:rgba(255,255,255,40);}"
        "QTreeWidget QScrollBar::add-line:horizontal,QTreeWidget QScrollBar::sub-line:horizontal{width:0;}");
    searchResults->header()->setStretchLastSection(false);
    searchResults->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    searchResults->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    searchResults->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ovLay->addWidget(searchResults, 1);

    // Suchpanel-Verbindungen
    connect(searchBtn, &QToolButton::toggled, this,
            [searchPanel, searchEdit, spTabRow, searchOverlay](bool on) {
        searchPanel->setVisible(on);
        if (on) { searchEdit->clear(); searchEdit->setFocus(); }
        else    { spTabRow->hide(); searchOverlay->hide(); }
    });
    connect(searchCloseBtn, &QToolButton::clicked, this, [searchBtn, this]() {
        searchBtn->setChecked(false);
        m_filePane->setNameFilter(QString());
    });
    connect(searchEdit, &QLineEdit::textChanged, this, [this, searchByName](const QString &text) {
        if (!*searchByName) return;
        m_filePane->setNameFilter(text);
    });
    connect(searchEdit, &QLineEdit::returnPressed, this,
            [this, searchEdit, searchResults, searchOverlay, spTabRow]() {
        const QString term = searchEdit->text().trimmed();
        if (term.isEmpty()) return;
        searchResults->clear();
        auto *loading = new QTreeWidgetItem(searchResults);
        loading->setText(0, tr("Suche läuft..."));

        const QPoint topLeft = m_vSplit->mapTo(this, QPoint(0, 0));
        searchOverlay->setGeometry(topLeft.x(), topLeft.y(), m_vSplit->width(),
                                   qMin(300, m_vSplit->height()));
        searchOverlay->show(); searchOverlay->raise(); spTabRow->show();

        auto *proc = new QProcess(this);
        proc->setProgram("baloosearch6");
        proc->setArguments({term});
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, searchResults](int, QProcess::ExitStatus) {
            const QString out = proc->readAllStandardOutput().trimmed();
            proc->deleteLater();
            searchResults->clear();
            if (out.isEmpty()) {
                auto *empty = new QTreeWidgetItem(searchResults);
                empty->setText(0, QObject::tr("Keine Ergebnisse"));
                return;
            }
            QFileIconProvider ip;
            for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
                const QString path = line.trimmed();
                if (!QFileInfo::exists(path)) continue;
                const QFileInfo fi(path);
                auto *it = new QTreeWidgetItem(searchResults);
                it->setIcon(0, ip.icon(fi));
                it->setText(0, fi.fileName());
                it->setText(1, QString("~/%1").arg(QDir::home().relativeFilePath(fi.absolutePath())));
                it->setText(2, fi.lastModified().toString("dd.MM.yy"));
                it->setData(0, Qt::UserRole, path);
            }
            if (searchResults->topLevelItemCount() == 0) {
                auto *empty = new QTreeWidgetItem(searchResults);
                empty->setText(0, QObject::tr("Keine Ergebnisse"));
            }
        });
        proc->start();
    });
    connect(searchResults, &QTreeWidget::itemClicked, this,
            [this, searchBtn, searchOverlay](QTreeWidgetItem *it, int) {
        const QString path = it->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;
        navigateTo(QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath());
        searchOverlay->hide(); searchBtn->setChecked(false);
    });

    // ── Vertikaler Splitter: Miller | Dateiliste ──
    m_vSplit = new QSplitter(Qt::Vertical);
    m_vSplit->setChildrenCollapsible(true);
    m_vSplit->setHandleWidth(4);
    m_vSplit->setStyleSheet(
        "QSplitter::handle { background:#232938; }"
        "QSplitter::handle:hover { background:#4c566a; }"
        "QSplitter { background:#1e2330; }");
    rootLay->addWidget(m_vSplit, 1);
    m_vSplit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_vSplit->setMinimumHeight(200);

    m_miller = new MillerArea();
    m_miller->setMinimumHeight(150);
    m_miller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_vSplit->addWidget(m_miller);

    auto *lowerWidget = new QWidget();
    lowerWidget->setStyleSheet("background:#161a22;");
    auto *lowerLay = new QVBoxLayout(lowerWidget);
    lowerLay->setContentsMargins(0, 0, 0, 0);
    lowerLay->setSpacing(0);
    m_toolbar  = new PaneToolbar();
    m_filePane = new FilePane();
    m_filePane->setStyleSheet("border:none;background:#161a22;");
    lowerLay->addWidget(m_toolbar);
    lowerLay->addWidget(m_filePane, 1);
    m_vSplit->addWidget(lowerWidget);
    m_vSplit->setSizes({200, 450});
    m_vSplit->setStretchFactor(0, 0);
    m_vSplit->setStretchFactor(1, 1);

    connect(millerToggle, &QToolButton::toggled, this, [this, millerToggle](bool checked) {
        if (checked) {
            m_vSplit->setSizes({200, 450});
            millerToggle->setIcon(QIcon::fromTheme("go-up"));
            millerToggle->setToolTip("Miller-Columns ausklappen");
        } else {
            m_vSplit->setSizes({0, 1});
            millerToggle->setIcon(QIcon::fromTheme("go-down"));
            millerToggle->setToolTip("Miller-Columns einblenden");
        }
    });

    // ── Verbindungen ──
    connect(m_miller, &MillerArea::pathChanged,     this, [this](const QString &path) {
        if (QFileInfo(path).isDir()) navigateTo(path);
    });
    connect(m_miller, &MillerArea::focusRequested,  this, &PaneWidget::focusRequested);
    connect(m_miller, &MillerArea::headerClicked,   this,
            [pathStack, breadcrumbBtn, this](const QString &path) {
        emit focusRequested();
        m_pathEdit->setText(path.isEmpty() ? currentPath() : path);
        m_pathEdit->selectAll();
        pathStack->setCurrentIndex(1);
        m_pathEdit->setFocus();
    });
    connect(m_filePane, &FilePane::fileSelected,  this, [this](const QString &path) {
        emit focusRequested(); updateFooter(path);
    });
    connect(m_filePane, &FilePane::fileActivated, this, [this](const QString &path) {
        emit focusRequested();
        if (QFileInfo(path).isDir()) navigateTo(path);
    });

    // Toolbar-Verbindungen
    connect(m_toolbar, &PaneToolbar::newFolderClicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Neuer Ordner"), tr("Ordnername:"),
                                             QLineEdit::Normal, tr("Neuer Ordner"), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        if (!QDir(currentPath()).mkdir(name))
            QMessageBox::warning(this, tr("Fehler"), tr("Ordner konnte nicht erstellt werden."));
    });
    connect(m_toolbar, &PaneToolbar::deleteClicked, this, [this]() {
        QModelIndex cur = m_filePane->view()->currentIndex();
        if (!cur.isValid()) return;
        auto *proxy = static_cast<QSortFilterProxyModel *>(m_filePane->view()->model());
        auto *src   = qobject_cast<QStandardItemModel *>(proxy->sourceModel());
        if (!src) return;
        auto *it = src->item(proxy->mapToSource(cur).row(), 0);
        if (!it) return;
        const QString path = it->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;
        if (QMessageBox::question(this, tr("Löschen"),
                tr("'%1' in den Papierkorb?").arg(QFileInfo(path).fileName()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
        auto *job = new KIO::DeleteOrTrashJob({QUrl::fromLocalFile(path)},
            KIO::AskUserActionInterface::Trash,
            KIO::AskUserActionInterface::DefaultConfirmation, this);
        job->start();
    });
    connect(m_toolbar, &PaneToolbar::sortClicked, this, [this]() {
        auto *hdr = m_filePane->view()->header();
        m_filePane->view()->sortByColumn(hdr->sortIndicatorSection(),
            hdr->sortIndicatorOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
    });
    connect(m_toolbar, &PaneToolbar::actionsClicked, this, [this]() {
        QModelIndex cur = m_filePane->view()->currentIndex();
        if (!cur.isValid()) return;
        m_filePane->view()->customContextMenuRequested(
            m_filePane->view()->visualRect(cur).center());
    });
    connect(m_toolbar, &PaneToolbar::upClicked, this, [this]() {
        QDir d(currentPath());
        if (d.cdUp()) navigateTo(d.absolutePath());
    });
    connect(m_toolbar, &PaneToolbar::foldersFirstToggled, this,
            [this](bool on) { m_filePane->setFoldersFirst(on); });
    connect(m_toolbar, &PaneToolbar::viewModeChanged, this,
            [this](int mode) { m_filePane->setViewMode(mode); });
    connect(m_toolbar, &PaneToolbar::backClicked, this, [this]() {
        if (!m_histBack.isEmpty()) { m_histFwd.push(currentPath()); navigateTo(m_histBack.pop()); }
    });
    connect(m_toolbar, &PaneToolbar::forwardClicked, this, [this]() {
        if (!m_histFwd.isEmpty()) { m_histBack.push(currentPath()); navigateTo(m_histFwd.pop()); }
    });
    connect(m_pathEdit, &QLineEdit::returnPressed, this,
            [this]() { navigateTo(m_pathEdit->text()); });

    m_miller->init();
    navigateTo(QDir::homePath());
    buildFooter(rootLay);
}

void PaneWidget::setFocused(bool f)
{
    m_focused = f;
    m_miller->setFocused(f);
    setStyleSheet(f ? "background:#1e2330;" : "background:#161b24;");

    if (auto *d = static_cast<FilePaneDelegate *>(m_filePane->view()->itemDelegate())) {
        d->focused = f;
        m_filePane->view()->viewport()->update();
    }

    // KORREKTUR: Header-Farbe auf Sidebar-Hintergrund #0f1218 gesetzt
    const QString hdrBg = "#0f1218";
    int hh = m_filePane->view()->header()->height();
    if (hh <= 0) hh = 24;

    m_filePane->setStyleSheet("background:#2e3440; border:none;");
    m_filePane->view()->setStyleSheet(
        QString(
            "QTreeView{background:%1;border:none;color:#d8dee9;outline:none;font-size:10px;}"
            "QTreeView::item{padding:2px 4px;}"
            "QTreeView::item:hover{background:#3b4252;}"
            "QTreeView::item:selected{background:#4c566a;color:#eceff4;}"

            // Header-Bereich
            "QHeaderView::section{background:%2;color:#88c0d0;border:none;"
            "border-right:1px solid #1e2330;padding:3px 6px;font-size:10px;}"
            "QHeaderView{background:%2; border:none;}"

            "QTreeView::corner{background:%2; border:none;}"
            "QTreeView QScrollBar:vertical{width:8px;background:#252b36;border:none;margin-top:%3px;}"
            "QTreeView QScrollBar::handle:vertical{background:#434c5e;border-radius:4px;min-height:20px;}"
            "QTreeView:hover QScrollBar::handle:vertical{background:#4c566a;}"
            "QTreeView QScrollBar::sub-line:vertical,QTreeView QScrollBar::add-line:vertical{height:0;}")
        .arg(f ? "#2e3440" : "#252b36")
        .arg(hdrBg) // Setzt #0f1218 ein
        .arg(hh));
}

QString PaneWidget::currentPath() const
{
    return m_pathEdit ? m_pathEdit->text() : QDir::homePath();
}

void PaneWidget::navigateTo(const QString &path)
{
    if (path.isEmpty() || path == "__drives__") return;
    const QString cur = currentPath();
    if (!cur.isEmpty() && cur != path) m_histBack.push(cur);
    m_histFwd.clear();
    m_filePane->setRootPath(path);
    m_pathEdit->setText(path);
    m_toolbar->setPath(path);
    if (!m_miller->cols().isEmpty() && m_miller->cols().size() > 1)
        m_miller->navigateTo(path);

    const QDir dir(path);
    int cnt = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count();
    qint64 sz = 0;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot))
        sz += fi.size();
    m_toolbar->setCount(cnt, sz);
    emit pathUpdated(path);
    updateFooter(path);
}

void PaneWidget::buildFooter(QVBoxLayout *rootLay)
{
    auto *fw = new FooterWidget(this);
    fw->onHeightChanged = [this]() {
        positionFooterPanel();
        updateFooter(currentPath());
    };
    m_footerBar      = fw;
    m_footerCount    = fw->countLbl;
    m_footerSelected = fw->selectedLbl;
    m_footerSize     = fw->sizeLbl;
    m_previewIcon    = fw->previewIcon;
    m_previewInfo    = fw->previewInfo;
    rootLay->addWidget(fw);
}

void PaneWidget::positionFooterPanel()
{
    if (!m_footerBar) return;
    const int h = m_footerBar->height();
    m_footerBar->setGeometry(0, height() - h, width(), h);
    m_footerBar->raise();
}

void PaneWidget::updateFooter(const QString &path)
{
    if (!m_footerCount) return;
    const QFileInfo fi(path);
    if (!fi.exists()) return;

    if (fi.isDir()) {
        const QDir dir(path);
        m_footerCount->setText(tr("%1 Elemente").arg(
            dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count()));
        quint64 sz = 0;
        for (const QFileInfo &e : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot))
            sz += e.size();
        if      (sz < 1024)        m_footerSize->setText(QString("%1 B").arg(sz));
        else if (sz < 1024 * 1024) m_footerSize->setText(QString("%1 KB").arg(sz / 1024));
        else                       m_footerSize->setText(QString("%1 MB").arg(sz / (1024 * 1024)));
        if (m_footerSelected) m_footerSelected->hide();
    } else {
        m_footerCount->setText(QDir(fi.absolutePath()).dirName());
        const quint64 sz = fi.size();
        if      (sz < 1024)        m_footerSize->setText(QString("%1 B").arg(sz));
        else if (sz < 1024 * 1024) m_footerSize->setText(QString("%1 KB").arg(sz / 1024));
        else                       m_footerSize->setText(QString("%1 MB").arg(sz / (1024 * 1024)));
        if (m_footerSelected) { m_footerSelected->setText(tr("1 ausgewählt")); m_footerSelected->show(); }
    }

    if (!m_previewIcon || !m_previewInfo) return;

    const int footerH  = m_footerBar ? m_footerBar->height() : 120;
    const int iconSize = qBound(32, footerH - 40, 300);
    m_previewIcon->setFixedSize(iconSize, iconSize);
    m_previewIcon->setPixmap(QFileIconProvider().icon(fi).pixmap(iconSize, iconSize));

    QString info = "<table cellpadding='1' cellspacing='0' style='color:#d8dee9;font-size:11px;'>";
    auto addRow = [&info](const QString &label, const QString &val) {
        info += QString("<tr><td style='padding-right:20px;white-space:nowrap;'>%1</td>"
                        "<td style='white-space:nowrap;'>%2</td></tr>").arg(label, val);
    };

    addRow("Name",     fi.fileName().toHtmlEscaped());
    addRow("Typ",      fi.isDir() ? tr("Ordner") :
                       fi.suffix().isEmpty() ? tr("Datei") : fi.suffix().toUpper() + tr("-Datei"));
    addRow("Erstellt", fi.birthTime().toString("yyyy-MM-dd  hh:mm"));
    addRow("Geändert", fi.lastModified().toString("yyyy-MM-dd  hh:mm"));

    const qint64 days = fi.lastModified().daysTo(QDateTime::currentDateTime());
    addRow("Alter", days == 0 ? tr("Heute") :
                    days == 1 ? tr("Gestern") :
                    days < 30  ? tr("%1 t").arg(days) :
                    days < 365 ? tr("%1 m").arg(days / 30) : tr("%1 j").arg(days / 365));

    if (!fi.isDir()) {
        addRow("Größe in MB:",  m_footerSize->text());
        addRow("Größe in MiB:", QString("~%1 MiB").arg(fi.size() / (1024.0 * 1024), 0, 'f', 1));
    } else {
        addRow("Elemente", QString::number(QDir(fi.absoluteFilePath())
                               .entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count()));
        addRow("Größe", m_footerSize->text());
    }

    const QFile::Permissions p = fi.permissions();
    QString perm;
    perm += fi.isDir() ? "d" : "-";
    perm += (p & QFile::ReadOwner)  ? "r" : "-"; perm += (p & QFile::WriteOwner) ? "w" : "-"; perm += (p & QFile::ExeOwner)  ? "x" : "-";
    perm += (p & QFile::ReadGroup)  ? "r" : "-"; perm += (p & QFile::WriteGroup) ? "w" : "-"; perm += (p & QFile::ExeGroup)  ? "x" : "-";
    perm += (p & QFile::ReadOther)  ? "r" : "-"; perm += (p & QFile::WriteOther) ? "w" : "-"; perm += (p & QFile::ExeOther)  ? "x" : "-";
    addRow("Attribute", perm);

    info += "</table>";
    m_previewInfo->setText(info);
}

bool PaneWidget::eventFilter(QObject *obj, QEvent *ev)
{
    return QWidget::eventFilter(obj, ev);
}

void PaneWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    positionFooterPanel();
}

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("SplitCommander");
    resize(1280, 900);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLay = new QHBoxLayout(central);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    m_sidebar = new Sidebar(this);
    m_sidebar->setFixedWidth(250);
    rootLay->addWidget(m_sidebar);
    rootLay->addWidget(new SidebarHandle(m_sidebar, central));

    m_panesSplitter = new PaneSplitter(Qt::Horizontal, central);
    m_panesSplitter->setHandleWidth(12);
    m_panesSplitter->setChildrenCollapsible(true);
    m_panesSplitter->setStyleSheet("QSplitter{background:#0a0d14;}");

    m_leftPane  = new PaneWidget();
    m_rightPane = new PaneWidget();
    m_panesSplitter->addWidget(m_leftPane);
    m_panesSplitter->addWidget(m_rightPane);
    rootLay->addWidget(m_panesSplitter, 1);

    // Fokus-Verwaltung
    connect(m_leftPane,  &PaneWidget::focusRequested, this, [this]() {
        m_leftPane->setFocused(true); m_rightPane->setFocused(false);
    });
    connect(m_rightPane, &PaneWidget::focusRequested, this, [this]() {
        m_rightPane->setFocused(true); m_leftPane->setFocused(false);
    });

    // Navigation (inkl. Solid-Mount)
    auto mountAndNavigate = [this](const QString &path, bool leftPane) {
        auto navigate = [this, leftPane](const QString &p) {
            if (leftPane) { m_leftPane->navigateTo(p);  m_leftPane->setFocused(true);  m_rightPane->setFocused(false); }
            else          { m_rightPane->navigateTo(p); m_rightPane->setFocused(true); m_leftPane->setFocused(false); }
        };
        if (path.startsWith("solid:")) {
            Solid::Device dev(path.mid(6));
            auto *acc = dev.as<Solid::StorageAccess>();
            if (!acc) return;
            if (acc->isAccessible()) {
                navigate(acc->filePath());
                m_leftPane->miller()->refreshDrives();
                m_rightPane->miller()->refreshDrives();
                m_sidebar->updateDrives();
            } else {
                connect(acc, &Solid::StorageAccess::setupDone, this,
                        [this, navigate, acc](Solid::ErrorType, QVariant, const QString &) {
                    if (acc->isAccessible()) {
                        navigate(acc->filePath());
                        m_leftPane->miller()->refreshDrives();
                        m_rightPane->miller()->refreshDrives();
                        m_sidebar->updateDrives();
                    }
                }, Qt::SingleShotConnection);
                acc->setup();
            }
        } else {
            navigate(path);
        }
    };

    connect(m_sidebar, &Sidebar::driveClicked, this, [this, mountAndNavigate](const QString &path) {
        mountAndNavigate(path, !m_rightPane->isFocused());
    });
    connect(m_sidebar, &Sidebar::driveClickedRight, this, [mountAndNavigate](const QString &path) {
        mountAndNavigate(path, false);
    });
    connect(m_sidebar, &Sidebar::addCurrentPathToPlaces, this, [this]() {
        m_sidebar->addPlace(m_leftPane->currentPath());
    });
    connect(m_sidebar, &Sidebar::requestActivePath, this, [this](QString *out) {
        if (out) *out = m_leftPane->currentPath();
    });
    connect(m_sidebar, &Sidebar::layoutChangeRequested, this, &MainWindow::applyLayout);
    connect(m_sidebar, &Sidebar::tagClicked, this, [this](const QString &tagName) {
        activePane()->filePane()->showTaggedFiles(tagName);
    });
    connect(m_sidebar, &Sidebar::drivesChanged, this, [this]() {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
    });

    // ── Settings-Änderungen live anwenden ─────────────────────────────────
    connect(m_sidebar, &Sidebar::hiddenFilesChanged, this, [this](bool) {
        // Beide Panes neu laden — populate() liest showHidden selbst aus Settings
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
        // Miller-Columns ebenfalls aktualisieren
        for (auto *col : m_leftPane->miller()->cols())  col->populateDir(col->path());
        for (auto *col : m_rightPane->miller()->cols()) col->populateDir(col->path());
    });
    connect(m_sidebar, &Sidebar::settingsChanged, this, [this]() {
        // Theme wurde bereits per qApp->setStyleSheet gesetzt —
        // alle Widgets müssen style() neu einlesen und neu zeichnen
        for (QWidget *w : QApplication::topLevelWidgets()) {
            w->style()->unpolish(w);
            w->style()->polish(w);
            w->update();
        }
        // Badge-Farben: Views zum Neuzeichnen zwingen
        m_leftPane->filePane()->view()->viewport()->update();
        m_rightPane->filePane()->view()->viewport()->update();
        // Shortcuts neu registrieren
        registerShortcuts();
    });

    // Hot-Plug
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, [this](const QString &) {
        m_leftPane->miller()->refreshDrives(); m_rightPane->miller()->refreshDrives();
    });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, [this](const QString &) {
        m_leftPane->miller()->refreshDrives(); m_rightPane->miller()->refreshDrives();
    });

    // Spalten-Sync
    connect(m_leftPane->filePane(), &FilePane::columnsChanged, this,
            [this](int colId, bool visible) { m_rightPane->filePane()->setColumnVisible(colId, visible); });
    connect(m_rightPane->filePane(), &FilePane::columnsChanged, this,
            [this](int colId, bool visible) { m_leftPane->filePane()->setColumnVisible(colId, visible); });

    QSettings s("SplitCommander", "UI");
    m_currentMode = s.value("layoutMode", 1).toInt();
    applyLayout(m_currentMode);
    m_leftPane->setFocused(true);
    m_rightPane->setFocused(false);

    QTimer::singleShot(100, this, [this]() {
        m_leftPane->setFocused(true);
        m_rightPane->setFocused(false);
    });

    // ── Startup-Theme anwenden ─────────────────────────────────────────────
    if (!SettingsDialog::useSystemTheme()) {
        // Gespeichertes eigenes Theme laden
        const QString themeName = SettingsDialog::selectedTheme();
        for (const auto &t : SD_Styles::THEMES) {
            if (t.name == themeName) {
                qApp->setStyleSheet(t.stylesheet);
                break;
            }
        }
    }
    // Ist useSystemTheme true: kein setStyleSheet → KDE-Palette greift

    // Shortcuts beim Start registrieren
    registerShortcuts();
}

void MainWindow::registerShortcuts()
{
    // Alte Shortcuts löschen
    for (auto *sc : m_shortcuts) sc->deleteLater();
    m_shortcuts.clear();

    // Hilfslambda: Shortcut registrieren und in m_shortcuts speichern
    auto add = [this](const QString &id, std::function<void()> fn) {
        const QString seq = SettingsDialog::shortcut(id);
        if (seq.isEmpty()) return;
        auto *sc = new QShortcut(QKeySequence(seq), this);
        connect(sc, &QShortcut::activated, this, fn);
        m_shortcuts.append(sc);
    };

    // Navigation
    add("nav_up", [this]() {
        QDir d(activePane()->currentPath());
        if (d.cdUp()) activePane()->navigateTo(d.absolutePath());
    });
    add("nav_home", [this]() {
        activePane()->navigateTo(QDir::homePath());
    });
    add("nav_reload", [this]() {
        m_leftPane->miller()->refreshDrives();
        m_rightPane->miller()->refreshDrives();
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
    });

    // Pane-Fokus
    add("pane_focus_left", [this]() {
        m_leftPane->setFocused(true); m_rightPane->setFocused(false);
    });
    add("pane_focus_right", [this]() {
        m_rightPane->setFocused(true); m_leftPane->setFocused(false);
    });

    // Panes tauschen
    add("pane_swap", [this]() {
        const QString l = m_leftPane->currentPath();
        const QString r = m_rightPane->currentPath();
        m_leftPane->navigateTo(r);
        m_rightPane->navigateTo(l);
    });

    // Pfade synchronisieren
    add("pane_sync", [this]() {
        m_rightPane->navigateTo(m_leftPane->currentPath());
    });

    // Versteckte Dateien umschalten
    add("view_hidden", [this]() {
        QSettings gs("SplitCommander", "General");
        const bool cur = gs.value("showHidden", false).toBool();
        gs.setValue("showHidden", !cur);
        gs.sync();
        m_leftPane->navigateTo(m_leftPane->currentPath());
        m_rightPane->navigateTo(m_rightPane->currentPath());
        for (auto *col : m_leftPane->miller()->cols())  col->populateDir(col->path());
        for (auto *col : m_rightPane->miller()->cols()) col->populateDir(col->path());
    });

    // Layout wechseln
    add("view_layout", [this]() {
        int next = (m_currentMode + 1) % 3;
        QSettings gs("SplitCommander", "UI");
        gs.setValue("layoutMode", next); gs.sync();
        applyLayout(next);
    });
}

void MainWindow::applyLayout(int mode)
{
    m_currentMode = mode;
    QSettings s("SplitCommander", "UI");
    s.setValue("layoutMode", mode);

    const int total = m_panesSplitter->width() > 10 ? m_panesSplitter->width() : 1000;

    switch (mode) {
    case 0: // Klassisch
        m_panesSplitter->setOrientation(Qt::Horizontal);
        m_rightPane->hide(); m_leftPane->show();
        m_panesSplitter->setSizes({total, 0});
        break;
    case 1: // Standard Dual
        m_panesSplitter->setOrientation(Qt::Horizontal);
        m_leftPane->show(); m_rightPane->show();
        m_panesSplitter->setSizes({total / 2, total / 2});
        break;
    case 2: // Spalten Dual
        m_panesSplitter->setOrientation(Qt::Vertical);
        m_leftPane->show(); m_rightPane->show();
        {
            const int h = m_panesSplitter->height() > 10 ? m_panesSplitter->height() : 800;
            m_panesSplitter->setSizes({h / 2, h / 2});
        }
        break;
    }
}

#include "mainwindow.moc"
