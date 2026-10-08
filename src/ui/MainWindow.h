#pragma once

#include <QMainWindow>

class AdBlockInterceptor;
class AdBlockUpdater;
class CredentialManager;
class QCloseEvent;
class QEvent;
class QResizeEvent;
class QStackedWidget;
class QTimer;
class QUrl;
class QWebEngineProfile;
class Sidebar;
class SplitView;
class SettingsPage;
class TabManager;
class UrlBar;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createInterface();
    void connectInterface();
    void configureProfile();
    void createShortcuts();
    void restoreSession();
    void saveSession() const;

    int createTab(const QUrl &url, int workspace, bool activate,
                  bool essential = false, int folderId = -1, bool lazy = false);
    void activateTab(int id);
    void closeTab(int id);
    void cycleTabs(int direction);
    void switchWorkspace(int workspace);
    void toggleSplit(bool enabled);
    void applyViewLayout();
    void updateChrome();
    void updateSidebarTab(int id);
    void syncFolders();
    void setCompactMode(bool enabled);
    void updateShellGeometry();
    void setSearchTemplate(const QString &searchTemplate);
    void scheduleSessionSave();
    void showBrowserMenu(const QPoint &globalPosition);
    void showCredentialManager();
    void showSettings();
    void closeSettings();
    [[nodiscard]] int splitCandidate(int excludedId) const;

    QWidget *centralSurface_ = nullptr;
    QWidget *contentWidget_ = nullptr;
    QStackedWidget *contentStack_ = nullptr;
    QWidget *revealStrip_ = nullptr;
    QWidget *topRevealStrip_ = nullptr;
    QTimer *hideSidebarTimer_ = nullptr;
    QTimer *hideUrlBarTimer_ = nullptr;
    QTimer *sessionSaveTimer_ = nullptr;
    QTimer *minimizeTimer_ = nullptr;
    QWebEngineProfile *profile_ = nullptr;
    AdBlockInterceptor *adBlocker_ = nullptr;
    AdBlockUpdater *adBlockUpdater_ = nullptr;
    CredentialManager *credentialManager_ = nullptr;
    TabManager *tabManager_ = nullptr;
    Sidebar *sidebar_ = nullptr;
    UrlBar *urlBar_ = nullptr;
    SplitView *splitView_ = nullptr;
    SettingsPage *settingsPage_ = nullptr;
    int currentWorkspace_ = 0;
    int lastWorkspaceTab_[2] = {-1, -1};
    int currentTabId_ = -1;
    int secondaryTabId_ = -1;
    bool splitEnabled_ = false;
    bool compactMode_ = false;
    bool compactSidebarVisible_ = false;
    bool compactUrlBarVisible_ = false;
    bool restoringSession_ = false;
    bool settingsVisible_ = false;
    bool suspendOnMinimize_ = true;
    bool restoreSessionEnabled_ = true;
    QString searchTemplate_ = QStringLiteral("https://duckduckgo.com/?q=%1");
};
