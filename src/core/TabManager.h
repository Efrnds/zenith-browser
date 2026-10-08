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

class QWebEngineProfile;
class QWebEngineView;
class QWidget;
class CredentialManager;

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
        QPointer<QWebEngineView> view;
        QPointer<QWidget> viewParent;
        QByteArray historyState;
        QDateTime lastAccess;
        bool isDiscarded = false;
        bool isEssential = false;
        int folderId = -1;
    };

    struct Folder {
        int id = -1;
        int workspace = 0;
        QString name;
    };

    explicit TabManager(QWebEngineProfile *profile, CredentialManager *credentialManager,
                        QObject *parent = nullptr);

    int createTab(const QUrl &url, int workspace, QWidget *parent,
                  bool essential = false, int folderId = -1, bool lazy = false);
    void closeTab(int id);
    void activateTab(int id);
    void setVisibleTabs(const QSet<int> &ids);
    void suspendAll();
    QWebEngineView *ensureView(int id);
    [[nodiscard]] QByteArray serializedHistory(int id) const;
    void setTabEssential(int id, bool essential);
    void setTabFolder(int id, int folderId);

    int createFolder(int workspace, const QString &name, int requestedId = -1);
    void renameFolder(int id, const QString &name);
    void removeFolder(int id);

    [[nodiscard]] Tab *tab(int id);
    [[nodiscard]] const Tab *tab(int id) const;
    [[nodiscard]] QList<int> tabsInWorkspace(int workspace, bool includeEssentials = true) const;
    [[nodiscard]] QList<int> allTabIds() const;
    [[nodiscard]] QList<Folder> foldersInWorkspace(int workspace) const;
    [[nodiscard]] bool folderExists(int id) const;
    [[nodiscard]] bool contains(int id) const;

signals:
    void tabCreated(int id);
    void tabUpdated(int id);
    void tabClosed(int id);
    void tabDiscarded(int id, bool discarded);
    void folderCreated(int id);
    void folderUpdated(int id);
    void folderRemoved(int id);
    void pageLoadProgress(int id, int progress);
    void pageLoadFinished(int id, bool ok);

private:
    QWebEngineView *createView(Tab &tab);
    void discardView(Tab &tab);
    void connectView(Tab &tab);

    QWebEngineProfile *profile_;
    CredentialManager *credentialManager_;
    QHash<int, Tab> tabs_;
    QHash<int, Folder> folders_;
    QSet<int> visibleTabs_;
    int nextId_ = 1;
    int nextFolderId_ = 1;
};
