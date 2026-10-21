#include "ui/SettingsPage.h"
#include "ui/BrowserMenu.h"
#include "ui/ThemeManager.h"
#include "core/AdBlockRuleParser.h"
#include "core/AdBlockInterceptor.h"
#include "core/AdBlockUpdater.h"
#include "core/CredentialManager.h"
#include "core/WebProfile.h"
#include "core/WebView.h"
#include "core/TabManager.h"
#include "core/Navigation.h"
#include "core/BrowserData.h"
#include "core/SessionStore.h"
#include "core/DownloadManager.h"
#include "core/BookmarkCodec.h"
#include "ui/Sidebar.h"
#include "ui/UrlBar.h"
#include "ui/TabItemWidget.h"

#include <QApplication>
#include <cmath>
#include <QPalette>
#include <QTabBar>
#include <QMenu>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QSettings>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QLineEdit>
#include <QComboBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QPersistentModelIndex>
#include <QListWidget>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#undef signals
#undef slots
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#define signals Q_SIGNALS
#define slots Q_SLOTS

#include <X11/Xlib.h>
#undef None

namespace {
bool graphicalTests = false;
}

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesNetworkRules();
    void matchesAdDomainsAndExceptions();
    void rejectsCrossOriginCredentials();
    void serializesWebHistory();
    void reloadsDiscardedTabsRepeatedly();
    void resolvesNavigation();
    void persistsBrowserData();
    void recoversAtomicSession();
    void managesLazyTabs();
    void sanitizesDownloads();
    void navigatesAddressBar();
    void switchesWorkspacesByKeyboard();
    void changesAppearanceAndCapturesThemes();
    void organizesBrowserCommands();
    void configuresWebRendering();
    void roundTripsBookmarks();
    void downloadsAfterClosingView();
    void isolatesPrivateProfile();
    void queriesLibraryAsynchronously();
    void selectsOutlookCompatibility();
    void isolatesOutlookSettings();
    void protectsUnsavedFormsDuringSuspension();
    void discardsOnlyInactivePages();
    void compilesNativeAdBlock();
    void blocksSubresourcesAndOutlookSlots();
    void queriesSuggestionsAsynchronously();
    void preservesRulesWhenUpdateFails();
    void rejectsCredentialsOnUntrustedTransport();
    void isolatesCredentialBridgeFromPage();
    void exportsBookmarksAsynchronously();
    void resolvesNumberedTabShortcuts();
    void keepsDownloadSelectionDuringRefresh();
    void restoresClosedTabPlacementAndAudio();
    void boundsClosedHistoryMemory();
    void batchesSidebarUpdates();
    void resumesFindAfterClosingSearch();
    void ordersTabsLikeSidebar();
};

void CoreTests::parsesNetworkRules()
{
    const auto rules = AdBlockRuleParser::parse(
        "||ads.example.com^\n"
        "||tracker.example.org^$third-party\n"
        "@@||allowed.example.com^\n"
        "@@||ads.example.com^$domain=example.org\n"
        "||login.example.com^$badfilter\n"
        "||assets.example.com^advertising\n"
        "example.com##.advert\n"
        "! comment\n");

    QCOMPARE(rules.blockedDomains,
             QStringList({QStringLiteral("ads.example.com")}));
    QCOMPARE(rules.allowedDomains,
             QStringList({QStringLiteral("allowed.example.com")}));

    const auto restored = AdBlockRuleParser::deserialize(
        AdBlockRuleParser::serialize(rules));
    QCOMPARE(restored.blockedDomains, rules.blockedDomains);
    QCOMPARE(restored.allowedDomains, rules.allowedDomains);

    const auto compiled = AdBlockRuleParser::compile(rules);
    const auto compiledRestored = AdBlockRuleParser::deserializeCompiled(
        AdBlockRuleParser::serializeCompiled(compiled));
    QCOMPARE(compiledRestored.blockedDomains, compiled.blockedDomains);
    QCOMPARE(compiledRestored.allowedDomains, compiled.allowedDomains);
}

void CoreTests::matchesAdDomainsAndExceptions()
{
    AdBlockInterceptor interceptor;
    interceptor.setRules({QStringLiteral("doubleclick.net"),
                          QStringLiteral("ads.example.com")},
                         {QStringLiteral("allowed.ads.example.com")});
    QVERIFY(interceptor.wouldBlock(
        QUrl(QStringLiteral("https://securepubads.g.doubleclick.net/script.js"))));
    QVERIFY(interceptor.wouldBlock(
        QUrl(QStringLiteral("https://cdn.example.org/ads/banner.js"))));
    QVERIFY(!interceptor.wouldBlock(
        QUrl(QStringLiteral("https://allowed.ads.example.com/content.js"))));
    QVERIFY(!interceptor.wouldBlock(
        QUrl(QStringLiteral("https://example.com/advertising")), true));
}

void CoreTests::compilesNativeAdBlock()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AdBlockInterceptor blocker;
    struct State {
        bool done = false;
        WebKitUserContentFilter *filter = nullptr;
        GError *error = nullptr;
        ~State() {
            if (filter) webkit_user_content_filter_unref(filter);
            if (error) g_error_free(error);
        }
    };
    auto state = std::make_shared<State>();
    auto *store = webkit_user_content_filter_store_new(QFile::encodeName(dir.path()).constData());
    const QByteArray json = blocker.contentFilter();
    GBytes *bytes = g_bytes_new(json.constData(), json.size());
    webkit_user_content_filter_store_save(store, "test-filter", bytes, nullptr,
        +[](GObject *source, GAsyncResult *result, gpointer user) {
            std::unique_ptr<std::shared_ptr<State>> state(static_cast<std::shared_ptr<State> *>(user));
            (*state)->filter = webkit_user_content_filter_store_save_finish(
                WEBKIT_USER_CONTENT_FILTER_STORE(source), result, &(*state)->error);
            (*state)->done = true;
        }, new std::shared_ptr<State>(state));
    g_bytes_unref(bytes);
    QElapsedTimer timeout;
    timeout.start();
    while (!state->done && timeout.elapsed() < 20000) {
        g_main_context_iteration(nullptr, FALSE);
        QTest::qWait(10);
    }
    g_object_unref(store);
    QVERIFY(state->done);
    QVERIFY2(!state->error, state->error ? state->error->message : "");
    QVERIFY(state->filter);
}

void CoreTests::blocksSubresourcesAndOutlookSlots()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QStringList paths;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            auto request = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, &server, [&, socket, request] {
                request->append(socket->readAll());
                if (!request->contains("\r\n\r\n")) return;
                const QByteArray path = request->split(' ').value(1);
                paths.append(QString::fromUtf8(path));
                const QByteArray body = path == "/banner.js"
                    ? "document.title='Ad loaded';" : "window.normalResourceLoaded=true;";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    AdBlockInterceptor blocker;
    blocker.setRules({"127.0.0.1"});
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    profile.setRequestBlocker(&blocker);
    WebView view(&profile);
    view.resize(900, 700);
    view.show();
    const QString html = QStringLiteral(
        "<title>Clean</title><div id=app>"
        "<div data-message-ad-id=paid id=ad>Advertisement</div>"
        "<div role=option id=message>Normal email</div></div>"
        "<script src='http://127.0.0.1:%1/banner.js'></script>"
        "<script src='http://localhost:%1/normal.js'></script>"
        "<script>addEventListener('load',()=>setTimeout(()=>{"
        "document.title=window.normalResourceLoaded && getComputedStyle(ad).display==='none'"
        " && getComputedStyle(message).display!=='none' ? 'Filtered' : 'Broken';},0));</script>")
        .arg(server.serverPort());
    QSignalSpy loaded(&view, &WebView::loadFinished);
    view.loadHtml(html, QUrl("http://outlook.live.com/"));
    QVERIFY(loaded.wait(20000));
    QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("Filtered"), 10000);
    QVERIFY(!paths.contains("/banner.js"));
    QVERIFY(paths.contains("/normal.js"));

    paths.clear();
    blocker.setEnabled(false);
    loaded.clear();
    view.loadHtml(html, QUrl("http://outlook.live.com/"));
    QVERIFY(loaded.wait(20000));
    QTRY_VERIFY_WITH_TIMEOUT(paths.contains("/banner.js"), 10000);

    blocker.setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!profile.contentFilterPending(), 20000);
    paths.clear();
    loaded.clear();
    view.loadHtml(html, QUrl("http://unrelated.example/"));
    QVERIFY(loaded.wait(20000));
    QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("Broken"), 10000);
    QVERIFY(!paths.contains("/banner.js"));
}

void CoreTests::queriesSuggestionsAsynchronously()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/suggestions.sqlite");
    QVERIFY(data.ready());
    for (int index = 0; index < 110; ++index)
        QVERIFY(data.visit(QUrl(QString("https://history%1.example/").arg(index)), "Visited"));
    QVERIFY(data.bookmark(QUrl("https://history0.example/"), "Favorite and visited"));
    QVERIFY(data.bookmark(QUrl("https://favorite.example/"), "Favorite"));
    bool returned = false;
    bool received = false;
    data.requestSuggestions(this, [&](const BrowserData::QueryResult &result) {
        received = true;
        QVERIFY(returned);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.entries.size(), 100);
        QSet<QString> urls;
        for (const auto &entry : result.entries) urls.insert(entry.url);
        QCOMPARE(urls.size(), 100);
        const QSet<QString> favorites{result.entries[0].url, result.entries[1].url};
        QVERIFY(favorites.contains("https://history0.example/"));
        QVERIFY(favorites.contains("https://favorite.example/"));
    });
    returned = true;
    QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
}

void CoreTests::preservesRulesWhenUpdateFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile first(dir.path() + "/first.txt");
    QVERIFY(first.open(QIODevice::WriteOnly));
    const QByteArray valid = "[Adblock Plus 2.0]\n||test-ad.example^\n";
    QCOMPARE(first.write(valid), valid.size());
    first.close();
    QFile second(dir.path() + "/second.txt");
    AdBlockInterceptor blocker;
    AdBlockUpdater updater(&blocker, nullptr,
        {QUrl::fromLocalFile(first.fileName()), QUrl::fromLocalFile(second.fileName())});
    blocker.setRules({"keep.example"});
    updater.updateNow();
    QTRY_VERIFY_WITH_TIMEOUT(!updater.isUpdating(), 10000);
    QVERIFY(blocker.wouldBlock(QUrl("https://keep.example/script.js")));
    QVERIFY(!blocker.wouldBlock(QUrl("https://test-ad.example/script.js")));
    QVERIFY(second.open(QIODevice::WriteOnly));
    second.write("<html>Error page</html>");
    second.close();
    updater.updateNow();
    QTRY_VERIFY_WITH_TIMEOUT(!updater.isUpdating(), 10000);
    QVERIFY(blocker.wouldBlock(QUrl("https://keep.example/script.js")));
    QVERIFY(second.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(second.write(valid), valid.size());
    second.close();
    updater.updateNow();
    updater.updateNow();
    QTRY_VERIFY_WITH_TIMEOUT(!updater.isUpdating(), 10000);
    QVERIFY(blocker.wouldBlock(QUrl("https://test-ad.example/script.js")));
    QVERIFY(blocker.wouldBlock(QUrl("https://ads.msn.com/banner.js")));
}

void CoreTests::rejectsCredentialsOnUntrustedTransport()
{
    QVERIFY(Navigation::canUseCredentials(QUrl("https://example.org/login"), false));
    QVERIFY(!Navigation::canUseCredentials(QUrl("https://example.org/login"), true));
    QVERIFY(!Navigation::canUseCredentials(QUrl("http://example.org/login"), false));
    QVERIFY(!Navigation::canUseCredentials(QUrl("file:///tmp/login.html"), false));
    QVERIFY(!Navigation::canUseCredentials(QUrl("https:///login"), false));
}

void CoreTests::isolatesCredentialBridgeFromPage()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    WebView view(&profile);
    view.resize(900, 700);
    view.show();
    int calls = 0;
    view.setMessageHandler([&](const QString &message) {
        if (message == "isolated-ready") ++calls;
        return QString();
    });
    view.installPageScript("window.webkit.messageHandlers.credentials.postMessage('isolated-ready');");
    QSignalSpy loaded(&view, &WebView::loadFinished);
    view.loadHtml(R"HTML(<title>Waiting</title><script>
        const exposed = window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.credentials;
        document.title = exposed ? 'Exposed' : 'Isolated';
        </script><iframe srcdoc="<script>
        if(window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.credentials)
            parent.document.title='Exposed';
        </script>"></iframe>)HTML", QUrl("https://example.org/"));
    QVERIFY(loaded.wait(20000));
    QTRY_COMPARE_WITH_TIMEOUT(calls, 1, 10000);
    QCOMPARE(view.title(), QString("Isolated"));
}

void CoreTests::exportsBookmarksAsynchronously()
{
    QTemporaryDir dir;
    const QString databasePath = dir.path() + "/bookmarks.sqlite";
    BrowserData data(databasePath);
    QVERIFY(data.ready());
    QList<BrowserData::Entry> source;
    for (int index = 0; index < 1200; ++index)
        source.append({QString("https://bookmark%1.example/").arg(index), QString("Title < %1 & more").arg(index),
            index % 2 ? QString("Work & <Projects>") : QString()});
    bool ok = false;
    QCOMPARE(data.importBookmarks(source, &ok).size(), source.size());
    QVERIFY(ok);
    const QString destination = dir.path() + "/bookmarks.html";
    bool received = false;
    bool returned = false;
    data.requestBookmarkExport(destination, this, [&](const QString &error) {
        received = true;
        QVERIFY(returned);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    });
    returned = true;
    QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
    QFile exported(destination);
    QVERIFY(exported.open(QIODevice::ReadOnly));
    const auto decoded = BookmarkCodec::decode(exported.readAll());
    QCOMPARE(decoded.size(), source.size());
    int grouped = 0;
    for (const auto &entry : decoded) {
        QVERIFY(entry.title.startsWith("Title < "));
        QVERIFY(entry.title.endsWith(" & more"));
        QVERIFY(entry.folder.isEmpty() || entry.folder == "Work & <Projects>");
        if (!entry.folder.isEmpty()) ++grouped;
    }
    QCOMPARE(grouped, 600);
    received = false;
    data.requestBookmarkExport(databasePath, this, [&](const QString &error) {
        received = true;
        QVERIFY(!error.isEmpty());
    });
    QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
    BrowserData reader(databasePath, true);
    QVERIFY(reader.ready());
    QCOMPARE(reader.entries(true).size(), 100);
}

void CoreTests::resolvesNumberedTabShortcuts()
{
    for (int key = Qt::Key_1; key <= Qt::Key_8; ++key) {
        QCOMPARE(Navigation::command(key, Qt::ControlModifier), Navigation::SelectTab1 + key - Qt::Key_1);
        QCOMPARE(Navigation::command(key, Qt::NoModifier), int(Navigation::None));
        QCOMPARE(Navigation::command(key, Qt::ControlModifier | Qt::AltModifier), int(Navigation::None));
    }
    QCOMPARE(Navigation::command(Qt::Key_9, Qt::ControlModifier), int(Navigation::SelectLastTab));
    QCOMPARE(Navigation::command(Qt::Key_0, Qt::ControlModifier), int(Navigation::ZoomReset));
    QCOMPARE(Navigation::command(Qt::Key_R, Qt::ControlModifier | Qt::ShiftModifier), int(Navigation::HardReload));
    QCOMPARE(Navigation::command(Qt::Key_F5, Qt::ControlModifier), int(Navigation::HardReload));
    QCOMPARE(Navigation::command(Qt::Key_F3, Qt::NoModifier), int(Navigation::FindNext));
    QCOMPARE(Navigation::command(Qt::Key_F3, Qt::ShiftModifier), int(Navigation::FindPrevious));
    QCOMPARE(Navigation::command(Qt::Key_G, Qt::ControlModifier), int(Navigation::FindNext));
    QCOMPARE(Navigation::command(Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier), int(Navigation::FindPrevious));
}

void CoreTests::keepsDownloadSelectionDuringRefresh()
{
    QTemporaryDir dir;
    QFile history(dir.path() + "/downloads.json");
    QVERIFY(history.open(QIODevice::WriteOnly));
    QJsonArray records;
    for (int index = 0; index < 3; ++index)
        records.append(QJsonObject{{"path", dir.path() + QString("/file%1.zip").arg(index)}, {"completed", true}});
    history.write(QJsonDocument(records).toJson());
    history.close();
    DownloadManager manager(nullptr, nullptr, history.fileName());
    bool retained = false;
    QTimer::singleShot(0, &manager, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) return;
        if (auto *list = dialog->findChild<QListWidget *>(); list && list->count() == 3) {
            list->setCurrentRow(1);
            QPersistentModelIndex selected(list->currentIndex());
            manager.changed();
            retained = selected.isValid() && list->currentIndex() == selected;
        }
        dialog->reject();
    });
    manager.show(nullptr);
    QVERIFY(retained);
}

void CoreTests::restoresClosedTabPlacementAndAudio()
{
    QWidget host;
    TabManager manager(nullptr, nullptr);
    const int folder = manager.createFolder(0, "Work");
    const int a = manager.createTab(QUrl("https://a.example/"), 0, &host, false, -1, true);
    const int b = manager.createTab(QUrl("https://b.example/"), 0, &host, false, folder, true);
    const int c = manager.createTab(QUrl("https://c.example/"), 0, &host, false, -1, true);
    manager.tab(b)->muted = true;
    manager.closeTab(b);
    const int restored = manager.reopenClosedTab(&host);
    QCOMPARE(manager.allTabIds(), QList<int>({a, restored, c}));
    QVERIFY(manager.tab(restored)->muted);
    QCOMPARE(manager.tab(restored)->folderId, folder);
    QVERIFY(!manager.tab(restored)->view);
    manager.closeTab(restored);
    manager.removeFolder(folder);
    const int root = manager.reopenClosedTab(&host);
    QCOMPARE(manager.tab(root)->folderId, -1);
    QVERIFY(manager.tab(root)->muted);
    QCOMPARE(manager.allTabIds(), QList<int>({a, root, c}));
}

void CoreTests::boundsClosedHistoryMemory()
{
    QWidget host;
    TabManager manager(nullptr, nullptr);
    for (int index = 0; index < 12; ++index) {
        const int id = manager.createTab(QUrl(QString("https://history%1.example/").arg(index)), 0, &host, false, -1, true);
        manager.tab(id)->historyState = QByteArray(1024 * 1024, 'h');
        manager.closeTab(id);
    }
    qsizetype total = 0;
    int histories = 0;
    for (int index = 11; index >= 0; --index) {
        const int restored = manager.reopenClosedTab(&host);
        QVERIFY(restored >= 0);
        QCOMPARE(manager.tab(restored)->url, QUrl(QString("https://history%1.example/").arg(index)));
        total += manager.tab(restored)->historyState.size();
        if (!manager.tab(restored)->historyState.isEmpty()) ++histories;
    }
    QCOMPARE(total, qsizetype(8 * 1024 * 1024));
    QCOMPARE(histories, 8);
    QCOMPARE(manager.reopenClosedTab(&host), -1);
    const int large = manager.createTab(QUrl("https://large.example/"), 0, &host, false, -1, true);
    manager.tab(large)->historyState = QByteArray(2 * 1024 * 1024, 'h');
    manager.closeTab(large);
    const int restored = manager.reopenClosedTab(&host);
    QVERIFY(manager.tab(restored)->historyState.isEmpty());
    QCOMPARE(manager.tab(restored)->url, QUrl("https://large.example/"));
}

void CoreTests::batchesSidebarUpdates()
{
    Sidebar sidebar;
    Sidebar::TabVisual initial;
    initial.id = 0; initial.workspace = 0; initial.title = "Initial";
    sidebar.addTab(initial);
    QTRY_COMPARE(sidebar.findChildren<TabItemWidget *>().size(), 1);
    const QPointer<TabItemWidget> original = sidebar.findChildren<TabItemWidget *>().first();
    for (int id = 1; id <= 50; ++id) {
        Sidebar::TabVisual visual;
        visual.id = id; visual.workspace = 0; visual.title = QString("Tab %1").arg(id); visual.position = id;
        sidebar.updateTab(visual);
    }
    QVERIFY(original);
    QCOMPARE(sidebar.findChildren<TabItemWidget *>().size(), 1);
    QTRY_COMPARE(sidebar.findChildren<TabItemWidget *>().size(), 51);
    QVERIFY(!original);
    for (int id = 0; id <= 50; ++id) sidebar.removeTab(id);
    QCOMPARE(sidebar.findChildren<TabItemWidget *>().size(), 51);
    QTRY_VERIFY(sidebar.findChildren<TabItemWidget *>().isEmpty());
}

void CoreTests::resumesFindAfterClosingSearch()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    WebView view(&profile);
    view.resize(900, 700);
    view.show();
    QSignalSpy loaded(&view, &WebView::loadFinished);
    view.loadHtml("<p>needle first needle second</p>", QUrl("https://example.org/"));
    QVERIFY(loaded.wait(20000));
    QSignalSpy matches(&view, &WebView::findResult);
    view.find("needle");
    QVERIFY(matches.wait(10000));
    QCOMPARE(matches.last().at(0).toInt(), 2);
    view.finishFind();
    QCOMPARE(view.lastFindText(), QString("needle"));
    matches.clear();
    view.find(view.lastFindText());
    QVERIFY(matches.wait(10000));
    QCOMPARE(matches.last().at(0).toInt(), 2);
}

void CoreTests::ordersTabsLikeSidebar()
{
    QWidget host;
    TabManager manager(nullptr, nullptr);
    const int firstFolder = manager.createFolder(0, "First");
    const int secondFolder = manager.createFolder(0, "Second");
    const int root = manager.createTab(QUrl("https://root.example"), 0, &host, false, -1, true);
    const int second = manager.createTab(QUrl("https://second.example"), 0, &host, false, secondFolder, true);
    const int first = manager.createTab(QUrl("https://first.example"), 0, &host, false, firstFolder, true);
    const int pinned = manager.createTab(QUrl("https://pinned.example"), 1, &host, true, -1, true);
    manager.createTab(QUrl("https://other.example"), 1, &host, false, -1, true);
    QCOMPARE(manager.orderedTabs(0), QList<int>({pinned, first, second, root}));
    Sidebar sidebar;
    const auto setFolders = [&] {
        QList<Sidebar::FolderVisual> folders;
        for (const auto &folder : manager.foldersInWorkspace(0))
            folders.append({folder.id, 0, folder.name, int(folders.size())});
        sidebar.setFolders(folders);
    };
    setFolders();
    for (int id : manager.allTabIds()) {
        const auto *tab = manager.tab(id);
        Sidebar::TabVisual visual;
        visual.id = id; visual.workspace = tab->workspace; visual.folderId = tab->folderId;
        visual.title = tab->url.host(); visual.essential = tab->isEssential;
        visual.position = manager.allTabIds().indexOf(id);
        sidebar.addTab(visual);
    }
    const auto visibleOrder = [&] {
        QList<int> ids;
        for (auto *row : sidebar.findChildren<TabItemWidget *>()) ids.append(row->tabId());
        return ids;
    };
    QTRY_COMPARE(visibleOrder(), QList<int>({first, second, root}));
    manager.moveFolder(secondFolder, -1);
    setFolders();
    QCOMPARE(manager.orderedTabs(0), QList<int>({pinned, second, first, root}));
    QTRY_COMPARE(visibleOrder(), QList<int>({second, first, root}));
}

void CoreTests::rejectsCrossOriginCredentials()
{
    QVERIFY(CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("https://example.com/login")),
        QUrl(QStringLiteral("https://example.com"))));
    QVERIFY(!CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("https://example.com")),
        QUrl(QStringLiteral("https://evil.example"))));
    QVERIFY(!CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("http://example.com")),
        QUrl(QStringLiteral("http://example.com"))));
    QVERIFY(CredentialManager::isSameHttpsOrigin(QUrl("https://example.com:443/a"), QUrl("https://example.com")));
    QVERIFY(!CredentialManager::isSameHttpsOrigin(QUrl("https://example.com:8443"), QUrl("https://example.com")));
}

void CoreTests::serializesWebHistory()
{
    if (!graphicalTests)
        QSKIP("WebKit integration requires an X11 display; CI sets ZENITH_REQUIRE_DISPLAY=1");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    WebProfile profile(dir.path() + QStringLiteral("/data"), dir.path() + QStringLiteral("/cache"));

    WebView source(&profile);
    source.resize(900, 700);
    source.show();
    QVERIFY(QTest::qWaitForWindowExposed(&source, 10000));
    QVERIFY(!source.restoreSessionState(QByteArrayLiteral("not-a-session")));

    const QString pagePath = dir.path() + QStringLiteral("/page.html");
    QFile page(pagePath);
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write("<title>History test</title><p>ok</p>");
    page.close();
    const QUrl pageUrl = QUrl::fromLocalFile(pagePath);

    QSignalSpy loaded(&source, &WebView::loadFinished);
    source.setUrl(pageUrl);
    QVERIFY(loaded.wait(20000));
    QCOMPARE(source.url(), pageUrl);

    const QString secondPath = dir.path() + QStringLiteral("/second.html");
    QFile second(secondPath);
    QVERIFY(second.open(QIODevice::WriteOnly));
    second.write("<title>Second</title>");
    second.close();
    const QUrl secondUrl = QUrl::fromLocalFile(secondPath);
    loaded.clear();
    source.setUrl(secondUrl);
    QVERIFY(loaded.wait(20000));
    loaded.clear();
    source.back();
    QVERIFY(loaded.wait(20000));
    QCOMPARE(source.url(), pageUrl);
    QVERIFY(source.canGoForward());

    const QByteArray state = source.sessionState();
    QVERIFY(!state.isEmpty());

    WebView restored(&profile);
    QSignalSpy restoredLoad(&restored, &WebView::loadFinished);
    QVERIFY(restored.restoreSessionState(state));
    if (restored.url().isEmpty())
        restored.setUrl(pageUrl);
    restored.resize(900, 700);
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored, 10000));
    if (restored.url() != pageUrl && restoredLoad.isEmpty())
        QVERIFY(restoredLoad.wait(20000));
    QCOMPARE(restored.url(), pageUrl);
    QVERIFY(restored.canGoForward());
    restoredLoad.clear();
    restored.forward();
    QVERIFY(restoredLoad.wait(20000));
    QCOMPARE(restored.url(), secondUrl);
}

void CoreTests::reloadsDiscardedTabsRepeatedly()
{
    if (!graphicalTests)
        QSKIP("WebKit integration requires an X11 display; CI sets ZENITH_REQUIRE_DISPLAY=1");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    WebProfile profile(dir.path() + QStringLiteral("/data"), dir.path() + QStringLiteral("/cache"));
    QWidget host;
    host.resize(900, 700);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host, 10000));
    TabManager manager(&profile, nullptr);
    const QString path = dir.path() + QStringLiteral("/restore.html");
    QFile page(path);
    QVERIFY(page.open(QIODevice::WriteOnly));
    page.write("<title>Discard recovery</title><p>restored</p>");
    page.close();
    const QUrl url = QUrl::fromLocalFile(path);
    const int id = manager.createTab(url, 0, &host);
    for (int cycle = 0; cycle < 3; ++cycle) {
        auto *record = manager.tab(id);
        auto *view = manager.ensureView(id);
        QVERIFY(view);
        view->resize(900, 700);
        view->show();
        QSignalSpy loaded(view, &WebView::loadFinished);
        manager.startLoad(*record);
        if (loaded.isEmpty())
            QVERIFY(loaded.wait(20000));
        QCOMPARE(loaded.last().at(0).toBool(), true);
        QCOMPARE(view->url(), url);
        QCOMPARE(view->title(), QStringLiteral("Discard recovery"));
        QVERIFY(!manager.serializedHistory(id).isEmpty());
        manager.discardTab(id);
        QVERIFY(record->isDiscarded);
        QVERIFY(!record->view);
        QVERIFY(!record->loadStarted);
        QVERIFY(!record->historyApplied);
        QCOMPARE(record->url, url);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    const int lazyId = manager.createTab(url, 0, &host, false, -1, true);
    QVERIFY(manager.ensureView(lazyId));
    manager.discardTab(lazyId);
    QCOMPARE(manager.tab(lazyId)->url, url);
}

void CoreTests::protectsUnsavedFormsDuringSuspension()
{
    if (!graphicalTests) QSKIP("WebKit integration requires an X11 display");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    QCOMPARE(webkit_web_context_get_cache_model(profile.context()), WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER);
    WebView view(&profile);
    view.resize(900, 700);
    view.show();
    QVERIFY(!webkit_settings_get_enable_page_cache(profile.settings()));
    const QList<QPair<QString, bool>> cases = {
        {"<input type=search><textarea></textarea><select><option>A</option><option>B</option></select>", true},
        {"<input id=f><script>f.value='unsaved'</script>", false},
        {"<textarea id=f></textarea><script>f.value='draft'</script>", false},
        {"<input id=f type=checkbox><script>f.checked=true</script>", false},
        {"<select id=f><option>A</option><option>B</option></select><script>f.selectedIndex=1</script>", false},
        {"<input type=password value=secret>", false},
        {"<div contenteditable>draft</div>", false},
        {"<iframe srcdoc='draft'></iframe>", false}
    };
    for (const auto &[html, expected] : cases) {
        QSignalSpy loaded(&view, &WebView::loadFinished);
        view.loadHtml(html, QUrl("https://example.org/"));
        QVERIFY(loaded.wait(20000));
        bool finished = false;
        bool allowed = !expected;
        view.checkCanSuspend([&](bool result) { allowed = result; finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QCOMPARE(allowed, expected);
    }
}

void CoreTests::discardsOnlyInactivePages()
{
    if (!graphicalTests) QSKIP("WebKit integration requires an X11 display");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    QWidget host;
    TabManager manager(&profile, nullptr);
    QList<int> ids;
    for (int index = 0; index < 4; ++index) {
        const int id = manager.createTab(QUrl("https://example.org/"), 0, &host);
        ids.append(id);
        auto *view = manager.ensureView(id);
        QSignalSpy loaded(view, &WebView::loadFinished);
        view->loadHtml("<input type=search><p>Unchanged page</p>", QUrl("https://example.org/"));
        QVERIFY(loaded.wait(20000));
        manager.tab(id)->lastAccess = QDateTime::currentDateTimeUtc().addSecs(-300 + index * 10);
    }
    manager.setVisibleTabs({ids.last()});
    manager.suspendInactive(30, 3);
    manager.suspendInactive(30, 3);
    const auto liveCount = [&] {
        int live = 0;
        for (int id : ids) if (manager.tab(id)->view) ++live;
        return live;
    };
    QTRY_COMPARE_WITH_TIMEOUT(liveCount(), 3, 10000);
    QVERIFY(manager.tab(ids.last())->view);
    manager.suspendInactive(180);
    QTRY_COMPARE_WITH_TIMEOUT(liveCount(), 1, 10000);
    QVERIFY(manager.tab(ids.last())->view);
}

void CoreTests::resolvesNavigation()
{
    const QString search = QStringLiteral("https://duckduckgo.com/?q=%1");
    QCOMPARE(Navigation::fromInput("localhost:8080/test", search), QUrl("http://localhost:8080/test"));
    QCOMPARE(Navigation::fromInput("[::1]:9000/", search), QUrl("http://[::1]:9000/"));
    QCOMPARE(Navigation::fromInput("example.org:8443/test", search), QUrl("https://example.org:8443/test"));
    QVERIFY(Navigation::fromInput("javascript:alert(1)", search).isEmpty());
    QVERIFY(Navigation::fromInput("data:text/html,hello", search).isEmpty());
    QCOMPARE(Navigation::origin(QUrl("https://user:secret@EXAMPLE.org:443/a?q=1")), QString("https://example.org"));
    QVERIFY(Navigation::fromInput("search words", search).toString().contains("q=search words"));
}

void CoreTests::persistsBrowserData()
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/browser.sqlite";
    const QUrl url("https://example.org/page");
    {
        BrowserData data(path);
        QVERIFY2(data.ready(), qPrintable(data.error()));
        QVERIFY(data.visit(url, "Example"));
        QVERIFY(!data.visit(QUrl("file:///private"), "Private"));
        QVERIFY(data.bookmark(url, "Saved", "Work"));
        QVERIFY(data.bookmark(url, "Updated", "Work"));
        QCOMPARE(data.entries(true).size(), 1);
        QVERIFY(data.setPermission(url, "notifications", -1));
        QCOMPARE(data.permission(QUrl("https://example.org:443/other"), "notifications"), -1);
        QCOMPARE(data.permission(QUrl("http://example.org"), "notifications"), 0);
        QVERIFY(data.setZoom(url, 9));
    }
    BrowserData restored(path);
    QCOMPARE(restored.entries(true, "updated").size(), 1);
    QCOMPARE(restored.entries(false).size(), 1);
    QCOMPARE(restored.zoom(url), 5.0);
    QVERIFY(restored.clearHistory());
    QVERIFY(restored.entries(false).isEmpty());
    QCOMPARE(restored.entries(true).size(), 1);
    QVERIFY(restored.clearPermissions());
    QCOMPARE(restored.permission(url, "notifications"), 0);
}

void CoreTests::recoversAtomicSession()
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/session.json";
    SessionStore store(path);
    const QVariantMap first{{"tabs", QVariantList{QVariantMap{{"url", "https://example.org"}}}}, {"active", 0}};
    QVERIFY(store.save(first));
    auto second = first;
    second["active"] = 1;
    QVERIFY(store.save(second));
    QCOMPARE(store.load().value("active").toInt(), 1);
    QFile corrupted(path);
    QVERIFY(corrupted.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corrupted.write("{truncated");
    corrupted.close();
    QCOMPARE(store.load().value("active").toInt(), 0);
    QVERIFY(store.save(second));
    QCOMPARE(store.load().value("active").toInt(), 1);
    SessionStore unwritable(dir.path() + "/missing/session.json");
    QVERIFY(!unwritable.save(first));
    QCOMPARE(store.load().value("active").toInt(), 1);
}

void CoreTests::managesLazyTabs()
{
    // A lazy tab must never create a GTK view, even in a headless unit test.
    TabManager manager(nullptr, nullptr);
    QWidget host;
    manager.setWorkspaceCount(4);
    const int a = manager.createTab(QUrl("https://a.example"), 2, &host, false, -1, true);
    const int b = manager.createTab(QUrl("https://b.example"), 2, &host, false, -1, true);
    const int c = manager.createTab(QUrl("https://c.example"), 2, &host, false, -1, true);
    QVERIFY(!manager.tab(a)->view);
    manager.moveTab(a, 1);
    QCOMPARE(manager.allTabIds(), QList<int>({b, a, c}));
    manager.moveTabBefore(c, b);
    QCOMPARE(manager.allTabIds(), QList<int>({c, b, a}));
    const int folder = manager.createFolder(2, "Work");
    QVERIFY(folder > 0);
    const int otherFolder = manager.createFolder(2, "Research");
    manager.moveFolder(otherFolder, -1);
    QCOMPARE(manager.foldersInWorkspace(2).first().id, otherFolder);
    manager.moveFolderBefore(folder, otherFolder);
    QCOMPARE(manager.foldersInWorkspace(2).first().id, folder);
    manager.setTabFolder(a, folder);
    manager.setTabWorkspace(a, 3);
    QCOMPARE(manager.tab(a)->folderId, -1);
    QCOMPARE(manager.tabsInWorkspace(3, false), QList<int>({a}));
    manager.tab(b)->historyState = QByteArrayLiteral("saved-history");
    manager.closeTab(b);
    QVERIFY(!manager.contains(b));
    const int restored = manager.reopenClosedTab(&host);
    QCOMPARE(manager.tab(restored)->url, QUrl("https://b.example"));
    QCOMPARE(manager.tab(restored)->historyState, QByteArrayLiteral("saved-history"));
    QVERIFY(!manager.tab(restored)->view);
}

void CoreTests::sanitizesDownloads()
{
    QCOMPARE(DownloadManager::safeName("../../secrets.txt"), QString("secrets.txt"));
    QCOMPARE(DownloadManager::safeName("C:\\folder\\file.zip"), QString("file.zip"));
    QCOMPARE(DownloadManager::safeName(".."), QString("download"));
    QCOMPARE(DownloadManager::safeName("\nname"), QString("_name"));
    QCOMPARE(DownloadManager::safeName(QString::fromUtf8("report\u202Efdp.exe")), QString("report_fdp.exe"));
    QCOMPARE(DownloadManager::safeName(QString::fromUtf8("report\u2028name.pdf")), QString("report_name.pdf"));
}

void CoreTests::navigatesAddressBar()
{
    UrlBar bar;
    bar.setUrl(QUrl("https://trusted.example:secret@actual.example/"));
    QCOMPARE(bar.findChild<QLineEdit *>("addressInput")->text(), QString("actual.example"));
    bar.resize(800, 58);
    bar.show();
    auto *input = bar.findChild<QLineEdit *>();
    QVERIFY(input);
    QSignalSpy navigation(&bar, &UrlBar::navigateRequested);
    input->setText("localhost:8080/a");
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigation.size(), 1);
    QCOMPARE(navigation.first().first().toUrl(), QUrl("http://localhost:8080/a"));
    input->setText("javascript:alert(1)");
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigation.size(), 1);
    bar.setSuggestions({"https://example.org"});
    QVERIFY(input->completer());
}

void CoreTests::switchesWorkspacesByKeyboard()
{
    Sidebar sidebar;
    sidebar.resize(sidebar.preferredWidth(), 700);
    sidebar.setWorkspaces({"Pessoal", "Trabalho", "Pesquisa"});
    auto *selector = sidebar.findChild<QTabBar *>("workspaceTabs");
    QVERIFY(selector);
    QCOMPARE(selector->count(), 3);
    sidebar.setWorkspace(0);
    QSignalSpy selected(&sidebar, &Sidebar::workspaceSelected);
    QTest::keyClick(selector, Qt::Key_Right);
    QCOMPARE(selector->currentIndex(), 1);
    QCOMPARE(selected.size(), 1);
    sidebar.setWorkspace(1);
    Sidebar::TabVisual first;
    first.id = 10; first.workspace = 1; first.title = "Example"; first.position = 0;
    sidebar.addTab(first);
    QTRY_COMPARE(sidebar.findChildren<TabItemWidget *>().size(), 1);
    auto rows = sidebar.findChildren<TabItemWidget *>();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first()->accessibleName(), QString("Example"));
    QSignalSpy activated(&sidebar, &Sidebar::tabActivated);
    QTest::keyClick(rows.first(), Qt::Key_Return);
    QCOMPARE(activated.size(), 1);
    sidebar.show();
    QCoreApplication::processEvents();
    if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
        QVERIFY(sidebar.grab().save("/tmp/zenith-sidebar.png"));
}

void CoreTests::changesAppearanceAndCapturesThemes()
{
    auto &theme = ThemeManager::instance();
    const QString originalMode = theme.mode(), originalAccent = theme.accent();
    QWidget window;
    auto *layout = new QHBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new Sidebar(&window);
    const QStringList spaces{"Pessoal", "Trabalho", "Pesquisa", "Estudos"};
    sidebar->setWorkspaces(spaces);
    QSignalSpy create(sidebar, &Sidebar::createWorkspaceRequested);
    auto *add = sidebar->findChild<QToolButton *>("createWorkspaceButton");
    QVERIFY(add);
    add->click();
    QCOMPARE(create.size(), 1);
    QStringList maximum;
    for (int i = 0; i < 16; ++i) maximum.append(QString::number(i));
    sidebar->setWorkspaces(maximum);
    QVERIFY(!add->isEnabled());
    sidebar->setWorkspaces(spaces);
    QVERIFY(add->isEnabled());
    Sidebar::TabVisual sample;
    sample.id = 100; sample.workspace = 0; sample.title = "Página de teste · ativa";
    sidebar->addTab(sample);
    sample.id = 101; sample.position = 1; sample.title = "Página de teste · suspensa"; sample.discarded = true;
    sidebar->addTab(sample);
    QTRY_COMPARE(sidebar->findChildren<TabItemWidget *>().size(), 2);
    sidebar->selectTab(100);
    auto *right = new QWidget(&window);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(new UrlBar(right));
    auto *settings = new SettingsPage(right);
    settings->setWorkspaces(spaces, 0);
    rightLayout->addWidget(settings);
    layout->addWidget(sidebar);
    layout->addWidget(right, 1);
    auto *navigation = settings->findChild<QListWidget *>("settingsNavigation");
    QVERIFY(navigation);
    navigation->setCurrentRow(1);
    auto *mode = settings->findChild<QComboBox *>("browserTheme");
    auto *accent = settings->findChild<QComboBox *>("browserAccent");
    QVERIFY(mode && accent);
    window.resize(1280, 800);
    window.show();
    const auto luminance = [](QColor c) {
        const auto channel = [](double x) { return x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4); };
        return .2126 * channel(c.redF()) + .7152 * channel(c.greenF()) + .0722 * channel(c.blueF());
    };
    for (const QString name : {QStringLiteral("dark"), QStringLiteral("light")}) {
        mode->setCurrentIndex(mode->findData(name));
        QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, mode->currentIndex())));
        QCOMPARE(theme.mode(), name);
        QCOMPARE(QSettings().value("appearance/theme").toString(), name);
        const double field = luminance(theme.color("#202023")), border = luminance(theme.color("#3f3f46"));
        QVERIFY((qMax(field, border) + .05) / (qMin(field, border) + .05) >= 3.0);
        for (const QString background : {QStringLiteral("#121214"), QStringLiteral("#18181b"), QStringLiteral("#27272a")}) {
            const double bg = luminance(theme.color(background)), text = luminance(theme.color("#a1a1aa"));
            QVERIFY((qMax(bg, text) + .05) / (qMin(bg, text) + .05) >= 4.5);
        }
        for (const QString color : {QStringLiteral("violet"), QStringLiteral("blue"), QStringLiteral("teal"), QStringLiteral("amber")}) {
            accent->setCurrentIndex(accent->findData(color));
            QVERIFY(QMetaObject::invokeMethod(accent, "activated", Q_ARG(int, accent->currentIndex())));
            QCOMPARE(theme.accent(), color);
            const double bg = luminance(theme.color("#202023")), fg = luminance(theme.color("#8b7cf6"));
            QVERIFY((qMax(bg, fg) + .05) / (qMin(bg, fg) + .05) >= 4.5);
        }
        theme.setAccent("violet");
        accent->setCurrentIndex(accent->findData("violet"));
        QCoreApplication::processEvents();
        QCOMPARE(window.palette().color(QPalette::Window), theme.color("#121214"));
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
            QVERIFY(window.grab().save("/tmp/zenith-" + name + ".png"));
        window.resize(760, 540);
        QCoreApplication::processEvents();
        QVERIFY(mode->width() > 80);
        QVERIFY(mode->mapTo(&window, mode->rect().bottomRight()).x() < window.width());
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
            QVERIFY(window.grab().save("/tmp/zenith-" + name + "-narrow.png"));
        window.resize(1280, 800);
    }
    theme.setMode(originalMode);
    theme.setAccent(originalAccent);
}

void CoreTests::organizesBrowserCommands()
{
    QCOMPARE(Navigation::command(Qt::Key_Comma, Qt::ControlModifier), int(Navigation::Settings));
    QCOMPARE(Navigation::command(Qt::Key_F10, Qt::NoModifier), int(Navigation::BrowserMenu));
    QCOMPARE(Navigation::command(Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier), int(Navigation::NewPrivateWindow));
    UrlBar bar;
    QSignalSpy sites(&bar, &UrlBar::siteControlsRequested);
    QSignalSpy bookmarks(&bar, &UrlBar::bookmarkRequested);
    QSignalSpy downloads(&bar, &UrlBar::downloadsRequested);
    QSignalSpy compact(&bar, &UrlBar::compactToggled);
    auto *site = bar.findChild<QToolButton *>("securityIndicator");
    auto *bookmark = bar.findChild<QToolButton *>("bookmarkButton");
    QVERIFY(site && bookmark);
    bar.setUrl(QUrl("about:blank"));
    QVERIFY(!site->isEnabled() && !bookmark->isEnabled());
    bar.setUrl(QUrl("https://example.org"));
    site->click(); bookmark->click();
    bar.findChild<QToolButton *>("downloadsButton")->click();
    bar.findChild<QToolButton *>("compactButton")->click();
    QCOMPARE(sites.size(), 1); QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(downloads.size(), 1); QCOMPARE(compact.size(), 1);
    bar.setPageAvailable(false);
    QVERIFY(!site->isEnabled() && !bookmark->isEnabled());
    QVERIFY(bar.findChild<QToolButton *>("downloadsButton")->isEnabled());
    bar.setPageAvailable(true);
    QVERIFY(site->isEnabled() && bookmark->isEnabled());
    SettingsPage preferences;
    preferences.setSearchTemplate("https://search.example/?q=%1");
    auto *search = preferences.findChild<QComboBox *>("searchEngine");
    QVERIFY(search);
    QCOMPARE(search->currentData().toString(), QString("https://search.example/?q=%1"));
    BrowserMenu::State state;
    state.hasPage = true; state.hasSite = true; state.compact = true; state.split = true; state.zoom = 125;
    BrowserMenu menu(state);
    const auto action = [](QMenu &menu, int command) {
        return menu.findChild<QAction *>("command-" + QString::number(command));
    };
    QSet<int> commands;
    for (auto *entry : menu.findChildren<QAction *>()) {
        if (!entry->data().isValid()) continue;
        QVERIFY(!commands.contains(entry->data().toInt()));
        commands.insert(entry->data().toInt());
    }
    QVERIFY(action(menu, Navigation::Settings));
    QVERIFY(action(menu, Navigation::Settings)->parent() == &menu);
    QVERIFY(action(menu, Navigation::ToggleCompact)->isChecked());
    QVERIFY(action(menu, Navigation::ToggleSplit)->isChecked());
    QVERIFY(action(menu, Navigation::Settings)->isEnabled());
    QVERIFY(action(menu, Navigation::Print)->parent() != &menu);
    QVERIFY(action(menu, Navigation::ImportBookmarks)->parent() == action(menu, Navigation::Bookmarks)->parent());
    QSignalSpy requested(&menu, &BrowserMenu::commandRequested);
    const QString original = ThemeManager::instance().mode();
    for (const QString theme : {QStringLiteral("dark"), QStringLiteral("light")}) {
        ThemeManager::instance().setMode(theme);
        menu.popup(QPoint(40, 40));
        QCoreApplication::processEvents();
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) QVERIFY(menu.grab().save("/tmp/zenith-menu-" + theme + ".png"));
        menu.hide();
    }
    menu.popup(QPoint(40, 40));
    menu.setActiveAction(action(menu, Navigation::Settings));
    QTest::keyClick(&menu, Qt::Key_Return);
    QCOMPARE(requested.size(), 1);
    QCOMPARE(requested.first().first().toInt(), int(Navigation::Settings));
    ThemeManager::instance().setMode(original);
    state.privateMode = true; state.hasPage = false; state.hasSite = false;
    BrowserMenu privateMenu(state);
    for (int command : {Navigation::Credentials, Navigation::SaveSession, Navigation::DefaultProfile,
                        Navigation::SiteControls, Navigation::Bookmark, Navigation::Print})
        QVERIFY(!action(privateMenu, command)->isEnabled());
    Sidebar sidebar;
    sidebar.resize(sidebar.preferredWidth(), 540);
    sidebar.setWorkspaces({"Pessoal", "Trabalho", "Pesquisa"});
    sidebar.show();
    QCoreApplication::processEvents();
    auto *tabs = sidebar.findChild<QTabBar *>("workspaceTabs");
    QVERIFY(tabs);
    QCOMPARE(tabs->accessibleTabName(2), QString("Pesquisa"));
    QVERIFY(tabs->mapTo(&sidebar, QPoint()).y() > sidebar.height() / 2);
    for (auto *button : sidebar.findChildren<QToolButton *>()) QVERIFY(button->text() != QString("Configurações"));
    QSignalSpy rename(&sidebar, &Sidebar::workspaceActionRequested);
    QTimer::singleShot(0, &sidebar, [&] {
        auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!popup) return;
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) popup->grab().save("/tmp/zenith-workspace-menu.png");
        for (QAction *entry : popup->actions())
            if (entry->text() == "Renomear este espaço") { popup->setActiveAction(entry); QTest::keyClick(popup, Qt::Key_Return); return; }
        popup->close();
    });
    sidebar.findChild<QToolButton *>("workspaceMenuButton")->click();
    QCOMPARE(rename.size(), 1);
    QCOMPARE(rename.first().first().toInt(), 1);
    QSignalSpy newTab(&sidebar, &Sidebar::newTabRequested);
    auto *newTabButton = sidebar.findChild<QPushButton *>("newTabButton");
    QVERIFY(newTabButton);
    sidebar.setWorkspace(1);
    newTabButton->click();
    QCOMPARE(newTab.size(), 1);
    QStringList names;
    for (int index = 0; index < 16; ++index) names.append(QString("Espaço %1").arg(index));
    sidebar.setWorkspaces(names);
    sidebar.setWorkspace(15);
    QCoreApplication::processEvents();
    QCOMPARE(tabs->currentIndex(), 15);
    QVERIFY(tabs->rect().intersects(tabs->tabRect(15)));
}

void CoreTests::configuresWebRendering()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    auto &theme = ThemeManager::instance();
    const QString sites = theme.siteMode(), rendering = theme.textRendering();
    theme.setSiteMode("light");
    gboolean dark = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, nullptr);
    QVERIFY(!dark);
    theme.setSiteMode("dark");
    g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, nullptr);
    QVERIFY(dark);
    theme.setTextRendering("smooth");
    int antialias = 0;
    g_object_get(gtk_settings_get_default(), "gtk-xft-antialias", &antialias, nullptr);
    QCOMPARE(antialias, 1);
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    WebView view(&profile);
    view.resize(900, 700);
    view.show();
    QSignalSpy loaded(&view, &WebView::loadFinished);
    const QString document = R"HTML(<meta name="color-scheme" content="light dark">
        <style>body{color-scheme:light dark}input{font:inherit}</style>
        <input value="Texto de teste"><script>
        document.title = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
        </script>)HTML";
    theme.setSiteMode("light");
    view.loadHtml(document, QUrl("https://rendering.example/"));
    QVERIFY(loaded.wait(20000));
    QCOMPARE(view.title(), QString("light"));
    theme.setSiteMode("dark");
    loaded.clear();
    view.loadHtml(document, QUrl("https://rendering.example/"));
    QVERIFY(loaded.wait(20000));
    QCOMPARE(view.title(), QString("dark"));
    theme.setSiteMode(sites);
    theme.setTextRendering(rendering);
}

void CoreTests::roundTripsBookmarks()
{
    const QList<BrowserData::Entry> source{{"https://example.org/?a=1&b=2", "A < B", "Trabalho", 0},
        {"https://other.example", "Other", {}, 0}};
    const auto decoded = BookmarkCodec::decode(BookmarkCodec::encode(source));
    QCOMPARE(decoded.size(), 2);
    QCOMPARE(decoded.first().url, QString("https://example.org/?a=1&b=2"));
    QCOMPARE(decoded.first().title, QString("A < B"));
    QCOMPARE(decoded.first().folder, QString("Trabalho"));
    QVERIFY(BookmarkCodec::decode("<a href='javascript:alert(1)'>bad</a>").isEmpty());
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/browser.sqlite");
    QVERIFY(data.bookmark(QUrl(source.first().url), "Original"));
    bool ok = false;
    const auto added = data.importBookmarks(decoded, &ok);
    QVERIFY(ok);
    QCOMPARE(added.size(), 1);
    QCOMPARE(data.entries(true, "Original").size(), 1);
    QCOMPARE(data.entries(true).size(), 2);
}

void CoreTests::downloadsAfterClosingView()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QByteArray payload(8192, 'z');
    connect(&server, &QTcpServer::newConnection, &server, [&server, payload] {
        while (auto *socket = server.nextPendingConnection()) {
            auto request = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, request, payload] {
                request->append(socket->readAll());
                if (!request->contains("\r\n\r\n")) return;
                if (request->startsWith("GET /redirect "))
                    socket->write("HTTP/1.1 302 Found\r\nLocation: /file\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                else
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"../../unsafe.txt\"\r\nContent-Length: "
                        + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    const QString path = dir.path() + "/result.bin";
    profile.downloads()->setDestinationChooser([path](const QString &name) {
        if (name != "unsafe.txt") return QString();
        return path;
    });
    auto view = std::make_unique<WebView>(&profile);
    const QByteArray url = QStringLiteral("http://127.0.0.1:%1/redirect").arg(server.serverPort()).toUtf8();
    WebKitDownload *download = webkit_web_context_download_uri(profile.context(), url.constData());
    QVERIFY(download);
    view.reset();
    QTRY_VERIFY_WITH_TIMEOUT(!profile.downloads()->items().isEmpty(), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(profile.downloads()->items().first().finished, 20000);
    QVERIFY(!profile.downloads()->items().first().failed);
    QVERIFY(!profile.downloads()->items().first().native);
    QFile result(path);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), payload);
}

void CoreTests::isolatesPrivateProfile()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    {
        WebProfile profile(dir.path() + "/private", dir.path() + "/cache", nullptr, true);
        QVERIFY(webkit_website_data_manager_is_ephemeral(webkit_web_context_get_website_data_manager(profile.context())));
        QVERIFY(profile.data()->bookmark(QUrl("https://private.example"), "Private"));
        QVERIFY(!QFile::exists(dir.path() + "/private/browser.sqlite"));
    }
    WebProfile second(dir.path() + "/private", dir.path() + "/cache", nullptr, true);
    QVERIFY(second.data()->entries(true).isEmpty());
}

void CoreTests::queriesLibraryAsynchronously()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/data.sqlite");
    QVERIFY(data.bookmark(QUrl("https://async.example"), "Async"));
    bool received = false;
    data.requestEntries(true, "Async", 0, this, [&received](const BrowserData::QueryResult &result) {
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        const auto &entries = result.entries;
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().url, QString("https://async.example"));
        received = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
}

void CoreTests::selectsOutlookCompatibility()
{
    for (const QString &host : {QStringLiteral("outlook.live.com"), QStringLiteral("outlook.office.com"), QStringLiteral("outlook.office365.com")}) {
        const QString agent = Navigation::outlookUserAgent(QUrl("https://" + host + "/mail/"));
        QVERIFY(agent.contains("Macintosh"));
        QVERIFY(agent.contains("Version/18.5 Safari/605.1.15"));
    }
    for (const QString &url : {QStringLiteral("https://example.org"), QStringLiteral("https://outlook.live.com.evil.example"),
             QStringLiteral("https://eviloutlook.live.com"), QStringLiteral("https://outlook.live.com@evil.example"),
             QStringLiteral("http://outlook.live.com"), QStringLiteral("about:blank")})
        QVERIFY(Navigation::outlookUserAgent(QUrl(url)).isEmpty());
}

void CoreTests::isolatesOutlookSettings()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    WebView outlook(&profile);
    WebView ordinary(&profile);
    const QString original = ordinary.userAgent();
    outlook.loadHtml("<title>Compatibility</title>", QUrl("https://outlook.live.com/mail/"));
    QVERIFY(outlook.userAgent().contains("Macintosh"));
    QCOMPARE(ordinary.userAgent(), original);
    outlook.loadHtml("<title>Ordinary</title>", QUrl("https://example.org/"));
    QCOMPARE(outlook.userAgent(), original);
}

int main(int argc, char **argv)
{
    XInitThreads();
    qputenv("GDK_BACKEND", "x11");
    graphicalTests = ThemeManager::initializeGtk();
    if (!graphicalTests && qEnvironmentVariableIsSet("ZENITH_REQUIRE_DISPLAY")) {
        fprintf(stderr, "ZENITH_REQUIRE_DISPLAY: X11 unavailable\n");
        return 1;
    }
    qputenv("QT_QPA_PLATFORM", graphicalTests ? "xcb" : "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("ZenithTests");
    QCoreApplication::setApplicationName("CoreTests");
    ThemeManager::instance().apply();
    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "CoreTests.moc"
