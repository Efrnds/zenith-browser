#include "ui/ThemeManager.h"
#include "ui/Sidebar.h"
#include <QTabBar>
#include <QTimer>
#include <QSignalBlocker>

#include "ui/TabItemWidget.h"
#include "ui/ZenithIcons.h"

#include <QButtonGroup>
#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPointer>
#include <QListWidget>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
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

class TabListHost final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), themeColor(QStringLiteral("#18181b")));
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
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(expanded ? QWidget::tr("Fechar pasta %1").arg(name)
                                    : QWidget::tr("Abrir pasta %1").arg(name));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setClipRect(rect());
        painter.fillRect(rect(), themeColor(QStringLiteral("#18181b")));
        zenithIcon(ZenithIcon::Folder, themeColor(QStringLiteral("#d4d4d8")), 16)
            .paint(&painter, QRect(8, 10, 16, 16), Qt::AlignCenter);

        painter.setPen(themeColor(QStringLiteral("#d4d4d8")));
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
        painter.setBrush(themeColor(QStringLiteral("#a1a1aa")));
        painter.drawPath(chevron);
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(themeColor(QStringLiteral("#8b7cf6")), 1));
            painter.drawRect(rect().adjusted(1, 1, -2, -2));
        }
    }

private:
    QString name_;
    bool expanded_;
};
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
    , body_(new QWidget(this))
    , essentialsLabel_(new QLabel(tr("Fixadas"), body_))
    , sectionLabel_(new QLabel(tr("Abas"), body_))
    , workspaceMenuButton_(new QToolButton(body_))
    , newTabButton_(new QPushButton(tr("Nova aba"), body_))
    , workspaceTabs_(new QTabBar(body_))
    , essentialsList_(new QListWidget(body_))
    , tabScroll_(new QScrollArea(body_))
    , tabHost_(new TabListHost)
{
    setObjectName(QStringLiteral("sidebar"));
    body_->setObjectName(QStringLiteral("sidebarBody"));
    body_->setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kSidebarWidth);
    setMouseTracking(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(true);

    essentialsLabel_->setObjectName(QStringLiteral("sectionLabel"));
    sectionLabel_->setObjectName(QStringLiteral("workspaceHeading"));
    sectionLabel_->setMinimumWidth(0);
    auto *workspaceLayout = new QHBoxLayout;
    workspaceLayout->setContentsMargins(0, 6, 0, 0);
    workspaceLayout->setSpacing(4);
    workspaceTabs_->setObjectName("workspaceTabs");
    workspaceTabs_->setAccessibleName(tr("Espaços de trabalho"));
    workspaceTabs_->setExpanding(false);
    workspaceTabs_->setUsesScrollButtons(true);
    workspaceTabs_->setDrawBase(false);
    workspaceTabs_->setFocusPolicy(Qt::StrongFocus);
    workspaceLayout->addWidget(workspaceTabs_, 1);
    setWorkspaces({tr("Espaço 1"), tr("Espaço 2")});
    connect(workspaceTabs_, &QTabBar::currentChanged, this, &Sidebar::workspaceSelected);

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

    workspaceMenuButton_->setObjectName("workspaceMenuButton");
    workspaceMenuButton_->setIcon(zenithIcon(ZenithIcon::More));
    workspaceMenuButton_->setToolTip(tr("Opções do espaço atual"));
    workspaceMenuButton_->setAccessibleName(tr("Opções do espaço atual"));

    auto *tabsHeader = new QHBoxLayout;
    tabsHeader->setContentsMargins(4, 0, 0, 0);
    tabsHeader->addWidget(sectionLabel_);
    tabsHeader->addStretch();
    tabsHeader->addWidget(workspaceMenuButton_);

    tabHost_->setObjectName(QStringLiteral("tabHost"));
    tabHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    tabHost_->setAutoFillBackground(true);
    tabScroll_->setObjectName(QStringLiteral("tabScroll"));
    tabScroll_->setWidget(tabHost_);
    tabScroll_->setWidgetResizable(true);
    tabScroll_->setFrameShape(QFrame::NoFrame);
    tabScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabScroll_->viewport()->setObjectName(QStringLiteral("tabScrollViewport"));
    tabScroll_->viewport()->setAutoFillBackground(true);
    tabScroll_->viewport()->installEventFilter(this);
    tabScroll_->viewport()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabScroll_->viewport(), &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        showWorkspaceMenu(tabScroll_->viewport()->mapToGlobal(point));
    });

    auto *createSpace = new QToolButton(body_);
    createSpace->setObjectName("createWorkspaceButton");
    createSpace->setIcon(zenithIcon(ZenithIcon::Add));
    createSpace->setToolTip(tr("Novo espaço de trabalho"));
    createSpace->setAccessibleName(tr("Novo espaço de trabalho"));
    workspaceLayout->addWidget(createSpace);
    connect(createSpace, &QToolButton::clicked, this, &Sidebar::createWorkspaceRequested);

    body_->setAutoFillBackground(true);
    auto *layout = new QVBoxLayout(body_);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    layout->addWidget(essentialsLabel_);
    layout->addWidget(essentialsList_);
    layout->addLayout(tabsHeader);
    layout->addWidget(tabScroll_, 1);
    layout->addLayout(workspaceLayout);
    newTabButton_->setParent(tabHost_);
    rebuild();

    connect(newTabButton_, &QPushButton::clicked, this, &Sidebar::newTabRequested);
    connect(workspaceMenuButton_, &QToolButton::clicked, this, [this] {
        showWorkspaceMenu(workspaceMenuButton_->mapToGlobal(QPoint(0, workspaceMenuButton_->height())));
    });
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
    body_->setGeometry(0, 0, width(), height());
}

int Sidebar::slideOffset() const
{
    return slideOffset_;
}

void Sidebar::setSlideOffset(const int offset)
{
    slideOffset_ = offset;
    clearMask();
    if (!body_)
        return;
    body_->setGeometry(offset, 0, kSidebarWidth, std::max(height(), 1));
}

void Sidebar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!body_)
        return;
    body_->setGeometry(slideOffset_, 0, kSidebarWidth, height());
}

void Sidebar::addTab(const TabVisual &tab)
{
    tabs_.insert(tab.id, tab);
    scheduleRebuild();
}

void Sidebar::updateTab(const TabVisual &tab)
{
    const auto existing = tabs_.constFind(tab.id);
    const bool structuralChange = existing == tabs_.cend()
        || existing->workspace != tab.workspace
        || existing->folderId != tab.folderId
        || existing->position != tab.position
        || existing->essential != tab.essential;
    tabs_.insert(tab.id, tab);
    if (structuralChange) {
        scheduleRebuild();
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
    scheduleRebuild();
}

void Sidebar::selectTab(const int id)
{
    selectedTabId_ = id;
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
    if (workspace < 0 || workspace >= workspaceTabs_->count())
        return;
    currentWorkspace_ = workspace;
    const QSignalBlocker blocker(workspaceTabs_);
    workspaceTabs_->setCurrentIndex(workspace);
    sectionLabel_->setText(fontMetrics().elidedText(workspaceNames_.value(workspace), Qt::ElideRight, 156));
    sectionLabel_->setToolTip(workspaceNames_.value(workspace));
    rebuildTree();
}

void Sidebar::setFolders(const QList<FolderVisual> &folders)
{
    folders_.clear();
    for (const auto &folder : folders)
        folders_.insert(folder.id, folder);
    scheduleRebuild();
}

void Sidebar::setWorkspaces(const QStringList &names)
{
    const QSignalBlocker blocker(workspaceTabs_);
    workspaceNames_ = names;
    while (workspaceTabs_->count()) workspaceTabs_->removeTab(0);
    for (int index = 0; index < names.size(); ++index) {
        workspaceTabs_->addTab(QString::number(index + 1));
        workspaceTabs_->setTabToolTip(index, names.at(index));
        workspaceTabs_->setTabWhatsThis(index, names.at(index));
        workspaceTabs_->setAccessibleTabName(index, names.at(index));
    }
    currentWorkspace_ = qBound(0, currentWorkspace_, qMax(0, int(names.size()) - 1));
    workspaceTabs_->setCurrentIndex(currentWorkspace_);
    sectionLabel_->setText(fontMetrics().elidedText(names.value(currentWorkspace_), Qt::ElideRight, 156));
    sectionLabel_->setToolTip(names.value(currentWorkspace_));
    if (auto *button = findChild<QToolButton *>("createWorkspaceButton")) button->setEnabled(names.size() < 16);
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

void Sidebar::scheduleRebuild()
{
    if (rebuildPending_) return;
    rebuildPending_ = true;
    QTimer::singleShot(0, this, [this] {
        rebuildPending_ = false;
        rebuild();
        selectTab(selectedTabId_);
    });
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
        return a.position < b.position;
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
    const int folder = watched->property("folderId").toInt();
    auto *header = qobject_cast<QWidget *>(watched);
    if (folder > 0 && header) {
        if (event->type() == QEvent::MouseButtonPress) {
            header->setProperty("dragStart", static_cast<QMouseEvent *>(event)->position().toPoint());
        } else if (event->type() == QEvent::MouseMove) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if ((mouse->buttons() & Qt::LeftButton)
                && (mouse->position().toPoint() - header->property("dragStart").toPoint()).manhattanLength() >= QApplication::startDragDistance()) {
                QPointer<QDrag> drag = new QDrag(header);
                auto *mime = new QMimeData;
                mime->setData("application/x-zenith-folder", QByteArray::number(folder));
                drag->setMimeData(mime);
                drag->exec(Qt::MoveAction);
                if (drag) drag->deleteLater();
                return true;
            }
        } else if (event->type() == QEvent::DragEnter) {
            auto *drag = static_cast<QDragEnterEvent *>(event);
            const auto *source = qobject_cast<QWidget *>(drag->source());
            if (drag->mimeData()->hasFormat("application/x-zenith-folder") && source && source->window() == window()) drag->acceptProposedAction();
            return true;
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            const auto *source = qobject_cast<QWidget *>(drop->source());
            if (source && source->window() == window() && source->property("folderId").toInt() > 0) {
                const int sourceId = source->property("folderId").toInt();
                drop->acceptProposedAction();
                QTimer::singleShot(0, this, [this, sourceId, folder] { emit folderDropRequested(sourceId, folder); });
            }
            return true;
        }
    }
    if (watched->property("folderId").isValid() && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Space) {
            const int id = watched->property("folderId").toInt();
            QTimer::singleShot(0, this, [this, id] { toggleFolder(id); });
            return true;
        }
    }
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
    for (QWidget *child : tabHost_->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        if (child != newTabButton_) delete child;
    tabHost_->update();

    QList<FolderVisual> visibleFolders;
    for (const auto &folder : std::as_const(folders_)) {
        if (folder.workspace == currentWorkspace_)
            visibleFolders.append(folder);
    }
    std::ranges::sort(visibleFolders, [](const FolderVisual &a, const FolderVisual &b) {
        return a.position < b.position;
    });

    QList<TabVisual> visibleTabs;
    for (const auto &tab : std::as_const(tabs_)) {
        if (!tab.essential && tab.workspace == currentWorkspace_)
            visibleTabs.append(tab);
    }
    std::ranges::sort(visibleTabs, [](const TabVisual &a, const TabVisual &b) {
        return a.position < b.position;
    });

    QSet<int> foldersOnScreen;
    for (const auto &folder : visibleFolders) {
        const bool expanded = folderExpanded_.value(folder.id, true);
        auto *header = new FolderHeaderWidget(folder.name, expanded, tabHost_);
        header->setAcceptDrops(true);
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
    QWidget *previous = workspaceMenuButton_;
    for (QWidget *row : std::as_const(tabRows_)) {
        QWidget::setTabOrder(previous, row);
        previous = row;
    }
    QWidget::setTabOrder(previous, newTabButton_);
    QWidget::setTabOrder(newTabButton_, workspaceTabs_);
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
    newTabButton_->setGeometry(0, y + 4, width, 34);
    newTabButton_->show();
    y += 42;
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
    connect(widget, &TabItemWidget::moveRequested, this, &Sidebar::tabDropRequested, Qt::QueuedConnection);
    connect(widget, &TabItemWidget::closeRequested, this, [this](const int id) {
        QTimer::singleShot(0, this, [this, id] { emit closeTabRequested(id); });
    });
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
    connect(menu.addAction(tr("Abrir em tela dividida")), &QAction::triggered, this, [this, id] { emit splitTabRequested(id); });
    connect(menu.addAction(tr("Duplicar aba")), &QAction::triggered, this, [this, id] { emit duplicateTabRequested(id); });
    connect(menu.addAction(tr("Silenciar / reativar")), &QAction::triggered, this, [this, id] { emit muteTabRequested(id); });
    connect(menu.addAction(tr("Mover para cima")), &QAction::triggered, this, [this, id] { emit tabMoveRequested(id, -1); });
    connect(menu.addAction(tr("Mover para baixo")), &QAction::triggered, this, [this, id] { emit tabMoveRequested(id, 1); });
    connect(menu.addAction(tr("Fechar outras abas")), &QAction::triggered, this, [this, id] { emit closeOtherTabsRequested(id); });
    auto *following = menu.addAction(tr("Fechar abas seguintes"));
    following->setEnabled(!iterator->essential);
    connect(following, &QAction::triggered, this, [this, id] { emit closeFollowingTabsRequested(id); });
    QMenu *spaces = menu.addMenu(tr("Mover para espaço"));
    for (int index = 0; index < workspaceTabs_->count(); ++index)
        connect(spaces->addAction(workspaceNames_.at(index)), &QAction::triggered, this, [this, id, index] { emit tabWorkspaceRequested(id, index); });
    QAction *essential = menu.addAction(iterator->essential ? tr("Desafixar aba")
                                                             : tr("Fixar aba"));
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
    connect(menu.addAction(tr("Mover pasta para cima")), &QAction::triggered, this, [this, id] { emit folderMoveRequested(id, -1); });
    connect(menu.addAction(tr("Mover pasta para baixo")), &QAction::triggered, this, [this, id] { emit folderMoveRequested(id, 1); });
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

void Sidebar::showWorkspaceMenu(const QPoint &globalPosition)
{
    QMenu menu(this);
    connect(menu.addAction(tr("Nova aba")), &QAction::triggered, this, &Sidebar::newTabRequested);
    connect(menu.addAction(tr("Reabrir aba fechada\tCtrl+Shift+T")), &QAction::triggered, this, &Sidebar::reopenTabRequested);
    connect(menu.addAction(zenithIcon(ZenithIcon::Folder), tr("Nova pasta")), &QAction::triggered, this, [this] {
        const QString name = QInputDialog::getText(this, tr("Nova pasta"), tr("Nome da pasta:")).trimmed();
        if (!name.isEmpty()) emit createFolderRequested(name);
    });
    menu.addSeparator();
    connect(menu.addAction(tr("Renomear este espaço")), &QAction::triggered, this, [this] { emit workspaceActionRequested(1); });
    auto *create = menu.addAction(tr("Criar espaço"));
    create->setEnabled(workspaceNames_.size() < 16);
    connect(create, &QAction::triggered, this, &Sidebar::createWorkspaceRequested);
    auto *remove = menu.addAction(tr("Excluir este espaço vazio"));
    bool empty = workspaceNames_.size() > 1 && currentWorkspace_ == workspaceNames_.size() - 1;
    for (const auto &tab : std::as_const(tabs_)) if (tab.workspace == currentWorkspace_) empty = false;
    remove->setEnabled(empty);
    if (!empty) remove->setToolTip(tr("Só é possível excluir o último espaço, sem abas."));
    connect(remove, &QAction::triggered, this, [this] { emit workspaceActionRequested(2); });
    menu.exec(globalPosition);
}
