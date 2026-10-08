#include "ui/TabItemWidget.h"
#include "ui/ZenithIcons.h"

#include <QCursor>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

#include <algorithm>

TabItemWidget::TabItemWidget(const int tabId, QWidget *parent)
    : QWidget(parent)
    , tabId_(tabId)
    , fullTitle_(tr("Nova aba"))
{
    setObjectName(QStringLiteral("tabItem"));
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Aba"));
    setFixedHeight(36);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

int TabItemWidget::tabId() const
{
    return tabId_;
}

QSize TabItemWidget::sizeHint() const
{
    return QSize(200, 36);
}

void TabItemWidget::setTitle(const QString &title)
{
    fullTitle_ = title.trimmed().isEmpty() ? tr("Nova aba") : title;
    setToolTip(fullTitle_ + (discarded_ ? tr(" (hibernada)") : QString()));
    update();
}

void TabItemWidget::setIcon(const QIcon &icon)
{
    icon_ = icon;
    update();
}

void TabItemWidget::setDiscarded(const bool discarded)
{
    discarded_ = discarded;
    setToolTip(fullTitle_ + (discarded_ ? tr(" (hibernada)") : QString()));
    update();
}

void TabItemWidget::setCollapsed(const bool collapsed)
{
    collapsed_ = collapsed;
    update();
}

void TabItemWidget::setActive(const bool active)
{
    if (active_ == active)
        return;
    active_ = active;
    update();
}

void TabItemWidget::setLeading(const int leading)
{
    leading_ = std::max(0, leading);
    update();
}

bool TabItemWidget::pointerInRow() const
{
    if (!isVisible() || !window() || !window()->isVisible())
        return false;
    return QRect(mapToGlobal(QPoint(0, 0)), size()).contains(QCursor::pos());
}

QRect TabItemWidget::closeRect() const
{
    return QRect(width() - 26, (height() - 18) / 2, 18, 18);
}

void TabItemWidget::syncPointer()
{
    const bool inside = pointerInRow();
    if (inside == pointerInside_)
        return;
    pointerInside_ = inside;
    update();
}

void TabItemWidget::enterEvent(QEnterEvent *event)
{
    pointerInside_ = true;
    update();
    QWidget::enterEvent(event);
}

void TabItemWidget::leaveEvent(QEvent *event)
{
    pointerInside_ = false;
    update();
    QWidget::leaveEvent(event);
}

void TabItemWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit activated(tabId_);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TabItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!collapsed_ && closeRect().contains(event->position().toPoint()))
            emit closeRequested(tabId_);
        else
            emit activated(tabId_);
    }
    QWidget::mousePressEvent(event);
}

void TabItemWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setClipRect(rect());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#18181b")));
    painter.drawRect(rect());

    if (active_ || pointerInside_) {
        painter.setBrush(QColor(active_ ? QStringLiteral("#27272a") : QStringLiteral("#202023")));
        painter.drawRoundedRect(rect().adjusted(0, 1, -1, -2), 7, 7);
    }
    if (hasFocus()) {
        painter.setPen(QPen(QColor(QStringLiteral("#8b7cf6")), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 7, 7);
        painter.setPen(Qt::NoPen);
    }

    if (collapsed_)
        return;

    const QRect iconRect(leading_ + 8, (height() - 16) / 2, 16, 16);
    const QIcon drawn = icon_.isNull() ? style()->standardIcon(QStyle::SP_FileIcon) : icon_;
    drawn.paint(&painter, iconRect, Qt::AlignCenter);

    const bool showClose = pointerInside_;
    const int textRight = width() - (showClose || discarded_ ? 30 : 10);
    const QRect textRect(iconRect.right() + 8, 0, std::max(0, textRight - iconRect.right() - 8), height());
    painter.setPen(QColor(QStringLiteral("#d4d4d8")));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(fullTitle_, Qt::ElideRight, textRect.width()));

    if (showClose) {
        zenithIcon(ZenithIcon::Close, QColor(QStringLiteral("#a1a1aa")), 12)
            .paint(&painter, closeRect(), Qt::AlignCenter);
    } else if (discarded_) {
        const QRect sleepRect(width() - 24, (height() - 14) / 2, 14, 14);
        zenithIcon(ZenithIcon::Sleep, QColor(QStringLiteral("#8b7cf6")), 14)
            .paint(&painter, sleepRect, Qt::AlignCenter);
    }
}
