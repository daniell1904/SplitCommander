#include "drivedelegate.h"
#include "config.h"
#include "thememanager.h"
#include "scglobal.h"
#include <QApplication>
#include <QPainter>
#include <QIcon>
#include <QUrl>

void DriveDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);

    if (opt.state & QStyle::State_Selected) {
        p->fillRect(opt.rect, QColor(TM().colors().bgSelect));
    } else {
        p->fillRect(opt.rect, QColor(TM().colors().bgList)); // Grundhintergrund
        if (m_fader) {
            double hov = m_fader->opacity(idx.row());
            if (hov > 0.0) {
                QColor hC = QColor(TM().colors().bgHover);
                hC.setAlphaF(hC.alphaF() * hov);
                p->fillRect(opt.rect, hC);
            }
        } else if (opt.state & QStyle::State_MouseOver) {
            p->fillRect(opt.rect, QColor(TM().colors().bgHover));
        }
    }

    const QRect    r       = opt.rect;
    const QIcon    icon    = idx.data(Qt::DecorationRole).value<QIcon>();
    const QString  name    = idx.data(Qt::DisplayRole).toString();
    const QString  path    = idx.data(Qt::UserRole).toString();
    const int      iconSz  = m_showBars ? Config::driveIconSize() : Config::sidebarIconSize();
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
    { QFont _f = qApp->font(); _f.setPointSize(10); p->setFont(_f); }

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
            const int     sizeX    = r.right() - sizeW - 6;

            // Abstände aus uiSpacing
            const int sp     = Config::uiSpacing();
            const int textH  = fm.height();
            const int topPad = sp * 2;          // Oben zur Schrift
            const int barPad = sp;              // Schrift-UK zum Balken
            const int barH   = 3;              // Balkenhöhe (fix)

            const int textY = r.top() + topPad;
            const int barY  = textY + textH + barPad;
            const int lineH = textH;

            // IP-Adresse zwischen Name und Größe (nur Netzlaufwerke)
            QString ipStr;
            if (isKioPath && Config::showDriveIp()) {
                QUrl u(path); u.setUserInfo(QString());
                QString host = u.host();
                QString urlPath = u.path();
                if (!host.isEmpty()) {
                    ipStr = QStringLiteral("(//") + host + urlPath + QStringLiteral(")");
                }
            }
            const int ipInlineW = ipStr.isEmpty() ? 0 : fm.horizontalAdvance(ipStr) + 6;
            const int availNameW = textW - sizeW - (ipInlineW > 0 ? ipInlineW + 6 : 0) - 6;
            const int nameDrawW  = qMax(availNameW, 0);
            const int ipInlineX  = textX + nameDrawW + 6;

            p->setPen(QColor(TM().colors().textPrimary));
            p->drawText(textX, textY, nameDrawW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(name, Qt::ElideRight, nameDrawW));

            if (!ipStr.isEmpty() && ipInlineW > 0) {
                const int ipAvailW = sizeX - ipInlineX - 4;
                if (ipAvailW > 20) {
                    p->setPen(QColor(TM().colors().textMuted));
                    p->drawText(ipInlineX, textY, ipAvailW, lineH, Qt::AlignLeft | Qt::AlignVCenter,
                                fm.elidedText(ipStr, Qt::ElideRight, ipAvailW));
                }
            }

            p->setPen(QColor(TM().colors().textLight));
            p->drawText(sizeX, textY, usedW, lineH, Qt::AlignLeft | Qt::AlignVCenter, usedStr);
            p->setPen(QColor(TM().colors().textAccent));
            p->drawText(sizeX + usedW, textY, restW, lineH, Qt::AlignLeft | Qt::AlignVCenter, restStr);

            p->setBrush(QColor(TM().colors().splitter)); p->setPen(Qt::NoPen);
            p->drawRoundedRect(textX, barY, textW, barH, 1, 1);
            p->setBrush(QColor(TM().colors().accentHover));
            p->drawRoundedRect(textX, barY, (int)(textW * pct * m_animProgress), barH, 1, 1);


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
        { QFont _f = qApp->font(); _f.setPointSize(8); p->setFont(_f); }
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
    return QSize(200, m_showBars ? Config::sidebarDriveRowHeight() : Config::sidebarRowHeight());
}
