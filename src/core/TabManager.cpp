#include "core/TabManager.h"

#include "core/CredentialManager.h"
#include "core/WebProfile.h"
#include "core/WebView.h"
#include "core/DownloadManager.h"
#include "core/BrowserData.h"

#include <algorithm>
#include <QTimer>

TabManager::TabManager(WebProfile *profile, CredentialManager *credentialManager, QObject *parent)
    : QObject(parent)
    , profile_(profile)
    , credentialManager_(credentialManager)
{
}

TabManager::~TabManager() = default;

void TabManager::setPopupFactory(PopupFactory factory)
{
    popupFactory_ = std::move(factory);
}

int TabManager::createTab(const QUrl &url, const int workspace, QWidget *parent,
                          const bool essential, const int folderId, const bool lazy,
                          WebView *related)
{
    if (workspace < 0 || workspace >= workspaceCount_) return -1;
    const int id = nextId_++;

    Tab tab;
    tab.id = id;
    tab.workspace = workspace;
    tab.url = url;
    tab.title = tr("Nova aba");
    tab.viewParent = parent;
    tab.lastAccess = QDateTime::currentDateTimeUtc();
    tab.isEssential = essential;
    tab.folderId = folderExists(folderId) && folders_.value(folderId).workspace == workspace ? folderId : -1;

    tabs_.insert(id, tab);
    order_.append(id);
    if (!lazy)
        createView(tabs_[id], related);

    emit tabCreated(id);
    return id;
}

void TabManager::discardTab(const int id)
{
    auto *record = tab(id);
    if (!record || !record->view)
        return;
    visibleTabs_.remove(id);
    discardView(*record);
}

void TabManager::closeTab(const int id)
{
    const auto iterator = tabs_.find(id);
    if (iterator == tabs_.end()) {
        return;
    }

    visibleTabs_.remove(id);
    Tab closed = iterator.value();
    closed.restoreIndex = order_.indexOf(id);
    closed.historyState = serializedHistory(id);
    closed.view = nullptr;
    closed.viewParent = nullptr;
    closedTabs_.append(closed);
    while (closedTabs_.size() > 20)
        closedTabs_.removeFirst();
    qsizetype historyBytes = 0;
    for (const auto &entry : closedTabs_) historyBytes += entry.historyState.size();
    for (auto &entry : closedTabs_) {
        if (historyBytes <= 8 * 1024 * 1024) break;
        historyBytes -= entry.historyState.size();
        entry.historyState.clear();
    }
    if (iterator->view) {
        disconnect(iterator->view, nullptr, this, nullptr);
        iterator->view->hide();
        iterator->view->setParent(nullptr);
        iterator->view->deleteLater();
    }
    tabs_.erase(iterator);
    order_.removeAll(id);
    emit tabClosed(id);
}

void TabManager::activateTab(const int id, bool materialize)
{
    auto *record = tab(id);
    if (!record) {
        return;
    }

    record->lastAccess = QDateTime::currentDateTimeUtc();
    if (materialize) ensureView(id);
    else if (record->url == QUrl("about:blank") && record->historyState.isEmpty()) record->isDiscarded = false;
    emit tabUpdated(id);
}

void TabManager::setVisibleTabs(const QSet<int> &ids)
{
    const auto now = QDateTime::currentDateTimeUtc();
    for (const int id : visibleTabs_) {
        if (!ids.contains(id)) {
            if (auto *record = tab(id)) record->lastAccess = now;
        }
    }
    visibleTabs_ = ids;
    for (const int id : ids) {
        if (auto *record = tab(id)) {
            record->lastAccess = now;
            ensureView(id);
        }
    }
}

void TabManager::suspendAll()
{
    visibleTabs_.clear();
    suspendInactive(0);
}

void TabManager::suspendInactive(int idleSeconds, int maxLive)
{
    if (!profile_) return;
    if (profile_->downloads()->active()) return;
    auto candidates = allTabIds();
    std::stable_sort(candidates.begin(), candidates.end(), [this](int a, int b) { return tab(a)->lastAccess < tab(b)->lastAccess; });
    suspendCandidates(std::move(candidates), idleSeconds, maxLive);
}

void TabManager::suspendCandidates(QList<int> candidates, int idleSeconds, int maxLive)
{
    if (!profile_ || profile_->downloads()->active()) return;
    if (maxLive > 0) {
        int live = 0;
        for (const int id : allTabIds()) if (tab(id)->view) ++live;
        if (live <= maxLive) return;
    }
    while (!candidates.isEmpty()) {
        const int id = candidates.takeFirst();
        auto *record = tab(id);
        if (!record || !record->view || visibleTabs_.contains(id) || pendingSuspensions_.contains(id)
            || record->lastAccess.secsTo(QDateTime::currentDateTimeUtc()) < idleSeconds) continue;
        const QPointer<WebView> view = record->view;
        const QPointer<TabManager> self(this);
        const QDateTime lastAccess = record->lastAccess;
        const QUrl url = view->url();
        pendingSuspensions_.insert(id);
        view->checkCanSuspend([self, view, id, maxLive, idleSeconds, lastAccess, url, candidates = std::move(candidates)](bool allowed) {
            if (!self) return;
            self->pendingSuspensions_.remove(id);
            QTimer::singleShot(0, self, [self, candidates, idleSeconds, maxLive] {
                if (self) self->suspendCandidates(candidates, idleSeconds, maxLive);
            });
            const auto *current = self->tab(id);
            if (!view || !current || current->lastAccess != lastAccess || view->url() != url
                || !allowed || self->visibleTabs_.contains(id)
                || self->profile_->downloads()->active() || !view->maySuspend()) return;
            if (maxLive > 0) {
                int live = 0;
                for (const int candidate : self->allTabIds())
                    if (self->tab(candidate)->view) ++live;
                if (live <= maxLive) return;
            }
            auto *record = self->tab(id);
            if (record && record->view == view) self->discardView(*record);
        });
        return;
    }
}
int TabManager::reopenClosedTab(QWidget *parent)
{
    if (closedTabs_.isEmpty()) return -1;
    Tab closed = closedTabs_.takeLast();
    const int id = createTab(closed.url, std::min(closed.workspace, workspaceCount_ - 1), parent, closed.isEssential, closed.folderId, true);
    auto *record = tab(id);
    record->title = closed.title;
    record->icon = closed.icon;
    record->historyState = closed.historyState;
    record->muted = closed.muted;
    record->isDiscarded = true;
    order_.removeAll(id);
    order_.insert(std::clamp(closed.restoreIndex, 0, int(order_.size())), id);
    for (int candidate : allTabIds()) emit tabUpdated(candidate);
    return id;
}

WebView *TabManager::ensureView(const int id)
{
    auto *record = tab(id);
    if (!record)
        return nullptr;
    if (record->view && !record->view->engineAlive()) {
        WebView *dead = record->view;
        disconnect(dead, nullptr, this, nullptr);
        record->view = nullptr;
        record->loadStarted = false;
        record->historyApplied = false;
        dead->hide();
        dead->setParent(nullptr);
        dead->deleteLater();
    }
    return record->view ? record->view.data() : createView(*record);
}

void TabManager::startLoad(Tab &record)
{
    if (!record.view || record.loadStarted)
        return;
    record.loadStarted = true;
    if (!record.historyApplied && !record.historyState.isEmpty()) {
        record.historyApplied = true;
        if (record.view->restoreSessionState(record.historyState)) return;
    }
    const QUrl live = record.view->url();
    if ((live.isEmpty() || !live.isValid()) && record.url.isValid() && !record.url.isEmpty()
        && record.url != QUrl(QStringLiteral("about:blank")))
        record.view->setUrl(record.url);
}

QByteArray TabManager::serializedHistory(const int id) const
{
    const auto *record = tab(id);
    if (!record)
        return {};
    if (!record->view)
        return record->historyState.size() <= 1536 * 1024 ? record->historyState : QByteArray{};
    const QByteArray state = record->view->sessionState();
    const QByteArray result = state.isEmpty() ? record->historyState : state;
    return result.size() <= 1536 * 1024 ? result : QByteArray{};
}

void TabManager::setTabEssential(const int id, const bool essential)
{
    if (auto *record = tab(id)) {
        record->isEssential = essential;
        if (essential) {
            record->folderId = -1;
        }
        emit tabUpdated(id);
    }
}

void TabManager::setTabFolder(const int id, const int folderId)
{
    auto *record = tab(id);
    if (!record || record->isEssential) {
        return;
    }
    if (folderId >= 0) {
        const auto iterator = folders_.constFind(folderId);
        if (iterator == folders_.cend() || iterator->workspace != record->workspace) {
            return;
        }
    }
    record->folderId = folderId;
    emit tabUpdated(id);
}

int TabManager::createFolder(const int workspace, const QString &name, const int requestedId)
{
    const QString cleanName = name.trimmed().left(128);
    if (cleanName.isEmpty() || workspace < 0 || workspace >= workspaceCount_) {
        return -1;
    }

    const int id = requestedId > 0 && !folders_.contains(requestedId)
        ? requestedId
        : nextFolderId_;
    nextFolderId_ = std::max(nextFolderId_, id + 1);
    folders_.insert(id, Folder {id, workspace, cleanName});
    folderOrder_.append(id);
    emit folderCreated(id);
    return id;
}

void TabManager::renameFolder(const int id, const QString &name)
{
    const QString cleanName = name.trimmed().left(128);
    auto iterator = folders_.find(id);
    if (iterator == folders_.end() || cleanName.isEmpty()) {
        return;
    }
    iterator->name = cleanName;
    emit folderUpdated(id);
}

void TabManager::removeFolder(const int id)
{
    if (!folders_.remove(id)) {
        return;
    }
    folderOrder_.removeAll(id);
    for (auto iterator = tabs_.begin(); iterator != tabs_.end(); ++iterator) {
        if (iterator->folderId == id) {
            iterator->folderId = -1;
            emit tabUpdated(iterator.key());
        }
    }
    emit folderRemoved(id);
}

TabManager::Tab *TabManager::tab(const int id)
{
    const auto iterator = tabs_.find(id);
    return iterator == tabs_.end() ? nullptr : &iterator.value();
}

const TabManager::Tab *TabManager::tab(const int id) const
{
    const auto iterator = tabs_.constFind(id);
    return iterator == tabs_.cend() ? nullptr : &iterator.value();
}

QList<int> TabManager::tabsInWorkspace(const int workspace, const bool includeEssentials) const
{
    QList<int> ids;
    ids.reserve(tabs_.size());
    for (const int id : order_) {
        const auto *record = tab(id);
        if (record && (record->workspace == workspace || (includeEssentials && record->isEssential))) {
            ids.append(id);
        }
    }
    return ids;
}

QList<int> TabManager::orderedTabs(int workspace) const
{
    if (workspace < 0 || workspace >= workspaceCount_) return {};
    QList<int> result;
    QList<int> ungrouped;
    QHash<int, QList<int>> grouped;
    for (int id : order_) {
        const auto *record = tab(id);
        if (!record) continue;
        if (record->isEssential) result.append(id);
        else if (record->workspace == workspace) {
            if (folders_.contains(record->folderId) && folders_.value(record->folderId).workspace == workspace)
                grouped[record->folderId].append(id);
            else ungrouped.append(id);
        }
    }
    for (const auto &folder : foldersInWorkspace(workspace)) result.append(grouped.value(folder.id));
    result.append(ungrouped);
    return result;
}

QList<int> TabManager::allTabIds() const
{
    return order_;
}

void TabManager::setWorkspaceCount(int count) { workspaceCount_ = std::clamp(count, 1, 16); }

bool TabManager::removeWorkspace(int workspace, int destination)
{
    if (workspaceCount_ <= 1 || workspace < 0 || workspace >= workspaceCount_
        || destination == workspace || destination >= workspaceCount_ || destination < -1) return false;
    const auto pristine = [](const Tab &tab) {
        return tab.url == QUrl("about:blank") && tab.historyState.isEmpty() && !tab.view;
    };
    if (destination < 0) {
        for (const auto &record : std::as_const(tabs_))
            if (record.workspace == workspace && !record.isEssential && !pristine(record)) return false;
        destination = workspace == 0 ? 1 : workspace - 1;
    }
    const auto remap = [workspace, destination](int value) {
        const int moved = value == workspace ? destination : value;
        return moved - (moved > workspace ? 1 : 0);
    };
    for (auto &folder : folders_) folder.workspace = remap(folder.workspace);
    for (auto &record : tabs_) record.workspace = remap(record.workspace);
    for (auto &closed : closedTabs_) closed.workspace = remap(closed.workspace);
    --workspaceCount_;
    const auto ids = allTabIds();
    for (int id : ids) emit tabUpdated(id);
    for (int id : std::as_const(folderOrder_)) emit folderUpdated(id);
    return true;
}

void TabManager::setTabWorkspace(int id, int workspace)
{
    if (workspace < 0 || workspace >= workspaceCount_) return;
    if (auto *record = tab(id)) {
        record->workspace = workspace;
        record->folderId = -1;
        emit tabUpdated(id);
    }
}
void TabManager::moveTab(int id, int delta)
{
    const auto *record = tab(id);
    if (!record || !delta) return;
    const auto siblings = tabsInWorkspace(record->workspace, false);
    const int from = siblings.indexOf(id);
    const int to = from + (delta < 0 ? -1 : 1);
    if (from < 0 || to < 0 || to >= siblings.size()) return;
    const int other = siblings.at(to);
    order_.swapItemsAt(order_.indexOf(id), order_.indexOf(other));
    emit tabUpdated(id);
    emit tabUpdated(other);
}

void TabManager::moveTabBefore(int source, int target)
{
    auto *from = tab(source);
    const auto *to = tab(target);
    if (!from || !to || source == target || from->workspace != to->workspace
        || from->isEssential || to->isEssential) return;
    from->folderId = to->folderId;
    order_.removeAll(source);
    order_.insert(order_.indexOf(target), source);
    for (const int id : allTabIds()) emit tabUpdated(id);
}

QList<TabManager::Folder> TabManager::foldersInWorkspace(const int workspace) const
{
    QList<Folder> result;
    for (const int id : folderOrder_) {
        const auto folder = folders_.value(id);
        if (folder.workspace == workspace) {
            result.append(folder);
        }
    }
    return result;
}

void TabManager::moveFolder(int id, int delta)
{
    if (!folders_.contains(id) || !delta) return;
    const auto siblings = foldersInWorkspace(folders_.value(id).workspace);
    int from = -1;
    for (int index = 0; index < siblings.size(); ++index)
        if (siblings.at(index).id == id) from = index;
    const int to = from + (delta < 0 ? -1 : 1);
    if (from < 0 || to < 0 || to >= siblings.size()) return;
    const int other = siblings.at(to).id;
    folderOrder_.swapItemsAt(folderOrder_.indexOf(id), folderOrder_.indexOf(other));
    emit folderUpdated(id);
}

void TabManager::moveFolderBefore(int source, int target)
{
    if (!folders_.contains(source) || !folders_.contains(target) || source == target
        || folders_.value(source).workspace != folders_.value(target).workspace) return;
    folderOrder_.removeAll(source);
    folderOrder_.insert(folderOrder_.indexOf(target), source);
    emit folderUpdated(source);
}

bool TabManager::folderExists(const int id) const
{
    return folders_.contains(id);
}

bool TabManager::contains(const int id) const
{
    return tabs_.contains(id);
}

WebView *TabManager::createView(Tab &record, WebView *related)
{
    if (record.view || !record.viewParent || !profile_)
        return record.view;

    auto *view = new WebView(profile_, record.viewParent, related);
    record.crashed = false;
    view->setMuted(record.muted);
    view->hide();
    if (popupFactory_)
        view->setPopupFactory(popupFactory_);
    if (credentialManager_)
        credentialManager_->attachToView(view);
    record.view = view;
    connectView(record);

    const bool wasDiscarded = record.isDiscarded;
    record.isDiscarded = false;
    if (wasDiscarded)
        emit tabDiscarded(record.id, false);
    return view;
}

void TabManager::discardView(Tab &record)
{
    if (!record.view)
        return;

    const QUrl liveUrl = record.view->url();
    if (liveUrl.isValid() && !liveUrl.isEmpty())
        record.url = liveUrl;
    const QString liveTitle = record.view->title().trimmed();
    record.title = liveTitle.isEmpty() ? record.title : liveTitle;
    record.icon = record.view->icon().isNull() ? record.icon : record.view->icon();
    record.muted = record.view->isMuted();
    record.historyState = serializedHistory(record.id);

    WebView *view = record.view;
    disconnect(view, nullptr, this, nullptr);
    record.view = nullptr;
    record.loadStarted = false;
    record.historyApplied = false;
    record.isDiscarded = true;
    view->hide();
    view->setParent(nullptr);
    view->deleteLater();
    emit tabDiscarded(record.id, true);
    emit tabUpdated(record.id);
}

void TabManager::connectView(Tab &record)
{
    const int id = record.id;
    WebView *view = record.view;
    connect(view, &WebView::pageFocused, this, [this, id] { emit tabFocused(id); });
    connect(view, &WebView::newTabRequested, this, [this, id](const QUrl &url, bool foreground) {
        emit newTabRequested(id, url, foreground);
    });

    connect(view, &WebView::urlChanged, this, [this, id](const QUrl &url) {
        if (!url.isValid() || url.isEmpty())
            return;
        if (auto *record = tab(id)) {
            record->url = url;
            if (record->view) record->view->setZoom(profile_->data()->zoom(url));
            emit tabUpdated(id);
        }
    });
    connect(view, &WebView::titleChanged, this, [this, id](const QString &title) {
        const QString clean = title.trimmed();
        if (clean.isEmpty())
            return;
        if (auto *record = tab(id)) {
            record->title = clean;
            emit tabUpdated(id);
        }
    });
    connect(view, &WebView::iconChanged, this, [this, id](const QIcon &icon) {
        if (auto *record = tab(id)) {
            record->icon = icon;
            emit tabUpdated(id);
        }
    });
    connect(view, &WebView::loadProgress, this, [this, id](const int progress) {
        emit pageLoadProgress(id, progress);
    });
    connect(view, &WebView::loadFinished, this, [this, id](const bool ok) {
        if (ok)
            if (auto *record = tab(id)) record->historyState = serializedHistory(id);
        emit pageLoadFinished(id, ok);
    });
    connect(view, &WebView::audioChanged, this, [this, id] {
        if (auto *record = tab(id); record && record->view)
            record->muted = record->view->isMuted();
        emit tabUpdated(id);
    });
    connect(view, &WebView::processTerminated, this, [this, id] {
        if (auto *record = tab(id)) record->crashed = true;
        emit tabUpdated(id);
        emit tabError(id, tr("O processo desta página encerrou. Use Recarregar para recuperar a aba."));
    });
    connect(view, &WebView::loadError, this, [this, id](const QString &message) {
        emit tabError(id, message);
    });
    connect(view, &WebView::closeRequested, this, [this, id] {
        emit tabCloseRequested(id);
    });
    connect(view, &WebView::acceleratorPressed, this, [this](const int shortcut) {
        emit tabAccelerator(shortcut);
    });
    connect(view, &WebView::notificationActivated, this, [this, id] {
        emit tabAttentionRequested(id);
    });
}
