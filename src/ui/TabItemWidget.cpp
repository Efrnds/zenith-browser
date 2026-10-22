#include "ui/ThemeManager.h"
#include "ui/TabItemWidget.h"
#include <QSettings>
#include "ui/ZenithIcons.h"
#include "ui/UiMetrics.h"
#include "ui/PageWidgets.h"

#include <QCursor>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPointer>
#include <QAccessibleWidget>
#include <QTimer>

#include <algorithm>

namespace {
class AccessibleTab final : public QAccessibleWidget
{
public:
    explicit AccessibleTab(TabItemWidget *tab) : QAccessibleWidget(tab, QAccessible::PageTab) {}
    QAccessible::State state() const override {
        auto result = QAccessibleWidget::state();
        result.selectable = true;
        result.selected = static_cast<TabItemWidget *>(widget())->isActive();
        return result;
    }
    QStringList actionNames() const override {
        return {pressAction(), setFocusAction(), QStringLiteral("close")};
    }
    QString localizedActionName(const QString &name) const override {
        return name == "close" ? TabItemWidget::tr("Fechar aba") : QAccessibleWidget::localizedActionName(name);
    }
    void doAction(const QString &name) override {
        auto *tab = static_cast<TabItemWidget *>(widget());
        if (!tab->isEnabled()) return;
        if (name == pressAction() || name == "close") {
            QTimer::singleShot(0, tab, [tab, name] {
                if (name == "close") emit tab->closeRequested(tab->tabId());
                else emit tab->activated(tab->tabId());
            });
        } else QAccessibleWidget::doAction(name);
    }
    QStringList keyBindingsForAction(const QString &name) const override {
        return name == pressAction() ? QStringList{"Enter", "Space"} : QStringList{};
    }
};
}

TabItemWidget::TabItemWidget(const int tabId, QWidget *parent)
    : QWidget(parent)
    , tabId_(tabId)
    , fullTitle_(tr("Nova aba"))
{
    static const bool accessible = [] {
        QAccessible::installFactory([](const QString &name, QObject *object) -> QAccessibleInterface * {
            if (name == "TabItemWidget") return new AccessibleTab(static_cast<TabItemWidget *>(object));
            return nullptr;
        });
        return true;
    }();
    Q_UNUSED(accessible);
    setObjectName(QStringLiteral("tabItem"));
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Aba"));
    setFixedHeight(qBound(32, QSettings().value("appearance/tabHeight", 36).toInt(), 48));
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

int TabItemWidget::tabId() const
{
    return tabId_;
}

QSize TabItemWidget::sizeHint() const
{
    return QSize(200, height());
}

void TabItemWidget::setTitle(const QString &title)
{
    fullTitle_ = title.trimmed().isEmpty() ? tr("Nova aba") : title;
    setAccessibleName(fullTitle_);
    setToolTip(PageWidgets::plainToolTip(fullTitle_ + (discarded_ ? tr(" (hibernada)") : QString())));
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
    setToolTip(PageWidgets::plainToolTip(fullTitle_ + (discarded_ ? tr(" (hibernada)") : QString())));
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
    QAccessible::State changed;
    changed.selected = true;
    QAccessibleStateChangeEvent event(this, changed);
    QAccessible::updateAccessibility(&event);
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
    return QRect(width() - UiMetrics::CloseHit - 2, (height() - UiMetrics::CloseHit) / 2, UiMetrics::CloseHit, UiMetrics::CloseHit);
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
        dragStart_ = event->position().toPoint();
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
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setClipRect(rect());
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColor(QStringLiteral("#18181b")));
    painter.drawRect(rect());

    if (active_ || pointerInside_) {
        painter.setBrush(themeColor(active_ ? QStringLiteral("#27272a") : QStringLiteral("#202023")));
        painter.drawRoundedRect(rect().adjusted(0, 1, -1, -2), 7, 7);
    }
    if (hasFocus()) {
        painter.setPen(QPen(themeColor(QStringLiteral("#8b7cf6")), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 7, 7);
        painter.setPen(Qt::NoPen);
    }

    if (collapsed_)
        return;

    const QRect iconRect(leading_ + 8, (height() - 16) / 2, 16, 16);
    const QIcon drawn = icon_.isNull() ? zenithIcon(ZenithIcon::Document) : icon_;
    drawn.paint(&painter, iconRect, Qt::AlignCenter);

    const bool showClose = pointerInside_ || hasFocus();
    const int textRight = closeRect().left() - 4;
    const QRect textRect(iconRect.right() + 8, 0, std::max(0, textRight - iconRect.right() - 8), height());
    painter.setPen(themeColor(QStringLiteral("#d4d4d8")));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(fullTitle_, Qt::ElideRight, textRect.width()));

    if (showClose) {
        zenithIcon(ZenithIcon::Close, themeColor(QStringLiteral("#a1a1aa")), 12)
            .paint(&painter, closeRect().adjusted(6, 6, -6, -6), Qt::AlignCenter);
    } else if (discarded_) {
        const QRect sleepRect(width() - 24, (height() - 14) / 2, 14, 14);
        zenithIcon(ZenithIcon::Sleep, themeColor(QStringLiteral("#8b7cf6")), 14)
            .paint(&painter, sleepRect, Qt::AlignCenter);
    }
}

void TabItemWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || collapsed_
        || (event->position().toPoint() - dragStart_).manhattanLength() < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    QPointer<QDrag> drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setData("application/x-zenith-tab", QByteArray::number(tabId_));
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    if (drag) drag->deleteLater();
}
void TabItemWidget::dragEnterEvent(QDragEnterEvent *event)
{
    const auto *source = qobject_cast<TabItemWidget *>(event->source());
    if (event->mimeData()->hasFormat("application/x-zenith-tab") && source && source->window() == window())
        event->acceptProposedAction();
}
void TabItemWidget::dropEvent(QDropEvent *event)
{
    const auto *source = qobject_cast<TabItemWidget *>(event->source());
    if (!source || source->window() != window()) return;
    const int sourceId = source->tabId();
    event->acceptProposedAction();
    // The sidebar rebuild may delete either row, so dispatch after the drop ends.
    QMetaObject::invokeMethod(this, [this, sourceId] { emit moveRequested(sourceId, tabId_); }, Qt::QueuedConnection);
}
