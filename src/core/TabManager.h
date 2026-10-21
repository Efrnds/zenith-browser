#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include <functional>

class QWidget;
class CredentialManager;
class WebProfile;
class WebView;

class TabManager final : public QObject
{
    Q_OBJECT

public:
    struct Tab {
        int id = -1;
        int workspace = 0;
        QUrl url;
        QString title;
        QIcon icon;
        QPointer<WebView> view;
        QPointer<QWidget> viewParent;
        QByteArray historyState;
        QDateTime lastAccess;
        bool isDiscarded = false;
        bool isEssential = false;
        bool historyApplied = false;
        bool loadStarted = false;
        bool crashed = false;
        bool muted = false;
        int folderId = -1;
        int restoreIndex = -1;
    };

    struct Folder {
        int id = -1;
        int workspace = 0;
        QString name;
    };

    using PopupFactory = std::function<WebView *(WebView *opener, const QUrl &url)>;

    explicit TabManager(WebProfile *profile, CredentialManager *credentialManager,
                        QObject *parent = nullptr);
    ~TabManager() override;

    int createTab(const QUrl &url, int workspace, QWidget *parent,
                  bool essential = false, int folderId = -1, bool lazy = false,
                  WebView *related = nullptr);
    void setPopupFactory(PopupFactory factory);
    void closeTab(int id);
    void discardTab(int id);
    void activateTab(int id);
    void setVisibleTabs(const QSet<int> &ids);
    void suspendAll();
    void suspendInactive(int idleSeconds, int maxLive = -1);
    int reopenClosedTab(QWidget *parent);
    WebView *ensureView(int id);
    void startLoad(Tab &record);
    [[nodiscard]] QByteArray serializedHistory(int id) const;
    void setTabEssential(int id, bool essential);
    void setTabFolder(int id, int folderId);
    void setWorkspaceCount(int count);
    void setTabWorkspace(int id, int workspace);
    void moveTab(int id, int delta);
    void moveTabBefore(int source, int target);

    int createFolder(int workspace, const QString &name, int requestedId = -1);
    void renameFolder(int id, const QString &name);
    void removeFolder(int id);
    void moveFolder(int id, int delta);
    void moveFolderBefore(int source, int target);

    [[nodiscard]] Tab *tab(int id);
    [[nodiscard]] const Tab *tab(int id) const;
    [[nodiscard]] QList<int> tabsInWorkspace(int workspace, bool includeEssentials = true) const;
    [[nodiscard]] QList<int> orderedTabs(int workspace) const;
    [[nodiscard]] QList<int> allTabIds() const;
    [[nodiscard]] QList<Folder> foldersInWorkspace(int workspace) const;
    [[nodiscard]] bool folderExists(int id) const;
    [[nodiscard]] bool contains(int id) const;

signals:
    void tabCreated(int id);
    void tabUpdated(int id);
    void tabClosed(int id);
    void tabCloseRequested(int id);
    void tabDiscarded(int id, bool discarded);
    void folderCreated(int id);
    void folderUpdated(int id);
    void folderRemoved(int id);
    void pageLoadProgress(int id, int progress);
    void pageLoadFinished(int id, bool ok);
    void tabAccelerator(int shortcut);
    void tabAttentionRequested(int id);
    void tabError(int id, const QString &message);
    void tabFocused(int id);

private:
    void suspendCandidates(QList<int> candidates, int idleSeconds, int maxLive);
    WebView *createView(Tab &tab, WebView *related = nullptr);
    void discardView(Tab &tab);
    void connectView(Tab &tab);

    WebProfile *profile_;
    PopupFactory popupFactory_;
    CredentialManager *credentialManager_;
    QHash<int, Tab> tabs_;
    QHash<int, Folder> folders_;
    QSet<int> visibleTabs_;
    QSet<int> pendingSuspensions_;
    QList<Tab> closedTabs_;
    QList<int> order_;
    int workspaceCount_ = 2;
    QList<int> folderOrder_;
    int nextId_ = 1;
    int nextFolderId_ = 1;
};
