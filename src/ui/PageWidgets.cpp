#include "ui/PageWidgets.h"
#include "ui/ThemeManager.h"
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleOptionViewItem>

QVBoxLayout *PageWidgets::section(QVBoxLayout *parent, const QString &title, const QString &description)
{
    auto *frame = new QFrame;
    frame->setObjectName("settingsSection");
    frame->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);
    auto *label = new QLabel(title, frame);
    label->setObjectName("sectionHeading");
    label->setWordWrap(true);
    layout->addWidget(label);
    if (!description.isEmpty()) {
        auto *help = new QLabel(description, frame);
        help->setObjectName("settingsDescription");
        help->setWordWrap(true);
        layout->addWidget(help);
    }
    parent->addWidget(frame);
    return layout;
}

void PageWidgets::heading(QVBoxLayout *layout, const QString &title, const QString &description)
{
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);
    auto *heading = new QLabel(title);
    heading->setObjectName("dialogHeading");
    heading->setWordWrap(true);
    layout->addWidget(heading);
    auto *help = new QLabel(description);
    help->setObjectName("settingsDescription");
    help->setWordWrap(true);
    layout->addWidget(help);
}

QLabel *PageWidgets::emptyState(QWidget *parent, const QString &title, const QString &description)
{
    auto *label = new QLabel(title + "\n\n" + description, parent);
    label->setTextFormat(Qt::PlainText);
    label->setObjectName("emptyState");
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(120);
    return label;
}

QSize DetailListDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
{
    return QSize(120, index.data(Qt::UserRole + 5).toBool() ? 76 : 64);
}

void DetailListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();
    const QIcon icon = opt.icon;
    opt.icon = {};
    const QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    painter->save();
    painter->setClipRect(option.rect);
    QRect area = option.rect.adjusted(14, 8, -14, -8);
    if (!icon.isNull()) { icon.paint(painter, QRect(area.left(), area.top() + 8, 20, 20)); area.adjust(30, 0, 0, 0); }
    const int split = text.indexOf('\n');
    const QString title = split < 0 ? text : text.left(split);
    const QString detail = split < 0 ? QString() : text.mid(split + 1).replace('\n', " · ");
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const QColor foreground = selected ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Text);
    QFont titleFont = option.font; titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont); painter->setPen(foreground);
    painter->drawText(QRect(area.left(), area.top(), area.width(), 23), Qt::AlignVCenter,
        QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, area.width()));
    painter->setFont(option.font);
    painter->setPen(selected ? foreground : option.palette.color(QPalette::PlaceholderText));
    painter->drawText(QRect(area.left(), area.top() + 25, area.width(), 19), Qt::AlignVCenter,
        QFontMetrics(option.font).elidedText(detail, Qt::ElideRight, area.width()));
    if (index.data(Qt::UserRole + 5).toBool()) {
        QRectF track(area.left(), option.rect.bottom() - 10, area.width(), 3);
        painter->fillRect(track, option.palette.color(QPalette::AlternateBase));
        track.setWidth(track.width() * qBound(0.0, index.data(Qt::UserRole + 4).toDouble(), 1.0));
        painter->fillRect(track, option.palette.color(QPalette::Link));
    }
    painter->restore();
}

ThemePreview::ThemePreview(QWidget *parent) : QWidget(parent)
{
    setObjectName("themePreview");
    setAccessibleName(tr("Prévia das cores da interface"));
    setMinimumHeight(128);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void ThemePreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(themeColor("#3f3f46")); painter.setBrush(themeColor("#121214"));
    painter.drawRoundedRect(bounds, 8, 8);
    const qreal sidebar = qMin(112., bounds.width() * .28);
    painter.fillRect(QRectF(9, 9, sidebar - 8, height() - 18), themeColor("#18181b"));
    painter.setPen(themeColor("#a1a1aa"));
    painter.drawText(QRectF(18, 13, sidebar - 24, 24), Qt::AlignVCenter, tr("Seu espaço"));
    painter.setPen(Qt::NoPen); painter.setBrush(themeColor("#27272a"));
    painter.drawRoundedRect(QRectF(14, 44, sidebar - 16, 28), 5, 5);
    painter.setPen(themeColor("#8b7cf6"));
    painter.drawText(QRectF(23, 44, sidebar - 27, 28), Qt::AlignVCenter, tr("Aba ativa"));
    painter.setPen(themeColor("#a1a1aa"));
    painter.drawText(QRectF(23, 80, sidebar - 27, 24), Qt::AlignVCenter, tr("Nova aba"));
    painter.setPen(themeColor("#323238")); painter.setBrush(themeColor("#202023"));
    painter.drawRoundedRect(QRectF(sidebar + 14, 14, width() - sidebar - 30, 26), 5, 5);
    painter.setPen(themeColor("#a1a1aa"));
    painter.drawText(QRectF(sidebar + 24, 14, width() - sidebar - 48, 26), Qt::AlignVCenter, tr("Pesquisar"));
    painter.setPen(themeColor("#f4f4f5"));
    painter.drawText(QRectF(sidebar + 22, 52, width() - sidebar - 42, 50), Qt::AlignVCenter, tr("Seu conteúdo, em foco."));
}
