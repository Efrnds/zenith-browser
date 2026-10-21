#include "ui/SettingsPage.h"
#include "ui/ThemeManager.h"
#include "core/Navigation.h"

#include "ui/ZenithIcons.h"
#include "core/WebProfile.h"
#include <QSysInfo>

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
#include <QSpinBox>
#include <QSettings>
#include <QScrollArea>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>

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
    searchEngine_->setObjectName("searchEngine");
    searchEngine_->setAccessibleName(tr("Mecanismo de busca"));
    navigation_->setObjectName(QStringLiteral("settingsNavigation"));
    navigation_->addItems({tr("Geral"), tr("Aparência"), tr("Espaços"), tr("Privacidade"),
                           tr("Performance"), tr("Sobre")});
    navigation_->setFixedWidth(148);
    navigation_->setCurrentRow(0);

    pages_->addWidget(makeGeneralPage());
    pages_->addWidget(makeAppearancePage());
    pages_->addWidget(makeWorkspacesPage());
    pages_->addWidget(makePrivacyPage());
    pages_->addWidget(makePerformancePage());
    pages_->addWidget(makeAboutPage());

    for (int index = 0; index < pages_->count(); ++index) {
        QWidget *page = pages_->widget(index);
        pages_->removeWidget(page);
        auto *scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(page);
        pages_->insertWidget(index, scroll);
    }

    auto *back = new QPushButton(tr("Voltar"), this);
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
    int index = searchEngine_->findData(value);
    if (index < 0 && value.contains("%1")) {
        const QSignalBlocker blocker(searchEngine_);
        searchEngine_->addItem(tr("Personalizado"), value);
        index = searchEngine_->count() - 1;
    }
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
        tr("%1 regras ativas\n%2 navegações bloqueadas nesta sessão\n%3")
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
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->addRow(tr("Mecanismo de busca"), searchEngine_);
    layout->addLayout(form);
    auto *custom = new QPushButton(tr("Adicionar busca personalizada"), page);
    connect(custom, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString value = QInputDialog::getText(this, tr("Busca personalizada"),
            tr("Endereço de busca (use %1 para o termo):"), QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok) return;
        if (!value.contains("%1") || !Navigation::isWebUrl(QUrl(value.arg("zenith")))) {
            QMessageBox::warning(this, tr("Busca personalizada"), tr("Use um endereço HTTP ou HTTPS com %1 no lugar da pesquisa."));
            return;
        }
        setSearchTemplate(value);
        emit searchTemplateChanged(value);
    });
    layout->addWidget(custom, 0, Qt::AlignLeft);
    layout->addWidget(restoreSession_);
    auto *defaultBrowser = new QPushButton(tr("Definir como navegador padrão"), page);
    connect(defaultBrowser, &QPushButton::clicked, this, &SettingsPage::defaultBrowserRequested);
    layout->addWidget(defaultBrowser, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Aparência"),
                              tr("Sua interface, suas cores. As mudanças são aplicadas na hora."));
    auto &theme = ThemeManager::instance();
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *mode = new QComboBox(page);
    mode->setObjectName("browserTheme");
    mode->setAccessibleName(tr("Tema do navegador"));
    mode->addItem(tr("Sistema"), "system");
    mode->addItem(tr("Claro"), "light");
    mode->addItem(tr("Escuro"), "dark");
    mode->setCurrentIndex(mode->findData(theme.mode()));
    form->addRow(tr("Tema do navegador"), mode);
    connect(mode, &QComboBox::activated, this, [mode](int index) { ThemeManager::instance().setMode(mode->itemData(index).toString()); });
    auto *accent = new QComboBox(page);
    accent->setObjectName("browserAccent");
    accent->setAccessibleName(tr("Cor de destaque"));
    for (const auto &entry : QList<QPair<QString, QString>>{{tr("Violeta"), "violet"}, {tr("Azul"), "blue"}, {tr("Verde"), "teal"}, {tr("Âmbar"), "amber"}})
        accent->addItem(entry.first, entry.second);
    accent->setCurrentIndex(accent->findData(theme.accent()));
    form->addRow(tr("Cor de destaque"), accent);
    connect(accent, &QComboBox::activated, this, [accent](int index) { ThemeManager::instance().setAccent(accent->itemData(index).toString()); });
    auto *sites = new QComboBox(page);
    sites->setObjectName("siteTheme");
    sites->setAccessibleName(tr("Preferência dos sites"));
    sites->addItem(tr("Sistema"), "system");
    sites->addItem(tr("Claro"), "light");
    sites->addItem(tr("Escuro"), "dark");
    sites->setCurrentIndex(sites->findData(theme.siteMode()));
    form->addRow(tr("Preferência dos sites"), sites);
    connect(sites, &QComboBox::activated, this, [sites](int index) { ThemeManager::instance().setSiteMode(sites->itemData(index).toString()); });
    auto *text = new QComboBox(page);
    text->setObjectName("textRendering");
    text->setAccessibleName(tr("Renderização de texto"));
    text->addItem(tr("Suave"), "smooth");
    text->addItem(tr("Mais definido"), "sharp");
    text->addItem(tr("Sistema"), "system");
    text->setCurrentIndex(text->findData(theme.textRendering()));
    form->addRow(tr("Texto nas páginas"), text);
    connect(text, &QComboBox::activated, this, [text](int index) { ThemeManager::instance().setTextRendering(text->itemData(index).toString()); });
    layout->addLayout(form);
    auto *explanation = new QLabel(tr("O tema dos sites é uma preferência: cada página decide como usá-la. Texto suave usa antialiasing em tons de cinza para evitar bordas coloridas."), page);
    explanation->setWordWrap(true);
    explanation->setObjectName("settingsDescription");
    layout->addWidget(explanation);
    layout->addWidget(compactMode_);
    auto *motion = new QCheckBox(tr("Reduzir animações da interface"), page);
    motion->setChecked(QSettings().value(QStringLiteral("preferences/reducedMotion"), false).toBool());
    connect(motion, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings().setValue(QStringLiteral("preferences/reducedMotion"), enabled);
        emit reducedMotionChanged(enabled);
    });
    layout->addWidget(motion);
    layout->addStretch();
    return page;
}

void SettingsPage::setWorkspaces(const QStringList &names, int current)
{
    workspaceList_->clear();
    for (int index = 0; index < names.size(); ++index)
        workspaceList_->addItem(names.at(index) + (index == current ? tr(" · atual") : QString()));
    workspaceList_->setCurrentRow(current);
    if (auto *button = findChild<QPushButton *>("settingsCreateWorkspace")) button->setEnabled(names.size() < 16);
}

QWidget *SettingsPage::makeWorkspacesPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Espaços"), tr("Separe suas abas por contexto. Crie até 16 espaços e alterne pelo seletor da barra lateral."));
    workspaceList_ = new QListWidget(page);
    workspaceList_->setAccessibleName(tr("Espaços de trabalho"));
    workspaceList_->setMinimumHeight(120);
    layout->addWidget(workspaceList_);
    const QList<QPair<QString, int>> actions{{tr("Criar espaço"), 0}, {tr("Renomear espaço atual"), 1}, {tr("Excluir último espaço vazio"), 2}};
    for (const auto &action : actions) {
        auto *button = new QPushButton(action.first, page);
        if (action.second == 0) button->setObjectName("settingsCreateWorkspace");
        connect(button, &QPushButton::clicked, this, [this, id = action.second] { emit workspaceActionRequested(id); });
        layout->addWidget(button, 0, Qt::AlignLeft);
    }
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makePrivacyPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Privacidade"),
                              tr("Bloqueio de rede e credenciais protegidas pelo sistema."));
    layout->addWidget(adBlock_);
    auto *thirdParty = new QCheckBox(tr("Bloquear cookies de terceiros"), page);
    auto *tracking = new QCheckBox(tr("Prevenir rastreamento"), page);
    thirdParty->setChecked(QSettings().value(QStringLiteral("privacy/blockThirdPartyCookies"), false).toBool());
    tracking->setChecked(QSettings().value(QStringLiteral("privacy/trackingPrevention"), false).toBool());
    auto updatePrivacy = [this, thirdParty, tracking] {
        QSettings settings;
        settings.setValue(QStringLiteral("privacy/blockThirdPartyCookies"), thirdParty->isChecked());
        settings.setValue(QStringLiteral("privacy/trackingPrevention"), tracking->isChecked());
        emit privacyPolicyChanged(thirdParty->isChecked(), tracking->isChecked());
    };
    connect(thirdParty, &QCheckBox::toggled, this, updatePrivacy);
    connect(tracking, &QCheckBox::toggled, this, updatePrivacy);
    layout->addWidget(thirdParty);
    layout->addWidget(tracking);
    adBlockStatus_->setObjectName(QStringLiteral("settingsStatus"));
    adBlockStatus_->setWordWrap(true);
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
        tr("O descarte protege mídia, captura, formulários alterados e downloads. Páginas protegidas podem exceder a meta. Zero desativa o descarte por inatividade."));
    layout->addWidget(suspendOnMinimize_);
    auto *minutes = new QSpinBox(page);
    minutes->setRange(0, 120);
    minutes->setSuffix(tr(" min"));
    QSettings settings;
    minutes->setValue(settings.value(QStringLiteral("preferences/discardAfterSeconds"), 180).toInt() / 60);
    connect(minutes, &QSpinBox::valueChanged, this, [](int value) {
        QSettings settings;
        settings.setValue(QStringLiteral("preferences/discardAfterSeconds"), value * 60);
    });
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->addRow(tr("Descartar após inatividade"), minutes);
    auto *maxLive = new QSpinBox(page);
    maxLive->setRange(0, 64);
    maxLive->setValue(settings.value(QStringLiteral("preferences/maxLiveTabs"), 3).toInt());
    maxLive->setSpecialValueText(tr("Sem limite"));
    connect(maxLive, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(QStringLiteral("preferences/maxLiveTabs"), value);
    });
    form->addRow(tr("Meta de páginas carregadas"), maxLive);
    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeAboutPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Sobre"),
                              tr("Zenith Browser, interface C++20 com WebKitGTK."));
    auto *version = new QLabel(tr("Versão %1").arg(QCoreApplication::applicationVersion()), page);
    version->setObjectName(QStringLiteral("settingsStatus"));
    layout->addWidget(version);
    auto *engine = new QLabel(tr("Qt %1\nWebKitGTK %2\n%3\nHost gráfico: X11 / XWayland")
        .arg(QString::fromLatin1(qVersion()), WebProfile::engineVersion(), QSysInfo::currentCpuArchitecture()), page);
    engine->setWordWrap(true);
    engine->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(engine);
    layout->addStretch();
    return page;
}
