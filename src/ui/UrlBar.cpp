#include "ui/UrlBar.h"
#include "ui/ZenithIcons.h"
#include "core/Navigation.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QCompleter>
#include <QStringListModel>

UrlBar::UrlBar(QWidget *parent)
    : QWidget(parent)
    , backButton_(new QToolButton(this))
    , forwardButton_(new QToolButton(this))
    , reloadButton_(new QToolButton(this))
    , compactButton_(new QToolButton(this))
    , bookmarkButton_(new QToolButton(this))
    , downloadsButton_(new QToolButton(this))
    , moreButton_(new QToolButton(this))
    , securityIndicator_(new QToolButton(this))
    , addressInput_(new QLineEdit(this))
{
    setObjectName(QStringLiteral("urlBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setFixedHeight(58);

    backButton_->setIcon(zenithIcon(ZenithIcon::Back));
    forwardButton_->setIcon(zenithIcon(ZenithIcon::Forward));
    reloadButton_->setIcon(zenithIcon(ZenithIcon::Reload));
    compactButton_->setIcon(zenithIcon(ZenithIcon::Sidebar));
    compactButton_->setCheckable(true);
    bookmarkButton_->setIcon(zenithIcon(ZenithIcon::Bookmark));
    downloadsButton_->setIcon(zenithIcon(ZenithIcon::Download));
    moreButton_->setIcon(zenithIcon(ZenithIcon::Menu));
    compactButton_->setObjectName("compactButton");
    bookmarkButton_->setObjectName("bookmarkButton");
    downloadsButton_->setObjectName("downloadsButton");
    moreButton_->setObjectName("browserMenuButton");
    bookmarkButton_->setEnabled(false);
    bookmarkButton_->hide();

    backButton_->setToolTip(tr("Voltar"));
    forwardButton_->setToolTip(tr("Avançar"));
    reloadButton_->setToolTip(tr("Recarregar"));
    compactButton_->setToolTip(tr("Modo compacto"));
    bookmarkButton_->setToolTip(tr("Adicionar favorito · Ctrl+D"));
    downloadsButton_->setToolTip(tr("Downloads · Ctrl+J"));
    moreButton_->setToolTip(tr("Menu do navegador"));
    backButton_->setAccessibleName(tr("Voltar"));
    forwardButton_->setAccessibleName(tr("Avançar"));
    reloadButton_->setAccessibleName(tr("Recarregar"));
    compactButton_->setAccessibleName(tr("Modo compacto"));
    bookmarkButton_->setAccessibleName(tr("Adicionar favorito"));
    downloadsButton_->setAccessibleName(tr("Downloads"));
    moreButton_->setAccessibleName(tr("Menu do navegador"));

    addressSurface_ = new QWidget(this);
    addressSurface_->setObjectName(QStringLiteral("addressSurface"));
    auto *addressLayout = new QHBoxLayout(addressSurface_);
    addressLayout->setContentsMargins(2, 0, 2, 0);
    addressLayout->setSpacing(2);

    securityIndicator_->setObjectName(QStringLiteral("securityIndicator"));
    securityIndicator_->setText(QStringLiteral("WEB"));
    securityIndicator_->setAccessibleName(tr("Permissões deste site"));
    securityIndicator_->setEnabled(false);
    securityIndicator_->hide();
    securityIndicator_->setFixedWidth(42);

    addressInput_->setObjectName(QStringLiteral("addressInput"));
    addressInput_->setPlaceholderText(tr("Pesquisar ou digitar endereço"));
    addressInput_->setClearButtonEnabled(true);
    addressInput_->installEventFilter(this);
    connect(addressInput_, &QLineEdit::textChanged, this, [this] { centerClearButton(); });
    addressLayout->addWidget(securityIndicator_);
    addressLayout->addWidget(addressInput_, 1);
    addressLayout->addWidget(bookmarkButton_);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(5);
    layout->addWidget(compactButton_);
    layout->addSpacing(3);
    layout->addWidget(backButton_);
    layout->addWidget(forwardButton_);
    layout->addWidget(reloadButton_);
    layout->addSpacing(7);
    layout->addWidget(addressSurface_, 1);
    layout->addSpacing(7);
    layout->addWidget(downloadsButton_);
    layout->addWidget(moreButton_);

    connect(backButton_, &QToolButton::clicked, this, &UrlBar::backRequested);
    connect(forwardButton_, &QToolButton::clicked, this, &UrlBar::forwardRequested);
    connect(reloadButton_, &QToolButton::clicked, this, [this] {
        loading_ ? emit stopRequested() : emit reloadRequested();
    });
    connect(compactButton_, &QToolButton::clicked, this, &UrlBar::compactToggled);
    connect(bookmarkButton_, &QToolButton::clicked, this, &UrlBar::bookmarkRequested);
    connect(downloadsButton_, &QToolButton::clicked, this, &UrlBar::downloadsRequested);
    connect(securityIndicator_, &QToolButton::clicked, this, &UrlBar::siteControlsRequested);
    connect(moreButton_, &QToolButton::clicked, this, [this] {
        emit menuRequested(menuPosition());
    });
    connect(addressInput_, &QLineEdit::returnPressed, this, [this] {
        const QUrl url = urlFromInput();
        if (url.isValid()) {
            emit navigateRequested(url);
        }
    });
}

void UrlBar::setUrl(const QUrl &url, bool certificateException)
{
    currentUrl_ = url;
    QString display = url == QUrl(QStringLiteral("about:blank"))
        ? QString()
        : url.toDisplayString(QUrl::RemoveUserInfo);
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
    const bool web = pageAvailable_ && Navigation::isWebUrl(url);
    securityIndicator_->setEnabled(web);
    bookmarkButton_->setEnabled(web);
    securityIndicator_->setVisible(web);
    bookmarkButton_->setVisible(web);
    securityIndicator_->setText(certificateException ? QStringLiteral("EXC") : secure ? QStringLiteral("TLS") : QStringLiteral("WEB"));
    securityIndicator_->setToolTip(certificateException ? tr("Certificado aceito por exceção nesta sessão") : secure ? tr("Conexão HTTPS") : tr("Conexão sem HTTPS"));
}

void UrlBar::setPageAvailable(bool available)
{
    pageAvailable_ = available;
    reloadButton_->setEnabled(available);
    const bool web = available && Navigation::isWebUrl(currentUrl_);
    securityIndicator_->setEnabled(web);
    bookmarkButton_->setEnabled(web);
    securityIndicator_->setVisible(web);
    bookmarkButton_->setVisible(web);
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

QPoint UrlBar::menuPosition() const
{
    return moreButton_->mapToGlobal(QPoint(0, moreButton_->height()));
}

void UrlBar::setCompactEnabled(const bool enabled)
{
    const QSignalBlocker blocker(compactButton_);
    compactButton_->setChecked(enabled);
    compactButton_->setToolTip(enabled ? tr("Desativar modo compacto") : tr("Ativar modo compacto"));
}

void UrlBar::setSearchTemplate(const QString &searchTemplate)
{
    if (searchTemplate.contains(QStringLiteral("%1")))
        searchTemplate_ = searchTemplate;
}

void UrlBar::setSuggestions(const QStringList &urls)
{
    if (!addressInput_->completer()) {
        auto *completer = new QCompleter(addressInput_);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setModel(new QStringListModel(completer));
        addressInput_->setCompleter(completer);
    }
    auto *model = qobject_cast<QStringListModel *>(addressInput_->completer()->model());
    model->setStringList(urls);
}

void UrlBar::focusAddress()
{
    addressInput_->setFocus(Qt::ShortcutFocusReason);
    addressInput_->selectAll();
}

void UrlBar::clearAddressFocus()
{
    addressInput_->clearFocus();
}

void UrlBar::injectKey(const int key, const Qt::KeyboardModifiers modifiers, const QString &text)
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(addressInput_, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
    QApplication::sendEvent(addressInput_, &release);
}

bool UrlBar::addressHasFocus() const
{
    return addressInput_->hasFocus();
}

void UrlBar::centerClearButton()
{
    constexpr int buttonSize = 18;
    for (QToolButton *button : addressInput_->findChildren<QToolButton *>()) {
        button->setFixedSize(buttonSize, buttonSize);
        button->setIconSize(QSize(12, 12));
        const int y = qMax(0, (addressInput_->height() - buttonSize) / 2);
        if (button->y() != y)
            button->move(button->x(), y);
    }
}

bool UrlBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == addressInput_
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        QTimer::singleShot(0, this, [this] { centerClearButton(); });
    }
    if (watched == addressInput_
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        addressSurface_->setProperty("focused", addressInput_->hasFocus());
        addressSurface_->style()->unpolish(addressSurface_);
        addressSurface_->style()->polish(addressSurface_);
        emit addressFocusChanged(addressInput_->hasFocus());
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
    return Navigation::fromInput(addressInput_->text(), searchTemplate_);
}
