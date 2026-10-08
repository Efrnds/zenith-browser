#include "ui/SettingsPage.h"

#include "ui/ZenithIcons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
QVBoxLayout *pageLayout(QWidget *page, const QString &title, const QString &description)
{
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    auto *heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("settingsHeading"));
    layout->addWidget(heading);
    auto *subtitle = new QLabel(description, page);
    subtitle->setObjectName(QStringLiteral("settingsDescription"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    return layout;
}
}

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , navigation_(new QListWidget(this))
    , pages_(new QStackedWidget(this))
    , searchEngine_(new QComboBox(this))
    , restoreSession_(new QCheckBox(tr("Restaurar a sessão anterior"), this))
    , compactMode_(new QCheckBox(tr("Ocultar sidebar e barra superior"), this))
    , adBlock_(new QCheckBox(tr("Bloquear anúncios e rastreadores"), this))
    , suspendOnMinimize_(new QCheckBox(tr("Descartar páginas ao minimizar"), this))
    , adBlockStatus_(new QLabel(this))
{
    setObjectName(QStringLiteral("settingsPage"));
    navigation_->setObjectName(QStringLiteral("settingsNavigation"));
    navigation_->addItems({tr("Geral"), tr("Aparência"), tr("Privacidade"),
                           tr("Performance"), tr("Sobre")});
    navigation_->setFixedWidth(176);
    navigation_->setCurrentRow(0);

    pages_->addWidget(makeGeneralPage());
    pages_->addWidget(makeAppearancePage());
    pages_->addWidget(makePrivacyPage());
    pages_->addWidget(makePerformancePage());
    pages_->addWidget(makeAboutPage());

    auto *back = new QPushButton(tr("Voltar para a página"), this);
    back->setIcon(zenithIcon(ZenithIcon::Back));
    back->setObjectName(QStringLiteral("settingsBackButton"));

    auto *left = new QVBoxLayout;
    left->setContentsMargins(10, 14, 0, 14);
    left->setSpacing(10);
    left->addWidget(back);
    left->addWidget(navigation_, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(left);
    layout->addWidget(pages_, 1);

    connect(back, &QPushButton::clicked, this, &SettingsPage::closeRequested);
    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(searchEngine_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        emit searchTemplateChanged(searchEngine_->itemData(index).toString());
    });
    connect(restoreSession_, &QCheckBox::toggled, this, &SettingsPage::restoreSessionChanged);
    connect(compactMode_, &QCheckBox::toggled, this, &SettingsPage::compactModeChanged);
    connect(adBlock_, &QCheckBox::toggled, this, &SettingsPage::adBlockChanged);
    connect(suspendOnMinimize_, &QCheckBox::toggled,
            this, &SettingsPage::suspendOnMinimizeChanged);
}

void SettingsPage::setSearchTemplate(const QString &value)
{
    const int index = searchEngine_->findData(value);
    if (index >= 0) {
        const QSignalBlocker blocker(searchEngine_);
        searchEngine_->setCurrentIndex(index);
    }
}

void SettingsPage::setCompactMode(const bool enabled)
{
    const QSignalBlocker blocker(compactMode_);
    compactMode_->setChecked(enabled);
}

void SettingsPage::setAdBlockEnabled(const bool enabled)
{
    const QSignalBlocker blocker(adBlock_);
    adBlock_->setChecked(enabled);
}

void SettingsPage::setAdBlockStatus(const quint64 blocked, const qsizetype rules,
                                    const QString &status)
{
    adBlockStatus_->setText(
        tr("%1 regras ativas\n%2 requisições bloqueadas nesta sessão\n%3")
            .arg(rules).arg(blocked).arg(status));
}

void SettingsPage::setSuspendOnMinimize(const bool enabled)
{
    const QSignalBlocker blocker(suspendOnMinimize_);
    suspendOnMinimize_->setChecked(enabled);
}

void SettingsPage::setRestoreSession(const bool enabled)
{
    const QSignalBlocker blocker(restoreSession_);
    restoreSession_->setChecked(enabled);
}

QWidget *SettingsPage::makeGeneralPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Geral"), tr("Inicialização e pesquisa."));
    searchEngine_->addItem(tr("DuckDuckGo"), QStringLiteral("https://duckduckgo.com/?q=%1"));
    searchEngine_->addItem(tr("Google"), QStringLiteral("https://www.google.com/search?q=%1"));
    searchEngine_->addItem(tr("Brave Search"), QStringLiteral("https://search.brave.com/search?q=%1"));
    searchEngine_->addItem(tr("Bing"), QStringLiteral("https://www.bing.com/search?q=%1"));
    auto *form = new QFormLayout;
    form->addRow(tr("Mecanismo de busca"), searchEngine_);
    layout->addLayout(form);
    layout->addWidget(restoreSession_);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Aparência"),
                              tr("Controle como a interface ocupa a janela."));
    layout->addWidget(compactMode_);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makePrivacyPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Privacidade"),
                              tr("Bloqueio de rede e credenciais protegidas pelo sistema."));
    layout->addWidget(adBlock_);
    adBlockStatus_->setObjectName(QStringLiteral("settingsStatus"));
    layout->addWidget(adBlockStatus_);
    auto *update = new QPushButton(tr("Atualizar listas agora"), page);
    connect(update, &QPushButton::clicked, this, &SettingsPage::updateAdBlockRequested);
    layout->addWidget(update, 0, Qt::AlignLeft);
    auto *credentials = new QPushButton(tr("Gerenciar senhas e credenciais"), page);
    connect(credentials, &QPushButton::clicked, this, &SettingsPage::manageCredentialsRequested);
    layout->addWidget(credentials, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makePerformancePage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(
        page, tr("Performance"),
        tr("Abas em segundo plano já são descartadas. Esta opção também libera a aba ativa quando a janela permanece minimizada."));
    layout->addWidget(suspendOnMinimize_);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeAboutPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Sobre"),
                              tr("Zenith Browser, interface C++20 com QtWebEngine."));
    auto *version = new QLabel(tr("Versão %1").arg(QCoreApplication::applicationVersion()), page);
    version->setObjectName(QStringLiteral("settingsStatus"));
    layout->addWidget(version);
    layout->addStretch();
    return page;
}
