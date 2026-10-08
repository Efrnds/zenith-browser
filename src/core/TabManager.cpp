#include "core/TabManager.h"
#include "core/CredentialManager.h"

#include <QDataStream>
#include <QIODevice>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <algorithm>

TabManager::TabManager(QWebEngineProfile *profile, CredentialManager *credentialManager,
                       QObject *parent)
    : QObject(parent)
    , profile_(profile)
    , credentialManager_(credentialManager)
{
    Q_ASSERT(profile_);
}

int TabManager::createTab(const QUrl &url, const int workspace, QWidget *parent,
                          const bool essential, const int folderId, const bool lazy)
{
    const int id = nextId_++;

    Tab tab;
    tab.id = id;
    tab.workspace = workspace;
    tab.url = url;
    tab.title = tr("Nova aba");
    tab.viewParent = parent;
    tab.lastAccess = QDateTime::currentDateTimeUtc();
    tab.isEssential = essential;
    tab.folderId = folderExists(folderId) ? folderId : -1;

    tabs_.insert(id, tab);
    if (!lazy)
        createView(tabs_[id]);

    emit tabCreated(id);
    return id;
}

void TabManager::closeTab(const int id)
{
    const auto iterator = tabs_.find(id);
    if (iterator == tabs_.end()) {
        return;
    }

    visibleTabs_.remove(id);
    if (iterator->view) {
        iterator->view->hide();
        iterator->view->setParent(nullptr);
        iterator->view->deleteLater();
    }
    tabs_.erase(iterator);
    emit tabClosed(id);
}

void TabManager::activateTab(const int id)
{
    auto *record = tab(id);
    if (!record) {
        return;
    }

    record->lastAccess = QDateTime::currentDateTimeUtc();
    ensureView(id);
    emit tabUpdated(id);
}

void TabManager::setVisibleTabs(const QSet<int> &ids)
{
    visibleTabs_ = ids;
    const auto now = QDateTime::currentDateTimeUtc();
    for (const int id : ids) {
        if (auto *record = tab(id)) {
            record->lastAccess = now;
            ensureView(id);
        }
    }
    for (auto iterator = tabs_.begin(); iterator != tabs_.end(); ++iterator) {
        if (!visibleTabs_.contains(iterator.key()))
            discardView(iterator.value());
    }
}

void TabManager::suspendAll()
{
    setVisibleTabs({});
}

QWebEngineView *TabManager::ensureView(const int id)
{
    auto *record = tab(id);
    if (!record)
        return nullptr;
    return record->view ? record->view.data() : createView(*record);
}

QByteArray TabManager::serializedHistory(const int id) const
{
    const auto *record = tab(id);
    if (!record)
        return {};
    if (!record->view)
        return record->historyState;

    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << *record->view->history();
    return stream.status() == QDataStream::Ok ? state : QByteArray {};
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
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty() || workspace < 0 || workspace > 1) {
        return -1;
    }

    const int id = requestedId > 0 && !folders_.contains(requestedId)
        ? requestedId
        : nextFolderId_;
    nextFolderId_ = std::max(nextFolderId_, id + 1);
    folders_.insert(id, Folder {id, workspace, cleanName});
    emit folderCreated(id);
    return id;
}

void TabManager::renameFolder(const int id, const QString &name)
{
    const QString cleanName = name.trimmed();
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
    for (auto iterator = tabs_.cbegin(); iterator != tabs_.cend(); ++iterator) {
        if (iterator->workspace == workspace || (includeEssentials && iterator->isEssential)) {
            ids.append(iterator.key());
        }
    }
    std::ranges::sort(ids);
    return ids;
}

QList<int> TabManager::allTabIds() const
{
    QList<int> ids = tabs_.keys();
    std::ranges::sort(ids);
    return ids;
}

QList<TabManager::Folder> TabManager::foldersInWorkspace(const int workspace) const
{
    QList<Folder> result;
    for (auto iterator = folders_.cbegin(); iterator != folders_.cend(); ++iterator) {
        if (iterator->workspace == workspace) {
            result.append(iterator.value());
        }
    }
    std::ranges::sort(result, [](const Folder &left, const Folder &right) {
        return left.id < right.id;
    });
    return result;
}

bool TabManager::folderExists(const int id) const
{
    return folders_.contains(id);
}

bool TabManager::contains(const int id) const
{
    return tabs_.contains(id);
}

QWebEngineView *TabManager::createView(Tab &record)
{
    if (record.view || !record.viewParent)
        return record.view;

    auto *view = new QWebEngineView(record.viewParent);
    view->hide();
    view->setPage(new QWebEnginePage(profile_, view));
    if (credentialManager_)
        credentialManager_->attachToPage(view->page());
    record.view = view;
    connectView(record);

    bool restoredHistory = false;
    if (!record.historyState.isEmpty()) {
        QDataStream stream(record.historyState);
        stream >> *view->history();
        restoredHistory = stream.status() == QDataStream::Ok
            && view->history()->currentItem().isValid();
    }
    if (!restoredHistory && record.url.isValid() && !record.url.isEmpty())
        view->setUrl(record.url);

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

    record.url = record.view->url();
    record.title = record.view->title().trimmed().isEmpty() ? record.title : record.view->title();
    record.icon = record.view->icon().isNull() ? record.icon : record.view->icon();
    record.historyState = serializedHistory(record.id);

    QWebEngineView *view = record.view;
    record.view = nullptr;
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
    QWebEngineView *view = record.view;

    connect(view, &QWebEngineView::urlChanged, this, [this, id](const QUrl &url) {
        if (auto *record = tab(id)) {
            record->url = url;
            emit tabUpdated(id);
        }
    });
    connect(view, &QWebEngineView::titleChanged, this, [this, id](const QString &title) {
        if (auto *record = tab(id)) {
            record->title = title.trimmed().isEmpty() ? tr("Nova aba") : title;
            emit tabUpdated(id);
        }
    });
    connect(view, &QWebEngineView::iconChanged, this, [this, id](const QIcon &icon) {
        if (auto *record = tab(id)) {
            record->icon = icon;
            emit tabUpdated(id);
        }
    });
    connect(view, &QWebEngineView::loadProgress, this, [this, id](const int progress) {
        emit pageLoadProgress(id, progress);
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, id](const bool ok) {
        emit pageLoadFinished(id, ok);
    });
}
