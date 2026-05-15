#include "drivedelegate.h"
#include "thememanager.h"
#include "scglobal.h"
#include <QPainter>
#include <QIcon>
#include <QUrl>

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
    const int      iconSz  = 22;
    const int      iconX   = r.left() + 8;
    const int      iconY   = r.top() + (r.height() - iconSz) / 2;
    const int      textX   = r.left() + 40;
    const int      textW   = r.width() - 50;

    if (!icon.isNull()) {
        QPixmap pix = icon.pixmap(48, 48, QIcon::Normal, QIcon::On);
        if (!pix.isNull()) {
            p->drawPixmap(iconX, iconY, pix.scaled(iconSz, iconSz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }
    p->setFont(QFont("sans-serif", 10));

    const bool isKioPath = path.contains(QStringLiteral(":/"))
                           && !path.startsWith(QStringLiteral("/"))
                           && !path.startsWith(QStringLiteral("solid:"))
                           && !path.startsWith(QStringLiteral("file:"));
    const double total = idx.data(Qt::UserRole + 10).toDouble();
    const double free  = idx.data(Qt::UserRole + 11).toDouble();
    if (m_showBars && (path.startsWith("/") || (isKioPath && total > 0))) {
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
            const int     lineH    = 24;
            const int     barY     = r.top() + 24;

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

            // Host/IP unter dem Balken (nur für Netzwerk)
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
        }
    } else if (isKioPath) {
        // KIO-Pfad ohne Balken: Name oben, URL/Host unten klein
        QFontMetrics fm(p->font());
        const int lineH = r.height() / 2;
        p->setPen(QColor(TM().colors().textPrimary));
        p->drawText(textX, r.top(), textW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(name, Qt::ElideRight, textW));
        QUrl u(path); u.setUserInfo(QString());
        const QString subtitle = u.host() + (u.path().isEmpty() || u.path() == "/" ? "" : u.path());
        p->setFont(QFont("sans-serif", 8));
        p->setPen(QColor(TM().colors().textAccent));
        p->drawText(textX, r.top() + lineH, textW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(p->font()).elidedText(subtitle, Qt::ElideRight, textW));
    } else {
        // Nicht eingehängt oder lokal: Name gedämpft + Eject-Symbol rechts
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
