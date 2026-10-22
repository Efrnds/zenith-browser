#include "ui/SettingsPage.h"
#include "ui/PageWidgets.h"
#include "ui/UiMetrics.h"
#include "core/Preferences.h"
#include <QToolButton>
#include <QTimer>
#include <QPointer>
#include <QApplication>
#include <QClipboard>
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
#include <QScrollBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QAbstractButton>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>

namespace {
QVBoxLayout *pageLayout(QWidget *page, const QString &title, const QString &description)
{
    auto *layout = new QVBoxLayout(page);
    page->setMaximumWidth(UiMetrics::ReadingWidth);
    layout->setContentsMargins(UiMetrics::Page, UiMetrics::Section, UiMetrics::Page, UiMetrics::Page);
    layout->setSpacing(UiMetrics::Section);
    auto *heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("settingsHeading"));
    heading->setWordWrap(true);
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
    , compactMode_(new QCheckBox(tr("Ocultar barras ao navegar"), this))
    , adBlock_(new QCheckBox(tr("Bloquear anúncios e rastreadores"), this))
    , suspendOnMinimize_(new QCheckBox(tr("Descartar ao minimizar"), this))
    , adBlockStatus_(new QLabel(this))
{
    setObjectName(QStringLiteral("settingsPage"));
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    pages_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    searchEngine_->setObjectName("searchEngine");
    searchEngine_->setAccessibleName(tr("Mecanismo de busca"));
    navigation_->setObjectName(QStringLiteral("settingsNavigation"));
    navigation_->addItems({tr("Geral"), tr("Aparência"), tr("Espaços"), tr("Privacidade"),
                           tr("Desempenho"), tr("Downloads"), tr("Perfis e dados"), tr("Sobre")});
    navigation_->setFixedWidth(148);
    navigation_->setSpacing(4);
    navigation_->setCurrentRow(0);

    pages_->addWidget(makeGeneralPage());
    pages_->addWidget(makeAppearancePage());
    pages_->addWidget(makeWorkspacesPage());
    pages_->addWidget(makePrivacyPage());
    pages_->addWidget(makePerformancePage());
    pages_->addWidget(makeDownloadsPage());
    pages_->addWidget(makeProfilesPage());
    pages_->addWidget(makeAboutPage());
    const QList<QPair<QWidget *, QString>> directControls{
        {searchEngine_, "preferences/searchTemplate"}, {restoreSession_, "preferences/restoreSession"},
        {compactMode_, "preferences/compactMode"}, {adBlock_, "preferences/adblock"},
        {suspendOnMinimize_, "preferences/suspendOnMinimize"}};
    for (const auto &entry : directControls) entry.first->setProperty("preferenceKey", entry.second);
    const QList<QPair<QString, QString>> namedControls{
        {"browserTheme", "appearance/theme"}, {"browserAccent", "appearance/accent"},
        {"siteTheme", "appearance/sites"}, {"textRendering", "appearance/textRendering"},
        {"thirdPartyCookies", "privacy/blockThirdPartyCookies"}, {"trackingPrevention", "privacy/trackingPrevention"},
        {"reducedMotion", "preferences/reducedMotion"}, {"discardMinutes", "preferences/discardAfterSeconds"},
        {"maxLiveTabs", "preferences/maxLiveTabs"}, {"downloadDirectory", "downloads/directory"}};
    for (const auto &entry : namedControls)
        if (auto *control = findChild<QWidget *>(entry.first)) control->setProperty("preferenceKey", entry.second);
    findChild<QSpinBox *>("discardMinutes")->setProperty("preferenceUnits", "minutes");
    for (auto *control : findChildren<QWidget *>()) {
        const QString key = control->property("preferenceKey").toString();
        if (!key.isEmpty()) control->setToolTip(Preferences::effectDescription(key));
    }

    for (int index = 0; index < pages_->count(); ++index) {
        QWidget *page = pages_->widget(index);
        pages_->removeWidget(page);
        auto *scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(page);
        pages_->insertWidget(index, scroll);
    }

    auto *back = new QPushButton(tr("Voltar"), this);
    back->setIcon(zenithIcon(ZenithIcon::Back));
    back->setObjectName(QStringLiteral("settingsBackButton"));

    search_ = new QLineEdit(this);
    search_->setObjectName("settingsSearch");
    search_->setPlaceholderText(tr("Buscar nas configurações"));
    search_->setAccessibleName(tr("Buscar nas configurações"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::returnPressed, this, [this] {
        if (searchTarget_ && searchTarget_->isVisible() && searchTarget_->isEnabled()) {
            searchTarget_->setFocus(Qt::TabFocusReason);
            return;
        }
        QWidget *page = pages_->currentWidget();
        for (QWidget *control : page->findChildren<QWidget *>()) {
            if (!control->isVisible() || !control->isEnabled() || !(control->focusPolicy() & Qt::TabFocus)) continue;
            control->setFocus(Qt::TabFocusReason);
            return;
        }
    });
    search_->setMinimumWidth(120);
    category_ = new QComboBox(this);
    category_->setObjectName("settingsCategory");
    category_->setAccessibleName(tr("Categoria de configurações"));
    for (int i = 0; i < navigation_->count(); ++i) category_->addItem(navigation_->item(i)->text());
    auto *header = new QHBoxLayout;
    header->setContentsMargins(UiMetrics::Section, UiMetrics::Small, UiMetrics::Section, 0);
    header->addWidget(back);
    auto *title = new QLabel(tr("Configurações"), this);
    title->setObjectName("sectionHeading");
    header->addWidget(title);
    header->addStretch();
    auto *tools = new QHBoxLayout;
    tools->setSpacing(10);
    tools->setContentsMargins(UiMetrics::Section, UiMetrics::Small, UiMetrics::Section, UiMetrics::Small);
    tools->addWidget(search_, 1);
    tools->addWidget(category_);
    auto *body = new QHBoxLayout;
    body->setContentsMargins(12, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(navigation_);
    body->addWidget(pages_, 1);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addLayout(tools);
    layout->addLayout(body, 1);
    auto *empty = new QWidget;
    auto *emptyLayout = pageLayout(empty, tr("Nenhuma preferência encontrada"),
        tr("Tente outro termo, como tema, cookies ou memória."));
    auto *clear = new QPushButton(tr("Limpar busca"), empty);
    emptyLayout->addWidget(clear, 0, Qt::AlignLeft);
    emptyLayout->addStretch();
    pages_->addWidget(empty);
    pages_->setCurrentIndex(0);
    refreshPreferenceControls();
    connect(clear, &QPushButton::clicked, search_, &QLineEdit::clear);
    connect(back, &QPushButton::clicked, this, &SettingsPage::closeRequested);
    connect(search_, &QLineEdit::textChanged, this, &SettingsPage::filterPreferences);
    connect(category_, &QComboBox::activated, this, [this](int index) { navigation_->setCurrentRow(index); });
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int index) {
        if (index < 0) return;
        pages_->setCurrentIndex(index);
        category_->setCurrentIndex(index);
    });
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, &SettingsPage::refreshPreferenceControls);
    const QList<QPair<QCheckBox *, QString>> persistedFlags{
        {restoreSession_, "preferences/restoreSession"}, {compactMode_, "preferences/compactMode"},
        {adBlock_, "preferences/adblock"}, {suspendOnMinimize_, "preferences/suspendOnMinimize"}};
    for (const auto &entry : persistedFlags)
        connect(entry.first, &QCheckBox::toggled, this, [key = entry.second](bool value) { QSettings().setValue(key, value); });
    connect(this, &SettingsPage::searchTemplateChanged, this, [](const QString &value) {
        QSettings().setValue("preferences/searchTemplate", value);
    });
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
    auto *searchSection = PageWidgets::section(layout, tr("Pesquisa"), tr("Escolha onde pesquisar pela barra de endereço e pela nova aba."));
    searchEngine_->setMaximumWidth(UiMetrics::FieldWidth);
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->addRow(tr("Mecanismo de busca"), searchEngine_);
    searchSection->addLayout(form);
    auto *customLayout = PageWidgets::disclosure(searchSection, tr("Busca personalizada"), "customSearch");
    auto *custom = new QLineEdit(page);
    custom->setObjectName("customSearchTemplate");
    custom->setPlaceholderText(tr("https://buscador.example/?q=%1"));
    custom->setAccessibleName(tr("Endereço de busca personalizado, com %1 para o termo"));
    auto *saveSearch = new QPushButton(tr("Salvar busca personalizada"), page);
    saveSearch->setObjectName("saveCustomSearch");
    auto *searchStatus = new QLabel(page);
    searchStatus->setWordWrap(true);
    customLayout->addWidget(custom);
    customLayout->addWidget(saveSearch, 0, Qt::AlignLeft);
    customLayout->addWidget(searchStatus);
    searchStatus->hide();
    connect(saveSearch, &QPushButton::clicked, this, [this, custom, searchStatus] {
        searchStatus->show();
        const QString value = custom->text().trimmed();
        if (!value.contains("%1") || !Navigation::isWebUrl(QUrl(value.arg("zenith")))) {
            searchStatus->setText(tr("Use um endereço HTTP ou HTTPS com %1 no lugar da pesquisa."));
            return;
        }
        setSearchTemplate(value);
        emit searchTemplateChanged(value);
        searchStatus->setText(tr("Busca personalizada salva."));
    });
    auto *startup = PageWidgets::section(layout, tr("Ao abrir o Zenith"), tr("Continue de onde parou na próxima inicialização."));
    startup->addWidget(restoreSession_);
    auto *defaultBrowser = new QPushButton(tr("Definir como navegador padrão"), page);
    connect(defaultBrowser, &QPushButton::clicked, this, &SettingsPage::defaultBrowserRequested);
    auto *integration = PageWidgets::section(layout, tr("Navegador padrão"), tr("Abra links de outros aplicativos no Zenith."));
    integration->addWidget(defaultBrowser, 0, Qt::AlignLeft);
    addReset(layout, "general");
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Aparência"),
                              tr("Temas e tamanho da interface."));
    auto &theme = ThemeManager::instance();
    auto *interface = PageWidgets::section(layout, tr("Tema e destaque"));
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *mode = new QComboBox(page);
    mode->setMaximumWidth(260);
    mode->setObjectName("browserTheme");
    mode->setAccessibleName(tr("Tema do navegador"));
    mode->addItem(tr("Sistema"), "system");
    mode->addItem(tr("Claro"), "light");
    mode->addItem(tr("Escuro"), "dark");
    mode->setCurrentIndex(mode->findData(theme.mode()));
    form->addRow(tr("Tema do navegador"), mode);
    connect(mode, &QComboBox::activated, this, [mode](int index) { ThemeManager::instance().setMode(mode->itemData(index).toString()); });
    auto *accent = new QComboBox(page);
    accent->setMaximumWidth(260);
    accent->setObjectName("browserAccent");
    accent->setAccessibleName(tr("Cor de destaque"));
    for (const auto &entry : QList<QPair<QString, QString>>{{tr("Violeta"), "violet"}, {tr("Azul"), "blue"}, {tr("Verde"), "teal"}, {tr("Âmbar"), "amber"}})
        accent->addItem(entry.first, entry.second);
    accent->setCurrentIndex(accent->findData(theme.accent()));
    form->addRow(tr("Cor de destaque"), accent);
    connect(accent, &QComboBox::activated, this, [accent](int index) { ThemeManager::instance().setAccent(accent->itemData(index).toString()); });
    interface->addLayout(form);
    auto *density = new QComboBox(page);
    density->setObjectName("densityPreset");
    density->setAccessibleName(tr("Densidade da interface"));
    density->addItems({tr("Compacta"), tr("Padrão"), tr("Confortável"), tr("Personalizada")});
    form->addRow(tr("Densidade"), density);
    connect(density, &QComboBox::activated, this, [](int index) {
        if (index < 0 || index > 2) return;
        const int sizes[][3]{{12, 212, 32}, {13, 236, 36}, {14, 260, 44}};
        QSettings settings;
        settings.setValue("appearance/fontSize", sizes[index][0]);
        settings.setValue("appearance/sidebarWidth", sizes[index][1]);
        settings.setValue("appearance/tabHeight", sizes[index][2]);
        ThemeManager::instance().apply();
    });
    auto *customColor = PageWidgets::disclosure(interface, tr("Cor personalizada"), "customColor");
    auto *customRow = new QHBoxLayout;
    auto *customAccent = new QLineEdit(page);
    customAccent->setObjectName("customAccent");
    customAccent->setPlaceholderText("#RRGGBB");
    customAccent->setAccessibleName(tr("Cor de destaque hexadecimal"));
    customAccent->setMaxLength(7);
    customAccent->setMaximumWidth(160);
    auto *applyAccent = new QPushButton(tr("Aplicar cor"), page);
    auto *colorStatus = new QLabel(page);
    colorStatus->setWordWrap(true);
    customRow->addWidget(customAccent);
    customRow->addWidget(applyAccent);
    customRow->addStretch();
    customColor->addWidget(new QLabel(tr("Use #RRGGBB. O tom se adapta ao tema para manter contraste."), page));
    customColor->addLayout(customRow);
    customColor->addWidget(colorStatus);
    colorStatus->hide();
    auto *preview = PageWidgets::disclosure(interface, tr("Prévia da interface"), "themePreview");
    preview->addWidget(new ThemePreview(page));
    connect(applyAccent, &QPushButton::clicked, this, [customAccent, colorStatus] {
        const QString color = customAccent->text().trimmed();
        if (QRegularExpression("^#[0-9a-fA-F]{6}$").match(color).hasMatch()) {
            colorStatus->clear();
            colorStatus->hide();
            ThemeManager::instance().setAccent(color.toLower());
        } else { colorStatus->setText(tr("Informe uma cor no formato #RRGGBB.")); colorStatus->show(); }
    });
    auto *dimensions = PageWidgets::section(layout, tr("Dimensões e leitura"), tr("Ajuste o espaço das abas e o tamanho dos textos da interface. O recorte externo dos cantos depende do gerenciador de janelas."));
    auto *dimensionsForm = new QFormLayout;
    dimensionsForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    addNumber(dimensionsForm, tr("Fonte da interface (px)"), "appearance/fontSize", 13, 11, 18, true);
    addNumber(dimensionsForm, tr("Largura da lateral (px)"), "appearance/sidebarWidth", 236, 180, 320, true);
    addNumber(dimensionsForm, tr("Altura das abas (px)"), "appearance/tabHeight", 36, 32, 48, true);
    addNumber(dimensionsForm, tr("Raio dos cantos (px)"), "appearance/cornerRadius", 10, 0, 20, true);
    dimensions->addLayout(dimensionsForm);
    auto *web = PageWidgets::section(layout, tr("Conteúdo das páginas"));
    auto *webForm = new QFormLayout;
    webForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *sites = new QComboBox(page);
    sites->setMaximumWidth(260);
    sites->setObjectName("siteTheme");
    sites->setAccessibleName(tr("Preferência dos sites"));
    sites->addItem(tr("Sistema"), "system");
    sites->addItem(tr("Claro"), "light");
    sites->addItem(tr("Escuro"), "dark");
    sites->setCurrentIndex(sites->findData(theme.siteMode()));
    webForm->addRow(tr("Preferência dos sites"), sites);
    connect(sites, &QComboBox::activated, this, [sites](int index) { ThemeManager::instance().setSiteMode(sites->itemData(index).toString()); });
    auto *text = new QComboBox(page);
    text->setMaximumWidth(260);
    text->setObjectName("textRendering");
    text->setAccessibleName(tr("Renderização de texto"));
    text->addItem(tr("Suave"), "smooth");
    text->addItem(tr("Mais definido"), "sharp");
    text->addItem(tr("Sistema"), "system");
    text->setCurrentIndex(text->findData(theme.textRendering()));
    webForm->addRow(tr("Texto nas páginas"), text);
    connect(text, &QComboBox::activated, this, [text](int index) { ThemeManager::instance().setTextRendering(text->itemData(index).toString()); });
    addNumber(webForm, tr("Fonte padrão dos sites (px)"), "web/defaultFontSize", 16, 12, 32, false);
    addNumber(webForm, tr("Zoom padrão (%)"), "web/defaultZoom", 100, 50, 200, false);
    web->addLayout(webForm);
    auto *explanation = new QLabel(tr("O tema dos sites é uma preferência: cada página decide como usá-la. Texto suave usa antialiasing em tons de cinza para evitar bordas coloridas."), page);
    explanation->setWordWrap(true);
    explanation->setObjectName("settingsDescription");
    web->addWidget(explanation);
    auto *behavior = PageWidgets::section(layout, tr("Espaço e movimento"));
    behavior->addWidget(compactMode_);
    auto *motion = new QCheckBox(tr("Reduzir animações da interface"), page);
    motion->setObjectName("reducedMotion");
    motion->setChecked(QSettings().value(QStringLiteral("preferences/reducedMotion"), false).toBool());
    connect(motion, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings().setValue(QStringLiteral("preferences/reducedMotion"), enabled);
        emit reducedMotionChanged(enabled);
    });
    behavior->addWidget(motion);
    addReset(layout, "appearance");
    layout->addStretch();
    return page;
}

void SettingsPage::setWorkspaces(const QStringList &names, int current)
{
    workspaceList_->clear();
    for (int index = 0; index < names.size(); ++index) {
        auto *item = new QListWidgetItem(names.at(index) + "\n" + (index == current ? tr("Espaço atual") : tr("Pressione Enter para abrir")), workspaceList_);
        item->setData(Qt::UserRole, index);
    }
    workspaceList_->setCurrentRow(current);
    workspaceList_->setFixedHeight(qBound(136, int(names.size()) * 64 + 8, 264));
    if (auto *button = findChild<QPushButton *>("settingsCreateWorkspace")) button->setEnabled(names.size() < 16);
}

QWidget *SettingsPage::makeWorkspacesPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Espaços"), tr("Separe suas abas por contexto. Crie até 16 espaços e alterne pelo seletor da barra lateral."));
    auto *listSection = PageWidgets::section(layout, tr("Seus espaços"), tr("Selecione um espaço e pressione Enter para abri-lo."));
    workspaceList_ = new QListWidget(page);
    workspaceList_->setItemDelegate(new DetailListDelegate(workspaceList_));
    workspaceList_->setObjectName("workspaceList");
    workspaceList_->setAccessibleName(tr("Espaços de trabalho"));
    workspaceList_->setMinimumHeight(120);
    listSection->addWidget(workspaceList_);
    connect(workspaceList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { emit workspaceSelected(item->data(Qt::UserRole).toInt()); });
    auto *organization = PageWidgets::section(layout, tr("Organização"), tr("Selecione o espaço para editar. Ao excluir um espaço ocupado, suas abas e pastas podem ser transferidas para outro."));
    const QList<QPair<QString, int>> actions{{tr("Criar espaço"), 0}, {tr("Renomear selecionado"), 1}, {tr("Excluir selecionado"), 2}};
    for (const auto &action : actions) {
        auto *button = new QPushButton(action.first, page);
        if (action.second == 0) button->setObjectName("settingsCreateWorkspace");
        connect(button, &QPushButton::clicked, this, [this, id = action.second] {
            const auto *selected = workspaceList_->currentItem();
            const int target = selected ? selected->data(Qt::UserRole).toInt() : -1;
            if (id == 2) emit workspaceTargetActionRequested(id, target);
            else beginWorkspaceEdit(id, id == 1 && selected ? selected->text().section('\n', 0, 0) : QString(), target);
        });
        organization->addWidget(button, 0, Qt::AlignLeft);
    }
    auto *editor = new QWidget(page);
    editor->setObjectName("workspaceEditor");
    editor->setProperty("escapeHidesEditor", true);
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    auto *name = new QLineEdit(editor);
    name->setObjectName("workspaceName");
    name->setAccessibleName(tr("Nome do espaço"));
    name->setPlaceholderText(tr("Nome do espaço"));
    name->setMaxLength(80);
    auto *save = new QPushButton(tr("Salvar espaço"), editor);
    save->setEnabled(false);
    connect(name, &QLineEdit::returnPressed, save, &QPushButton::click);
    connect(name, &QLineEdit::textChanged, save, [save](const QString &value) { save->setEnabled(!value.trimmed().isEmpty()); });
    connect(save, &QPushButton::clicked, this, [this, editor, name] {
        emit workspaceNameRequested(editor->property("action").toInt(), name->text().trimmed(), editor->property("target").toInt());
        editor->hide();
    });
    editorLayout->addWidget(name);
    editorLayout->addWidget(save, 0, Qt::AlignLeft);
    auto *cancel = new QPushButton(tr("Cancelar edição"), editor);
    connect(cancel, &QPushButton::clicked, editor, &QWidget::hide);
    editorLayout->addWidget(cancel, 0, Qt::AlignLeft);
    organization->addWidget(editor);
    editor->hide();
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makePrivacyPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Privacidade"),
                              tr("Controle os dados de navegação e a proteção contra rastreamento."));
    auto *blocking = PageWidgets::section(layout, tr("Anúncios e rastreadores"), tr("Reduza publicidade e rastreamento durante a navegação."));
    blocking->addWidget(adBlock_);
    auto *thirdParty = new QCheckBox(tr("Bloquear cookies de terceiros"), page);
    auto *tracking = new QCheckBox(tr("Prevenir rastreamento"), page);
    thirdParty->setObjectName("thirdPartyCookies");
    thirdParty->setChecked(QSettings().value(QStringLiteral("privacy/blockThirdPartyCookies"), false).toBool());
    tracking->setObjectName("trackingPrevention");
    tracking->setChecked(QSettings().value(QStringLiteral("privacy/trackingPrevention"), false).toBool());
    auto updatePrivacy = [this, thirdParty, tracking] {
        QSettings settings;
        settings.setValue(QStringLiteral("privacy/blockThirdPartyCookies"), thirdParty->isChecked());
        settings.setValue(QStringLiteral("privacy/trackingPrevention"), tracking->isChecked());
        emit privacyPolicyChanged(thirdParty->isChecked(), tracking->isChecked());
    };
    connect(thirdParty, &QCheckBox::toggled, this, updatePrivacy);
    connect(tracking, &QCheckBox::toggled, this, updatePrivacy);
    auto *data = PageWidgets::section(layout, tr("Dados de sites"), tr("Restrições adicionais podem afetar login e conteúdo incorporado."));
    data->addWidget(thirdParty);
    data->addWidget(tracking);
    adBlockStatus_->setObjectName(QStringLiteral("settingsStatus"));
    adBlockStatus_->setWordWrap(true);
    blocking->addWidget(adBlockStatus_);
    auto *update = new QPushButton(tr("Atualizar listas agora"), page);
    connect(update, &QPushButton::clicked, this, &SettingsPage::updateAdBlockRequested);
    blocking->addWidget(update, 0, Qt::AlignLeft);
    auto *credentials = new QPushButton(tr("Gerenciar senhas e credenciais"), page);
    connect(credentials, &QPushButton::clicked, this, &SettingsPage::manageCredentialsRequested);
    auto *passwords = PageWidgets::section(layout, tr("Senhas"), tr("Consulte e remova credenciais do cofre do sistema."));
    passwords->addWidget(credentials, 0, Qt::AlignLeft);
    addPreference(passwords, tr("Oferecer para salvar senhas"), "privacy/savePasswords", true);
    addPreference(passwords, tr("Preencher credenciais salvas"), "privacy/fillPasswords", true);
    auto *content = PageWidgets::section(layout, tr("Conteúdo e navegação"), tr("Alterar JavaScript pode exigir recarregar a página. Câmera, microfone, localização e notificações exigem autorização por site."));
    addPreference(content, tr("Permitir JavaScript"), "web/javascript", true);
    addPreference(content, tr("Rolagem suave nos sites"), "web/smoothScrolling", true);
    auto *cleanup = PageWidgets::section(layout, tr("Limpeza e segurança"), tr("Gerencie histórico, cache, cookies, permissões e exceções de certificados."));
    addCommand(cleanup, tr("Limpar dados de navegação"), Navigation::ClearData);
    addReset(layout, "privacy");
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makePerformancePage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(
        page, tr("Desempenho"),
        tr("Ajuste quantas páginas permanecem carregadas em segundo plano."));
    auto *tabs = PageWidgets::section(layout, tr("Abas em segundo plano"), tr("Mídia, captura, formulários alterados e downloads são protegidos. Por isso, a meta pode ser excedida."));
    tabs->parentWidget()->setProperty("keywords", "memória RAM economia suspender descarte abas");
    auto *minutes = new QSpinBox(page);
    minutes->setObjectName("discardMinutes");
    minutes->setMaximumWidth(180);
    minutes->setRange(0, 120);
    minutes->setSuffix(tr(" min"));
    minutes->setSpecialValueText(tr("Não descartar"));
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
    maxLive->setObjectName("maxLiveTabs");
    maxLive->setMaximumWidth(180);
    maxLive->setRange(0, 64);
    maxLive->setValue(settings.value(QStringLiteral("preferences/maxLiveTabs"), 3).toInt());
    maxLive->setSpecialValueText(tr("Sem limite"));
    connect(maxLive, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(QStringLiteral("preferences/maxLiveTabs"), value);
    });
    form->addRow(tr("Meta de páginas carregadas"), maxLive);
    tabs->addLayout(form);
    auto *window = PageWidgets::section(layout, tr("Ao minimizar"), tr("Aplica o descarte às páginas que podem ser suspensas com segurança."));
    window->addWidget(suspendOnMinimize_);
    addReset(layout, "performance");
    layout->addStretch();
    return page;
}

void SettingsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool compact = width() < UiMetrics::CompactSettings;
    navigation_->setVisible(!compact);
    category_->setVisible(compact);
}

void SettingsPage::filterPreferences(const QString &query)
{
    const auto normalize = [](QString text) {
        return text.normalized(QString::NormalizationForm_D)
            .remove(QRegularExpression(QStringLiteral("[\\p{Mn}]"))).toCaseFolded();
    };
    QString needle = normalize(query.trimmed());
    const QHash<QString, QString> aliases{{"dark", "escuro"}, {"light", "claro"},
        {"adblock", "anuncios"}, {"performance", "desempenho"}, {"password", "senha"},
        {"workspace", "espacos"}, {"font", "fonte"}};
    auto words = needle.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (auto &word : words) word = aliases.value(word, word);
    needle = words.join(' ');
    const auto matches = [&](const QString &text) {
        const QString normalized = normalize(text);
        for (const auto &word : words) if (!normalized.contains(word)) return false;
        return true;
    };
    const quint64 generation = ++filterGeneration_;
    searchTarget_.clear();
    if (!needle.isEmpty() && !searching_) {
        categoryBeforeSearch_ = navigation_->currentRow();
        scrollBeforeSearch_.clear();
        for (int i = 0; i < navigation_->count(); ++i)
            if (auto *scroll = qobject_cast<QScrollArea *>(pages_->widget(i)))
                scrollBeforeSearch_.insert(i, scroll->verticalScrollBar()->value());
    }
    const bool clearing = needle.isEmpty() && searching_;
    searching_ = !needle.isEmpty();
    int first = -1;
    QHash<int, QWidget *> firstMatches;
    QHash<int, QWidget *> targets;
    for (int i = 0; i < navigation_->count(); ++i) {
        bool matched = false;
        const bool categoryMatch = matches(navigation_->item(i)->text());
        for (auto *section : pages_->widget(i)->findChildren<QFrame *>("settingsSection")) {
            QString text = section->property("keywords").toString();
            for (auto *label : section->findChildren<QLabel *>()) text += ' ' + label->text();
            for (auto *button : section->findChildren<QAbstractButton *>()) text += ' ' + button->text();
            for (auto *combo : section->findChildren<QComboBox *>())
                for (int item = 0; item < combo->count(); ++item) text += ' ' + combo->itemText(item);
            const bool visible = categoryMatch || matches(text);
            section->setVisible(visible);
            matched |= visible;
            if (visible && !firstMatches.contains(i)) firstMatches.insert(i, section);
            if (visible && searching_ && !targets.contains(i)) {
                for (auto *label : section->findChildren<QLabel *>())
                    if (label->buddy() && matches(label->text())) { targets.insert(i, label->buddy()); break; }
                if (!targets.contains(i))
                    for (auto *control : section->findChildren<QWidget *>()) {
                        QString label = control->accessibleName();
                        if (auto *button = qobject_cast<QAbstractButton *>(control)) label += ' ' + button->text();
                        if (!label.isEmpty() && matches(label) && (control->focusPolicy() & Qt::TabFocus)) {
                            targets.insert(i, control); break;
                        }
                    }
            }
            for (auto *body : section->findChildren<QWidget *>("settingsDisclosure")) {
                const QString name = body->property("disclosureName").toString();
                auto *toggle = findChild<QToolButton *>(name + "Toggle");
                if (!toggle) continue;
                if (searching_ && !body->property("searchExpanded").isValid()) {
                    body->setProperty("searchExpanded", toggle->isChecked());
                }
                if (searching_ && visible) toggle->setChecked(true);
                else if (clearing && body->property("searchExpanded").isValid()) {
                    toggle->setChecked(body->property("searchExpanded").toBool());
                    body->setProperty("searchExpanded", {});
                }
            }
        }
        navigation_->item(i)->setHidden(!matched);
        auto *model = qobject_cast<QStandardItemModel *>(category_->model());
        if (model) model->item(i)->setEnabled(matched);
        if (matched && first < 0) first = i;
    }
    if (first < 0) {
        pages_->setCurrentIndex(navigation_->count());
    } else {
        const int current = clearing ? categoryBeforeSearch_ : navigation_->currentRow();
        const int selected = current >= 0 && !navigation_->item(current)->isHidden() ? current : first;
        navigation_->setCurrentRow(selected);
        pages_->setCurrentIndex(selected);
        category_->setCurrentIndex(selected);
        searchTarget_ = targets.value(selected);
        if (searching_ && firstMatches.contains(selected)) {
            const QPointer<QWidget> match(firstMatches.value(selected));
            QTimer::singleShot(0, this, [this, match, generation] {
                if (generation != filterGeneration_) return;
                if (match) if (auto *scroll = qobject_cast<QScrollArea *>(pages_->currentWidget()))
                    if (scroll->widget()->isAncestorOf(match)) scroll->ensureWidgetVisible(searchTarget_ ? searchTarget_.data() : match.data());
            });
        }
        if (clearing) QTimer::singleShot(0, this, [this, generation] {
            if (generation != filterGeneration_) return;
            for (auto it = scrollBeforeSearch_.cbegin(); it != scrollBeforeSearch_.cend(); ++it)
                if (auto *scroll = qobject_cast<QScrollArea *>(pages_->widget(it.key())))
                    scroll->verticalScrollBar()->setValue(it.value());
        });
    }
}

void SettingsPage::refreshPreferenceControls()
{
    setSearchTemplate(Preferences::value("preferences/searchTemplate").toString());
    setCompactMode(Preferences::value("preferences/compactMode").toBool());
    setAdBlockEnabled(Preferences::value("preferences/adblock").toBool());
    setSuspendOnMinimize(Preferences::value("preferences/suspendOnMinimize").toBool());
    setRestoreSession(Preferences::value("preferences/restoreSession").toBool());
    const auto &theme = ThemeManager::instance();
    const QList<QPair<QString, QString>> appearance{
        {"browserTheme", theme.mode()}, {"browserAccent", theme.accent()},
        {"siteTheme", theme.siteMode()}, {"textRendering", theme.textRendering()}};
    for (const auto &entry : appearance) {
        if (auto *combo = findChild<QComboBox *>(entry.first)) {
            const QSignalBlocker blocker(combo);
            int index = combo->findData(entry.second);
            if (index < 0 && entry.first == "browserAccent" && entry.second.startsWith('#')) {
                for (int i = combo->count() - 1; i >= 0; --i) if (combo->itemData(i).toString().startsWith('#')) combo->removeItem(i);
                combo->addItem(tr("Personalizada"), entry.second);
                index = combo->count() - 1;
            }
            combo->setCurrentIndex(index);
        }
    }
    QSettings settings;
    if (auto *density = findChild<QComboBox *>("densityPreset")) {
        const QSignalBlocker blocker(density);
        const int font = Preferences::value("appearance/fontSize").toInt();
        const int width = Preferences::value("appearance/sidebarWidth").toInt();
        const int height = Preferences::value("appearance/tabHeight").toInt();
        density->setCurrentIndex(font == 12 && width == 212 && height == 32 ? 0
            : font == 13 && width == 236 && height == 36 ? 1
            : font == 14 && width == 260 && height == 44 ? 2 : 3);
    }
    for (auto *check : findChildren<QCheckBox *>()) {
        const QString key = check->property("preferenceKey").toString();
        if (key.isEmpty()) continue;
        const QSignalBlocker blocker(check);
        check->setChecked(Preferences::value(key).toBool());
    }
    for (auto *spin : findChildren<QSpinBox *>()) {
        const QString key = spin->property("preferenceKey").toString();
        if (key.isEmpty() || spin->property("preferenceUnits").toString() == "minutes") continue;
        const QSignalBlocker blocker(spin);
        spin->setValue(Preferences::value(key).toInt());
    }
    if (auto *directory = findChild<QLineEdit *>("downloadDirectory")) directory->setText(QDir::toNativeSeparators(settings.value("downloads/directory", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString()));
    if (auto *accent = findChild<QLineEdit *>("customAccent"); accent && theme.accent().startsWith('#')) accent->setText(theme.accent());
    const QList<QPair<QString, QString>> flags{
        {"thirdPartyCookies", "privacy/blockThirdPartyCookies"},
        {"trackingPrevention", "privacy/trackingPrevention"},
        {"reducedMotion", "preferences/reducedMotion"}};
    for (const auto &entry : flags) {
        auto *check = findChild<QCheckBox *>(entry.first);
        const QSignalBlocker blocker(check);
        check->setChecked(settings.value(entry.second, false).toBool());
    }
    auto *minutes = findChild<QSpinBox *>("discardMinutes");
    auto *limit = findChild<QSpinBox *>("maxLiveTabs");
    const QSignalBlocker blockMinutes(minutes), blockLimit(limit);
    minutes->setValue(settings.value("preferences/discardAfterSeconds", 180).toInt() / 60);
    limit->setValue(settings.value("preferences/maxLiveTabs", 3).toInt());
}

QWidget *SettingsPage::makeAboutPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Sobre"),
                              tr("Zenith Browser, interface C++20 com motor web nativo da plataforma."));
    auto *version = new QLabel(tr("Versão %1").arg(QCoreApplication::applicationVersion()), page);
    version->setObjectName(QStringLiteral("settingsStatus"));
    auto *identity = PageWidgets::section(layout, tr("Zenith Browser"), tr("Abas verticais, espaços e uma interface nativa para navegar."));
    identity->addWidget(version);
#if defined(Q_OS_WIN)
    const QString engineName = QStringLiteral("Microsoft Edge WebView2");
    const QString windowHost = tr("Host gráfico: Windows");
#elif defined(Q_OS_MACOS)
    const QString engineName = QStringLiteral("Apple WKWebView");
    const QString windowHost = tr("Host gráfico: macOS");
#else
    const QString engineName = QStringLiteral("WebKitGTK");
    const QString windowHost = tr("Host gráfico: X11 / XWayland");
#endif
    auto *engine = new QLabel(tr("Qt %1\n%2 %3\n%4\n%5")
        .arg(QString::fromLatin1(qVersion()), engineName, WebProfile::engineVersion(),
             QSysInfo::currentCpuArchitecture(), windowHost), page);
    engine->setWordWrap(true);
    engine->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto *technical = PageWidgets::section(layout, tr("Informações do aplicativo"));
    technical->addWidget(engine);
    auto *copy = new QPushButton(tr("Copiar informações"), page);
    connect(copy, &QPushButton::clicked, this, [engine, version] { QApplication::clipboard()->setText(version->text() + "\n" + engine->text()); });
    technical->addWidget(copy, 0, Qt::AlignLeft);
    auto *shortcuts = PageWidgets::section(layout, tr("Atalhos de teclado"));
    auto *keys = new QLabel(tr("Ctrl+T · Nova aba\nCtrl+W · Fechar aba ou tela interna\nCtrl+Shift+T · Reabrir aba\nCtrl+L · Endereço\nCtrl+F · Buscar na página\nF3 / Shift+F3 · Próxima / anterior\nCtrl+D · Salvar favorito\nCtrl+H · Histórico\nCtrl+J · Downloads\nCtrl+, · Configurações\nCtrl+Tab · Próxima aba\nCtrl+1…8 / Ctrl+9 · Selecionar aba\nF11 · Tela cheia"), page);
    keys->setWordWrap(true);
    shortcuts->addWidget(keys);
    layout->addStretch();
    return page;
}

void SettingsPage::addCommand(QVBoxLayout *section, const QString &label, int command)
{
    auto *button = new QPushButton(label, section->parentWidget());
    section->addWidget(button, 0, Qt::AlignLeft);
    connect(button, &QPushButton::clicked, this, [this, command] { emit commandRequested(command); });
}

void SettingsPage::addPreference(QVBoxLayout *section, const QString &label, const QString &key, bool fallback)
{
    auto *check = new QCheckBox(label, section->parentWidget());
    check->setProperty("preferenceKey", key);
    check->setProperty("preferenceDefault", fallback);
    check->setChecked(QSettings().value(key, fallback).toBool());
    section->addWidget(check);
    connect(check, &QCheckBox::toggled, this, [this, key](bool enabled) {
        QSettings().setValue(key, enabled);
        emit browsingPreferencesChanged();
    });
}

void SettingsPage::addNumber(QFormLayout *form, const QString &label, const QString &key,
    int fallback, int minimum, int maximum, bool appearance)
{
    auto *spin = new QSpinBox(this);
    spin->setObjectName(key);
    spin->setAccessibleName(label);
    spin->setProperty("preferenceKey", key);
    spin->setProperty("preferenceDefault", fallback);
    spin->setRange(minimum, maximum);
    spin->setValue(QSettings().value(key, fallback).toInt());
    spin->setMaximumWidth(180);
    spin->setKeyboardTracking(false);
    if (key == "web/defaultFontSize") spin->setToolTip(tr("Recarregue as páginas para conferir o novo tamanho padrão."));
    form->addRow(label, spin);
    connect(spin, &QSpinBox::valueChanged, this, [this, key, appearance](int value) {
        QSettings().setValue(key, value);
        if (appearance) ThemeManager::instance().apply();
        else emit browsingPreferencesChanged();
    });
}

void SettingsPage::beginWorkspaceEdit(int action, const QString &name, int target)
{
    search_->clear();
    navigation_->setCurrentRow(2);
    auto *editor = findChild<QWidget *>("workspaceEditor");
    editor->setProperty("action", action);
    editor->setProperty("target", target);
    auto *input = editor->findChild<QLineEdit *>("workspaceName");
    input->setText(name);
    editor->findChild<QPushButton *>()->setText(action == 0 ? tr("Criar espaço") : tr("Salvar nome"));
    editor->show();
    input->setFocus();
    input->selectAll();
    if (auto *scroll = qobject_cast<QScrollArea *>(pages_->currentWidget())) scroll->ensureWidgetVisible(editor);
}

QWidget *SettingsPage::makeDownloadsPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Downloads"), tr("Destino e confirmação das transferências."));
    auto *downloads = PageWidgets::section(layout, tr("Downloads"), tr("Escolha onde salvar arquivos. Downloads automáticos recebem nomes únicos para preservar arquivos existentes."));
    auto *directory = new QLineEdit(page);
    directory->setObjectName("downloadDirectory");
    directory->setReadOnly(true);
    directory->setAccessibleName(tr("Pasta de downloads"));
    downloads->addWidget(directory);
    auto *choose = new QPushButton(tr("Escolher pasta"), page);
    downloads->addWidget(choose, 0, Qt::AlignLeft);
    connect(choose, &QPushButton::clicked, this, [this, directory] {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Pasta de downloads"), directory->text());
        if (path.isEmpty()) return;
        QSettings().setValue("downloads/directory", path);
        directory->setText(QDir::toNativeSeparators(path));
    });
    addPreference(downloads, tr("Perguntar onde salvar cada arquivo"), "downloads/askDestination", true);
    addCommand(downloads, tr("Abrir downloads"), Navigation::Downloads);
    addReset(layout, "downloads");
    layout->addStretch();
    return page;
}

QWidget *SettingsPage::makeProfilesPage()
{
    auto *page = new QWidget;
    auto *layout = pageLayout(page, tr("Perfis e dados"), tr("Bibliotecas, perfis e sessão. Cada perfil mantém seus dados separados."));
    auto *library = PageWidgets::section(layout, tr("Favoritos e histórico"));
    addCommand(library, tr("Abrir favoritos"), Navigation::Bookmarks);
    addCommand(library, tr("Abrir histórico"), Navigation::History);
    addCommand(library, tr("Importar favoritos de HTML"), Navigation::ImportBookmarks);
    addCommand(library, tr("Exportar favoritos para HTML"), Navigation::ExportBookmarks);
    auto *profiles = PageWidgets::section(layout, tr("Perfis e sessão"), tr("Perfis separam dados de navegação. Janelas privadas não gravam a sessão."));
    const QString profile = qApp->property("zenithProfile").toString();
    auto *identity = new QLabel(tr("Perfil atual: %1%2").arg(profile.isEmpty() ? tr("padrão") : profile,
        qApp->property("zenithPrivate").toBool() ? tr(" · janela privada") : QString()), page);
    identity->setWordWrap(true);
    profiles->addWidget(identity);
    addCommand(profiles, tr("Abrir outro perfil"), Navigation::OpenProfile);
    addCommand(profiles, tr("Usar este perfil como padrão"), Navigation::DefaultProfile);
    addCommand(profiles, tr("Salvar sessão agora"), Navigation::SaveSession);
    layout->addStretch();
    return page;
}

void SettingsPage::addReset(QVBoxLayout *layout, const QString &category)
{
    auto *section = PageWidgets::section(layout, tr("Restaurar preferências"),
        tr("Restaura somente as opções desta categoria. Favoritos, histórico, espaços e senhas são preservados."));
    auto *ask = new QPushButton(tr("Restaurar esta categoria"), section->parentWidget());
    ask->setObjectName("reset-" + category);
    section->addWidget(ask, 0, Qt::AlignLeft);
    auto *confirmation = new QWidget(section->parentWidget());
    confirmation->setProperty("escapeHidesEditor", true);
    auto *row = new QHBoxLayout(confirmation);
    row->setContentsMargins(0, 0, 0, 0);
    auto *apply = new QPushButton(tr("Confirmar restauração"), confirmation);
    apply->setObjectName("resetConfirm-" + category);
    auto *cancel = new QPushButton(tr("Cancelar"), confirmation);
    cancel->setObjectName("resetCancel-" + category);
    row->addWidget(apply); row->addWidget(cancel); row->addStretch();
    section->addWidget(confirmation);
    confirmation->hide();
    connect(ask, &QPushButton::clicked, confirmation, &QWidget::show);
    connect(cancel, &QPushButton::clicked, confirmation, &QWidget::hide);
    connect(apply, &QPushButton::clicked, this, [this, category, confirmation] {
        Preferences::resetCategory(category);
        emit preferencesReset();
        refreshPreferenceControls();
        confirmation->hide();
    });
}
