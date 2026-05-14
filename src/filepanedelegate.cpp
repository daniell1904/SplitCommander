#include "filepanedelegate.h"
#include "config.h"
#include "thememanager.h"
#include "filepane.h"
#include "tagmanager.h"
#include <QPainter>
#include <QIcon>

FilePaneDelegate::FilePaneDelegate(QObject *par) : QStyledItemDelegate(par) {}

// Delegate für Kompakt/Symbole — holt Icon direkt in der gewünschten Größe

QString FilePaneDelegate::formatAge(qint64 s) {
  if (s < 0)
    return {};
  if (s < 60)
    return QString("%1s").arg(s);
  if (s < 3600)
    return QString("%1m").arg(s / 60);
  if (s < 86400)
    return QString("%1h").arg(s / 3600);
  if (s < 86400 * 30)
    return QString("%1t").arg(s / 86400);
  if (s < 86400 * 365)
    return QString("%1M").arg(s / 86400 / 30);
  return QString("%1J").arg(s / 86400 / 365);
}

QColor FilePaneDelegate::ageColor(qint64 s) {
  if (s < 3600)
    return Config::ageBadgeColor(0);
  if (s < 86400)
    return Config::ageBadgeColor(1);
  if (s < 86400 * 7)
    return Config::ageBadgeColor(2);
  if (s < 86400 * 30)
    return Config::ageBadgeColor(3);
  if (s < 86400 * 365)
    return Config::ageBadgeColor(4);
  return Config::ageBadgeColor(5);
}

void FilePaneDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                             const QModelIndex &idx) const {
  QStyleOptionViewItem o = opt;
  initStyleOption(&o, idx);

  bool sel = o.state & QStyle::State_Selected;
  bool hov = o.state & QStyle::State_MouseOver;

  QColor bg, bgAlt;
  if (focused) {
    bg = QColor(TM().colors().bgList);
    bgAlt = QColor(TM().colors().bgAlternate);
  } else {
    bg = QColor(TM().colors().bgDeep);
    bgAlt = QColor(TM().colors().bgBox);
  }

  QColor bgFinal = sel   ? QColor(TM().colors().bgSelect)
                   : hov ? QColor(TM().colors().bgHover)
                         : (idx.row() % 2 ? bgAlt : bg);
  p->fillRect(o.rect, bgFinal);

  int col = idx.data(Qt::UserRole + 99).toInt();
  QRect r = o.rect.adjusted(4, 0, -4, 0);
  QFont f = o.font;
  f.setPointSize(fontSize);
  p->setFont(f);

  QColor tc = (sel || hov) ? QColor(TM().colors().textLight)
                           : QColor(TM().colors().textPrimary);
  QColor dc = (sel || hov) ? QColor(TM().colors().textLight)
                           : QColor(TM().colors().textMuted);

  if (col == FP_NAME) {
    // New-File-Indicator
    if (Config::showNewIndicator()) {

      qint64 ageSecs = idx.data(Qt::UserRole + 2).toLongLong();
      if (ageSecs > 0 && ageSecs < 86400 * 2) {
        QRect strip(o.rect.left(), o.rect.top(), 3, o.rect.height());
        p->fillRect(strip, ageColor(ageSecs));
      }
    }
    QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    const int ic = qBound(12, rowHeight - 6, 48);
    if (!icon.isNull()) {
      QPixmap pm = icon.pixmap(QSize(32, 32));
      if (pm.isNull())
        pm = icon.pixmap(QSize(16, 16));
      if (!pm.isNull())
        p->drawPixmap(
            r.left(), r.top() + (r.height() - ic) / 2,
            pm.scaled(ic, ic, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    r.setLeft(r.left() + ic + 4);
    p->setPen(tc);
    p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight,
                                         r.width()));

  } else if (col == FP_ALTER) {
    qint64 secs = idx.data(Qt::UserRole).toLongLong();
    if (secs < 0)
      return;
    QString age = formatAge(secs);
    QColor bc = ageColor(secs);

    const int BW = 44, BH = 14;
    QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2,
             BW, BH);

    p->setRenderHint(QPainter::Antialiasing, false);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->setBrush(bc);
    p->setPen(Qt::NoPen);
    p->drawRect(br);

    QColor textCol =
        (bc.lightness() > 140) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    p->setPen(textCol);
    QFont fb = f;
    fb.setBold(false);
    fb.setPointSizeF(7.5);
    fb.setHintingPreference(QFont::PreferFullHinting);
    p->setFont(fb);
    p->drawText(br, Qt::AlignCenter, age);
    p->setFont(f);

  } else if (col == FP_TAGS) {
    QString tagName = idx.data().toString();
    if (!tagName.isEmpty()) {
      QString colorStr = TagManager::instance().tagColor(tagName);
      QColor tagCol =
          colorStr.isEmpty() ? QColor(TM().colors().accent) : QColor(colorStr);
      int textW = o.fontMetrics.horizontalAdvance(tagName);
      int BW = qMin(textW + 12, r.width()), BH = 16;
      QRect br(r.left() + (r.width() - BW) / 2, r.top() + (r.height() - BH) / 2,
               BW, BH);

      p->setRenderHint(QPainter::Antialiasing, false);
      p->setBrush(tagCol.darker(180));
      p->setPen(QPen(tagCol, 1));
      p->drawRect(br);

      QFont fb = f;
      fb.setBold(false);
      fb.setPointSizeF(7.5);
      fb.setHintingPreference(QFont::PreferFullHinting);
      p->setFont(fb);
      p->setPen(QColor(255, 255, 255));
      p->drawText(br, Qt::AlignCenter,
                  o.fontMetrics.elidedText(tagName, Qt::ElideRight, BW - 6));
      p->setFont(f);
    }
  } else {
    p->setPen(col == FP_GROESSE ? tc : dc);
    if (col == FP_RECHTE) {
      QFont fm = f;
      fm.setFamily(QStringLiteral("monospace"));
      fm.setPointSize(9);
      p->setFont(fm);
    }
    p->drawText(r, Qt::AlignVCenter | Qt::AlignHCenter,
                o.fontMetrics.elidedText(idx.data().toString(), Qt::ElideRight,
                                         r.width()));
    p->setFont(f);
  }

  p->setPen(QColor(TM().colors().bgHover));
  p->drawLine(o.rect.bottomLeft(), o.rect.bottomRight());
}

QSize FilePaneDelegate::sizeHint(const QStyleOptionViewItem &,
                                 const QModelIndex &) const {
  return QSize(0, rowHeight);
}
