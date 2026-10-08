#include "ui/MainWindow.h"

#include "core/AdBlockInterceptor.h"
#include "core/AdBlockUpdater.h"
#include "core/CredentialManager.h"
#include "core/TabManager.h"
#include "ui/Sidebar.h"
#include "ui/SettingsPage.h"
#include "ui/SplitView.h"
#include "ui/UrlBar.h"
#include "ui/ZenithIcons.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QTimer>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <algorithm>

namespace {
const QUrl kNewTabUrl(QStringLiteral("about:blank"));

QByteArray iconBytes(const QIcon &icon)
{
    if (icon.isNull())
        return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (buffer.open(QIODevice::WriteOnly))
        icon.pixmap(32, 32).save(&buffer, "PNG");
    return bytes;
}

QIcon iconFromBytes(const QByteArray &bytes)
{
    QPixmap pixmap;
    return pixmap.loadFromData(bytes, "PNG") ? QIcon(pixmap) : QIcon {};
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Zenith Browser"));
    setMinimumSize(760, 520);
    resize(1280, 800);

    createInterface();
    configureProfile();
    adBlockUpdater_ = new AdBlockUpdater(adBlocker_, this);
    credentialManager_ = new CredentialManager(this);
    tabManager_ = new TabManager(profile_, credentialManager_, this);
    hideSidebarTimer_ = new QTimer(this);
    hideSidebarTimer_->setSingleShot(true);
    hideSidebarTimer_->setInterval(260);
    hideUrlBarTimer_ = new QTimer(this);
    hideUrlBarTimer_->setSingleShot(true);
    hideUrlBarTimer_->setInterval(260);
    sessionSaveTimer_ = new QTimer(this);
    sessionSaveTimer_->setSingleShot(true);
    sessionSaveTimer_->setInterval(700);
    minimizeTimer_ = new QTimer(this);
    minimizeTimer_->setSingleShot(true);
    minimizeTimer_->setInterval(15'000);
    connect(minimizeTimer_, &QTimer::timeout, this, [this] {
        if (suspendOnMinimize_ && isMinimized() && tabManager_)
            tabManager_->suspendAll();
    });
    connectInterface();
    createShortcuts();
    restoreSession();
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    if (qApp)
        qApp->removeEventFilter(this);
    if (profile_) {
        profile_->setUrlRequestInterceptor(nullptr);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange || !tabManager_)
        return;
    if (isMinimized() && suspendOnMinimize_) {
        minimizeTimer_->start();
    } else {
        minimizeTimer_->stop();
        applyViewLayout();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateShellGeometry();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
        && tabManager_ && !QApplication::activeModalWidget()) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        const auto mods = key->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
        const bool nextTab = key->key() == Qt::Key_Tab && mods == Qt::ControlModifier;
        const bool previousTab = (key->key() == Qt::Key_Tab && mods == (Qt::ControlModifier | Qt::ShiftModifier))
            || (key->key() == Qt::Key_Backtab && mods == Qt::ControlModifier);
        if (nextTab || previousTab) {
            if (event->type() == QEvent::KeyPress)
                cycleTabs(previousTab ? -1 : 1);
            event->accept();
            return true;
        }
    }
    if (watched == revealStrip_ && event->type() == QEvent::Enter && compactMode_) {
        hideSidebarTimer_->stop();
        compactSidebarVisible_ = true;
        updateShellGeometry();
    }
    if (compactMode_ && event->type() == QEvent::Enter
        && (watched == topRevealStrip_ || watched == urlBar_)) {
        hideUrlBarTimer_->stop();
        compactUrlBarVisible_ = true;
        updateShellGeometry();
    }
    if (compactMode_ && watched == urlBar_ && event->type() == QEvent::Leave) {
        const QPoint onBar = urlBar_->mapFromGlobal(QCursor::pos());
        const QPoint onStrip = topRevealStrip_->mapFromGlobal(QCursor::pos());
        if (!urlBar_->rect().contains(onBar) && !topRevealStrip_->rect().contains(onStrip))
            hideUrlBarTimer_->start();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::createInterface()
{
    centralSurface_ = new QWidget(this);
    centralSurface_->setObjectName(QStringLiteral("centralSurface"));
    setCentralWidget(centralSurface_);

    contentWidget_ = new QWidget(centralSurface_);
    contentWidget_->setObjectName(QStringLiteral("contentSurface"));
    contentWidget_->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget_->setAutoFillBackground(true);
    QPalette contentPalette = contentWidget_->palette();
    contentPalette.setColor(QPalette::Window, QColor(QStringLiteral("#121214")));
    contentWidget_->setPalette(contentPalette);
    sidebar_ = new Sidebar(centralSurface_);
    urlBar_ = new UrlBar(contentWidget_);
    splitView_ = new SplitView(contentWidget_);
    settingsPage_ = new SettingsPage(contentWidget_);
    contentStack_ = new QStackedWidget(contentWidget_);
    contentStack_->addWidget(splitView_);
    contentStack_->addWidget(settingsPage_);
    revealStrip_ = new QWidget(centralSurface_);
    revealStrip_->setObjectName(QStringLiteral("revealStrip"));
    revealStrip_->setMouseTracking(true);
    revealStrip_->installEventFilter(this);
    topRevealStrip_ = new QWidget(centralSurface_);
    topRevealStrip_->setObjectName(QStringLiteral("revealStrip"));
    topRevealStrip_->setMouseTracking(true);
    topRevealStrip_->installEventFilter(this);
    urlBar_->installEventFilter(this);

    auto *contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(urlBar_);
    contentLayout->addWidget(contentStack_, 1);
    revealStrip_->hide();
    topRevealStrip_->hide();
    sidebar_->raise();
}

void MainWindow::connectInterface()
{
    connect(settingsPage_, &SettingsPage::closeRequested, this, &MainWindow::closeSettings);
    connect(settingsPage_, &SettingsPage::searchTemplateChanged,
            this, &MainWindow::setSearchTemplate);
    connect(settingsPage_, &SettingsPage::compactModeChanged,
            this, &MainWindow::setCompactMode);
    connect(settingsPage_, &SettingsPage::adBlockChanged, this, [this](const bool enabled) {
        adBlocker_->setEnabled(enabled);
        sidebar_->setAdBlockEnabled(enabled);
        scheduleSessionSave();
    });
    connect(settingsPage_, &SettingsPage::updateAdBlockRequested,
            adBlockUpdater_, &AdBlockUpdater::updateNow);
    connect(settingsPage_, &SettingsPage::manageCredentialsRequested,
            this, &MainWindow::showCredentialManager);
    connect(settingsPage_, &SettingsPage::suspendOnMinimizeChanged,
            this, [this](const bool enabled) {
                suspendOnMinimize_ = enabled;
                scheduleSessionSave();
            });
    connect(settingsPage_, &SettingsPage::restoreSessionChanged,
            this, [this](const bool enabled) {
                restoreSessionEnabled_ = enabled;
                scheduleSessionSave();
            });
    connect(adBlockUpdater_, &AdBlockUpdater::statusChanged, this, [this] {
        settingsPage_->setAdBlockStatus(adBlocker_->blockedCount(),
                                        adBlocker_->ruleCount(),
                                        adBlockUpdater_->statusText());
    });

    connect(credentialManager_, &CredentialManager::saveRequested, this,
            [this](const QString &origin, const QString &username, const QString &password) {
                const auto answer = QMessageBox::question(
                    this, tr("Salvar credencial"),
                    tr("Salvar o login “%1” para %2 no cofre seguro do sistema?")
                        .arg(username, origin),
                    QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Save);
                if (answer != QMessageBox::Save)
                    return;
                QString error;
                if (!credentialManager_->save({origin, username, password}, &error))
                    QMessageBox::warning(this, tr("Credencial não salva"), error);
            });

    connect(sidebar_, &Sidebar::newTabRequested, this, [this] {
        createTab(kNewTabUrl, currentWorkspace_, true);
    });
    connect(sidebar_, &Sidebar::tabActivated, this, &MainWindow::activateTab);
    connect(sidebar_, &Sidebar::closeTabRequested, this, &MainWindow::closeTab);
    connect(sidebar_, &Sidebar::workspaceSelected, this, &MainWindow::switchWorkspace);
    connect(sidebar_, &Sidebar::tabEssentialRequested, tabManager_, &TabManager::setTabEssential);
    connect(sidebar_, &Sidebar::tabFolderRequested, tabManager_, &TabManager::setTabFolder);
    connect(sidebar_, &Sidebar::createFolderRequested, this, [this](const QString &name) {
        tabManager_->createFolder(currentWorkspace_, name);
    });
    connect(sidebar_, &Sidebar::renameFolderRequested, tabManager_, &TabManager::renameFolder);
    connect(sidebar_, &Sidebar::removeFolderRequested, tabManager_, &TabManager::removeFolder);
    connect(sidebar_, &Sidebar::adBlockToggled, this, [this](const bool enabled) {
        adBlocker_->setEnabled(enabled);
        sidebar_->setAdBlockEnabled(enabled);
        scheduleSessionSave();
    });
    connect(sidebar_, &Sidebar::compactModeToggled, this, &MainWindow::setCompactMode);
    connect(sidebar_, &Sidebar::searchTemplateChanged, this, &MainWindow::setSearchTemplate);
    connect(sidebar_, &Sidebar::pointerEntered, this, [this] {
        if (!compactMode_)
            return;
        hideSidebarTimer_->stop();
        compactSidebarVisible_ = true;
        updateShellGeometry();
    });
    connect(sidebar_, &Sidebar::pointerLeft, this, [this] {
        if (compactMode_)
            hideSidebarTimer_->start();
    });
    connect(hideSidebarTimer_, &QTimer::timeout, this, [this] {
        compactSidebarVisible_ = false;
        updateShellGeometry();
    });
    connect(hideUrlBarTimer_, &QTimer::timeout, this, [this] {
        compactUrlBarVisible_ = false;
        updateShellGeometry();
    });
    connect(sessionSaveTimer_, &QTimer::timeout, this, [this] { saveSession(); });

    connect(urlBar_, &UrlBar::navigateRequested, this, [this](const QUrl &url) {
        if (auto *record = tabManager_->tab(currentTabId_); record && record->view) {
            record->view->setUrl(url);
        }
    });
    connect(urlBar_, &UrlBar::backRequested, this, [this] {
        if (auto *record = tabManager_->tab(currentTabId_); record && record->view) {
            record->view->back();
        }
    });
    connect(urlBar_, &UrlBar::forwardRequested, this, [this] {
        if (auto *record = tabManager_->tab(currentTabId_); record && record->view) {
            record->view->forward();
        }
    });
    connect(urlBar_, &UrlBar::reloadRequested, this, [this] {
        if (auto *record = tabManager_->tab(currentTabId_); record && record->view) {
            record->view->reload();
        }
    });
    connect(urlBar_, &UrlBar::stopRequested, this, [this] {
        if (auto *record = tabManager_->tab(currentTabId_); record && record->view) {
            record->view->stop();
        }
    });
    connect(urlBar_, &UrlBar::splitToggled, this, &MainWindow::toggleSplit);
    connect(urlBar_, &UrlBar::menuRequested, this, &MainWindow::showBrowserMenu);

    connect(tabManager_, &TabManager::tabCreated, this, [this](const int id) {
        updateSidebarTab(id);
        scheduleSessionSave();
    });
    connect(tabManager_, &TabManager::tabUpdated, this, [this](const int id) {
        updateSidebarTab(id);
        if (id == currentTabId_) {
            updateChrome();
        }
        scheduleSessionSave();
    });
    connect(tabManager_, &TabManager::pageLoadProgress, this,
            [this](const int id, const int progress) {
                if (id == currentTabId_) {
                    urlBar_->setLoading(progress < 100);
                }
            });
    connect(tabManager_, &TabManager::pageLoadFinished, this,
            [this](const int id, const bool) {
                if (id == currentTabId_) {
                    urlBar_->setLoading(false);
                    updateChrome();
                }
            });
    connect(tabManager_, &TabManager::folderCreated, this, [this](const int) {
        syncFolders();
        scheduleSessionSave();
    });
    connect(tabManager_, &TabManager::folderUpdated, this, [this](const int) {
        syncFolders();
        scheduleSessionSave();
    });
    connect(tabManager_, &TabManager::folderRemoved, this, [this](const int) {
        syncFolders();
        scheduleSessionSave();
    });
}

void MainWindow::configureProfile()
{
    profile_ = new QWebEngineProfile(QStringLiteral("Zenith"), this);
    profile_->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile_->setHttpCacheMaximumSize(64 * 1024 * 1024);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    profile_->setPersistentStoragePath(dataRoot + QStringLiteral("/webengine"));
    profile_->setCachePath(cacheRoot + QStringLiteral("/webengine"));

    auto *settings = profile_->settings();
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);

    adBlocker_ = new AdBlockInterceptor(this);
    profile_->setUrlRequestInterceptor(adBlocker_);
}

void MainWindow::createShortcuts()
{
    auto *focusUrl = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
    connect(focusUrl, &QShortcut::activated, urlBar_, &UrlBar::focusAddress);

    auto *newTab = new QShortcut(QKeySequence::AddTab, this);
    connect(newTab, &QShortcut::activated, this, [this] {
        createTab(kNewTabUrl, currentWorkspace_, true);
    });

    auto *closeCurrent = new QShortcut(QKeySequence::Close, this);
    connect(closeCurrent, &QShortcut::activated, this, [this] {
        closeTab(currentTabId_);
    });

    auto *toggleSplitShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+\\")), this);
    connect(toggleSplitShortcut, &QShortcut::activated, this, [this] {
        toggleSplit(!splitEnabled_);
    });
}

void MainWindow::restoreSession()
{
    restoringSession_ = true;
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    const bool adBlockEnabled = settings.value(QStringLiteral("preferences/adblock"), true).toBool();
    adBlocker_->setEnabled(adBlockEnabled);
    sidebar_->setAdBlockEnabled(adBlockEnabled);
    setSearchTemplate(settings.value(QStringLiteral("preferences/searchTemplate"),
                                     QStringLiteral("https://duckduckgo.com/?q=%1")).toString());
    suspendOnMinimize_ =
        settings.value(QStringLiteral("preferences/suspendOnMinimize"), true).toBool();
    restoreSessionEnabled_ =
        settings.value(QStringLiteral("preferences/restoreSession"), true).toBool();

    currentWorkspace_ = settings.value(QStringLiteral("session/workspace"), 0).toInt();
    if (currentWorkspace_ < 0 || currentWorkspace_ > 1) {
        currentWorkspace_ = 0;
    }
    sidebar_->setWorkspace(currentWorkspace_);

    const int folderCount = settings.beginReadArray(QStringLiteral("session/folders"));
    for (int index = 0; index < folderCount; ++index) {
        settings.setArrayIndex(index);
        tabManager_->createFolder(settings.value(QStringLiteral("workspace")).toInt(),
                                  settings.value(QStringLiteral("name")).toString(),
                                  settings.value(QStringLiteral("id")).toInt());
    }
    settings.endArray();
    syncFolders();

    const int count = settings.beginReadArray(QStringLiteral("session/tabs"));
    for (int index = 0; restoreSessionEnabled_ && index < count; ++index) {
        settings.setArrayIndex(index);
        const QUrl url = settings.value(QStringLiteral("url")).toUrl();
        const int workspace = settings.value(QStringLiteral("workspace"), 0).toInt();
        if (url.isValid() && workspace >= 0 && workspace <= 1) {
            const int id = createTab(
                url, workspace, false,
                settings.value(QStringLiteral("essential"), false).toBool(),
                settings.value(QStringLiteral("folderId"), -1).toInt(), true);
            if (auto *record = tabManager_->tab(id)) {
                record->title = settings.value(QStringLiteral("title"), tr("Nova aba")).toString();
                record->historyState = settings.value(QStringLiteral("history")).toByteArray();
                record->icon = iconFromBytes(settings.value(QStringLiteral("icon")).toByteArray());
                record->isDiscarded = true;
                updateSidebarTab(id);
            }
        }
    }
    settings.endArray();

    const auto workspaceTabs = tabManager_->tabsInWorkspace(currentWorkspace_);
    const auto allTabs = tabManager_->allTabIds();
    const int activeIndex = settings.value(QStringLiteral("session/activeTabIndex"), 0).toInt();
    const int restoredActive = activeIndex >= 0 && activeIndex < allTabs.size()
        ? allTabs.at(activeIndex)
        : -1;
    if (workspaceTabs.isEmpty()) {
        createTab(kNewTabUrl, currentWorkspace_, true);
    } else if (workspaceTabs.contains(restoredActive)) {
        activateTab(restoredActive);
    } else {
        activateTab(workspaceTabs.constFirst());
    }
    setCompactMode(settings.value(QStringLiteral("preferences/compactMode"), false).toBool());
    restoringSession_ = false;
    scheduleSessionSave();
}

void MainWindow::saveSession() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("session/workspace"), currentWorkspace_);
    settings.setValue(QStringLiteral("session/activeTabIndex"),
                      tabManager_->allTabIds().indexOf(currentTabId_));
    settings.setValue(QStringLiteral("preferences/adblock"), adBlocker_->isEnabled());
    settings.setValue(QStringLiteral("preferences/compactMode"), compactMode_);
    settings.setValue(QStringLiteral("preferences/searchTemplate"), searchTemplate_);
    settings.setValue(QStringLiteral("preferences/suspendOnMinimize"), suspendOnMinimize_);
    settings.setValue(QStringLiteral("preferences/restoreSession"), restoreSessionEnabled_);

    settings.beginWriteArray(QStringLiteral("session/folders"));
    int folderIndex = 0;
    for (int workspace = 0; workspace < 2; ++workspace) {
        for (const auto &folder : tabManager_->foldersInWorkspace(workspace)) {
            settings.setArrayIndex(folderIndex++);
            settings.setValue(QStringLiteral("id"), folder.id);
            settings.setValue(QStringLiteral("workspace"), folder.workspace);
            settings.setValue(QStringLiteral("name"), folder.name);
        }
    }
    settings.endArray();

    settings.beginWriteArray(QStringLiteral("session/tabs"));

    int index = 0;
    for (const int id : tabManager_->allTabIds()) {
        const auto *record = tabManager_->tab(id);
        if (!record || !record->url.isValid())
            continue;
        settings.setArrayIndex(index++);
        settings.setValue(QStringLiteral("url"), record->url);
        settings.setValue(QStringLiteral("workspace"), record->workspace);
        settings.setValue(QStringLiteral("essential"), record->isEssential);
        settings.setValue(QStringLiteral("folderId"), record->folderId);
        settings.setValue(QStringLiteral("title"), record->title);
        settings.setValue(QStringLiteral("history"), tabManager_->serializedHistory(id));
        settings.setValue(QStringLiteral("icon"), iconBytes(record->icon));
    }
    settings.endArray();
    settings.sync();
}

int MainWindow::createTab(const QUrl &url, const int workspace, const bool activate,
                          const bool essential, const int folderId, const bool lazy)
{
    const int id = tabManager_->createTab(url, workspace, splitView_, essential, folderId, lazy);
    if (activate) {
        if (workspace != currentWorkspace_) {
            switchWorkspace(workspace);
        }
        activateTab(id);
        QTimer::singleShot(0, urlBar_, &UrlBar::focusAddress);
    }
    return id;
}

void MainWindow::activateTab(const int id)
{
    auto *record = tabManager_->tab(id);
    if (!record) {
        return;
    }

    if (!record->isEssential && record->workspace != currentWorkspace_) {
        currentWorkspace_ = record->workspace;
        sidebar_->setWorkspace(currentWorkspace_);
    }
    if (!record->isEssential && record->workspace >= 0 && record->workspace <= 1)
        lastWorkspaceTab_[record->workspace] = id;

    const int previousPrimary = currentTabId_;
    if (splitEnabled_ && id == secondaryTabId_) {
        secondaryTabId_ = previousPrimary;
    }
    currentTabId_ = id;
    settingsVisible_ = false;
    contentStack_->setCurrentWidget(splitView_);
    tabManager_->activateTab(id);
    sidebar_->selectTab(id);
    applyViewLayout();
    scheduleSessionSave();
}

void MainWindow::cycleTabs(const int direction)
{
    if (!tabManager_ || direction == 0)
        return;

    QList<int> order;
    const auto all = tabManager_->allTabIds();
    for (const int id : all) {
        const auto *record = tabManager_->tab(id);
        if (record && record->isEssential)
            order.append(id);
    }
    QSet<int> placed;
    for (const auto &folder : tabManager_->foldersInWorkspace(currentWorkspace_)) {
        for (const int id : all) {
            const auto *record = tabManager_->tab(id);
            if (!record || record->isEssential || record->workspace != currentWorkspace_
                || record->folderId != folder.id) {
                continue;
            }
            order.append(id);
            placed.insert(id);
        }
    }
    for (const int id : all) {
        const auto *record = tabManager_->tab(id);
        if (!record || record->isEssential || record->workspace != currentWorkspace_
            || placed.contains(id)) {
            continue;
        }
        order.append(id);
    }
    if (order.isEmpty())
        return;

    int index = order.indexOf(currentTabId_);
    if (index < 0)
        index = direction > 0 ? 0 : order.size() - 1;
    else
        index = (index + (direction > 0 ? 1 : -1) + order.size()) % order.size();
    activateTab(order.at(index));
}

void MainWindow::closeTab(const int id)
{
    if (!tabManager_->contains(id)) {
        return;
    }

    if (lastWorkspaceTab_[0] == id)
        lastWorkspaceTab_[0] = -1;
    if (lastWorkspaceTab_[1] == id)
        lastWorkspaceTab_[1] = -1;

    const bool closingPrimary = id == currentTabId_;
    const bool closingSecondary = id == secondaryTabId_;
    tabManager_->closeTab(id);
    sidebar_->removeTab(id);

    if (closingSecondary) {
        secondaryTabId_ = -1;
        splitEnabled_ = false;
    }

    if (closingPrimary) {
        const auto remaining = tabManager_->tabsInWorkspace(currentWorkspace_);
        currentTabId_ = remaining.isEmpty() ? -1 : remaining.constFirst();
        if (currentTabId_ < 0) {
            currentTabId_ = createTab(kNewTabUrl, currentWorkspace_, false);
        }
        tabManager_->activateTab(currentTabId_);
        sidebar_->selectTab(currentTabId_);
    }

    if (!tabManager_->contains(secondaryTabId_)) {
        secondaryTabId_ = -1;
        splitEnabled_ = false;
    }
    applyViewLayout();
    scheduleSessionSave();
}

void MainWindow::switchWorkspace(const int workspace)
{
    if (workspace < 0 || workspace > 1 || workspace == currentWorkspace_)
        return;

    currentWorkspace_ = workspace;
    sidebar_->setWorkspace(workspace);
    splitEnabled_ = false;
    secondaryTabId_ = -1;
    urlBar_->setSplitEnabled(false);

    int nextId = lastWorkspaceTab_[workspace];
    const auto *remembered = tabManager_->tab(nextId);
    if (!remembered || remembered->isEssential || remembered->workspace != workspace)
        nextId = -1;
    if (nextId < 0) {
        const auto tabs = tabManager_->tabsInWorkspace(workspace, false);
        nextId = tabs.isEmpty() ? createTab(kNewTabUrl, workspace, false) : tabs.constFirst();
    }

    currentTabId_ = nextId;
    tabManager_->activateTab(currentTabId_);
    sidebar_->selectTab(currentTabId_);
    applyViewLayout();
    scheduleSessionSave();
}

void MainWindow::toggleSplit(const bool enabled)
{
    if (!tabManager_->contains(currentTabId_)) {
        return;
    }

    splitEnabled_ = enabled;
    if (splitEnabled_) {
        secondaryTabId_ = splitCandidate(currentTabId_);
        if (secondaryTabId_ < 0) {
            secondaryTabId_ = createTab(kNewTabUrl, currentWorkspace_, false);
        }
    } else {
        secondaryTabId_ = -1;
    }

    urlBar_->setSplitEnabled(splitEnabled_);
    applyViewLayout();
}

void MainWindow::applyViewLayout()
{
    auto *primary = tabManager_->tab(currentTabId_);
    QWebEngineView *primaryView = tabManager_->ensureView(currentTabId_);
    if (!primary || !primaryView) {
        splitView_->showEmptyState();
        tabManager_->setVisibleTabs({});
        updateChrome();
        return;
    }

    QSet<int> visible {currentTabId_};
    if (splitEnabled_) {
        auto *secondary = tabManager_->tab(secondaryTabId_);
        QWebEngineView *secondaryView = tabManager_->ensureView(secondaryTabId_);
        if (secondary && secondaryView
            && (secondary->workspace == currentWorkspace_ || secondary->isEssential)) {
            splitView_->setSplitViews(primaryView, secondaryView);
            visible.insert(secondaryTabId_);
        } else {
            splitEnabled_ = false;
            secondaryTabId_ = -1;
            splitView_->setSingleView(primaryView);
        }
    } else {
        splitView_->setSingleView(primaryView);
    }

    urlBar_->raise();
    tabManager_->setVisibleTabs(visible);
    urlBar_->setSplitEnabled(splitEnabled_);
    updateChrome();
}

void MainWindow::updateChrome()
{
    const auto *record = tabManager_->tab(currentTabId_);
    if (!record || !record->view) {
        setWindowTitle(tr("Zenith Browser"));
        urlBar_->setUrl({});
        urlBar_->setNavigationState(false, false);
        return;
    }

    urlBar_->setUrl(record->view->url());
    urlBar_->setNavigationState(record->view->history()->canGoBack(),
                                record->view->history()->canGoForward());
    const QString title = record->title.trimmed().isEmpty() ? tr("Nova aba") : record->title;
    setWindowTitle(tr("%1 · Zenith").arg(title));
}

void MainWindow::updateSidebarTab(const int id)
{
    const auto *record = tabManager_->tab(id);
    if (!record)
        return;
    const Sidebar::TabVisual visual {
        record->id,
        record->workspace,
        record->folderId,
        record->title,
        record->icon,
        record->isDiscarded,
        record->isEssential,
    };
    sidebar_->updateTab(visual);
    sidebar_->selectTab(currentTabId_);
}

void MainWindow::syncFolders()
{
    QList<Sidebar::FolderVisual> folders;
    for (int workspace = 0; workspace < 2; ++workspace) {
        for (const auto &folder : tabManager_->foldersInWorkspace(workspace))
            folders.append({folder.id, folder.workspace, folder.name});
    }
    sidebar_->setFolders(folders);
}

void MainWindow::setCompactMode(const bool enabled)
{
    compactMode_ = enabled;
    compactSidebarVisible_ = enabled && sidebar_->underMouse();
    compactUrlBarVisible_ = enabled && urlBar_->underMouse();
    sidebar_->setCompactEnabled(enabled);
    updateShellGeometry();
    scheduleSessionSave();
}

void MainWindow::updateShellGeometry()
{
    if (!centralSurface_ || !contentWidget_ || !sidebar_)
        return;
    const int width = centralSurface_->width();
    const int height = centralSurface_->height();
    const int sidebarWidth = sidebar_->preferredWidth();

    if (compactMode_) {
        urlBar_->setVisible(compactUrlBarVisible_);
        const int contentOffset = compactSidebarVisible_ ? sidebarWidth : 0;
        contentWidget_->setGeometry(contentOffset, 0,
                                    std::max(0, width - contentOffset), height);
        sidebar_->setVisible(compactSidebarVisible_);
        if (compactSidebarVisible_)
            sidebar_->setGeometry(0, 0, sidebarWidth, height);
        revealStrip_->setGeometry(0, 0, 5, height);
        topRevealStrip_->setGeometry(5, 0, std::max(0, width - 5), 5);
        revealStrip_->show();
        topRevealStrip_->show();
        revealStrip_->raise();
        topRevealStrip_->raise();
        if (compactSidebarVisible_)
            sidebar_->raise();
    } else {
        urlBar_->show();
        sidebar_->show();
        revealStrip_->hide();
        topRevealStrip_->hide();
        sidebar_->setGeometry(0, 0, sidebarWidth, height);
        contentWidget_->setGeometry(sidebarWidth, 0, std::max(0, width - sidebarWidth), height);
        sidebar_->raise();
    }
}

void MainWindow::setSearchTemplate(const QString &searchTemplate)
{
    if (!searchTemplate.contains(QStringLiteral("%1")))
        return;
    searchTemplate_ = searchTemplate;
    urlBar_->setSearchTemplate(searchTemplate_);
    sidebar_->setSearchTemplate(searchTemplate_);
    scheduleSessionSave();
}

void MainWindow::scheduleSessionSave()
{
    if (!restoringSession_ && sessionSaveTimer_)
        sessionSaveTimer_->start();
}

void MainWindow::showBrowserMenu(const QPoint &globalPosition)
{
    QMenu menu(this);
    QAction *newTab = menu.addAction(zenithIcon(ZenithIcon::Add), tr("Nova aba"));
    newTab->setShortcut(QKeySequence::AddTab);
    connect(newTab, &QAction::triggered, this, [this] {
        createTab(kNewTabUrl, currentWorkspace_, true);
    });

    QAction *save = menu.addAction(tr("Salvar sessão agora"));
    connect(save, &QAction::triggered, this, [this] { saveSession(); });
    QAction *settings = menu.addAction(zenithIcon(ZenithIcon::Settings), tr("Configurações"));
    connect(settings, &QAction::triggered, this, &MainWindow::showSettings);
    menu.addSeparator();

    QAction *adBlock = menu.addAction(zenithIcon(ZenithIcon::Shield),
                                      tr("Bloqueador de anúncios"));
    adBlock->setCheckable(true);
    adBlock->setChecked(adBlocker_->isEnabled());
    connect(adBlock, &QAction::toggled, this, [this](const bool enabled) {
        adBlocker_->setEnabled(enabled);
        sidebar_->setAdBlockEnabled(enabled);
        scheduleSessionSave();
    });
    QAction *adBlockStatus = menu.addAction(
        tr("%1 bloqueios nesta sessão · %2")
            .arg(adBlocker_->blockedCount())
            .arg(adBlockUpdater_->statusText()));
    adBlockStatus->setEnabled(false);
    QAction *updateAdBlock = menu.addAction(tr("Atualizar listas do AdBlock"));
    connect(updateAdBlock, &QAction::triggered, adBlockUpdater_, &AdBlockUpdater::updateNow);

    QAction *compact = menu.addAction(tr("Modo compacto"));
    compact->setCheckable(true);
    compact->setChecked(compactMode_);
    connect(compact, &QAction::toggled, this, &MainWindow::setCompactMode);

    QMenu *search = menu.addMenu(tr("Mecanismo de busca"));
    const QList<QPair<QString, QString>> engines {
        {tr("DuckDuckGo"), QStringLiteral("https://duckduckgo.com/?q=%1")},
        {tr("Google"), QStringLiteral("https://www.google.com/search?q=%1")},
        {tr("Brave Search"), QStringLiteral("https://search.brave.com/search?q=%1")},
        {tr("Bing"), QStringLiteral("https://www.bing.com/search?q=%1")},
    };
    for (const auto &[name, value] : engines) {
        QAction *action = search->addAction(name);
        action->setCheckable(true);
        action->setChecked(searchTemplate_ == value);
        connect(action, &QAction::triggered, this, [this, value] {
            setSearchTemplate(value);
        });
    }

    QAction *credentials = menu.addAction(tr("Senhas e credenciais"));
    connect(credentials, &QAction::triggered, this, &MainWindow::showCredentialManager);

    menu.exec(globalPosition);
}

void MainWindow::showCredentialManager()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Senhas e credenciais"));
    dialog.resize(520, 340);

    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    list->setAlternatingRowColors(true);
    for (const auto &entry : credentialManager_->entries()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(entry.origin, entry.username), list);
        item->setData(Qt::UserRole, entry.origin);
    }
    if (list->count() == 0) {
        auto *empty = new QListWidgetItem(
            tr("Nenhuma credencial salva.\nEntre em um site HTTPS para adicionar uma."), list);
        empty->setFlags(Qt::NoItemFlags);
    }
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *remove = buttons->addButton(tr("Remover selecionada"),
                                      QDialogButtonBox::DestructiveRole);
    remove->setEnabled(false);
    connect(list, &QListWidget::itemSelectionChanged, &dialog, [list, remove] {
        remove->setEnabled(list->currentItem() != nullptr);
    });
    connect(remove, &QPushButton::clicked, &dialog, [this, list] {
        auto *item = list->currentItem();
        if (!item)
            return;
        QString error;
        if (credentialManager_->remove(item->data(Qt::UserRole).toString(), &error)) {
            delete list->takeItem(list->row(item));
        } else {
            QMessageBox::warning(this, tr("Credencial não removida"), error);
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showSettings()
{
    settingsPage_->setSearchTemplate(searchTemplate_);
    settingsPage_->setCompactMode(compactMode_);
    settingsPage_->setAdBlockEnabled(adBlocker_->isEnabled());
    settingsPage_->setSuspendOnMinimize(suspendOnMinimize_);
    settingsPage_->setRestoreSession(restoreSessionEnabled_);
    settingsPage_->setAdBlockStatus(adBlocker_->blockedCount(),
                                    adBlocker_->ruleCount(),
                                    adBlockUpdater_->statusText());
    settingsVisible_ = true;
    contentStack_->setCurrentWidget(settingsPage_);
}

void MainWindow::closeSettings()
{
    settingsVisible_ = false;
    contentStack_->setCurrentWidget(splitView_);
    applyViewLayout();
}

int MainWindow::splitCandidate(const int excludedId) const
{
    const auto tabs = tabManager_->tabsInWorkspace(currentWorkspace_);
    for (const int id : tabs) {
        if (id != excludedId) {
            return id;
        }
    }
    return -1;
}
