#include "ui/BrowserMenu.h"
#include "ui/PageWidgets.h"
#include <QShowEvent>
#include <QStyle>
#include <QLabel>
#include "ui/SettingsPage.h"
#include "ui/ThemeManager.h"
#include "ui/MainWindow.h"

#include "core/AdBlockInterceptor.h"
#include "core/AdBlockUpdater.h"
#include "core/CredentialManager.h"
#include "core/TabManager.h"
#include "core/WebProfile.h"
#include "core/WebView.h"
#include "core/BrowserData.h"
#include "core/DownloadManager.h"
#include "core/Navigation.h"
#include "core/SessionStore.h"
#include "core/Preferences.h"
#include "ui/Sidebar.h"
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
#include "ui/FindBar.h"
#include "ui/InternalPageStack.h"
#include "ui/UiMetrics.h"
#include <QDialogButtonBox>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QListWidget>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QTimer>
#include <QStatusBar>
#include <QVariantMap>
#include <QProcess>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>

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

MainWindow::MainWindow(bool privateMode, QWidget *parent)
    : QMainWindow(parent)
    , privateMode_(privateMode)
{
    setWindowTitle(privateMode_ ? tr("Zenith Browser · Privado") : tr("Zenith Browser"));
    setMinimumSize(760, 520);
    resize(1280, 800);
    setObjectName("browserWindow");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setContentsMargins(6, 6, 6, 6);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, [this] { PageWidgets::syncWindowChrome(this); if (sidebar_) { updateShellGeometry(); syncPageSurfaces(); } });

    createInterface();
    auto *feedback = statusBar();
    feedback->setSizeGripEnabled(false);
    feedback->hide();
    connect(feedback, &QStatusBar::messageChanged, feedback, [feedback](const QString &message) {
        feedback->setVisible(!message.isEmpty());
    });
    configureProfile();
    adBlockUpdater_ = new AdBlockUpdater(adBlocker_, this);
    credentialManager_ = new CredentialManager(this);
    tabManager_ = new TabManager(profile_, privateMode_ ? nullptr : credentialManager_, this);
    QSettings workspaceSettings;
    const QStringList savedNames = workspaceSettings.value(QStringLiteral("preferences/workspaces")).toStringList();
    if (!savedNames.isEmpty() && savedNames.size() <= 16) workspaceNames_ = savedNames;
    sidebar_->setWorkspaces(workspaceNames_);
    tabManager_->setWorkspaceCount(workspaceNames_.size());
    connect(tabManager_, &TabManager::newTabRequested, this, [this](int sourceId, const QUrl &url, bool foreground) {
        const auto *source = tabManager_->tab(sourceId);
        if (!source) return;
        createTab(url, source->isEssential ? currentWorkspace_ : source->workspace, foreground);
    });
    tabManager_->setPopupFactory([this](WebView *opener, const QUrl &) {
        const int id = createTab(QUrl(), currentWorkspace_, false, false, -1, false, opener);
        QTimer::singleShot(0, this, [this, id] {
            if (tabManager_ && tabManager_->contains(id))
                activateTab(id);
        });
        const auto *record = tabManager_->tab(id);
        return record ? record->view.data() : nullptr;
    });
    hideSidebarTimer_ = new QTimer(this);
    hideSidebarTimer_->setSingleShot(true);
    hideSidebarTimer_->setInterval(260);
    sidebarSlide_ = new QPropertyAnimation(this, "sidebarReveal", this);
    sidebarSlide_->setDuration(QSettings().value(QStringLiteral("preferences/reducedMotion"), false).toBool() ? 0 : 220);
    sidebarSlide_->setEasingCurve(QEasingCurve::OutCubic);
    urlBarSlide_ = new QPropertyAnimation(this, "urlBarReveal", this);
    urlBarSlide_->setDuration(QSettings().value(QStringLiteral("preferences/reducedMotion"), false).toBool() ? 0 : 180);
    urlBarSlide_->setEasingCurve(QEasingCurve::OutCubic);
    connect(sidebarSlide_, &QPropertyAnimation::valueChanged, this, [this] {
        if (sidebar_)
            sidebar_->raise();
    });
    connect(sidebarSlide_, &QPropertyAnimation::finished, this, [this] {
        if (!sidebarSlideGuard_ && !compactSidebarVisible_)
            sidebar_->hide();
    });
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
    restoreSession();
    auto *idleTimer = new QTimer(this);
    idleTimer->setInterval(15'000);
    connect(idleTimer, &QTimer::timeout, this, [this] {
        QSettings settings;
        const int seconds = settings.value(QStringLiteral("preferences/discardAfterSeconds"), 180).toInt();
        if (seconds >= 60) tabManager_->suspendInactive(seconds);
        const int maxLive = settings.value(QStringLiteral("preferences/maxLiveTabs"), 3).toInt();
        if (maxLive > 0) tabManager_->suspendInactive(30, maxLive);
    });
    idleTimer->start();
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    WebView::setKeySink({});
    if (qApp)
        qApp->removeEventFilter(this);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (profile_->downloads()->active()
        && QMessageBox::question(this, tr("Downloads ativos"), tr("Sair e cancelar os downloads em andamento?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        event->ignore();
        return;
    }
    saveSession();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        const bool expanded = isMaximized() || isFullScreen();
        const int inset = expanded ? 0 : 6;
        setContentsMargins(inset, inset, inset, inset);
        setProperty("expanded", expanded);
        style()->unpolish(this); style()->polish(this);
        QTimer::singleShot(0, this, [this] { updateShellGeometry(); syncPageSurfaces(); });
    }
    if (event->type() != QEvent::WindowStateChange || !tabManager_)
        return;
    if (isMinimized() && suspendOnMinimize_) {
        minimizeTimer_->start();
    } else {
        minimizeTimer_->stop();
        applyViewLayout();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    PageWidgets::syncWindowChrome(this);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateShellGeometry();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
        && isActiveWindow() && tabManager_ && !QApplication::activeModalWidget()) {
        const auto *key = static_cast<const QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier
            && (settingsVisible_ || findBar_->isVisible())) {
            for (QWidget *editor = QApplication::focusWidget(); editor && editor != this; editor = editor->parentWidget()) {
                if (!editor->property("escapeHidesEditor").toBool()) continue;
                if (event->type() == QEvent::KeyPress) editor->hide();
                event->accept();
                return true;
            }
            if (event->type() == QEvent::KeyPress) {
                if (findBar_->isVisible()) findBar_->finish();
                else closeSettings();
            }
            event->accept();
            return true;
        }
        const int command = Navigation::command(key->key(), key->modifiers());
        if (command != Navigation::None) {
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) executeCommand(command);
            event->accept();
            return true;
        }
    }
    if (watched == revealStrip_ && event->type() == QEvent::Enter && compactMode_) {
        hideSidebarTimer_->stop();
        setCompactSidebarShown(true);
    }
    if (compactMode_ && event->type() == QEvent::Enter
        && (watched == topRevealStrip_ || watched == urlBar_)) {
        hideUrlBarTimer_->stop();
        showAddressBar(false);
    }
    if (compactMode_ && watched == urlBar_ && event->type() == QEvent::Leave) {
        const QPoint onBar = urlBar_->mapFromGlobal(QCursor::pos());
        const QPoint onStrip = topRevealStrip_->mapFromGlobal(QCursor::pos());
        if (!urlBar_->addressHasFocus() && !urlBar_->rect().contains(onBar)
            && !topRevealStrip_->rect().contains(onStrip))
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
    sidebar_ = new Sidebar(centralSurface_);
    urlBar_ = new UrlBar(contentWidget_);
    splitView_ = new SplitView(contentWidget_);
    settingsPage_ = new SettingsPage(contentWidget_);
    contentStack_ = new QStackedWidget(contentWidget_);
    contentStack_->addWidget(splitView_);
    contentStack_->addWidget(settingsPage_);
    internalNavigation_ = new InternalPageStack(contentStack_, splitView_);
    internalNavigation_->keep(settingsPage_);
    connect(internalNavigation_, &InternalPageStack::changed, this, [this](bool active) {
        settingsVisible_ = active;
        if (!tabManager_) return;
        setPageInputEnabled(!active && !findBar_->isVisible());
        if (!active) {
            applyViewLayout();
            if (auto *view = currentView()) view->setFocus(Qt::BacktabFocusReason);
        }
        updateChrome();
    });
    revealStrip_ = new QWidget(centralSurface_);
    revealStrip_->setObjectName(QStringLiteral("revealStrip"));
    revealStrip_->setMouseTracking(true);
    revealStrip_->setAutoFillBackground(true);
    revealStrip_->installEventFilter(this);
    topRevealStrip_ = new QWidget(centralSurface_);
    topRevealStrip_->setObjectName(QStringLiteral("revealStrip"));
    topRevealStrip_->setMouseTracking(true);
    topRevealStrip_->setAutoFillBackground(true);
    topRevealStrip_->installEventFilter(this);
    urlBar_->installEventFilter(this);

    auto *contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(urlBar_);
    contentLayout->addWidget(contentStack_, 1);
    findBar_ = new FindBar(contentWidget_);
    contentLayout->addWidget(findBar_);
    connect(findBar_, &FindBar::closed, this, [this] {
        setPageInputEnabled(!settingsVisible_);
        if (!settingsVisible_) if (auto *view = currentView()) view->setFocus();
    });
    revealStrip_->hide();
    topRevealStrip_->hide();
    sidebar_->raise();
}

void MainWindow::connectInterface()
{
    connect(profile_->downloads(), &DownloadManager::changed, this, [this] {
        urlBar_->setDownloadActivity(profile_->downloads()->active());
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *focused) {
        if (!focused || !isAncestorOf(focused) || urlBar_->isAncestorOf(focused)) return;
        if (!qobject_cast<QLineEdit *>(focused)) return;
        ++addressFocusToken_;
        WebView::setKeySink({});
        setPageInputEnabled(false);
    });
    connect(settingsPage_, &SettingsPage::preferencesReset, this, [this] {
        setSearchTemplate(Preferences::value("preferences/searchTemplate").toString());
        restoreSessionEnabled_ = Preferences::value("preferences/restoreSession").toBool();
        suspendOnMinimize_ = Preferences::value("preferences/suspendOnMinimize").toBool();
        adBlocker_->setEnabled(Preferences::value("preferences/adblock").toBool());
        profile_->setPrivacyPolicy(Preferences::value("privacy/blockThirdPartyCookies").toBool(),
                                  Preferences::value("privacy/trackingPrevention").toBool());
        const bool reduced = Preferences::value("preferences/reducedMotion").toBool();
        sidebarSlide_->setDuration(reduced ? 0 : 220);
        urlBarSlide_->setDuration(reduced ? 0 : 180);
        setCompactMode(Preferences::value("preferences/compactMode").toBool());
        ThemeManager::instance().reloadPreferences();
        for (const int id : tabManager_->allTabIds())
            if (auto *tab = tabManager_->tab(id); tab && tab->view) tab->view->applyBrowsingPreferences();
        scheduleSessionSave();
    });
    connect(settingsPage_, &SettingsPage::commandRequested, this, &MainWindow::executeCommand);
    connect(settingsPage_, &SettingsPage::browsingPreferencesChanged, this, [this] {
        for (const int id : tabManager_->allTabIds())
            if (auto *tab = tabManager_->tab(id); tab && tab->view) tab->view->applyBrowsingPreferences();
    });
    connect(settingsPage_, &SettingsPage::privacyPolicyChanged, this, [this](bool thirdParty, bool tracking) { profile_->setPrivacyPolicy(thirdParty, tracking); });
    connect(settingsPage_, &SettingsPage::reducedMotionChanged, this, [this](bool reduced) {
        sidebarSlide_->setDuration(reduced ? 0 : 220);
        urlBarSlide_->setDuration(reduced ? 0 : 180);
    });
    connect(settingsPage_, &SettingsPage::closeRequested, this, &MainWindow::closeSettings);
    connect(settingsPage_, &SettingsPage::defaultBrowserRequested, this, &MainWindow::setDefaultBrowser);
    connect(settingsPage_, &SettingsPage::searchTemplateChanged,
            this, &MainWindow::setSearchTemplate);
    connect(settingsPage_, &SettingsPage::compactModeChanged,
            this, &MainWindow::setCompactMode);
    connect(settingsPage_, &SettingsPage::adBlockChanged, this, [this](const bool enabled) {
        adBlocker_->setEnabled(enabled);
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
                QMessageBox prompt(QMessageBox::Question, tr("Salvar credencial"),
                    tr("Salvar o login “%1” para %2 no cofre seguro do sistema?")
                        .arg(username.left(128), origin),
                    QMessageBox::Save | QMessageBox::Cancel, this);
                prompt.setTextFormat(Qt::PlainText);
                prompt.setDefaultButton(QMessageBox::Cancel);
                if (prompt.exec() != QMessageBox::Save)
                    return;
                QString error;
                if (!credentialManager_->save({origin, username, password}, &error))
                    QMessageBox::warning(this, tr("Credencial não salva"), error);
            });

    connect(sidebar_, &Sidebar::newTabRequested, this, [this] {
        createTab(kNewTabUrl, currentWorkspace_, true);
    });
    connect(sidebar_, &Sidebar::tabActivated, this, &MainWindow::activateTab);
    connect(sidebar_, &Sidebar::reopenTabRequested, this, [this] { executeCommand(Navigation::ReopenTab); });
    connect(sidebar_, &Sidebar::closeTabRequested, this, &MainWindow::closeTab);
    connect(sidebar_, &Sidebar::workspaceSelected, this, &MainWindow::switchWorkspace);
    connect(sidebar_, &Sidebar::createWorkspaceRequested, this, [this] { manageWorkspace(0); });
    connect(sidebar_, &Sidebar::workspaceActionRequested, this, [this](int action) { manageWorkspace(action); });
    connect(sidebar_, &Sidebar::splitTabRequested, this, [this](int id) {
        if (settingsVisible_) leaveInternalPages();
        if (id == currentTabId_) { toggleSplit(!splitEnabled_); return; }
        const auto *record = tabManager_->tab(id);
        if (!record || (record->workspace != currentWorkspace_ && !record->isEssential)) return;
        secondaryTabId_ = id;
        splitEnabled_ = true;
        applyViewLayout();
        scheduleSessionSave();
    });
    connect(settingsPage_, &SettingsPage::workspaceActionRequested, this, [this](int action) { manageWorkspace(action); });
    connect(settingsPage_, &SettingsPage::workspaceNameRequested, this, [this](int action, const QString &name, int target) { manageWorkspace(action, name, target); });
    connect(settingsPage_, &SettingsPage::workspaceTargetActionRequested, this, [this](int action, int target) { manageWorkspace(action, {}, target); });
    connect(settingsPage_, &SettingsPage::workspaceSelected, this, &MainWindow::switchWorkspace);
    connect(sidebar_, &Sidebar::tabEssentialRequested, tabManager_, &TabManager::setTabEssential);
    connect(sidebar_, &Sidebar::tabFolderRequested, tabManager_, &TabManager::setTabFolder);
    connect(sidebar_, &Sidebar::tabWorkspaceRequested, this, [this](int id, int workspace) {
        tabManager_->setTabWorkspace(id, workspace);
        if (id == currentTabId_) activateTab(id);
    });
    connect(sidebar_, &Sidebar::tabMoveRequested, tabManager_, &TabManager::moveTab);
    connect(sidebar_, &Sidebar::tabDropRequested, tabManager_, &TabManager::moveTabBefore);
    connect(sidebar_, &Sidebar::duplicateTabRequested, this, [this](int id) {
        if (const auto *record = tabManager_->tab(id)) createTab(record->url, record->workspace, true);
    });
    connect(sidebar_, &Sidebar::muteTabRequested, this, [this](int id) {
        if (auto *record = tabManager_->tab(id)) {
            record->muted = !record->muted;
            if (record->view) record->view->setMuted(record->muted);
            updateSidebarTab(id);
        }
    });
    connect(sidebar_, &Sidebar::closeOtherTabsRequested, this, [this](int id) {
        const auto *record = tabManager_->tab(id);
        if (!record) return;
        const int workspace = record->isEssential ? currentWorkspace_ : record->workspace;
        if (QMessageBox::question(this, tr("Fechar outras abas"), tr("Fechar as outras abas não fixadas deste espaço?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes || !tabManager_->contains(id)) return;
        const auto ids = tabManager_->tabsInWorkspace(workspace, false);
        activateTab(id);
        for (const int candidate : ids)
            if (const auto *tab = tabManager_->tab(candidate); candidate != id && tab && !tab->isEssential) closeTab(candidate);
    });
    connect(sidebar_, &Sidebar::folderEditRequested, this, &MainWindow::editFolder);
    connect(sidebar_, &Sidebar::createFolderRequested, this, [this](const QString &name) {
        tabManager_->createFolder(currentWorkspace_, name);
    });
    connect(sidebar_, &Sidebar::renameFolderRequested, tabManager_, &TabManager::renameFolder);
    connect(sidebar_, &Sidebar::removeFolderRequested, tabManager_, &TabManager::removeFolder);
    connect(sidebar_, &Sidebar::folderMoveRequested, tabManager_, &TabManager::moveFolder);
    connect(sidebar_, &Sidebar::folderDropRequested, tabManager_, &TabManager::moveFolderBefore);
    connect(sidebar_, &Sidebar::closeFollowingTabsRequested, this, &MainWindow::closeFollowingTabs);
    connect(sidebar_, &Sidebar::pointerEntered, this, [this] {
        if (!compactMode_)
            return;
        hideSidebarTimer_->stop();
        setCompactSidebarShown(true);
    });
    connect(sidebar_, &Sidebar::pointerLeft, this, [this] {
        if (compactMode_)
            hideSidebarTimer_->start();
    });
    connect(hideSidebarTimer_, &QTimer::timeout, this, [this] {
        if (QApplication::mouseButtons() != Qt::NoButton || QApplication::activePopupWidget()
            || (QApplication::focusWidget() && sidebar_->isAncestorOf(QApplication::focusWidget()))) {
            hideSidebarTimer_->start();
            return;
        }
        setCompactSidebarShown(false);
    });
    connect(hideUrlBarTimer_, &QTimer::timeout, this, [this] {
        if (urlBar_ && urlBar_->addressHasFocus())
            return;
        compactUrlBarVisible_ = false;
        if (!compactMode_ || !urlBarSlide_) {
            updateShellGeometry();
            return;
        }
        urlBarSlide_->stop();
        urlBarSlide_->setStartValue(urlBarReveal_);
        urlBarSlide_->setEndValue(0);
        urlBarSlide_->start();
    });
    connect(sessionSaveTimer_, &QTimer::timeout, this, [this] { saveSession(); });

    connect(urlBar_, &UrlBar::navigateRequested, this, &MainWindow::navigateTo);
    connect(splitView_, &SplitView::navigateRequested, this, &MainWindow::navigateTo);
    connect(splitView_, &SplitView::commandRequested, this, &MainWindow::executeCommand);
    connect(urlBar_, &UrlBar::backRequested, this, [this] {
        if (settingsVisible_) { closeSettings(); return; }
        if (auto *view = currentView()) view->back();
    });
    connect(urlBar_, &UrlBar::forwardRequested, this, [this] {
        if (auto *view = currentView()) view->forward();
    });
    connect(urlBar_, &UrlBar::reloadRequested, this, [this] {
        if (auto *view = currentView()) view->reload();
    });
    connect(urlBar_, &UrlBar::stopRequested, this, [this] {
        if (auto *view = currentView()) view->stop();
    });
    connect(urlBar_, &UrlBar::compactToggled, this, [this] { setCompactMode(!compactMode_); });
    connect(urlBar_, &UrlBar::bookmarkRequested, this, &MainWindow::addBookmark);
    connect(urlBar_, &UrlBar::downloadsRequested, this, [this] { executeCommand(Navigation::Downloads); });
    connect(urlBar_, &UrlBar::siteControlsRequested, this, &MainWindow::showSiteControls);
    connect(urlBar_, &UrlBar::menuRequested, this, &MainWindow::showBrowserMenu);
    connect(urlBar_, &UrlBar::addressFocusChanged, this, [this](const bool focused) {
        if (focused)
            return;
        ++addressFocusToken_;
        setPageInputEnabled(!settingsVisible_ && !findBar_->isVisible());
        if (compactMode_)
            hideUrlBarTimer_->start();
    });

    connect(tabManager_, &TabManager::tabAttentionRequested, this, [this](const int id) {
        activateTab(id);
        showNormal();
        raise();
        activateWindow();
    });
    connect(tabManager_, &TabManager::tabCloseRequested, this, &MainWindow::closeTab);
    connect(tabManager_, &TabManager::tabFocused, this, [this](int id) {
        if (id == currentTabId_ || (splitEnabled_ && id == secondaryTabId_)) {
            activePanelId_ = id;
            updateChrome();
        }
    });
    connect(tabManager_, &TabManager::tabError, this, [this](int id, const QString &message) {
        if (id == currentTabId_) statusBar()->showMessage(message + tr(" · Recarregar: Ctrl+R"), 15000);
    });
    connect(tabManager_, &TabManager::tabAccelerator, this, [this](const int shortcut) {
        if (QApplication::activeModalWidget())
            return;
        executeCommand(shortcut);
    });
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
            [this](const int id, const bool ok) {
                const auto *record = tabManager_->tab(id);
                if (ok && record && !privateMode_)
                    profile_->data()->visit(record->url, record->title);
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
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    profile_ = new WebProfile(dataRoot + QStringLiteral("/webkit"),
                              cacheRoot + QStringLiteral("/webkit"), this, privateMode_);
    adBlocker_ = new AdBlockInterceptor(this);
    profile_->setRequestBlocker(adBlocker_);
}

void MainWindow::restoreSession()
{
    if (privateMode_) {
        sessionOpened_ = true;
        createTab(kNewTabUrl, currentWorkspace_, true);
        return;
    }
    restoringSession_ = true;
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    const bool adBlockEnabled = settings.value(QStringLiteral("preferences/adblock"), true).toBool();
    adBlocker_->setEnabled(adBlockEnabled);
    setSearchTemplate(settings.value(QStringLiteral("preferences/searchTemplate"),
                                     QStringLiteral("https://duckduckgo.com/?q=%1")).toString());
    suspendOnMinimize_ =
        settings.value(QStringLiteral("preferences/suspendOnMinimize"), true).toBool();
    restoreSessionEnabled_ =
        settings.value(QStringLiteral("preferences/restoreSession"), true).toBool();

    const SessionStore store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/session.json"));
    const QVariantMap snapshot = store.load();
    currentWorkspace_ = snapshot.isEmpty() ? settings.value(QStringLiteral("session/workspace"), 0).toInt() : snapshot.value("workspace").toInt();
    if (currentWorkspace_ < 0 || currentWorkspace_ >= workspaceNames_.size()) {
        currentWorkspace_ = 0;
    }
    sidebar_->setWorkspace(currentWorkspace_);

    QVariantList folders = snapshot.value("folders").toList();
    QVariantList tabs = snapshot.value("tabs").toList();
    if (snapshot.isEmpty()) {
      const int folderCount = std::min(settings.beginReadArray(QStringLiteral("session/folders")), 1000);
      for (int index = 0; index < folderCount; ++index) {
        settings.setArrayIndex(index);
        folders.append(QVariantMap{{"workspace", settings.value("workspace")}, {"name", settings.value("name")}, {"id", settings.value("id")}});
      }
      settings.endArray();
      const int count = std::min(settings.beginReadArray(QStringLiteral("session/tabs")), 1000);
      for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        tabs.append(QVariantMap{{"url", settings.value("url").toUrl().toString()},
            {"workspace", settings.value("workspace")}, {"essential", settings.value("essential")},
            {"folderId", settings.value("folderId", -1)}, {"title", settings.value("title")},
            {"history", QString::fromLatin1(settings.value("history").toByteArray().toBase64())},
            {"icon", QString::fromLatin1(settings.value("icon").toByteArray().toBase64())}});
      }
      settings.endArray();
    }
    for (const auto &value : folders) {
        const auto folder = value.toMap();
        tabManager_->createFolder(folder.value("workspace").toInt(), folder.value("name").toString(), folder.value("id").toInt());
    }
    syncFolders();
    for (const auto &value : tabs) {
        if (!restoreSessionEnabled_) break;
        const auto saved = value.toMap();
        const QUrl url(saved.value("url").toString());
        const int workspace = saved.value("workspace").toInt();
        const bool supported = Navigation::isWebUrl(url) || url.isLocalFile() || url == QUrl("about:blank");
        if (supported && workspace >= 0 && workspace < workspaceNames_.size()) {
            const int id = createTab(
                url, workspace, false,
                saved.value("essential").toBool(), saved.value("folderId", -1).toInt(), true);
            if (auto *record = tabManager_->tab(id)) {
                record->title = saved.value("title", tr("Nova aba")).toString().left(4096);
                const QByteArray history = saved.value("history").toByteArray();
                if (history.size() <= 2 * 1024 * 1024) record->historyState = QByteArray::fromBase64(history);
                const QByteArray icon = saved.value("icon").toByteArray();
                if (icon.size() <= 64 * 1024) record->icon = iconFromBytes(QByteArray::fromBase64(icon));
                record->isDiscarded = true;
                updateSidebarTab(id);
            }
        }
    }

    const auto workspaceTabs = tabManager_->tabsInWorkspace(currentWorkspace_);
    const auto allTabs = tabManager_->allTabIds();
    const int activeIndex = snapshot.isEmpty() ? settings.value(QStringLiteral("session/activeTabIndex"), 0).toInt() : snapshot.value("active").toInt();
    const int restoredActive = activeIndex >= 0 && activeIndex < allTabs.size()
        ? allTabs.at(activeIndex)
        : -1;
    const bool compact = settings.value(QStringLiteral("preferences/compactMode"), false).toBool();
    const auto openRestoredSession = [this, workspaceTabs, restoredActive, compact] {
        if (sessionOpened_)
            return;
        sessionOpened_ = true;
        if (workspaceTabs.isEmpty()) {
            createTab(kNewTabUrl, currentWorkspace_, true);
        } else if (workspaceTabs.contains(restoredActive)) {
            activateTab(restoredActive);
        } else {
            activateTab(workspaceTabs.constFirst());
        }
        setCompactMode(compact);
        restoringSession_ = false;
        scheduleSessionSave();
    };
    profile_->whenCookiesReady(openRestoredSession);
    QTimer::singleShot(1500, this, openRestoredSession);
}

void MainWindow::saveSession() const
{
    if (privateMode_) return;
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
    settings.setValue(QStringLiteral("preferences/workspaces"), workspaceNames_);

    QVariantList folders;
    for (int workspace = 0; workspace < workspaceNames_.size(); ++workspace) {
        for (const auto &folder : tabManager_->foldersInWorkspace(workspace)) {
            folders.append(QVariantMap{{"id", folder.id}, {"workspace", folder.workspace}, {"name", folder.name}});
        }
    }
    QVariantList tabs;
    int activeIndex = -1;
    for (const int id : tabManager_->allTabIds()) {
        const auto *record = tabManager_->tab(id);
        if (!record || !record->url.isValid())
            continue;
        if (id == currentTabId_) activeIndex = tabs.size();
        QByteArray history = record->historyState;
        if (history.size() > 1536 * 1024) history.clear();
        tabs.append(QVariantMap{{"url", record->url.toString(QUrl::RemoveUserInfo)},
            {"workspace", record->workspace}, {"essential", record->isEssential},
            {"folderId", record->folderId}, {"title", record->title.left(4096)},
            {"history", QString::fromLatin1(history.toBase64())},
            {"icon", QString::fromLatin1(iconBytes(record->icon).toBase64())}});
    }
    const SessionStore store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/session.json"));
    if (!store.save({{"tabs", tabs}, {"folders", folders}, {"workspace", currentWorkspace_}, {"active", activeIndex}}))
        const_cast<MainWindow *>(this)->statusBar()->showMessage(tr("Falha ao salvar sessão. A cópia anterior foi preservada."), 10000);
    else {
        settings.remove(QStringLiteral("session/tabs"));
        settings.remove(QStringLiteral("session/folders"));
    }
    settings.sync();
}

int MainWindow::createTab(const QUrl &url, const int workspace, const bool activate,
                          const bool essential, const int folderId, const bool lazy,
                          WebView *related)
{
    const int id = tabManager_->createTab(url, workspace, splitView_, essential, folderId, lazy || (!activate && !related) || (url == kNewTabUrl && !related),
                                          related);
    if (activate) {
        if (workspace != currentWorkspace_) {
            switchWorkspace(workspace);
        }
        activateTab(id);
        revealAddressBar();
    }
    return id;
}

void MainWindow::activateTab(const int id)
{
    auto *record = tabManager_->tab(id);
    if (!record) {
        return;
    }
    if (id != unloadedTabId_)
        unloadedTabId_ = -1;

    if (!record->isEssential && record->workspace != currentWorkspace_) {
        currentWorkspace_ = record->workspace;
        sidebar_->setWorkspace(currentWorkspace_);
    }
    if (!record->isEssential && record->workspace >= 0 && record->workspace < workspaceNames_.size())
        lastWorkspaceTab_[record->workspace] = id;

    const int previousPrimary = currentTabId_;
    if (splitEnabled_ && id == secondaryTabId_) {
        secondaryTabId_ = previousPrimary;
    }
    currentTabId_ = id;
    activePanelId_ = id;
    findBar_->finish();
    if (settingsVisible_) leaveInternalPages();
    tabManager_->activateTab(id, !(record->url == kNewTabUrl && record->historyState.isEmpty() && !record->view));
    sidebar_->selectTab(id);
    applyViewLayout();
    scheduleSessionSave();
}

void MainWindow::cycleTabs(const int direction)
{
    if (!tabManager_ || direction == 0)
        return;

    const auto order = tabManager_->orderedTabs(currentWorkspace_);
    if (order.isEmpty())
        return;

    int index = order.indexOf(currentTabId_);
    if (index < 0)
        index = direction > 0 ? 0 : order.size() - 1;
    else
        index = (index + (direction > 0 ? 1 : -1) + order.size()) % order.size();
    activateTab(order.at(index));
}

void MainWindow::closeCurrentTab()
{
    const auto *record = tabManager_->tab(currentTabId_);
    if (!record)
        return;
    if (!record->isEssential) {
        closeTab(currentTabId_);
        return;
    }

    const int id = currentTabId_;
    unloadedTabId_ = id;
    tabManager_->discardTab(id);
    int next = -1;
    for (const int candidate : tabManager_->tabsInWorkspace(currentWorkspace_, false)) {
        if (candidate != id) {
            next = candidate;
            break;
        }
    }
    if (next >= 0) {
        activateTab(next);
        return;
    }
    splitView_->showEmptyState();
    tabManager_->setVisibleTabs({});
    sidebar_->selectTab(id);
    updateChrome();
    scheduleSessionSave();
}

void MainWindow::closeTab(const int id)
{
    if (closingTab_ || !tabManager_->contains(id)) {
        return;
    }
    closingTab_ = true;
    if (unloadedTabId_ == id)
        unloadedTabId_ = -1;

    for (auto it = lastWorkspaceTab_.begin(); it != lastWorkspaceTab_.end(); ++it)
        if (it.value() == id) it.value() = -1;

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
        if (const auto *record = tabManager_->tab(currentTabId_))
        tabManager_->activateTab(currentTabId_, !(record->url == kNewTabUrl && record->historyState.isEmpty() && !record->view));
        sidebar_->selectTab(currentTabId_);
    }

    if (!tabManager_->contains(secondaryTabId_)) {
        secondaryTabId_ = -1;
        splitEnabled_ = false;
    }
    applyViewLayout();
    scheduleSessionSave();
    closingTab_ = false;
}

void MainWindow::switchWorkspace(const int workspace)
{
    if (workspace < 0 || workspace >= workspaceNames_.size() || workspace == currentWorkspace_)
        return;

    if (settingsVisible_) leaveInternalPages();
    currentWorkspace_ = workspace;
    sidebar_->setWorkspace(workspace);
    splitEnabled_ = false;
    secondaryTabId_ = -1;

    int nextId = lastWorkspaceTab_.value(workspace, -1);
    const auto *remembered = tabManager_->tab(nextId);
    if (!remembered || remembered->isEssential || remembered->workspace != workspace)
        nextId = -1;
    if (nextId < 0) {
        const auto tabs = tabManager_->tabsInWorkspace(workspace, false);
        nextId = tabs.isEmpty() ? createTab(kNewTabUrl, workspace, false) : tabs.constFirst();
    }

    currentTabId_ = nextId;
    if (const auto *record = tabManager_->tab(currentTabId_))
        tabManager_->activateTab(currentTabId_, !(record->url == kNewTabUrl && record->historyState.isEmpty() && !record->view));
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

    applyViewLayout();
}

void MainWindow::applyViewLayout()
{
    if (currentTabId_ >= 0 && currentTabId_ == unloadedTabId_) {
        splitView_->showEmptyState();
        tabManager_->setVisibleTabs({});
        updateChrome();
        return;
    }
    auto *primary = tabManager_->tab(currentTabId_);
    if (primary && !splitEnabled_ && primary->url == kNewTabUrl && primary->historyState.isEmpty() && !primary->view) {
        splitView_->showStartPage(workspaceNames_.value(currentWorkspace_), searchTemplate_);
        tabManager_->setVisibleTabs({});
        updateChrome();
        return;
    }
    WebView *primaryView = tabManager_->ensureView(currentTabId_);
    if (!primary || !primaryView) {
        splitView_->showEmptyState();
        tabManager_->setVisibleTabs({});
        updateChrome();
        return;
    }

    QSet<int> visible {currentTabId_};
    if (splitEnabled_) {
        auto *secondary = tabManager_->tab(secondaryTabId_);
        WebView *secondaryView = tabManager_->ensureView(secondaryTabId_);
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
    if (compactMode_)
        updateShellGeometry();
    if (primary)
        tabManager_->startLoad(*primary);
    if (splitEnabled_) {
        if (auto *secondary = tabManager_->tab(secondaryTabId_))
            tabManager_->startLoad(*secondary);
    }
    if (primaryView)
        QTimer::singleShot(0, primaryView, [primaryView] { primaryView->syncSurfaceSize(); });
    tabManager_->setVisibleTabs(visible);
    if (sidebar_)
        sidebar_->raise();
    if (urlBar_ && urlBar_->isVisible())
        urlBar_->raise();
    updateChrome();
}

void MainWindow::updateChrome()
{
    if (settingsVisible_) {
        urlBar_->setUrl({});
        urlBar_->setPageAvailable(false);
        urlBar_->setNavigationState(true, false);
        urlBar_->setLoading(false);
        setWindowTitle(contentStack_->currentWidget()->windowTitle()
            + (privateMode_ ? tr(" · Zenith · Privado") : tr(" · Zenith")));
        return;
    }
    const auto *record = tabManager_->tab(activeTabId());
    urlBar_->setPageAvailable(record && record->view);
    if (!record || !record->view) {
        setWindowTitle(privateMode_ ? tr("Zenith Browser · Privado") : tr("Zenith Browser"));
        urlBar_->setUrl({});
        urlBar_->setNavigationState(false, false);
        return;
    }

    const QUrl live = record->view->url();
    const QUrl displayed = live.isEmpty() ? record->url : live;
    urlBar_->setUrl(displayed, profile_->hasCertificateException(displayed));
    urlBar_->setNavigationState(record->view->canGoBack(), record->view->canGoForward());
    const QString title = record->title.trimmed().isEmpty() ? tr("Nova aba") : record->title;
    setWindowTitle(privateMode_ ? tr("%1 · Zenith · Privado").arg(title) : tr("%1 · Zenith").arg(title));
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
        (record->crashed ? tr("[Falhou] ") : record->muted ? tr("[Mudo] ") : record->view && record->view->isPlayingAudio() ? tr("[Áudio] ") : QString()) + record->title,
        record->icon.isNull() && record->url == kNewTabUrl ? QIcon(":/icons/zenith.png") : record->icon,
        record->isDiscarded,
        record->isEssential,
        int(tabManager_->allTabIds().indexOf(id)),
    };
    sidebar_->updateTab(visual);
    sidebar_->selectTab(currentTabId_);
}

void MainWindow::syncFolders()
{
    QList<Sidebar::FolderVisual> folders;
    for (int workspace = 0; workspace < workspaceNames_.size(); ++workspace) {
        for (const auto &folder : tabManager_->foldersInWorkspace(workspace))
            folders.append({folder.id, folder.workspace, folder.name, int(folders.size())});
    }
    sidebar_->setFolders(folders);
}

void MainWindow::setCompactMode(const bool enabled)
{
    compactMode_ = enabled;
    compactSidebarVisible_ = enabled && sidebar_->underMouse();
    compactUrlBarVisible_ = enabled && urlBar_->underMouse();
    sidebarSlideGuard_ = true;
    sidebarSlide_->stop();
    if (urlBarSlide_)
        urlBarSlide_->stop();
    sidebarSlideGuard_ = false;
    sidebar_->setSlideOffset(0);
    if (enabled) {
        sidebarReveal_ = compactSidebarVisible_ ? sidebar_->preferredWidth() : 0;
        urlBarReveal_ = compactUrlBarVisible_ ? UiMetrics::Toolbar : 0;
    } else {
        sidebarReveal_ = sidebar_->preferredWidth();
        urlBarReveal_ = UiMetrics::Toolbar;
        urlBar_->setMinimumHeight(0);
        urlBar_->setMaximumHeight(UiMetrics::Toolbar);
        urlBar_->setFixedHeight(UiMetrics::Toolbar);
    }
    urlBar_->setCompactEnabled(enabled);
    updateShellGeometry();
    QTimer::singleShot(0, this, [this] { syncPageSurfaces(); });
    scheduleSessionSave();
}

void MainWindow::showAddressBar(const bool focus)
{
    if (!urlBar_)
        return;
    if (!compactMode_) {
        urlBar_->show();
        if (focus)
            revealAddressBar();
        return;
    }
    hideUrlBarTimer_->stop();
    compactUrlBarVisible_ = true;
    urlBar_->setVisible(true);
    if (urlBarSlide_) {
        urlBarSlide_->stop();
        urlBarSlide_->setStartValue(urlBarReveal_);
        urlBarSlide_->setEndValue(UiMetrics::Toolbar);
        if (urlBarSlide_->startValue() != urlBarSlide_->endValue())
            urlBarSlide_->start();
        else
            setUrlBarReveal(UiMetrics::Toolbar);
    } else {
        setUrlBarReveal(UiMetrics::Toolbar);
    }
    if (focus)
        revealAddressBar();
}

void MainWindow::revealAddressBar()
{
    if (!urlBar_)
        return;
    const int token = ++addressFocusToken_;
    if (!privateMode_) {
        profile_->data()->requestSuggestions(this, [this, token](const BrowserData::QueryResult &result) {
            if (token != addressFocusToken_ || !result.error.isEmpty()) return;
            QStringList suggestions;
            for (const auto &entry : result.entries) suggestions.append(entry.url);
            urlBar_->setSuggestions(suggestions);
        });
    }
    if (compactMode_)
        showAddressBar(false);
    setPageInputEnabled(false);
    WebView::setKeySink(
        [this](const int key, const Qt::KeyboardModifiers modifiers, const QString &text) {
            if (!urlBar_)
                return false;
            if (key == Qt::Key_Escape && !(modifiers & Qt::ControlModifier)) {
                WebView::setKeySink({});
                urlBar_->clearAddressFocus();
                ++addressFocusToken_;
                setPageInputEnabled(true);
                if (auto *view = currentView()) view->setFocus(Qt::OtherFocusReason);
                if (compactMode_)
                    hideUrlBarTimer_->start();
                return true;
            }
            urlBar_->injectKey(key, modifiers, text);
            return true;
        },
        [this] {
            ++addressFocusToken_;
            setPageInputEnabled(true);
            if (urlBar_)
                urlBar_->clearAddressFocus();
            if (compactMode_)
                hideUrlBarTimer_->start();
        });
    activateWindow();
    urlBar_->focusAddress();
    const auto focusAgain = [this, token] {
        if (token != addressFocusToken_ || !urlBar_)
            return;
        setPageInputEnabled(false);
        activateWindow();
        urlBar_->focusAddress();
    };
    QTimer::singleShot(0, this, focusAgain);
    QTimer::singleShot(80, this, focusAgain);
}

int MainWindow::sidebarReveal() const
{
    return sidebarReveal_;
}

void MainWindow::setSidebarReveal(const int value)
{
    const int full = sidebar_ ? sidebar_->preferredWidth() : 236;
    sidebarReveal_ = qBound(0, value, full);
    if (compactMode_)
        updateShellGeometry();
}

int MainWindow::urlBarReveal() const
{
    return urlBarReveal_;
}

void MainWindow::setUrlBarReveal(const int value)
{
    urlBarReveal_ = qBound(0, value, UiMetrics::Toolbar);
    if (compactMode_)
        updateShellGeometry();
}

void MainWindow::syncPageSurfaces()
{
    if (!tabManager_)
        return;
    const auto sync = [](const int id, TabManager *manager) {
        if (auto *record = manager->tab(id); record && record->view)
            record->view->syncSurfaceSize();
    };
    sync(currentTabId_, tabManager_);
    sync(secondaryTabId_, tabManager_);
}

void MainWindow::setCompactSidebarShown(const bool shown)
{
    if (!compactMode_ || !sidebar_ || !sidebarSlide_ || !centralSurface_)
        return;
    compactSidebarVisible_ = shown;
    sidebar_->setSlideOffset(0);
    sidebarSlideGuard_ = true;
    sidebarSlide_->stop();
    sidebarSlideGuard_ = false;
    sidebarSlide_->setStartValue(sidebarReveal_);
    sidebarSlide_->setEndValue(shown ? sidebar_->preferredWidth() : 0);
    if (sidebarSlide_->startValue() == sidebarSlide_->endValue()) {
        setSidebarReveal(shown ? sidebar_->preferredWidth() : 0);
        return;
    }
    sidebarSlide_->start();
}

void MainWindow::updateShellGeometry()
{
    if (!centralSurface_ || !contentWidget_ || !sidebar_)
        return;
    const int width = centralSurface_->width();
    const int height = centralSurface_->height();
    const int sidebarWidth = sidebar_->preferredWidth();

    if (auto *contentLayout = qobject_cast<QVBoxLayout *>(contentWidget_->layout());
        contentLayout && contentLayout->indexOf(urlBar_) < 0) {
        urlBar_->setParent(contentWidget_);
        contentLayout->insertWidget(0, urlBar_);
    }

    if (compactMode_) {
        const int barHeight = qBound(0, urlBarReveal_, UiMetrics::Toolbar);
        const int shown = qBound(0, sidebarReveal_, sidebarWidth);
        const int contentX = qMax(shown, 12);
        const int contentY = barHeight > 0 ? 0 : 12;
        urlBar_->setMinimumHeight(0);
        urlBar_->setMaximumHeight(UiMetrics::Toolbar);
        urlBar_->setFixedHeight(qMax(1, barHeight));
        urlBar_->setVisible(barHeight > 0);
        sidebar_->setMinimumWidth(0);
        sidebar_->setMaximumWidth(sidebarWidth);
        sidebar_->setVisible(shown > 0);
        if (shown > 0) {
            sidebar_->setSlideOffset(0);
            sidebar_->setGeometry(0, 0, shown, height);
            sidebar_->raise();
        }
        contentWidget_->setGeometry(contentX, contentY,
                                    qMax(0, width - contentX),
                                    qMax(0, height - contentY));
        revealStrip_->setGeometry(0, 0, 12, height);
        revealStrip_->setVisible(shown < 12);
        revealStrip_->raise();
        topRevealStrip_->setGeometry(contentX, 0, qMax(0, width - contentX), 12);
        topRevealStrip_->setVisible(barHeight == 0);
        topRevealStrip_->raise();
    } else {
        sidebar_->setAttribute(Qt::WA_NativeWindow, false);
        urlBar_->setAttribute(Qt::WA_NativeWindow, false);
        urlBar_->setMinimumHeight(0);
        urlBar_->setMaximumHeight(UiMetrics::Toolbar);
        urlBar_->setFixedHeight(UiMetrics::Toolbar);
        if (auto *contentLayout = qobject_cast<QVBoxLayout *>(contentWidget_->layout());
            contentLayout && contentLayout->indexOf(urlBar_) < 0) {
            urlBar_->setParent(contentWidget_);
            contentLayout->insertWidget(0, urlBar_);
        }
        urlBar_->show();
        sidebar_->setMinimumWidth(0);
        sidebar_->setMaximumWidth(sidebarWidth);
        sidebar_->show();
        revealStrip_->hide();
        topRevealStrip_->hide();
        sidebar_->setGeometry(0, 0, sidebarWidth, height);
        contentWidget_->setGeometry(sidebarWidth, 0, std::max(0, width - sidebarWidth), height);
        sidebar_->raise();
        QTimer::singleShot(0, this, [this] { syncPageSurfaces(); });
    }
}

void MainWindow::setSearchTemplate(const QString &searchTemplate)
{
    if (!searchTemplate.contains(QStringLiteral("%1")))
        return;
    searchTemplate_ = searchTemplate;
    urlBar_->setSearchTemplate(searchTemplate_);
    scheduleSessionSave();
}

void MainWindow::scheduleSessionSave()
{
    if (!restoringSession_ && sessionSaveTimer_ && !sessionSaveTimer_->isActive())
        sessionSaveTimer_->start();
}

void MainWindow::showBrowserMenu(const QPoint &globalPosition)
{
    const auto *view = settingsVisible_ ? nullptr : currentView();
    BrowserMenu::State state;
    state.hasPage = view != nullptr;
    state.hasSite = view && Navigation::isWebUrl(view->url());
    state.privateMode = privateMode_;
    state.compact = compactMode_;
    state.split = splitEnabled_;
    state.fullscreen = isFullScreen();
    state.zoom = view ? qRound(view->zoom() * 100) : 100;
    BrowserMenu menu(state, this);
    connect(&menu, &BrowserMenu::commandRequested, this, [this](int command) {
        QTimer::singleShot(0, this, [this, command] { executeCommand(command); });
    });
    menu.exec(globalPosition);
}

void MainWindow::showCredentialManager()
{
    if (privateMode_) {
        statusBar()->showMessage(tr("O cofre de credenciais não é acessado nesta janela privada."), 7000);
        return;
    }
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    QDialog &dialog = *page;
    dialog.setWindowTitle(tr("Senhas e credenciais"));
    dialog.resize(520, 340);

    auto *layout = new QVBoxLayout(&dialog);
    PageWidgets::heading(layout, tr("Senhas e credenciais"),
        tr("Cofre do perfil %1. Selecione uma credencial para removê-la.")
            .arg(qApp->property("zenithProfile").toString().isEmpty() ? tr("padrão") : qApp->property("zenithProfile").toString()));
    auto *list = new QListWidget(&dialog);
    list->setObjectName("detailList");
    list->setItemDelegate(new DetailListDelegate(list));
    list->setAlternatingRowColors(true);
    for (const auto &entry : credentialManager_->entries()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(entry.origin, entry.username), list);
        item->setData(Qt::UserRole, entry.origin);
        item->setData(Qt::UserRole + 1, entry.username);
    }
    auto *empty = PageWidgets::emptyState(&dialog, tr("Nenhuma credencial salva"), tr("Entre em um site HTTPS e aceite salvar o login para encontrá-lo aqui."));
    empty->setVisible(list->count() == 0);
    list->setVisible(list->count() > 0);
    layout->addWidget(empty, 1);
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *remove = buttons->addButton(tr("Remover selecionada"),
                                      QDialogButtonBox::DestructiveRole);
    remove->setEnabled(false);
    connect(list, &QListWidget::itemSelectionChanged, &dialog, [list, remove] {
        remove->setEnabled(list->currentItem() != nullptr);
    });
    connect(remove, &QPushButton::clicked, &dialog, [this, list, empty] {
        auto *item = list->currentItem();
        if (!item)
            return;
        QString error;
        if (credentialManager_->remove(item->data(Qt::UserRole).toString(), &error, item->data(Qt::UserRole + 1).toString())) {
            delete list->takeItem(list->row(item));
            empty->setVisible(list->count() == 0);
            list->setVisible(list->count() > 0);
        } else {
            QMessageBox::warning(this, tr("Credencial não removida"), error);
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    presentInternalPage(page);
}

void MainWindow::showSettings()
{
    WebView::setKeySink({});
    ++addressFocusToken_;
    urlBar_->clearAddressFocus();
    findBar_->finish();
    settingsPage_->setWindowTitle(tr("Configurações"));
    setPageInputEnabled(false);
    settingsPage_->refreshPreferenceControls();
    settingsPage_->setWorkspaces(workspaceNames_, currentWorkspace_);
    settingsPage_->setSearchTemplate(searchTemplate_);
    settingsPage_->setCompactMode(compactMode_);
    settingsPage_->setAdBlockEnabled(adBlocker_->isEnabled());
    settingsPage_->setSuspendOnMinimize(suspendOnMinimize_);
    settingsPage_->setRestoreSession(restoreSessionEnabled_);
    settingsPage_->setAdBlockStatus(adBlocker_->blockedCount(),
                                    adBlocker_->ruleCount(),
                                    adBlockUpdater_->statusText());
    internalNavigation_->open(settingsPage_);
    updateChrome();
}

void MainWindow::closeSettings()
{
    internalNavigation_->back();
}

void MainWindow::leaveInternalPages()
{
    internalNavigation_->clear();
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

void MainWindow::navigateTo(const QUrl &url)
{
    if (settingsVisible_) leaveInternalPages();
    auto *record = tabManager_->tab(activeTabId());
    if (record) record->url = url;
    if (record && !record->view)
        tabManager_->ensureView(activeTabId());
    if (record && record->view) {
        record->loadStarted = true;
        record->view->setUrl(url);
        applyViewLayout();
        record->view->setFocus(Qt::OtherFocusReason);
    }
    WebView::setKeySink({});
    urlBar_->clearAddressFocus();
    ++addressFocusToken_;
    setPageInputEnabled(true);
    if (record && record->view)
        record->view->setFocus(Qt::OtherFocusReason);
    if (compactMode_)
        hideUrlBarTimer_->start();
}

void MainWindow::presentInternalPage(QDialog *page)
{
    WebView::setKeySink({});
    ++addressFocusToken_;
    urlBar_->clearAddressFocus();
    findBar_->finish();
    setPageInputEnabled(false);
    for (auto *button : page->findChildren<QPushButton *>()) button->setAutoDefault(false);
    if (auto *buttons = page->findChild<QDialogButtonBox *>()) {
        if (auto *close = buttons->button(QDialogButtonBox::Close)) {
            close->setText(tr("Voltar"));
            close->setIcon(zenithIcon(ZenithIcon::Back));
        }

    }
    internalNavigation_->open(page);
    updateChrome();
}
