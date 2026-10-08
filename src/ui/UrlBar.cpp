#include "ui/UrlBar.h"
#include "ui/ZenithIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

UrlBar::UrlBar(QWidget *parent)
    : QWidget(parent)
    , backButton_(new QToolButton(this))
    , forwardButton_(new QToolButton(this))
    , reloadButton_(new QToolButton(this))
    , splitButton_(new QToolButton(this))
    , moreButton_(new QToolButton(this))
    , securityIndicator_(new QLabel(QStringLiteral("WEB"), this))
    , addressInput_(new QLineEdit(this))
{
    setObjectName(QStringLiteral("urlBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setFixedHeight(58);

    backButton_->setIcon(zenithIcon(ZenithIcon::Back));
    forwardButton_->setIcon(zenithIcon(ZenithIcon::Forward));
    reloadButton_->setIcon(zenithIcon(ZenithIcon::Reload));
    splitButton_->setIcon(zenithIcon(ZenithIcon::Split));
    splitButton_->setCheckable(true);
    moreButton_->setIcon(zenithIcon(ZenithIcon::More));

    backButton_->setToolTip(tr("Voltar"));
    forwardButton_->setToolTip(tr("Avançar"));
    reloadButton_->setToolTip(tr("Recarregar"));
    splitButton_->setToolTip(tr("Alternar divisão de tela"));
    moreButton_->setToolTip(tr("Menu do navegador"));
    backButton_->setAccessibleName(tr("Voltar"));
    forwardButton_->setAccessibleName(tr("Avançar"));
    reloadButton_->setAccessibleName(tr("Recarregar"));
    splitButton_->setAccessibleName(tr("Alternar divisão de tela"));
    moreButton_->setAccessibleName(tr("Menu do navegador"));

    addressSurface_ = new QWidget(this);
    addressSurface_->setObjectName(QStringLiteral("addressSurface"));
    auto *addressLayout = new QHBoxLayout(addressSurface_);
    addressLayout->setContentsMargins(2, 0, 2, 0);
    addressLayout->setSpacing(2);

    securityIndicator_->setObjectName(QStringLiteral("securityIndicator"));
    securityIndicator_->setFixedWidth(42);
    securityIndicator_->setAlignment(Qt::AlignCenter);

    addressInput_->setObjectName(QStringLiteral("addressInput"));
    addressInput_->setPlaceholderText(tr("Pesquisar ou digitar endereço"));
    addressInput_->setClearButtonEnabled(true);
    addressInput_->installEventFilter(this);
    addressLayout->addWidget(securityIndicator_);
    addressLayout->addWidget(addressInput_, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(5);
    layout->addWidget(backButton_);
    layout->addWidget(forwardButton_);
    layout->addWidget(reloadButton_);
    layout->addSpacing(7);
    layout->addWidget(addressSurface_, 1);
    layout->addSpacing(7);
    layout->addWidget(splitButton_);
    layout->addWidget(moreButton_);

    connect(backButton_, &QToolButton::clicked, this, &UrlBar::backRequested);
    connect(forwardButton_, &QToolButton::clicked, this, &UrlBar::forwardRequested);
    connect(reloadButton_, &QToolButton::clicked, this, [this] {
        loading_ ? emit stopRequested() : emit reloadRequested();
    });
    connect(splitButton_, &QToolButton::toggled, this, &UrlBar::splitToggled);
    connect(moreButton_, &QToolButton::clicked, this, [this] {
        emit menuRequested(moreButton_->mapToGlobal(QPoint(0, moreButton_->height())));
    });
    connect(addressInput_, &QLineEdit::returnPressed, this, [this] {
        const QUrl url = urlFromInput();
        if (url.isValid()) {
            emit navigateRequested(url);
        }
    });
}

void UrlBar::setUrl(const QUrl &url)
{
    QString display = url == QUrl(QStringLiteral("about:blank"))
        ? QString()
        : url.toDisplayString(QUrl::RemovePassword);
    if (display.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        display.remove(0, 8);
    }
    if (display.endsWith('/') && url.path() == QStringLiteral("/")) {
        display.chop(1);
    }

    if (!addressInput_->hasFocus()) {
        addressInput_->setText(display);
    }
    const bool secure = url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
    securityIndicator_->setText(secure ? QStringLiteral("TLS") : QStringLiteral("WEB"));
    securityIndicator_->setToolTip(secure ? tr("Conexão HTTPS") : tr("Conexão sem HTTPS"));
}

void UrlBar::setNavigationState(const bool canGoBack, const bool canGoForward)
{
    backButton_->setEnabled(canGoBack);
    forwardButton_->setEnabled(canGoForward);
}

void UrlBar::setLoading(const bool loading)
{
    loading_ = loading;
    reloadButton_->setIcon(zenithIcon(loading_ ? ZenithIcon::Stop : ZenithIcon::Reload));
    reloadButton_->setToolTip(loading_ ? tr("Parar carregamento") : tr("Recarregar"));
}

void UrlBar::setSplitEnabled(const bool enabled)
{
    const QSignalBlocker blocker(splitButton_);
    splitButton_->setChecked(enabled);
}

void UrlBar::setSearchTemplate(const QString &searchTemplate)
{
    if (searchTemplate.contains(QStringLiteral("%1")))
        searchTemplate_ = searchTemplate;
}

void UrlBar::focusAddress()
{
    addressInput_->setFocus(Qt::ShortcutFocusReason);
    addressInput_->selectAll();
}

bool UrlBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == addressInput_
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        addressSurface_->setProperty("focused", addressInput_->hasFocus());
        addressSurface_->style()->unpolish(addressSurface_);
        addressSurface_->style()->polish(addressSurface_);
    }
    if (watched == addressInput_ && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !addressInput_->hasFocus()) {
            QTimer::singleShot(0, addressInput_, &QLineEdit::selectAll);
        }
    }
    return QWidget::eventFilter(watched, event);
}

QUrl UrlBar::urlFromInput() const
{
    const QString input = addressInput_->text().trimmed();
    if (input.isEmpty()) {
        return {};
    }

    const QUrl explicitUrl(input);
    if (!explicitUrl.scheme().isEmpty()) {
        return explicitUrl;
    }

    if (!input.contains(' ') && (input.contains('.') || input == QStringLiteral("localhost"))) {
        return QUrl(QStringLiteral("https://") + input);
    }

    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(input));
    return QUrl(searchTemplate_.arg(encoded));
}
