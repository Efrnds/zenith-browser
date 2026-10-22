#include "ui/StartPage.h"
#include "ui/ZenithIcons.h"
#include "core/Navigation.h"
#include "ui/UiMetrics.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

StartPage::StartPage(QWidget *parent) : QWidget(parent)
{
    setObjectName("startPage");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *content = new QWidget(this);
    content->setMaximumWidth(620);
    auto *body = new QVBoxLayout(content);
    body->setContentsMargins(UiMetrics::Page, UiMetrics::Page, UiMetrics::Page, UiMetrics::Page);
    body->setSpacing(UiMetrics::Section);
    workspace_ = new QLabel(content);
    workspace_->setObjectName("startWorkspace");
    workspace_->setWordWrap(true);
    auto *brand = new QLabel("Zenith", content);
    brand->setObjectName("startBrand");
    auto *intro = new QLabel(tr("Pesquise na web ou abra um endereço."), content);
    intro->setObjectName("startIntro");
    intro->setWordWrap(true);
    body->addWidget(workspace_);
    body->addWidget(brand);
    body->addWidget(intro);
    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    search_ = new QLineEdit(content);
    search_->setObjectName("startSearch");
    search_->setAccessibleName(tr("Pesquisar ou abrir endereço"));
    search_->setPlaceholderText(tr("Pesquisar ou digitar endereço"));
    search_->setClearButtonEnabled(true);
    search_->setMinimumWidth(0);
    auto *open = new QPushButton(tr("Abrir"), content);
    open->setObjectName("startOpen");
    open->setProperty("primary", true);
    open->setEnabled(false);
    searchRow->addWidget(search_, 1);
    searchRow->addWidget(open);
    body->addLayout(searchRow);
    engine_ = new QLabel(content);
    engine_->setWordWrap(true);
    engine_->setObjectName("settingsDescription");
    body->addWidget(engine_);
    auto submit = [this] {
        const QUrl url = Navigation::fromInput(search_->text(), searchTemplate_);
        if (!url.isEmpty()) { search_->clear(); emit navigateRequested(url); }
    };
    connect(search_, &QLineEdit::returnPressed, this, submit);
    connect(open, &QPushButton::clicked, this, submit);
    connect(search_, &QLineEdit::textChanged, open, [this, open](const QString &text) {
        open->setEnabled(!Navigation::fromInput(text, searchTemplate_).isEmpty());
    });
    auto *actions = new QHBoxLayout;
    actions->setSpacing(4);
    const QList<QPair<QString, int>> destinations{{tr("Favoritos"), Navigation::Bookmarks}, {tr("Histórico"), Navigation::History}, {tr("Downloads"), Navigation::Downloads}};
    const QList<ZenithIcon> icons{ZenithIcon::Bookmark, ZenithIcon::History, ZenithIcon::Download};
    for (int index = 0; index < destinations.size(); ++index) {
        auto *button = new QPushButton(destinations.at(index).first, content);
        button->setObjectName("startAction");
        button->setIcon(zenithIcon(icons.at(index)));
        button->setProperty("command", destinations.at(index).second);
        connect(button, &QPushButton::clicked, this, [this, command = destinations.at(index).second] { emit commandRequested(command); });
        actions->addWidget(button);
    }
    actions->addStretch();
    body->addSpacing(8);
    body->addLayout(actions);
    auto *shortcut = new QLabel(tr("Ctrl+L  Endereço    ·    Ctrl+T  Nova aba"), content);
    shortcut->setObjectName("startShortcuts");
    shortcut->setWordWrap(true);
    body->addSpacing(22);
    body->addWidget(shortcut);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addStretch(2);
    auto *center = new QHBoxLayout;
    center->addStretch();
    center->addWidget(content, 100);
    center->addStretch();
    layout->addLayout(center);
    layout->addStretch(3);
    setSearchTemplate("https://duckduckgo.com/?q=%1");
}

void StartPage::setWorkspaceName(const QString &value)
{
    workspace_->setText(value);
    workspace_->setAccessibleName(tr("Espaço atual: %1").arg(value));
}

void StartPage::setSearchTemplate(const QString &value)
{
    searchTemplate_ = value;
    const QString host = QUrl(value).host();
    const QString name = host == "duckduckgo.com" ? "DuckDuckGo" : host == "www.google.com" ? "Google"
        : host == "search.brave.com" ? "Brave Search" : host == "www.bing.com" ? "Bing" : tr("seu mecanismo personalizado");
    engine_->setText(tr("Pesquisa com %1").arg(name));
}
