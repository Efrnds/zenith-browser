#include "ui/PageWidgets.h"
#include "ui/ThemeManager.h"
#include "ui/UiMetrics.h"
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QGuiApplication>
#include <QTimer>
#include <QToolButton>
#include "ui/ZenithIcons.h"
#include <QtGui/qguiapplication_platform.h>
#if QT_CONFIG(xcb)
#include <X11/Xlib.h>
#undef None
#endif

QString PageWidgets::plainToolTip(const QString &text)
{
    return QStringLiteral("<qt>%1</qt>").arg(text.toHtmlEscaped().replace('\n', "<br>"));
}

void PageWidgets::syncWindowChrome(QWidget *window)
{
    if (!window || !window->isWindow()) return;
    window->setAutoFillBackground(true);
#if QT_CONFIG(xcb)
    if (QGuiApplication::platformName() != "xcb" || !window->testAttribute(Qt::WA_WState_Created)) return;
    auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!native) return;
    Display *display = native->display();
    const QByteArray variant = ThemeManager::instance().dark() ? "dark" : "light";
    XChangeProperty(display, window->winId(), XInternAtom(display, "_GTK_THEME_VARIANT", False),
        XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace,
        reinterpret_cast<const unsigned char *>(variant.constData()), variant.size());
    XSetWindowBackground(display, window->winId(), ThemeManager::instance().color("#18181b").rgb());
    XFlush(display);
#endif
}

QVBoxLayout *PageWidgets::section(QVBoxLayout *parent, const QString &title, const QString &description)
{
    auto *frame = new QFrame;
    frame->setObjectName("settingsSection");
    frame->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(UiMetrics::Section, UiMetrics::Section, UiMetrics::Section, UiMetrics::Section);
    layout->setSpacing(UiMetrics::Related);
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

QVBoxLayout *PageWidgets::disclosure(QVBoxLayout *parent, const QString &title, const QString &name)
{
    auto *toggle = new QToolButton(parent->parentWidget());
    toggle->setObjectName(name + "Toggle");
    toggle->setText(title);
    toggle->setAccessibleName(title);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setCheckable(true);
    toggle->setIcon(zenithIcon(ZenithIcon::ChevronRight));
    auto *body = new QWidget(parent->parentWidget());
    body->setObjectName("settingsDisclosure");
    body->setProperty("disclosureName", name);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, UiMetrics::Small, 0, 0);
    layout->setSpacing(UiMetrics::Small);
    parent->addWidget(toggle, 0, Qt::AlignLeft);
    parent->addWidget(body);
    body->hide();
    QObject::connect(toggle, &QToolButton::toggled, body, [body, toggle](bool expanded) {
        body->setVisible(expanded);
        toggle->setIcon(zenithIcon(expanded ? ZenithIcon::ChevronDown : ZenithIcon::ChevronRight));
    });
    return layout;
}

void PageWidgets::heading(QVBoxLayout *layout, const QString &title, const QString &description)
{
    if (QWidget *parent = layout->parentWidget())
        QTimer::singleShot(0, parent, [parent] { syncWindowChrome(parent->window()); });
    layout->setContentsMargins(UiMetrics::Page, UiMetrics::Page, UiMetrics::Page, UiMetrics::Section);
    layout->setSpacing(UiMetrics::Related);
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

QSize DetailListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const int height = qMax(64, QFontMetrics(option.font).height() * 2 + 22);
    return QSize(120, height + (index.data(Qt::UserRole + 5).toBool() ? 12 : 0));
}

void DetailListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = index.data(Qt::DisplayRole).toString();
    opt.text.clear();
    const QIcon icon = opt.icon;
    opt.icon = {};
    const QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    painter->save();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    painter->restore();
    painter->save();
    painter->setClipRect(option.rect);
    QRect area = option.rect.adjusted(14, 8, -14, -8);
    if (!icon.isNull()) { icon.paint(painter, QRect(area.left(), area.top() + 8, 20, 20)); area.adjust(30, 0, 0, 0); }
    const int split = text.indexOf('\n');
    const QString title = split < 0 ? text : text.left(split);
    const QString detail = split < 0 ? QString() : text.mid(split + 1).replace('\n', " · ");
    const QColor foreground = themeColor("#f4f4f5");
    QFont titleFont = option.font; titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont); painter->setPen(foreground);
    painter->drawText(QPoint(area.left(), area.top() + QFontMetrics(titleFont).ascent()),
        QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, area.width()));
    painter->setFont(option.font);
    painter->setPen(themeColor("#a1a1aa"));
    painter->drawText(QPoint(area.left(), area.top() + QFontMetrics(titleFont).height() + 6 + QFontMetrics(option.font).ascent()),
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
    painter.drawText(QRectF(18, 13, sidebar - 24, 24), Qt::AlignVCenter, tr("Espaço"));
    painter.setPen(Qt::NoPen); painter.setBrush(themeColor("#27272a"));
    painter.drawRoundedRect(QRectF(14, 44, sidebar - 16, 28), 5, 5);
    painter.setPen(themeColor("#8b7cf6"));
    painter.drawText(QRectF(23, 44, sidebar - 27, 28), Qt::AlignVCenter, tr("Aba"));
    painter.setPen(themeColor("#a1a1aa"));
    painter.drawText(QRectF(23, 80, sidebar - 27, 24), Qt::AlignVCenter, tr("Nova"));
    painter.setPen(themeColor("#323238")); painter.setBrush(themeColor("#202023"));
    painter.drawRoundedRect(QRectF(sidebar + 14, 14, width() - sidebar - 30, 26), 5, 5);
    painter.setPen(themeColor("#a1a1aa"));
    painter.drawText(QRectF(sidebar + 24, 14, width() - sidebar - 48, 26), Qt::AlignVCenter, tr("Pesquisar"));
    painter.setPen(themeColor("#f4f4f5"));
    painter.drawText(QRectF(sidebar + 22, 52, width() - sidebar - 42, 50), Qt::AlignVCenter, tr("Página"));
}
