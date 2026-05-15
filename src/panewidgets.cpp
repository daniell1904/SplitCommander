// --- panewidgets.cpp — Implementierung der internen Widget-Klassen ---

#include "panewidgets.h"
#include "config.h"
#include "thememanager.h"

#include <QCache>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStorageInfo>

#include <limits>

// --- MillerStrip ---
MillerStrip::MillerStrip(const QString &label, QWidget *parent)
    : QWidget(parent), m_label(label) {
  setFixedWidth(22);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setCursor(Qt::PointingHandCursor);
  setToolTip(label);
}

void MillerStrip::setLabel(const QString &label) {
  m_label = label;
  update();
}

void MillerStrip::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(),
             QColor(m_hovered ? TM().colors().bgBox : TM().colors().bgPanel));
  p.setPen(QColor(TM().colors().borderAlt));
  p.drawLine(width() - 1, 0, width() - 1, height());
  p.save();
  p.translate(width() / 2.0 + 4.0, (double)height() - 8.0);

  p.rotate(-90);
  p.setPen(
      QColor(m_hovered ? TM().colors().textPrimary : TM().colors().textAccent));
  QFont f = p.font();
  f.setPointSize(8);
  p.setFont(f);
  p.drawText(
      0, 0, p.fontMetrics().elidedText(m_label, Qt::ElideRight, height() - 16));
  p.restore();
}

void MillerStrip::mousePressEvent(QMouseEvent *) { emit clicked(); }
void MillerStrip::enterEvent(QEnterEvent *) {
  m_hovered = true;
  update();
}
void MillerStrip::leaveEvent(QEvent *) {
  m_hovered = false;
  update();
}

// --- PaneSplitterHandle ---
PaneSplitterHandle::PaneSplitterHandle(Qt::Orientation o, QSplitter *parent)
    : QSplitterHandle(o, parent) {
  setStyleSheet(QString("background:%1;").arg(TM().colors().splitter));
  connect(parent, &QSplitter::splitterMoved, this,
          [this](int, int) { update(); });
}

int PaneSplitterHandle::collapseState() const {
  const QList<int> sz = splitter()->sizes();
  if (sz.size() < 2)
    return 0;
  if (sz[0] == 0)
    return 1;
  if (sz[1] == 0)
    return 2;
  return 0;
}

void PaneSplitterHandle::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor(TM().colors().splitter));

  const int cx = width() / 2;
  const int cy = height() / 2;
  const int state = collapseState();

  if (state == 0) {
    p.setPen(QPen(QColor(255, 255, 255, m_hovered ? 140 : 50), 1));
    for (int i = -3; i <= 4; ++i) {
      const int y = cy + i * 4 - 2;
      p.drawLine(cx - 2, y, cx + 2, y);
    }
    QIcon::fromTheme("go-previous").paint(&p, cx - 7, cy - 34 - 7, 14, 14);
    QIcon::fromTheme("go-next").paint(&p, cx - 7, cy + 34 - 7, 14, 14);
  } else {
    p.setPen(QPen(QColor(255, 255, 255, m_hovered ? 240 : 150), 2.0,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QIcon::fromTheme(state == 1 ? "go-next" : "go-previous")
        .paint(&p, cx - 7, cy - 7, 14, 14);
  }
}

void PaneSplitterHandle::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  m_pressGlobal = e->globalPosition().toPoint();
  m_sizesAtPress = splitter()->sizes();
  QSplitterHandle::mousePressEvent(e);
}

void PaneSplitterHandle::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) {
    QSplitterHandle::mouseReleaseEvent(e);
    return;
  }
  const int moved = qAbs(e->globalPosition().toPoint().x() - m_pressGlobal.x());
  if (moved < 4) {
    QList<int> sz = splitter()->sizes();
    if (sz.size() < 2)
      return;
    const int total = sz[0] + sz[1];
    const int state = collapseState();
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

void PaneSplitterHandle::enterEvent(QEnterEvent *) {
  m_hovered = true;
  update();
}
void PaneSplitterHandle::leaveEvent(QEvent *) {
  m_hovered = false;
  update();
}

// --- PaneSplitter ---
PaneSplitter::PaneSplitter(Qt::Orientation o, QWidget *parent)
    : QSplitter(o, parent) {}

QSplitterHandle *PaneSplitter::createHandle() {
  return new PaneSplitterHandle(orientation(), this);
}

// --- SidebarHandle ---
SidebarHandle::SidebarHandle(QWidget *sidebar, QWidget *parent)
    : QWidget(parent), m_sidebar(sidebar) {
  setFixedWidth(10);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setMouseTracking(true);
  setToolTip("Sidebar ein-/ausklappen / ziehen");

  // keine Icons
}

void SidebarHandle::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor(TM().colors().bgMain));

  const int cx = width() / 2;
  const int cy = height() / 2;

  if (m_sidebar->isVisible()) {
    // Weiße Randlinie entfernt für nahtlosen Übergang

    p.setPen(QPen(QColor(255, 255, 255, m_hov ? 140 : 50), 1));
    for (int i = -3; i <= 4; ++i) {
      const int y = cy + 10 + i * 4;
      p.drawLine(cx - 2, y, cx + 2, y);
    }
    QIcon::fromTheme("go-previous").paint(&p, cx - 7, cy - 34 - 7, 14, 14);
  } else {
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
    f.setPointSize(7);
    f.setWeight(QFont::Light);
    p.setFont(f);
    p.setPen(QColor("#4c566a"));
    p.drawText(x2 + 4, 0, "| Dateimanager");
    p.restore();

    p.setPen(QPen(QColor(255, 255, 255, m_hov ? 140 : 50), 1));
    for (int i = -3; i <= 4; ++i) {
      const int y = cy + 10 + i * 4;
      p.drawLine(cx - 2, y, cx + 2, y);
    }
    QIcon::fromTheme("go-next").paint(&p, cx - 7, cy + 44 - 7, 14, 14);

    const int iconSize = 16;
    const int spacing = 8;
    const int total = m_icons.size() * (iconSize + spacing) - spacing;
    const int startY = height() - total - 14;
    for (int i = 0; i < m_icons.size(); ++i) {
      const int iy = startY + i * (iconSize + spacing);
      const int ix = cx - iconSize / 2;
      const bool hov = (m_hovIcon == i);
      if (hov) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(TM().colors().bgList));
        p.drawRoundedRect(ix - 4, iy - 3, iconSize + 8, iconSize + 6, 4, 4);
      }
      p.setOpacity(hov ? 1.0 : (m_hov ? 0.7 : 0.4));
      p.drawPixmap(ix, iy, m_icons[i].pixmap(iconSize, iconSize));
      p.setOpacity(1.0);
    }
  }
}

void SidebarHandle::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  m_pressGlobal = e->globalPosition().toPoint();
  m_pressWidth = m_sidebar->isVisible() ? m_sidebar->width() : 0;
  m_dragging = false;
}

void SidebarHandle::mouseMoveEvent(QMouseEvent *e) {
  if (!(e->buttons() & Qt::LeftButton)) {
    if (!m_sidebar->isVisible()) {
      const int iconSize = 16, spacing = 8;
      const int total = m_icons.size() * (iconSize + spacing) - spacing;
      const int startY = height() - total - 14;
      const int cx = width() / 2;
      const int prev = m_hovIcon;
      m_hovIcon = -1;
      for (int i = 0; i < m_icons.size(); ++i) {
        QRect r(cx - iconSize / 2 - 4, startY + i * (iconSize + spacing) - 3,
                iconSize + 8, iconSize + 6);
        if (r.contains(e->pos())) {
          m_hovIcon = i;
          break;
        }
      }
      if (m_hovIcon != prev)
        update();
    }
    return;
  }

  const int dx = e->globalPosition().toPoint().x() - m_pressGlobal.x();
  if (qAbs(dx) > 3)
    m_dragging = true;
  if (!m_dragging)
    return;

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
    auto s = Config::group("UI");
    s.writeEntry("sidebarWidth", newW);
    s.writeEntry("sidebarVisible", true);
    s.config()->sync();
  }
}

void SidebarHandle::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  if (m_dragging && !m_sidebar->isVisible()) {
    setFixedWidth(32);
    setCursor(Qt::PointingHandCursor);
    m_dragging = false;
    update();
    return;
  }
  if (!m_dragging) {
    const bool show = !m_sidebar->isVisible();
    m_sidebar->setVisible(show);
    if (show) {
      auto s = Config::group("UI");
      m_sidebar->setFixedWidth(s.readEntry("sidebarWidth", 250));
    }

    setFixedWidth(show ? 10 : 32);
    setCursor(show ? Qt::SizeHorCursor : Qt::PointingHandCursor);
    auto s = Config::group("UI");
    if (!show)
      s.writeEntry("sidebarWidth", m_sidebar->width());
    s.writeEntry("sidebarVisible", show);
    s.config()->sync();
  }
  m_dragging = false;
  update();
}

void SidebarHandle::enterEvent(QEnterEvent *) {
  m_hov = true;
  setCursor(m_sidebar->isVisible() ? Qt::SizeHorCursor
                                   : Qt::PointingHandCursor);
  update();
}

void SidebarHandle::leaveEvent(QEvent *) {
  m_hov = false;
  m_hovIcon = -1;
  setCursor(Qt::ArrowCursor);
  update();
}

// --- FooterWidget ---
FooterWidget::FooterWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QString("background:%1;border-top:1px solid %2;")
                    .arg(TM().colors().bgList, TM().colors().bgDeep));
  setFixedHeight(CLOSED_H);

  m_mainLay = new QVBoxLayout(this);
  m_mainLay->setContentsMargins(0, 0, 0, 0);
  m_mainLay->setSpacing(0);

  m_barRow = new QFrame();
  m_barRow->setFixedHeight(CLOSED_H);
  m_barRow->setStyleSheet(
      QString("QFrame { border:none; background: "
              "qlineargradient(x1:0,y1:0,x2:0,y2:1,"
              "stop:0 rgba(10,13,20,100), stop:0.4 %1, stop:1 %1); }")
          .arg(TM().colors().bgList));
  auto *barLay = new QHBoxLayout(m_barRow);
  barLay->setContentsMargins(8, 0, 8, 0);
  barLay->setSpacing(4);

  countLbl = new QLabel("0 Elemente");
  countLbl->setStyleSheet(
      QString("color:%1;font-size:10px;background:transparent;")
          .arg(TM().colors().textPrimary));
  selectedLbl = new QLabel();
  selectedLbl->setStyleSheet(
      QString("color:%1;font-size:10px;background:transparent;")
          .arg(TM().colors().textAccent));
  selectedLbl->hide();
  sizeLbl = new QLabel();
  sizeLbl->setStyleSheet(
      QString("color:%1;font-size:10px;background:transparent;")
          .arg(TM().colors().textPrimary));

  barLay->addWidget(countLbl);
  barLay->addWidget(selectedLbl);
  barLay->addStretch(1);
  barLay->addSpacing(90);
  barLay->addWidget(sizeLbl);
  m_mainLay->addWidget(m_barRow);
  m_barRow->setMouseTracking(true);
  m_barRow->installEventFilter(this);

  m_content = new QWidget();
  m_content->hide();
  auto *cLay = new QHBoxLayout(m_content);
  cLay->setContentsMargins(0, 0, 0, 0);
  cLay->setSpacing(0);

  auto *pvSide = new QWidget();
  pvSide->setStyleSheet(QString("background:%1; border:none;").arg(TM().colors().bgList));
  auto *pvLay = new QVBoxLayout(pvSide);
  pvLay->setContentsMargins(8, 8, 8, 8);
  pvLay->setSpacing(0);
  previewIcon = new QLabel();
  previewIcon->setAlignment(Qt::AlignCenter);
  previewIcon->setStyleSheet("background:transparent; border:none;");
  previewIcon->setFrameShape(QFrame::NoFrame);
  previewIcon->setScaledContents(false);
  previewIcon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  pvLay->addStretch();
  pvLay->addWidget(previewIcon, 0, Qt::AlignCenter);
  pvLay->addStretch();

  auto *divContainer = new QWidget();
  auto *divLay = new QVBoxLayout(divContainer);
  divLay->setContentsMargins(0, 12, 0, 12);
  auto *div = new QWidget();
  div->setFixedWidth(1);
  div->setStyleSheet(QString("background:%1;").arg(TM().colors().separator));
  divLay->addWidget(div);

  auto *infoSide = new QWidget();
  infoSide->setStyleSheet(QString("background:%1; border:none;").arg(TM().colors().bgList));
  auto *infoLay = new QVBoxLayout(infoSide);
  infoLay->setContentsMargins(8, 8, 8, 8);
  infoLay->setSpacing(4);
  previewInfo = new QLabel();
  previewInfo->setStyleSheet(QString("color:%1; font-size:10px; background:transparent; border:none;").arg(TM().colors().textLight));
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

void FooterWidget::setExpanded(bool on) {
  if (m_expanded == on)
    return;
  m_expanded = on;
  if (on) {
    m_content->show();
    setFixedHeight(OPEN_H);
  } else {
    m_content->hide();
    setFixedHeight(CLOSED_H);
  }
  update();
  if (onHeightChanged)
    onHeightChanged();
}

bool FooterWidget::eventFilter(QObject *obj, QEvent *ev) {
  if (obj != m_barRow)
    return false;

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
    QIcon::fromTheme(m_expanded ? "go-down" : "go-up")
        .paint(&p, cx + 17, cy - 7, 14, 14);
  }
  if (ev->type() == QEvent::MouseButtonPress) {
    auto *e = static_cast<QMouseEvent *>(ev);
    if (e->button() != Qt::LeftButton)
      return false;
    m_pressY = e->globalPosition().toPoint().y();
    m_pressH = height();
    m_dragging = false;
    const int cx = m_barRow->width() / 2;
    const int cy = m_barRow->height() / 2;
    m_clickOnArrow = QRect(cx + 16, cy - 8, 20, 16).contains(e->pos());
  }
  if (ev->type() == QEvent::MouseMove) {
    auto *e = static_cast<QMouseEvent *>(ev);
    if (!(e->buttons() & Qt::LeftButton)) {
      const int cx = m_barRow->width() / 2;
      const int cy = m_barRow->height() / 2;
      const bool ah = QRect(cx - 20, cy - 10, 60, 20).contains(e->pos());
      if (ah != m_arrowHov) {
        m_arrowHov = ah;
        m_barRow->update();
      }
      return false;
    }
    if (m_clickOnArrow)
      return false;
    const int dy = m_pressY - e->globalPosition().toPoint().y();
    if (qAbs(dy) > 3) {
      m_dragging = true;
      const int newH = qBound((int)CLOSED_H, m_pressH + dy, 320);
      if (newH > (int)CLOSED_H + 10 && !m_expanded) {
        m_expanded = true;
        m_content->show();
      }
      setFixedHeight(newH);
      m_barRow->update();
      if (onHeightChanged)
        onHeightChanged();
    }
  }
  if (ev->type() == QEvent::MouseButtonRelease) {
    auto *e = static_cast<QMouseEvent *>(ev);
    if (e->button() != Qt::LeftButton)
      return false;
    if (m_clickOnArrow && !m_dragging)
      setExpanded(!m_expanded);
    if (m_dragging && height() < (int)CLOSED_H + 20)
      setExpanded(false);
    m_dragging = m_clickOnArrow = false;
  }
  return false;
}

// --- MillerItemDelegate ---
MillerItemDelegate::MillerItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void MillerItemDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                               const QModelIndex &idx) const {
  const QRect r = opt.rect;
  const QString rawPath = idx.data(Qt::UserRole).toString();
  const QString path = rawPath.startsWith(QStringLiteral("file://"))
                           ? QUrl(rawPath).toLocalFile()
                           : rawPath;
  const double total = idx.data(Qt::UserRole + 10).toDouble();
  const bool hasBar = (total > 0);
  const bool isKioPath = path.contains(QStringLiteral(":/"))
                         && !path.startsWith(QStringLiteral("/"))
                         && !path.startsWith(QStringLiteral("solid:"))
                         && !path.startsWith(QStringLiteral("file:"));

  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);

  // 1. Hintergrund
  if (opt.state & QStyle::State_Selected) {
    p->fillRect(r, QColor(TM().colors().bgSelect));
  } else if (opt.state & QStyle::State_MouseOver) {
    p->fillRect(r, QColor(TM().colors().bgHover));
  } else {
    p->fillRect(r, QColor(TM().colors().bgList));
  }

  // 2. Maße (Exakt wie Sidebar)
  const int iconSz = 22;
  const int iconX = r.left() + 8;
  const int iconY = r.top() + (r.height() - iconSz) / 2;
  const int textX = r.left() + 40;
  const int textW = r.width() - 48;

  // 3. Icon zeichnen (Erzwingt das farbige Pixmap aus dem SVG durch
  // Downscaling)
  const QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
  if (!icon.isNull()) {
    QPixmap pix = icon.pixmap(48, 48, QIcon::Normal, QIcon::On);
    if (!pix.isNull()) {
      p->drawPixmap(iconX, iconY,
                    pix.scaled(iconSz, iconSz, Qt::IgnoreAspectRatio,
                               Qt::SmoothTransformation));
    }
  }

  // 4. Text & Balken
  const QString name = idx.data(Qt::DisplayRole).toString();
  p->setFont(QFont("sans-serif", 10));
  p->setPen(QColor((opt.state & QStyle::State_Selected)
                       ? TM().colors().textLight
                       : TM().colors().textPrimary));

  if (hasBar) {
    // Laufwerk-Layout (50px Höhe)
    const double free = idx.data(Qt::UserRole + 11).toDouble();
    const double pct = qBound(0.0, (total - free) / total, 1.0);

    // Name (oben zentriert in der ersten Hälfte)
    p->drawText(
        textX, r.top(), textW, 24, Qt::AlignLeft | Qt::AlignVCenter,
        QFontMetrics(p->font()).elidedText(name, Qt::ElideRight, textW));

    // Balken (auf 24px Tiefe)
    const int barY = r.top() + 24;
    p->setBrush(QColor(TM().colors().splitter));
    p->setPen(Qt::NoPen);
    p->drawRoundedRect(textX, barY, textW - 4, 3, 1, 1);
    p->setBrush(QColor(TM().colors().accentHover));
    p->drawRoundedRect(textX, barY, (int)((textW - 4) * pct), 3, 1, 1);

    // Host/IP unter dem Balken (wie Sidebar)
    if (isKioPath) {
      QUrl u(path); u.setUserInfo(QString());
      const QString hostStr = u.host();
      if (!hostStr.isEmpty()) {
        p->setFont(QFont("sans-serif", 7));
        p->setPen(QColor(TM().colors().textAccent));
        p->drawText(textX, barY + 9, textW, r.bottom() - (barY + 9),
                    Qt::AlignLeft | Qt::AlignTop, hostStr);
      }
    }
  } else {
    // Ordner-Layout (34px Höhe)
    p->drawText(
        textX, r.top(), textW, r.height(), Qt::AlignLeft | Qt::AlignVCenter,
        QFontMetrics(p->font()).elidedText(name, Qt::ElideRight, textW));
  }

  // 5. Trenner
  p->setPen(QPen(QColor(TM().colors().border), 1));
  p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());

  p->restore();

  // 6. Age-Badge
  if (!Config::showNewIndicator())
    return;
  if (rawPath.isEmpty())
    return;

  // Pfad-Check für Age-Badge (nutzt bereits normiertes 'path')
  if (path.isEmpty() || path.startsWith(QStringLiteral("solid:")) ||
      path.contains(QStringLiteral("://")))
    return;

  static QCache<QString, qint64> s_dirAgeCache(200);
  qint64 ageSecs = -1;

  const QFileInfo fi(path);
  if (fi.isDir()) {
    if (s_dirAgeCache.contains(path)) {
      ageSecs = *s_dirAgeCache.object(path);
    } else {
      qint64 newest = std::numeric_limits<qint64>::max();
      const QDateTime now = QDateTime::currentDateTime();
      const QFileInfoList entries =
          QDir(path).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
      int count = 0;
      for (const QFileInfo &child : entries) {
        const qint64 s = child.lastModified().secsTo(now);
        if (s < newest)
          newest = s;
        if (++count >= 50)
          break;
      }
      ageSecs = (newest == std::numeric_limits<qint64>::max()) ? 0 : newest;
      s_dirAgeCache.insert(path, new qint64(ageSecs));
    }
  } else {
    ageSecs = fi.lastModified().secsTo(QDateTime::currentDateTime());
  }

  if (ageSecs <= 0 || ageSecs >= 86400 * 2)
    return;

  const QColor col =
      (ageSecs < 3600) ? Config::ageBadgeColor(0) : Config::ageBadgeColor(1);

  p->fillRect(QRect(opt.rect.left(), opt.rect.top(), 3, opt.rect.height()),
              col);
}

QSize MillerItemDelegate::sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    Q_UNUSED(opt);
    const bool isDrive = idx.data(Qt::UserRole+12).toBool() || idx.data(Qt::UserRole+10).toDouble() > 0;
    return QSize(100, isDrive ? 50 : 34);
}
