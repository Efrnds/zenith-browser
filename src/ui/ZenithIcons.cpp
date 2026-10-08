#include "ui/ZenithIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

QIcon zenithIcon(const ZenithIcon type, const QColor &color, const int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal s = size;
    const QPointF center(s / 2.0, s / 2.0);
    switch (type) {
    case ZenithIcon::Add:
        painter.drawLine(QPointF(s * .25, center.y()), QPointF(s * .75, center.y()));
        painter.drawLine(QPointF(center.x(), s * .25), QPointF(center.x(), s * .75));
        break;
    case ZenithIcon::Back:
    case ZenithIcon::ChevronLeft:
        painter.drawLine(QPointF(s * .65, s * .22), QPointF(s * .35, s * .5));
        painter.drawLine(QPointF(s * .35, s * .5), QPointF(s * .65, s * .78));
        if (type == ZenithIcon::Back)
            painter.drawLine(QPointF(s * .36, s * .5), QPointF(s * .82, s * .5));
        break;
    case ZenithIcon::ChevronDown:
        painter.drawLine(QPointF(s * .22, s * .36), QPointF(s * .5, s * .64));
        painter.drawLine(QPointF(s * .5, s * .64), QPointF(s * .78, s * .36));
        break;
    case ZenithIcon::Forward:
    case ZenithIcon::ChevronRight:
        painter.drawLine(QPointF(s * .35, s * .22), QPointF(s * .65, s * .5));
        painter.drawLine(QPointF(s * .65, s * .5), QPointF(s * .35, s * .78));
        if (type == ZenithIcon::Forward)
            painter.drawLine(QPointF(s * .18, s * .5), QPointF(s * .64, s * .5));
        break;
    case ZenithIcon::Close:
        painter.drawLine(QPointF(s * .28, s * .28), QPointF(s * .72, s * .72));
        painter.drawLine(QPointF(s * .72, s * .28), QPointF(s * .28, s * .72));
        break;
    case ZenithIcon::Folder: {
        QPainterPath path;
        path.moveTo(s * .14, s * .32);
        path.lineTo(s * .4, s * .32);
        path.lineTo(s * .49, s * .42);
        path.lineTo(s * .86, s * .42);
        path.lineTo(s * .86, s * .78);
        path.lineTo(s * .14, s * .78);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case ZenithIcon::More:
        painter.setBrush(color);
        for (const qreal x : {.28, .5, .72})
            painter.drawEllipse(QPointF(s * x, s * .5), s * .065, s * .065);
        break;
    case ZenithIcon::Reload:
        painter.drawArc(QRectF(s * .2, s * .2, s * .6, s * .6), 35 * 16, 285 * 16);
        painter.drawLine(QPointF(s * .73, s * .2), QPointF(s * .8, s * .38));
        painter.drawLine(QPointF(s * .73, s * .2), QPointF(s * .56, s * .26));
        break;
    case ZenithIcon::Settings:
        painter.drawEllipse(QRectF(s * .24, s * .24, s * .52, s * .52));
        painter.drawEllipse(QRectF(s * .41, s * .41, s * .18, s * .18));
        painter.drawLine(QPointF(s * .5, s * .1), QPointF(s * .5, s * .24));
        painter.drawLine(QPointF(s * .5, s * .76), QPointF(s * .5, s * .9));
        painter.drawLine(QPointF(s * .1, s * .5), QPointF(s * .24, s * .5));
        painter.drawLine(QPointF(s * .76, s * .5), QPointF(s * .9, s * .5));
        break;
    case ZenithIcon::Shield: {
        QPainterPath path;
        path.moveTo(s * .5, s * .12);
        path.lineTo(s * .8, s * .24);
        path.lineTo(s * .75, s * .62);
        path.quadTo(s * .68, s * .8, s * .5, s * .88);
        path.quadTo(s * .32, s * .8, s * .25, s * .62);
        path.lineTo(s * .2, s * .24);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case ZenithIcon::Sleep:
        painter.drawArc(QRectF(s * .18, s * .14, s * .64, s * .7), 72 * 16, 224 * 16);
        break;
    case ZenithIcon::Split:
        painter.drawRoundedRect(QRectF(s * .15, s * .2, s * .7, s * .6), 2, 2);
        painter.drawLine(QPointF(s * .5, s * .2), QPointF(s * .5, s * .8));
        break;
    case ZenithIcon::Stop:
        painter.drawRect(QRectF(s * .3, s * .3, s * .4, s * .4));
        break;
    }
    painter.end();
    return QIcon(pixmap);
}
