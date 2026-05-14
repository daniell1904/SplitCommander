#include "drivedelegate.h"
#include "thememanager.h"
#include "scglobal.h"
#include <QPainter>
#include <QIcon>

void DriveDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);

    if (opt.state & QStyle::State_Selected)
        p->fillRect(opt.rect, QColor(TM().colors().bgSelect));
    else if (opt.state & QStyle::State_MouseOver)
        p->fillRect(opt.rect, QColor(TM().colors().bgHover));

    const QRect    r       = opt.rect;
    const QIcon    icon    = idx.data(Qt::DecorationRole).value<QIcon>();
    const QString  name    = idx.data(Qt::DisplayRole).toString();
    const QString  path    = idx.data(Qt::UserRole).toString();
    const int      iconSz  = 16;
    const int      iconX   = r.left() + 8;
    const int      iconY   = r.top() + (r.height() - iconSz) / 2;
    const int      textX   = r.left() + 32;
    const int      textW   = r.width() - 40;

    icon.paint(p, iconX, iconY, iconSz, iconSz);
    p->setFont(QFont("sans-serif", 9));

    if (m_showBars && (path.startsWith("/") || idx.data(Qt::UserRole + 10).toDouble() > 0)) {
        const double total = idx.data(Qt::UserRole + 10).toDouble();
        const double free  = idx.data(Qt::UserRole + 11).toDouble();

        if (total > 0) {
            const double used  = total - free;
            const double pct   = used / total;

            QFontMetrics fm(p->font());
            const QString usedStr  = sc_fmtStorage(used);
            const QString restStr  = QString(" / %1").arg(sc_fmtStorage(total));
            const int     usedW    = fm.horizontalAdvance(usedStr);
            const int     restW    = fm.horizontalAdvance(restStr);
            const int     sizeW    = usedW + restW;
            const int     nameW    = textW - sizeW - 6;
            const int     sizeX    = r.right() - sizeW - 6;
            const int     lineH    = r.height() / 2;
            const int     barY     = r.top() + lineH + (r.height() - lineH) / 2 - 1;

            p->setPen(QColor(TM().colors().textPrimary));
            p->drawText(textX, r.top(), nameW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(name, Qt::ElideRight, nameW));

            p->setPen(QColor(TM().colors().textLight));
            p->drawText(sizeX, r.top(), usedW, lineH, Qt::AlignLeft | Qt::AlignVCenter, usedStr);
            p->setPen(QColor(TM().colors().textAccent));
            p->drawText(sizeX + usedW, r.top(), restW, lineH, Qt::AlignLeft | Qt::AlignVCenter, restStr);

            p->setBrush(QColor(TM().colors().splitter)); p->setPen(Qt::NoPen);
            p->drawRoundedRect(textX, barY, textW, 3, 1, 1);
            p->setBrush(QColor(TM().colors().accentHover));
            p->drawRoundedRect(textX, barY, (int)(textW * pct), 3, 1, 1);
        } else {
            p->setPen(QColor(TM().colors().textPrimary));
            p->drawText(textX, r.top(), textW, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
        }
    } else {
        // Nicht eingehängt: Name gedämpft + Eject-Symbol rechts
        const bool unmounted = path.startsWith("solid:");
        p->setPen(QColor(unmounted ? TM().colors().textMuted : TM().colors().textPrimary));
        if (unmounted) {
            // Name linksbündig, Eject-Icon rechts
            const QIcon ejectIcon = QIcon::fromTheme(QStringLiteral("media-eject"));
            const int   eSz  = 12;
            const int   eX   = r.right() - eSz - 6;
            const int   eY   = r.top() + (r.height() - eSz) / 2;
            p->drawText(textX, r.top(), textW - eSz - 10, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
            ejectIcon.paint(p, eX, eY, eSz, eSz, Qt::AlignCenter, QIcon::Disabled);
        } else {
            p->drawText(textX, r.top(), textW, r.height(), Qt::AlignLeft | Qt::AlignVCenter, name);
        }
    }
    p->restore();

    // 1px Trennlinie am unteren Rand
    p->save();
    p->setPen(QPen(QColor(TM().colors().border), 1));
    p->drawLine(opt.rect.left(), opt.rect.bottom(), opt.rect.right(), opt.rect.bottom());
    p->restore();
}

QSize DriveDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &idx) const
{
    const QString path = idx.data(Qt::UserRole).toString();
    // Eingehängt (Pfad) oder ausgehängt (solid:): beide bekommen 44px wenn showBars
    return QSize(200, m_showBars ? SC_SIDEBAR_DRIVE_ROW_H : SC_SIDEBAR_ROW_H);
}
