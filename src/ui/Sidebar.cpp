#include "ui/Sidebar.h"

#include "ui/TabItemWidget.h"
#include "ui/ZenithIcons.h"

#include <QButtonGroup>
#include <QCursor>
#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
constexpr int kSidebarWidth = 236;
constexpr int kTabIdRole = Qt::UserRole + 1;
const QString kDuckDuckGoTemplate = QStringLiteral("https://duckduckgo.com/?q=%1");

class TabListHost final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), QColor(QStringLiteral("#18181b")));
    }
};

class FolderHeaderWidget final : public QWidget
{
public:
    FolderHeaderWidget(const QString &name, const bool expanded, QWidget *parent)
        : QWidget(parent)
        , name_(name)
        , expanded_(expanded)
    {
        setObjectName(QStringLiteral("folderHeader"));
        setFixedHeight(36);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(expanded ? QWidget::tr("Fechar pasta %1").arg(name)
                                    : QWidget::tr("Abrir pasta %1").arg(name));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setClipRect(rect());
        painter.fillRect(rect(), QColor(QStringLiteral("#18181b")));
        zenithIcon(ZenithIcon::Folder, QColor(QStringLiteral("#d4d4d8")), 16)
            .paint(&painter, QRect(8, 10, 16, 16), Qt::AlignCenter);

        painter.setPen(QColor(QStringLiteral("#d4d4d8")));
        painter.drawText(QRect(32, 0, width() - 56, height()),
                         Qt::AlignLeft | Qt::AlignVCenter, name_);

        QPainterPath chevron;
        const QRectF box(width() - 24, 10, 16, 16);
        if (expanded_) {
            chevron.moveTo(box.left() + 2, box.top() + 5);
            chevron.lineTo(box.center().x(), box.bottom() - 3);
            chevron.lineTo(box.right() - 2, box.top() + 5);
        } else {
            chevron.moveTo(box.left() + 5, box.top() + 2);
            chevron.lineTo(box.right() - 3, box.center().y());
            chevron.lineTo(box.left() + 5, box.bottom() - 2);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#a1a1aa")));
        painter.drawPath(chevron);
    }

private:
    QString name_;
    bool expanded_;
};
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
    , brandLabel_(new QLabel(tr("ZENITH"), this))
    , essentialsLabel_(new QLabel(tr("ESSENTIALS"), this))
    , sectionLabel_(new QLabel(tr("ABAS"), this))
    , collapseButton_(new QToolButton(this))
    , folderButton_(new QToolButton(this))
    , settingsButton_(new QToolButton(this))
    , newTabButton_(new QPushButton(tr("Nova aba"), this))
    , workspaceButtons_(new QButtonGroup(this))
    , essentialsList_(new QListWidget(this))
    , tabScroll_(new QScrollArea(this))
    , tabHost_(new TabListHost)
    , searchTemplate_(kDuckDuckGoTemplate)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(kSidebarWidth);
    setMouseTracking(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    QPalette sidebarPalette = palette();
    sidebarPalette.setColor(QPalette::Window, QColor(QStringLiteral("#18181b")));
    setPalette(sidebarPalette);
    setAutoFillBackground(true);

    brandLabel_->setObjectName(QStringLiteral("brandLabel"));
    essentialsLabel_->setObjectName(QStringLiteral("sectionLabel"));
    sectionLabel_->setObjectName(QStringLiteral("sectionLabel"));
    collapseButton_->setIcon(zenithIcon(ZenithIcon::ChevronLeft));
    collapseButton_->setToolTip(tr("Ativar modo compacto"));
    collapseButton_->setAccessibleName(tr("Ativar modo compacto"));

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(4, 0, 0, 0);
    headerLayout->addWidget(brandLabel_);
    headerLayout->addStretch();
    headerLayout->addWidget(collapseButton_);

    auto *workspaceLayout = new QHBoxLayout;
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(6);
    const QStringList workspaceEmojis {QStringLiteral("🌙"), QStringLiteral("🪐")};
    for (int workspace = 0; workspace < 2; ++workspace) {
        auto *button = new QPushButton(workspaceEmojis.at(workspace), this);
        button->setObjectName(QStringLiteral("workspaceButton"));
        button->setCheckable(true);
        button->setFixedSize(36, 36);
        button->setToolTip(tr("Espaço %1").arg(workspace + 1));
        button->setAccessibleName(tr("Espaço %1").arg(workspace + 1));
        workspaceButtons_->addButton(button, workspace);
        workspaceLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, workspace] {
            emit workspaceSelected(workspace);
        });
    }
    workspaceButtons_->button(0)->setChecked(true);

    essentialsList_->setObjectName(QStringLiteral("essentialsList"));
    essentialsList_->setViewMode(QListView::IconMode);
    essentialsList_->setFlow(QListView::LeftToRight);
    essentialsList_->setWrapping(true);
    essentialsList_->setResizeMode(QListView::Adjust);
    essentialsList_->setIconSize(QSize(22, 22));
    essentialsList_->setGridSize(QSize(48, 44));
    essentialsList_->setFixedHeight(48);
    essentialsList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    essentialsList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    essentialsList_->setContextMenuPolicy(Qt::CustomContextMenu);

    newTabButton_->setObjectName(QStringLiteral("newTabButton"));
    newTabButton_->setIcon(zenithIcon(ZenithIcon::Add));
    newTabButton_->setAccessibleName(tr("Nova aba"));

    folderButton_->setIcon(zenithIcon(ZenithIcon::Folder));
    folderButton_->setToolTip(tr("Criar pasta"));
    folderButton_->setAccessibleName(tr("Criar pasta"));

    settingsButton_->setIcon(zenithIcon(ZenithIcon::Settings));
    settingsButton_->setToolTip(tr("Preferências"));
    settingsButton_->setAccessibleName(tr("Preferências"));

    auto *tabsHeader = new QHBoxLayout;
    tabsHeader->setContentsMargins(4, 0, 0, 0);
    tabsHeader->addWidget(sectionLabel_);
    tabsHeader->addStretch();
    tabsHeader->addWidget(folderButton_);

    tabHost_->setObjectName(QStringLiteral("tabHost"));
    tabHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    tabHost_->setAutoFillBackground(true);
    QPalette hostPalette = tabHost_->palette();
    hostPalette.setColor(QPalette::Window, QColor(QStringLiteral("#18181b")));
    tabHost_->setPalette(hostPalette);
    tabScroll_->setObjectName(QStringLiteral("tabScroll"));
    tabScroll_->setWidget(tabHost_);
    tabScroll_->setWidgetResizable(true);
    tabScroll_->setFrameShape(QFrame::NoFrame);
    tabScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabScroll_->viewport()->setObjectName(QStringLiteral("tabScrollViewport"));
    tabScroll_->viewport()->setAutoFillBackground(true);
    tabScroll_->viewport()->setPalette(hostPalette);
    tabScroll_->viewport()->installEventFilter(this);

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->addLayout(workspaceLayout);
    footer->addStretch();
    footer->addWidget(settingsButton_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(8);
    layout->addLayout(headerLayout);
    layout->addWidget(essentialsLabel_);
    layout->addWidget(essentialsList_);
    layout->addWidget(newTabButton_);
    layout->addLayout(tabsHeader);
    layout->addWidget(tabScroll_, 1);
    layout->addLayout(footer);

    connect(collapseButton_, &QToolButton::clicked, this, [this] {
        emit compactModeToggled(!compactEnabled_);
    });
    connect(newTabButton_, &QPushButton::clicked, this, &Sidebar::newTabRequested);
    connect(folderButton_, &QToolButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, tr("Nova pasta"), tr("Nome da pasta:")).trimmed();
        if (!name.isEmpty())
            emit createFolderRequested(name);
    });
    connect(settingsButton_, &QToolButton::clicked, this, &Sidebar::showPreferencesMenu);
    connect(essentialsList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit tabActivated(item->data(kTabIdRole).toInt());
    });
    connect(essentialsList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit tabActivated(item->data(kTabIdRole).toInt());
    });
    connect(essentialsList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        if (auto *item = essentialsList_->itemAt(point))
            showTabMenu(item->data(kTabIdRole).toInt(), essentialsList_->viewport()->mapToGlobal(point));
    });
}

void Sidebar::addTab(const TabVisual &tab)
{
    tabs_.insert(tab.id, tab);
    rebuild();
}

void Sidebar::updateTab(const TabVisual &tab)
{
    const auto existing = tabs_.constFind(tab.id);
    const bool structuralChange = existing == tabs_.cend()
        || existing->workspace != tab.workspace
        || existing->folderId != tab.folderId
        || existing->essential != tab.essential;
    tabs_.insert(tab.id, tab);
    if (structuralChange) {
        rebuild();
        return;
    }

    if (tab.essential) {
        if (auto *item = essentialItems_.value(tab.id, nullptr)) {
            item->setIcon(tab.icon.isNull() ? style()->standardIcon(QStyle::SP_FileIcon) : tab.icon);
            item->setToolTip(tab.title);
        }
    } else if (auto *widget = widgets_.value(tab.id, nullptr)) {
        widget->setTitle(tab.title);
        widget->setIcon(tab.icon);
        widget->setDiscarded(tab.discarded);
    }
}

void Sidebar::removeTab(const int id)
{
    tabs_.remove(id);
    rebuild();
}

void Sidebar::selectTab(const int id)
{
    if (essentialItems_.contains(id))
        essentialsList_->setCurrentItem(essentialItems_.value(id));
    else
        essentialsList_->clearSelection();

    for (auto iterator = widgets_.cbegin(); iterator != widgets_.cend(); ++iterator)
        iterator.value()->setActive(iterator.key() == id);
    if (auto *widget = widgets_.value(id, nullptr))
        tabScroll_->ensureWidgetVisible(widget, 0, 20);
}

void Sidebar::setWorkspace(const int workspace)
{
    if (workspace < 0 || workspace > 1)
        return;
    currentWorkspace_ = workspace;
    if (auto *button = workspaceButtons_->button(workspace))
        button->setChecked(true);
    rebuildTree();
}

void Sidebar::setFolders(const QList<FolderVisual> &folders)
{
    folders_.clear();
    for (const auto &folder : folders)
        folders_.insert(folder.id, folder);
    rebuildTree();
}

void Sidebar::setAdBlockEnabled(const bool enabled)
{
    adBlockEnabled_ = enabled;
}

void Sidebar::setCompactEnabled(const bool enabled)
{
    compactEnabled_ = enabled;
    collapseButton_->setIcon(zenithIcon(enabled ? ZenithIcon::ChevronRight
                                                : ZenithIcon::ChevronLeft));
    collapseButton_->setToolTip(enabled ? tr("Desativar modo compacto")
                                        : tr("Ativar modo compacto"));
}

void Sidebar::setSearchTemplate(const QString &searchTemplate)
{
    searchTemplate_ = searchTemplate;
}

int Sidebar::preferredWidth() const
{
    return kSidebarWidth;
}

void Sidebar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    relayoutTabRows();
    QTimer::singleShot(0, this, [this] { relayoutTabRows(); });
}

void Sidebar::enterEvent(QEnterEvent *event)
{
    emit pointerEntered();
    QWidget::enterEvent(event);
}

void Sidebar::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (!rect().contains(mapFromGlobal(QCursor::pos())))
        emit pointerLeft();
}

void Sidebar::rebuild()
{
    rebuildEssentials();
    rebuildTree();
}

void Sidebar::rebuildEssentials()
{
    essentialItems_.clear();
    essentialsList_->clear();
    QList<TabVisual> essentials;
    for (const auto &tab : std::as_const(tabs_)) {
        if (tab.essential)
            essentials.append(tab);
    }
    std::ranges::sort(essentials, [](const TabVisual &a, const TabVisual &b) {
        return a.id < b.id;
    });
    for (const auto &tab : essentials) {
        auto *item = new QListWidgetItem(tab.icon.isNull()
                                             ? style()->standardIcon(QStyle::SP_FileIcon)
                                             : tab.icon,
                                         QString(), essentialsList_);
        item->setData(kTabIdRole, tab.id);
        item->setToolTip(tab.title);
        essentialItems_.insert(tab.id, item);
    }
    const int rows = std::max(1, static_cast<int>((essentialItems_.size() + 3) / 4));
    essentialsList_->setFixedHeight(std::min(rows, 2) * 44 + 4);
    essentialsLabel_->setVisible(!essentialItems_.isEmpty());
    essentialsList_->setVisible(!essentialItems_.isEmpty());
}

void Sidebar::toggleFolder(const int id)
{
    if (!folders_.contains(id))
        return;
    folderExpanded_.insert(id, !folderExpanded_.value(id, true));
    rebuildTree();
}

bool Sidebar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabScroll_->viewport() && event->type() == QEvent::Resize)
        relayoutTabRows();
    if (event->type() == QEvent::MouseButtonRelease) {
        const int id = watched->property("folderId").toInt();
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        if (id > 0 && mouse->button() == Qt::LeftButton) {
            toggleFolder(id);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Sidebar::rebuildTree()
{
    widgets_.clear();
    tabRows_.clear();
    qDeleteAll(tabHost_->findChildren<QWidget *>(Qt::FindDirectChildrenOnly));
    tabHost_->update();

    QList<FolderVisual> visibleFolders;
    for (const auto &folder : std::as_const(folders_)) {
        if (folder.workspace == currentWorkspace_)
            visibleFolders.append(folder);
    }
    std::ranges::sort(visibleFolders, [](const FolderVisual &a, const FolderVisual &b) {
        return a.id < b.id;
    });

    QList<TabVisual> visibleTabs;
    for (const auto &tab : std::as_const(tabs_)) {
        if (!tab.essential && tab.workspace == currentWorkspace_)
            visibleTabs.append(tab);
    }
    std::ranges::sort(visibleTabs, [](const TabVisual &a, const TabVisual &b) {
        return a.id < b.id;
    });

    QSet<int> foldersOnScreen;
    for (const auto &folder : visibleFolders) {
        const bool expanded = folderExpanded_.value(folder.id, true);
        auto *header = new FolderHeaderWidget(folder.name, expanded, tabHost_);
        header->setToolTip(expanded ? tr("Fechar pasta") : tr("Abrir pasta"));
        header->setProperty("folderId", folder.id);
        header->installEventFilter(this);
        connect(header, &QWidget::customContextMenuRequested, this,
                [this, folderId = folder.id, header](const QPoint &point) {
                    showFolderMenu(folderId, header->mapToGlobal(point));
                });
        header->setContextMenuPolicy(Qt::CustomContextMenu);
        foldersOnScreen.insert(folder.id);
        tabRows_.append(header);
        if (!expanded)
            continue;
        for (const auto &tab : visibleTabs) {
            if (tab.folderId != folder.id)
                continue;
            auto *widget = makeTabWidget(tab, tabHost_);
            widget->setLeading(14);
            widgets_.insert(tab.id, widget);
            tabRows_.append(widget);
        }
    }

    for (const auto &tab : visibleTabs) {
        if (foldersOnScreen.contains(tab.folderId))
            continue;
        auto *widget = makeTabWidget(tab, tabHost_);
        widgets_.insert(tab.id, widget);
        tabRows_.append(widget);
    }
    relayoutTabRows();
}

void Sidebar::relayoutTabRows()
{
    if (!tabScroll_ || !tabHost_ || layingOutTabs_)
        return;
    layingOutTabs_ = true;
    const int width = std::max(1, tabScroll_->viewport()->width());
    int y = 0;
    for (auto *row : tabRows_) {
        if (!row)
            continue;
        row->show();
        row->setGeometry(0, y, width, 36);
        y += 38;
    }
    const int height = std::max(y, tabScroll_->viewport()->height());
    tabHost_->setMinimumHeight(y);
    tabHost_->resize(width, height);
    tabHost_->update();
    tabScroll_->viewport()->update();
    layingOutTabs_ = false;
}

TabItemWidget *Sidebar::makeTabWidget(const TabVisual &tab, QWidget *parent)
{
    auto *widget = new TabItemWidget(tab.id, parent);
    widget->setTitle(tab.title);
    widget->setIcon(tab.icon);
    widget->setDiscarded(tab.discarded);
    connect(widget, &TabItemWidget::activated, this, &Sidebar::tabActivated);
    connect(widget, &TabItemWidget::closeRequested, this, &Sidebar::closeTabRequested);
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(widget, &QWidget::customContextMenuRequested, this, [this, id = tab.id, widget](const QPoint &point) {
        showTabMenu(id, widget->mapToGlobal(point));
    });
    return widget;
}

void Sidebar::showTabMenu(const int id, const QPoint &globalPosition)
{
    const auto iterator = tabs_.constFind(id);
    if (iterator == tabs_.cend())
        return;
    QMenu menu(this);
    QAction *essential = menu.addAction(iterator->essential ? tr("Remover dos Essentials")
                                                             : tr("Fixar nos Essentials"));
    connect(essential, &QAction::triggered, this, [this, id, value = !iterator->essential] {
        emit tabEssentialRequested(id, value);
    });
    if (!iterator->essential) {
        QMenu *moveMenu = menu.addMenu(tr("Mover para pasta"));
        QAction *noFolder = moveMenu->addAction(tr("Sem pasta"));
        connect(noFolder, &QAction::triggered, this, [this, id] { emit tabFolderRequested(id, -1); });
        for (const auto &folder : std::as_const(folders_)) {
            if (folder.workspace != currentWorkspace_)
                continue;
            QAction *action = moveMenu->addAction(folder.name);
            connect(action, &QAction::triggered, this, [this, id, folderId = folder.id] {
                emit tabFolderRequested(id, folderId);
            });
        }
    }
    menu.addSeparator();
    QAction *close = menu.addAction(tr("Fechar aba"));
    connect(close, &QAction::triggered, this, [this, id] { emit closeTabRequested(id); });
    menu.exec(globalPosition);
}

void Sidebar::showFolderMenu(const int id, const QPoint &globalPosition)
{
    const auto iterator = folders_.constFind(id);
    if (iterator == folders_.cend())
        return;
    QMenu menu(this);
    QAction *rename = menu.addAction(tr("Renomear pasta"));
    QAction *remove = menu.addAction(tr("Excluir pasta"));
    connect(rename, &QAction::triggered, this, [this, id, oldName = iterator->name] {
        const QString name = QInputDialog::getText(this, tr("Renomear pasta"),
                                                   tr("Nome da pasta:"), QLineEdit::Normal,
                                                   oldName).trimmed();
        if (!name.isEmpty())
            emit renameFolderRequested(id, name);
    });
    connect(remove, &QAction::triggered, this, [this, id] {
        if (QMessageBox::question(this, tr("Excluir pasta"),
                                  tr("As abas permanecerão abertas, mas sairão da pasta."))
            == QMessageBox::Yes) {
            emit removeFolderRequested(id);
        }
    });
    menu.exec(globalPosition);
}

void Sidebar::showPreferencesMenu()
{
    QMenu menu(this);
    QAction *adBlock = menu.addAction(zenithIcon(ZenithIcon::Shield), tr("Bloqueador de anúncios"));
    adBlock->setCheckable(true);
    adBlock->setChecked(adBlockEnabled_);
    connect(adBlock, &QAction::toggled, this, &Sidebar::adBlockToggled);

    QAction *compact = menu.addAction(tr("Modo compacto"));
    compact->setCheckable(true);
    compact->setChecked(compactEnabled_);
    connect(compact, &QAction::toggled, this, &Sidebar::compactModeToggled);

    QMenu *search = menu.addMenu(tr("Mecanismo de busca"));
    const QList<QPair<QString, QString>> engines {
        {tr("DuckDuckGo"), kDuckDuckGoTemplate},
        {tr("Google"), QStringLiteral("https://www.google.com/search?q=%1")},
        {tr("Brave Search"), QStringLiteral("https://search.brave.com/search?q=%1")},
        {tr("Bing"), QStringLiteral("https://www.bing.com/search?q=%1")},
    };
    for (const auto &[name, value] : engines) {
        QAction *action = search->addAction(name);
        action->setCheckable(true);
        action->setChecked(searchTemplate_ == value);
        connect(action, &QAction::triggered, this, [this, value] { emit searchTemplateChanged(value); });
    }
    search->addSeparator();
    QAction *custom = search->addAction(tr("URL personalizada"));
    connect(custom, &QAction::triggered, this, [this] {
        const QString value = QInputDialog::getText(
            this, tr("Mecanismo personalizado"),
            tr("URL com %1 no lugar da pesquisa:"), QLineEdit::Normal,
            searchTemplate_).trimmed();
        if (value.contains(QStringLiteral("%1")))
            emit searchTemplateChanged(value);
    });
    menu.exec(settingsButton_->mapToGlobal(QPoint(0, settingsButton_->height())));
}
