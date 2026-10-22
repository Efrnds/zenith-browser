#pragma once

#include <QMainWindow>
#include <QHash>

class AdBlockInterceptor;
class AdBlockUpdater;
class CredentialManager;
class QCloseEvent;
class QEvent;
class QPropertyAnimation;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;
class QTimer;
class QUrl;
class Sidebar;
class WebProfile;
class WebView;
class SplitView;
class SettingsPage;
class QDialog;
class FindBar;
class InternalPageStack;
class TabManager;
class UrlBar;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(int sidebarReveal READ sidebarReveal WRITE setSidebarReveal)
    Q_PROPERTY(int urlBarReveal READ urlBarReveal WRITE setUrlBarReveal)

public:
    explicit MainWindow(bool privateMode = false, QWidget *parent = nullptr);
    ~MainWindow() override;
    void openUrls(const QList<QUrl> &urls);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createInterface();
    void connectInterface();
    void configureProfile();
    void restoreSession();
    void saveSession() const;

    int createTab(const QUrl &url, int workspace, bool activate,
                  bool essential = false, int folderId = -1, bool lazy = false,
                  WebView *related = nullptr);
    void activateTab(int id);
    void closeTab(int id);
    void closeCurrentTab();
    void cycleTabs(int direction);
    void switchWorkspace(int workspace);
    void toggleSplit(bool enabled);
    void applyViewLayout();
    void updateChrome();
    void updateSidebarTab(int id);
    void syncFolders();
    void setCompactMode(bool enabled);
    void setCompactSidebarShown(bool shown);
    void scheduleCompactSidebarRecheck();
    void revealAddressBar();
    void showAddressBar(bool focus);
    void updateShellGeometry();
    void syncPageSurfaces();
    [[nodiscard]] int sidebarReveal() const;
    void setSidebarReveal(int value);
    [[nodiscard]] int urlBarReveal() const;
    void setUrlBarReveal(int value);
    void setSearchTemplate(const QString &searchTemplate);
    void navigateTo(const QUrl &url);
    void scheduleSessionSave();
    void showBrowserMenu(const QPoint &globalPosition);
    void showCredentialManager();
    void showSettings();
    void executeCommand(int command);
    WebView *currentView() const;
    int activeTabId() const;
    void setPageInputEnabled(bool enabled);
    void showFind();
    void presentInternalPage(QDialog *page);
    FindBar *findBar_ = nullptr;
    InternalPageStack *internalNavigation_ = nullptr;
    void leaveInternalPages();
    void showLibrary(bool bookmarks);
    void addBookmark();
    void showSiteControls();
    void showClearData();
    void openProfile();
    void setDefaultProfile();
    void setDefaultBrowser();
    void importBookmarks();
    void exportBookmarks();
    void manageWorkspace(int action, const QString &name = {}, int target = -1);
    void removeWorkspace(int target, int destination);
    void editFolder(int id);
    void closeFollowingTabs(int id);
    void closeSettings();
    [[nodiscard]] int splitCandidate(int excludedId) const;

    QWidget *centralSurface_ = nullptr;
    QWidget *contentWidget_ = nullptr;
    QStackedWidget *contentStack_ = nullptr;
    QWidget *revealStrip_ = nullptr;
    QWidget *topRevealStrip_ = nullptr;
    QTimer *hideSidebarTimer_ = nullptr;
    QPropertyAnimation *sidebarSlide_ = nullptr;
    QPropertyAnimation *urlBarSlide_ = nullptr;
    int sidebarReveal_ = 0;
    int urlBarReveal_ = 0;
    QTimer *hideUrlBarTimer_ = nullptr;
    QTimer *sessionSaveTimer_ = nullptr;
    QTimer *minimizeTimer_ = nullptr;
    WebProfile *profile_ = nullptr;
    AdBlockInterceptor *adBlocker_ = nullptr;
    AdBlockUpdater *adBlockUpdater_ = nullptr;
    CredentialManager *credentialManager_ = nullptr;
    TabManager *tabManager_ = nullptr;
    Sidebar *sidebar_ = nullptr;
    UrlBar *urlBar_ = nullptr;
    SplitView *splitView_ = nullptr;
    SettingsPage *settingsPage_ = nullptr;
    int currentWorkspace_ = 0;
    QHash<int, int> lastWorkspaceTab_;
    QStringList workspaceNames_ {QStringLiteral("Espaço 1"), QStringLiteral("Espaço 2")};
    int currentTabId_ = -1;
    int unloadedTabId_ = -1;
    int addressFocusToken_ = 0;
    int secondaryTabId_ = -1;
    int activePanelId_ = -1;
    bool splitEnabled_ = false;
    bool compactMode_ = false;
    bool compactSidebarVisible_ = false;
    bool sidebarSlideGuard_ = false;
    bool compactUrlBarVisible_ = false;
    bool restoringSession_ = false;
    bool sessionOpened_ = false;
    bool settingsVisible_ = false;
    bool closingTab_ = false;
    bool suspendOnMinimize_ = true;
    bool privateMode_ = false;
    bool restoreSessionEnabled_ = true;
    bool exportingBookmarks_ = false;
    QString searchTemplate_ = QStringLiteral("https://duckduckgo.com/?q=%1");
};
