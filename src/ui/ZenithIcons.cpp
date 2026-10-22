#include <QIconEngine>
#include <QApplication>
#include <QPalette>
#include "ui/ZenithIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QHash>

namespace {
void drawIcon(QPainter &painter, ZenithIcon type, const QColor &color, int size)
{
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
    case ZenithIcon::Document: {
        QPainterPath page;
        page.moveTo(s * .25, s * .12); page.lineTo(s * .58, s * .12);
        page.lineTo(s * .77, s * .32); page.lineTo(s * .77, s * .88);
        page.lineTo(s * .25, s * .88); page.closeSubpath();
        painter.drawPath(page);
        painter.drawLine(QPointF(s * .58, s * .12), QPointF(s * .58, s * .32));
        painter.drawLine(QPointF(s * .58, s * .32), QPointF(s * .77, s * .32));
        break;
    }
    case ZenithIcon::Menu:
        for (const qreal y : {.28, .5, .72})
            painter.drawLine(QPointF(s * .22, s * y), QPointF(s * .78, s * y));
        break;
    case ZenithIcon::History:
        painter.drawEllipse(QRectF(s * .18, s * .18, s * .64, s * .64));
        painter.drawLine(QPointF(s * .5, s * .32), QPointF(s * .5, s * .5));
        painter.drawLine(QPointF(s * .5, s * .5), QPointF(s * .64, s * .58));
        break;
    case ZenithIcon::Sidebar:
        painter.drawRoundedRect(QRectF(s * .14, s * .2, s * .72, s * .6), 2, 2);
        painter.drawLine(QPointF(s * .39, s * .2), QPointF(s * .39, s * .8));
        break;
    case ZenithIcon::Download:
        painter.drawLine(QPointF(s * .5, s * .15), QPointF(s * .5, s * .65));
        painter.drawLine(QPointF(s * .3, s * .45), QPointF(s * .5, s * .65));
        painter.drawLine(QPointF(s * .7, s * .45), QPointF(s * .5, s * .65));
        painter.drawLine(QPointF(s * .2, s * .83), QPointF(s * .8, s * .83));
        break;
    case ZenithIcon::Bookmark: {
        QPainterPath path;
        path.moveTo(s * .28, s * .15); path.lineTo(s * .72, s * .15);
        path.lineTo(s * .72, s * .85); path.lineTo(s * .5, s * .68);
        path.lineTo(s * .28, s * .85); path.closeSubpath();
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
        painter.drawLine(QPointF(s * .15, s * .3), QPointF(s * .85, s * .3));
        painter.drawLine(QPointF(s * .15, s * .7), QPointF(s * .85, s * .7));
        painter.setBrush(qApp->palette().color(QPalette::Window));
        painter.drawEllipse(QPointF(s * .35, s * .3), s * .12, s * .12);
        painter.drawEllipse(QPointF(s * .65, s * .7), s * .12, s * .12);
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
        painter.drawLine(QPointF(s * .28, s * .24), QPointF(s * .28, s * .76));
        painter.drawLine(QPointF(s * .66, s * .24), QPointF(s * .66, s * .76));
        break;
    case ZenithIcon::Split:
        painter.drawRoundedRect(QRectF(s * .15, s * .2, s * .7, s * .6), 2, 2);
        painter.drawLine(QPointF(s * .5, s * .2), QPointF(s * .5, s * .8));
        break;
    case ZenithIcon::Stop:
        painter.drawRect(QRectF(s * .3, s * .3, s * .4, s * .4));
        break;
    }
}
class IconEngine final : public QIconEngine {
public:
    IconEngine(ZenithIcon type, QColor color) : type_(type), color_(color) {}
    QIconEngine *clone() const override { return new IconEngine(type_, color_); }
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
        painter->save();
        const qreal side = qMin(rect.width(), rect.height());
        painter->translate(rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2);
        painter->scale(side / 18., side / 18.);
        QColor color = color_.isValid() ? color_ : qApp->palette().color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::ButtonText);
        drawIcon(*painter, type_, color, 18);
        painter->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pixmap(size); pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap); paint(&painter, QRect(QPoint(), size), mode, state);
        return pixmap;
    }
private:
    ZenithIcon type_;
    QColor color_;
};
}
QIcon zenithIcon(ZenithIcon type, const QColor &color, int)
{
    return QIcon(new IconEngine(type, color));
}
