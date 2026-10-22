#include "ui/SettingsPage.h"
#include "ProcessMetrics.h"
#include "ui/LibraryPage.h"
#include "ui/InternalPageStack.h"
#include "core/Preferences.h"
#include "ui/MainWindow.h"
#include "ui/FindBar.h"
#include <QSpinBox>
#include <QScopeGuard>
#include "ui/BrowserMenu.h"
#include "ui/StartPage.h"
#include "ui/PageWidgets.h"
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
#include <QAccessible>
#include <cmath>
#include <memory>
#include <QPalette>
#include <QTabBar>
#include <QMenu>
#include <QMainWindow>
#include <QStackedWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QCheckBox>
#include <QLabel>
#include <QStyle>
#include <QTranslator>
#include <QLibraryInfo>
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
#include <QKeySequence>
#include <QRegularExpression>
#include <QTextDocument>
#include <QLineEdit>
#include <QComboBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QPersistentModelIndex>
#include <QListWidget>
#include <QDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QClipboard>
#include <QCursor>
#include <dlfcn.h>

#undef signals
#undef slots
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <webkit2/webkit2.h>
#include "core/GtkClipboardGuard.h"
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
    void preservesInternalNavigation();
    void restoresOnlyCategoryPreferences();
    void migratesWorkspaceContents();
    void editsSelectedWorkspace();
    void appliesDensityPresets();
    void fitsWorkspaceHeadingToSidebar();
    void reportsMissingDownload();
    void keepsCustomAccentReadable();
    void preservesLibrarySelectionAcrossPages();
    void ignoresStaleLibrarySearch();
    void editsAndOpensLibraryEntries();
    void reportsLibraryReadFailure();
    void restoresInternalFocus();
    void findsCollapsedPreferenceAndRestoresPosition();
    void checksThemeRoleContrast();
    void exposesAccessibleTabActions();
    void inventoriesAndResetsPreferenceControls();
    void laysOutLibrariesAtLargeText();
    void confirmsFullHistoryClearWithActiveSearch();
    void treatsExternalTooltipContentAsText();
    void benchmarksBrowserWorkflows();
    void parsesNetworkRules();
    void matchesAdDomainsAndExceptions();
    void rejectsCrossOriginCredentials();
    void serializesWebHistory();
    void reloadsDiscardedTabsRepeatedly();
    void destroysProfilesWithPendingCookies();
    void resolvesNavigation();
    void persistsBrowserData();
    void recoversAtomicSession();
    void managesLazyTabs();
    void sanitizesDownloads();
    void navigatesAddressBar();
    void switchesWorkspacesByKeyboard();
    void changesAppearanceAndCapturesThemes();
    void organizesBrowserCommands();
    void usesNativeStartPage();
    void defersNativeBlankTabs();
    void laysOutInternalScreens();
    void searchesAndRefreshesPreferences();
    void customizesBrowserDimensions();
    void embedsFindBar();
    void compactSidebarTracksPointerAfterTabChanges();
    void embedsBrowserTools();
    void preservesAutomaticDownloadNames();
    void handlesFailedClipboardTargets();
    void defersWebViewKeySinkDispatch();
    void pastesIntoNativeInputs();
    void opensMiddleClickInBackground();
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

void CoreTests::preservesInternalNavigation()
{
    QStackedWidget host;
    auto *root = new QWidget;
    host.addWidget(root);
    InternalPageStack navigation(&host, root);
    auto *settings = new QWidget;
    settings->setObjectName("settings");
    auto *layout = new QVBoxLayout(settings);
    auto *query = new QLineEdit;
    layout->addWidget(query);
    navigation.keep(settings);
    navigation.open(settings);
    query->setText("texto preservado");
    auto *downloads = new QWidget;
    downloads->setObjectName("downloads");
    navigation.open(downloads);
    QCOMPARE(navigation.depth(), 2);
    navigation.back();
    QCOMPARE(host.currentWidget(), settings);
    QCOMPARE(query->text(), QString("texto preservado"));
    navigation.open(new QWidget);
    auto *duplicate = new QWidget;
    duplicate->setObjectName("settings");
    navigation.open(duplicate);
    QCOMPARE(navigation.depth(), 1);
    QCOMPARE(host.currentWidget(), settings);
    for (int i = 0; i < 20; ++i) navigation.open(new QWidget);
    QCOMPARE(navigation.depth(), 6);
    navigation.clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(host.count(), 2);
    QCOMPARE(host.currentWidget(), root);
    QVERIFY(!navigation.active());
}

void CoreTests::restoresOnlyCategoryPreferences()
{
    QSettings settings;
    QHash<QString, QVariant> saved;
    for (const auto &entry : Preferences::entries()) saved.insert(entry.key, settings.value(entry.key));
    const QString sentinel = "session/reset-test";
    const QVariant oldSentinel = settings.value(sentinel);
    const auto restore = qScopeGuard([&] {
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            if (it.value().isValid()) settings.setValue(it.key(), it.value()); else settings.remove(it.key());
        if (oldSentinel.isValid()) settings.setValue(sentinel, oldSentinel); else settings.remove(sentinel);
    });
    settings.setValue(sentinel, "dados preservados");
    settings.setValue("appearance/fontSize", 18);
    settings.setValue("privacy/savePasswords", false);
    Preferences::resetCategory("appearance");
    QCOMPARE(Preferences::value("appearance/fontSize").toInt(), 13);
    QVERIFY(!Preferences::value("privacy/savePasswords").toBool());
    QCOMPARE(settings.value(sentinel).toString(), QString("dados preservados"));
    Preferences::resetCategory("not-a-category");
    QCOMPARE(settings.value(sentinel).toString(), QString("dados preservados"));
}

void CoreTests::migratesWorkspaceContents()
{
    QWidget host;
    TabManager manager(nullptr, nullptr);
    manager.setWorkspaceCount(3);
    const int folder = manager.createFolder(1, "Pasta preservada");
    const int tab = manager.createTab(QUrl("https://example.org"), 1, &host, false, folder, true);
    const int closed = manager.createTab(QUrl("https://closed.example.org"), 1, &host, false, folder, true);
    manager.closeTab(closed);
    const int last = manager.createTab(QUrl("about:blank"), 2, &host, false, -1, true);
    QVERIFY(!manager.removeWorkspace(1));
    QVERIFY(!manager.removeWorkspace(1, 1));
    QVERIFY(manager.removeWorkspace(1, 2));
    QCOMPARE(manager.tab(tab)->workspace, 1);
    QCOMPARE(manager.tab(last)->workspace, 1);
    QCOMPARE(manager.foldersInWorkspace(1).size(), 1);
    QCOMPARE(manager.tab(tab)->folderId, folder);
    const int reopened = manager.reopenClosedTab(&host);
    QCOMPARE(manager.tab(reopened)->workspace, 1);
    QCOMPARE(manager.tab(reopened)->folderId, folder);
    QVERIFY(manager.removeWorkspace(0));
    QCOMPARE(manager.tab(tab)->workspace, 0);
    QVERIFY(!manager.removeWorkspace(0, 0));
    QVERIFY(!manager.tab(tab)->view);
}

void CoreTests::editsSelectedWorkspace()
{
    SettingsPage page;
    page.setWorkspaces({"Ativo", "Selecionado", "Outro"}, 0);
    page.resize(900, 700);
    page.show();
    auto *navigation = page.findChild<QListWidget *>("settingsNavigation");
    navigation->setCurrentRow(2);
    auto *spaces = page.findChild<QListWidget *>("workspaceList");
    spaces->setCurrentRow(1);
    for (auto *button : page.findChildren<QPushButton *>())
        if (button->text() == "Renomear selecionado") button->click();
    auto *name = page.findChild<QLineEdit *>("workspaceName");
    QVERIFY(name && name->isVisible());
    QCOMPARE(name->text(), QString("Selecionado"));
    QSignalSpy renamed(&page, &SettingsPage::workspaceNameRequested);
    name->setText("Renomeado");
    QTest::keyClick(name, Qt::Key_Return);
    QCOMPARE(renamed.size(), 1);
    QCOMPARE(renamed.first().at(2).toInt(), 1);
    QCOMPARE(renamed.first().at(1).toString(), QString("Renomeado"));
}

void CoreTests::appliesDensityPresets()
{
    QSettings settings;
    QHash<QString, QVariant> saved;
    const QStringList keys{"appearance/fontSize", "appearance/sidebarWidth", "appearance/tabHeight"};
    for (const auto &key : keys) saved.insert(key, settings.value(key));
    const auto restore = qScopeGuard([&] {
        for (const auto &key : keys)
            if (saved.value(key).isValid()) settings.setValue(key, saved.value(key)); else settings.remove(key);
        ThemeManager::instance().apply();
    });
    SettingsPage page;
    auto *preset = page.findChild<QComboBox *>("densityPreset");
    QVERIFY(preset);
    preset->setCurrentIndex(2);
    QVERIFY(QMetaObject::invokeMethod(preset, "activated", Q_ARG(int, 2)));
    QCOMPARE(settings.value(keys[0]).toInt(), 14);
    QCOMPARE(settings.value(keys[1]).toInt(), 260);
    QCOMPARE(settings.value(keys[2]).toInt(), 44);
    preset->setCurrentIndex(3);
    QVERIFY(QMetaObject::invokeMethod(preset, "activated", Q_ARG(int, 3)));
    QCOMPARE(settings.value(keys[1]).toInt(), 260);
    page.refreshPreferenceControls();
    QCOMPARE(preset->currentIndex(), 2);
}

void CoreTests::fitsWorkspaceHeadingToSidebar()
{
    QSettings settings;
    const auto old = settings.value("appearance/sidebarWidth");
    const auto restore = qScopeGuard([&] {
        if (old.isValid()) settings.setValue("appearance/sidebarWidth", old); else settings.remove("appearance/sidebarWidth");
        ThemeManager::instance().apply();
    });
    settings.setValue("appearance/sidebarWidth", 180);
    Sidebar sidebar;
    sidebar.setWorkspaces({"Espaço com nome longo e acentuação", "Trabalho"});
    sidebar.setWorkspace(0);
    sidebar.resize(180, 600);
    sidebar.show();
    QCoreApplication::processEvents();
    auto *heading = sidebar.findChild<QLabel *>("workspaceHeading");
    QVERIFY(heading);
    QVERIFY(heading->width() >= 72);
    const int narrow = heading->width();
    settings.setValue("appearance/sidebarWidth", 320);
    ThemeManager::instance().apply();
    QCoreApplication::processEvents();
    QVERIFY(heading->width() > narrow + 100);
    QCOMPARE(heading->text(), QString("Espaço com nome longo e acentuação"));
}

void CoreTests::reportsMissingDownload()
{
    QTemporaryDir dir;
    QFile history(dir.path() + "/downloads.json");
    QVERIFY(history.open(QIODevice::WriteOnly));
    history.write(QJsonDocument(QJsonArray{QJsonObject{{"path", dir.path() + "/removed.txt"}, {"completed", true}}}).toJson());
    history.close();
    DownloadManager manager(nullptr, nullptr, history.fileName());
    QWidget host;
    auto *page = manager.createPage(&host);
    host.show();
    page->show();
    auto *list = page->findChild<QListWidget *>("detailList");
    QCOMPARE(list->count(), 1);
    list->setCurrentRow(0);
    auto *open = page->findChild<QPushButton *>("openDownload");
    QVERIFY(open && open->isEnabled());
    open->click();
    bool reported = false;
    for (auto *label : page->findChildren<QLabel *>())
        if (label->isVisible() && label->text().contains("movido ou removido")) reported = true;
    QVERIFY(reported);
    QVERIFY(!QApplication::activeModalWidget());
}

void CoreTests::keepsCustomAccentReadable()
{
    auto &theme = ThemeManager::instance();
    const QString mode = theme.mode(), accent = theme.accent();
    const auto restore = qScopeGuard([&] { theme.setMode(mode); theme.setAccent(accent); });
    const auto luminance = [](QColor value) {
        const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(value.redF()) + 0.7152 * linear(value.greenF()) + 0.0722 * linear(value.blueF());
    };
    const auto contrast = [&](QColor first, QColor second) {
        const double a = luminance(first), b = luminance(second);
        return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
    };
    for (const QString mode : {QString("dark"), QString("light")}) {
        theme.setMode(mode);
        for (const QString accent : {QString("#00ff00"), QString("#0000ff"), QString("#ff0000"), QString("#ffff00"), QString("#000000"), QString("#ffffff")}) {
            theme.setAccent(accent);
            QVERIFY(contrast(theme.color("#8b7cf6"), theme.color("#27272a")) >= 4.5);
            QVERIFY(contrast(theme.color("#5b4fc4"), Qt::white) >= 4.5);
        }
    }
}

void CoreTests::preservesLibrarySelectionAcrossPages()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/library.sqlite");
    for (int i = 0; i < 160; ++i)
        QVERIFY(data.bookmark(QUrl(QString("https://example.org/%1").arg(i)), QString("Título %1").arg(i, 3, 10, QChar('0'))));
    LibraryPage page(&data, true);
    page.resize(760, 520);
    page.show();
    auto *list = page.findChild<QListWidget *>("detailList");
    auto *more = page.findChild<QPushButton *>("libraryMore");
    QTRY_COMPARE(list->count(), 100);
    QVERIFY(more->isEnabled());
    more->click();
    QTRY_COMPARE(list->count(), 160);
    list->setCurrentRow(130);
    list->scrollToItem(list->currentItem());
    const auto url = list->currentItem()->data(Qt::UserRole);
    const int scroll = list->verticalScrollBar()->value();
    page.refresh();
    QTRY_VERIFY(page.findChild<QAction *>("libraryForeground")->isEnabled());
    QCOMPARE(list->count(), 160);
    QCOMPARE(list->currentItem()->data(Qt::UserRole), url);
    QCOMPARE(list->verticalScrollBar()->value(), scroll);
    QVERIFY(!more->isEnabled());
}

void CoreTests::ignoresStaleLibrarySearch()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/library.sqlite");
    QVERIFY(data.bookmark(QUrl("https://one.example/"), "Primeiro"));
    QVERIFY(data.bookmark(QUrl("https://two.example/"), "Segundo"));
    LibraryPage page(&data, true);
    page.show();
    auto *list = page.findChild<QListWidget *>("detailList");
    auto *search = page.findChild<QLineEdit *>("librarySearch");
    QTRY_COMPARE(list->count(), 2);
    list->setCurrentRow(0);
    auto *open = page.findChild<QAction *>("libraryForeground");
    QVERIFY(open->isEnabled());
    search->setText("Primeiro");
    QVERIFY(!open->isEnabled());
    QTest::keyClick(search, Qt::Key_Return);
    search->setText("Segundo");
    QTest::keyClick(search, Qt::Key_Return);
    QTRY_COMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(), QString("https://two.example/"));
    search->setText("não existe");
    QTest::keyClick(search, Qt::Key_Return);
    QTRY_COMPARE(list->count(), 0);
    QVERIFY(page.findChild<QLabel *>("emptyState")->isVisible());
    QVERIFY(!open->isEnabled());
}

void CoreTests::editsAndOpensLibraryEntries()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/library.sqlite");
    const QUrl url("https://saved.example/");
    QVERIFY(data.bookmark(url, "Original", "Trabalho"));
    LibraryPage page(&data, true);
    page.show();
    auto *list = page.findChild<QListWidget *>("detailList");
    QTRY_COMPARE(list->count(), 1);
    list->setCurrentRow(0);
    QSignalSpy opened(&page, &LibraryPage::openRequested);
    page.findChild<QAction *>("libraryBackground")->trigger();
    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first().at(0).toUrl(), url);
    QVERIFY(!opened.first().at(1).toBool());
    QVERIFY(page.isVisible());
    page.findChild<QPushButton *>("libraryEdit")->click();
    auto *title = page.findChild<QLineEdit *>("bookmarkTitle");
    QVERIFY(title->isVisible());
    title->setText("Editado");
    QTest::keyClick(title, Qt::Key_Return);
    QTRY_VERIFY(list->item(0)->text().contains("Editado"));
    QCOMPARE(data.entries(true).first().title, QString("Editado"));
    QCOMPARE(data.entries(true).first().folder, QString("Trabalho"));
    page.findChild<QAction *>("libraryForeground")->trigger();
    QCOMPARE(opened.size(), 2);
    QVERIFY(opened.last().at(1).toBool());
    page.findChild<QPushButton *>("libraryRemove")->click();
    QTRY_COMPARE(list->count(), 0);
    QVERIFY(data.entries(true).isEmpty());
}

void CoreTests::reportsLibraryReadFailure()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/does-not-exist/library.sqlite");
    QVERIFY(!data.ready());
    LibraryPage page(&data, false);
    page.show();
    auto *retry = page.findChild<QPushButton *>("libraryRetry");
    QTRY_VERIFY(retry->isVisible());
    QVERIFY(page.findChild<QLabel *>("libraryStatus")->text().contains("Não foi possível"));
    QVERIFY(!page.findChild<QAction *>("libraryForeground")->isEnabled());
    QVERIFY(!QApplication::activeModalWidget());
}

void CoreTests::restoresInternalFocus()
{
    QStackedWidget host;
    auto *root = new QWidget;
    auto *rootLayout = new QVBoxLayout(root);
    auto *origin = new QLineEdit;
    rootLayout->addWidget(origin);
    host.addWidget(root);
    InternalPageStack navigation(&host, root);
    host.show();
    host.activateWindow();
    origin->setFocus(Qt::TabFocusReason);
    QTRY_VERIFY(origin->hasFocus());
    navigation.open(new QWidget);
    navigation.back();
    QTRY_VERIFY(origin->hasFocus());
    navigation.open(new QWidget);
    navigation.back();
    auto *next = new QWidget;
    auto *layout = new QVBoxLayout(next);
    auto *field = new QLineEdit;
    layout->addWidget(field);
    navigation.open(next);
    field->setFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(field->hasFocus());
}

void CoreTests::findsCollapsedPreferenceAndRestoresPosition()
{
    SettingsPage page;
    page.resize(760, 520);
    page.show();
    page.activateWindow();
    auto *navigation = page.findChild<QListWidget *>("settingsNavigation");
    navigation->setCurrentRow(1);
    QCoreApplication::processEvents();
    auto *stack = page.findChild<QStackedWidget *>();
    auto *scroll = qobject_cast<QScrollArea *>(stack->currentWidget());
    QVERIFY(scroll);
    scroll->verticalScrollBar()->setValue(200);
    const int position = scroll->verticalScrollBar()->value();
    auto *search = page.findChild<QLineEdit *>("settingsSearch");
    search->setText("zoom padrão");
    QTest::keyClick(search, Qt::Key_Return);
    auto *zoom = page.findChild<QSpinBox *>("web/defaultZoom");
    QTRY_VERIFY(zoom->hasFocus());
    search->clear();
    QTRY_COMPARE(scroll->verticalScrollBar()->value(), position);
    QCOMPARE(navigation->currentRow(), 1);
    search->setText("dark");
    QCOMPARE(navigation->currentRow(), 1);
    search->setText("busca personalizada");
    auto *toggle = page.findChild<QToolButton *>("customSearchToggle");
    QVERIFY(toggle->isChecked());
    QVERIFY(page.findChild<QLineEdit *>("customSearchTemplate")->isVisible());
    search->clear();
    QVERIFY(!toggle->isChecked());
    QCOMPARE(navigation->currentRow(), 1);
}

void CoreTests::checksThemeRoleContrast()
{
    auto &theme = ThemeManager::instance();
    const auto mode = theme.mode(), accent = theme.accent();
    const auto restore = qScopeGuard([&] { theme.setMode(mode); theme.setAccent(accent); });
    const auto luminance = [](QColor value) {
        const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(value.redF()) + 0.7152 * linear(value.greenF()) + 0.0722 * linear(value.blueF());
    };
    const auto contrast = [&](QColor first, QColor second) {
        const double a = luminance(first), b = luminance(second);
        return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
    };
    using Role = ThemeManager::Role;
    for (const QString mode : {QString("dark"), QString("light")}) {
        theme.setMode(mode);
        for (const QString accent : {QString("violet"), QString("blue"), QString("teal"), QString("amber"), QString("#00ff00")}) {
            theme.setAccent(accent);
            for (Role background : {Role::Content, Role::Sidebar, Role::Field, Role::Raised}) {
                for (Role foreground : {Role::Text, Role::Secondary, Role::Error})
                    QVERIFY2(contrast(theme.color(foreground), theme.color(background)) >= 4.5, qPrintable(mode + ' ' + accent));
                QVERIFY(contrast(theme.color(Role::Focus), theme.color(background)) >= 3.0);
            }
            QVERIFY(!qApp->styleSheet().contains(QRegularExpression("@[A-Za-z]+@")));
        }
    }
}

void CoreTests::exposesAccessibleTabActions()
{
    TabItemWidget tab(17);
    tab.setTitle("Pesquisa em português");
    auto *interface = QAccessible::queryAccessibleInterface(&tab);
    QVERIFY(interface);
    QCOMPARE(interface->role(), QAccessible::PageTab);
    QCOMPARE(interface->text(QAccessible::Name), QString("Pesquisa em português"));
    QVERIFY(interface->state().selectable);
    QVERIFY(!interface->state().selected);
    tab.setActive(true);
    QVERIFY(interface->state().selected);
    auto *actions = interface->actionInterface();
    QVERIFY(actions);
    QSignalSpy activated(&tab, &TabItemWidget::activated);
    QSignalSpy closed(&tab, &TabItemWidget::closeRequested);
    actions->doAction(QAccessibleActionInterface::pressAction());
    QTRY_COMPARE(activated.size(), 1);
    QCOMPARE(activated.first().first().toInt(), 17);
    QVERIFY(actions->actionNames().contains("close"));
    actions->doAction("close");
    QTRY_COMPARE(closed.size(), 1);
    tab.setEnabled(false);
    actions->doAction("close");
    QCoreApplication::processEvents();
    QCOMPARE(closed.size(), 1);
}

void CoreTests::benchmarksBrowserWorkflows()
{
    if (!graphicalTests) QSKIP("Browser benchmark requires X11");
    QSettings settings;
    QHash<QString, QVariant> saved;
    for (const auto &key : {QString("preferences/discardAfterSeconds"), QString("preferences/maxLiveTabs")}) {
        saved.insert(key, settings.value(key));
        settings.setValue(key, 0);
    }
    const auto restore = qScopeGuard([&] {
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            if (it.value().isValid()) settings.setValue(it.key(), it.value()); else settings.remove(it.key());
    });
    QTemporaryDir fixtures;
    QList<QUrl> urls;
    for (int i = 0; i < 5; ++i) {
        QFile file(fixtures.path() + QString("/page-%1.html").arg(i));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QString("<!doctype html><meta charset='utf-8'><title>Fixture %1</title><style>body{font:16px sans-serif;max-width:60em;margin:3em auto}</style><h1>Fixture %1</h1>").arg(i).toUtf8());
        for (int row = 0; row < 150; ++row)
            file.write(QString("<p>Parágrafo %1: leitura e renderização em português.</p>").arg(row).toUtf8());
        file.write("<svg width='640' height='240' xmlns='http://www.w3.org/2000/svg'><rect width='640' height='240' fill='#327c69'/><circle cx='320' cy='120' r='90' fill='#eee'/></svg><input placeholder='Formulário de teste'>");
        file.close();
        urls.append(QUrl::fromLocalFile(file.fileName()));
    }
    QJsonArray rounds;
    for (int round = 0; round < 3; ++round) {
        MainWindow window(true);
        window.show();
        window.activateWindow();
        auto *start = window.findChild<StartPage *>();
        QVERIFY(start);
        const auto command = [start](int value) {
            return QMetaObject::invokeMethod(start, "commandRequested", Q_ARG(int, value));
        };
        QTest::qWait(200);
        QJsonObject result{{"round", round}, {"initial", processTreeMemory()}};
        for (int i = 0; i < urls.size(); ++i) {
            window.openUrls({urls[i]});
            QTRY_VERIFY_WITH_TIMEOUT([&] {
                for (auto *view : window.findChildren<WebView *>())
                    if (view->title() == QString("Fixture %1").arg(i)) return true;
                return false;
            }(), 15000);
        }
        QCOMPARE(window.findChildren<WebView *>().size(), 5);
        QTest::qWait(200);
        result.insert("five_local_pages", processTreeMemory());
        for (int i = 0; i < 10; ++i) QVERIFY(command(Navigation::NewTab));
        QCOMPARE(window.findChildren<WebView *>().size(), 5);
        result.insert("ten_additional_blank_tabs", processTreeMemory());
        for (int i = 0; i < 10; ++i) {
            QVERIFY(command(Navigation::SelectLastTab));
            QVERIFY(command(Navigation::CloseTab));
        }
        const int widgetsBefore = window.findChildren<QWidget *>().size();
        QJsonArray latencies;
        for (int i = 0; i < 20; ++i) {
            QElapsedTimer timer;
            timer.start();
            QVERIFY(command(Navigation::Downloads));
            QCoreApplication::processEvents();
            auto *downloads = window.findChild<QDialog *>("downloadsPage");
            QVERIFY(downloads);
            QVERIFY(downloads->isVisible());
            latencies.append(timer.nsecsElapsed() / 1e6);
            QVERIFY(command(Navigation::CloseTab));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCOMPARE(window.findChildren<WebView *>().size(), 5);
        }
        result.insert("route_latency_ms", latencies);
        QJsonArray switching;
        for (int i = 0; i < 20; ++i) {
            QElapsedTimer timer;
            timer.start();
            QVERIFY(command(Navigation::NextTab));
            QCoreApplication::processEvents();
            switching.append(timer.nsecsElapsed() / 1e6);
        }
        result.insert("tab_switch_latency_ms", switching);
        QTest::qWait(200);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        result.insert("after_tools", processTreeMemory());
        result.insert("widgets_before", widgetsBefore);
        result.insert("widgets_after", window.findChildren<QWidget *>().size());
        QCOMPARE(window.findChildren<WebView *>().size(), 5);
        QVERIFY(window.findChildren<QDialog *>().isEmpty());
        rounds.append(result);
        window.close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    const QString output = qEnvironmentVariable("ZENITH_BENCHMARK_OUTPUT");
    if (!output.isEmpty()) {
        QSaveFile report(output);
        QVERIFY(report.open(QIODevice::WriteOnly));
        report.write(QJsonDocument(QJsonObject{{"version", ZENITH_VERSION}, {"qt", qVersion()},
            {"scope", "QtTest host with five deterministic local pages; includes test harness overhead"},
            {"rounds", rounds}}).toJson());
        QVERIFY(report.commit());
    }
}

void CoreTests::inventoriesAndResetsPreferenceControls()
{
    QSettings settings;
    QHash<QString, QVariant> saved;
    for (const auto &entry : Preferences::entries()) saved.insert(entry.key, settings.value(entry.key));
    const auto restore = qScopeGuard([&] {
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            if (it.value().isValid()) settings.setValue(it.key(), it.value()); else settings.remove(it.key());
        ThemeManager::instance().reloadPreferences();
    });
    settings.setValue("preferences/searchTemplate", "https://search.example/?q=%1");
    settings.setValue("appearance/fontSize", 18);
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/library.sqlite");
    QVERIFY(data.bookmark(QUrl("https://preserved.example/"), "Preservado"));
    SettingsPage page;
    connect(&page, &SettingsPage::preferencesReset, &ThemeManager::instance(), &ThemeManager::reloadPreferences);
    for (const auto &entry : Preferences::entries()) {
        QList<QWidget *> controls;
        for (auto *widget : page.findChildren<QWidget *>())
            if (widget->property("preferenceKey").toString() == entry.key) controls.append(widget);
        QCOMPARE(controls.size(), 1);
        QVERIFY2(!controls.first()->toolTip().isEmpty(), qPrintable(entry.key));
    }
    page.findChild<QPushButton *>("reset-appearance")->click();
    page.findChild<QPushButton *>("resetCancel-appearance")->click();
    QCOMPARE(Preferences::value("appearance/fontSize").toInt(), 18);
    QSignalSpy themeChanged(&ThemeManager::instance(), &ThemeManager::changed);
    page.findChild<QPushButton *>("reset-appearance")->click();
    page.findChild<QPushButton *>("resetConfirm-appearance")->click();
    QCOMPARE(themeChanged.size(), 1);
    QCOMPARE(Preferences::value("appearance/fontSize").toInt(), 13);
    QCOMPARE(page.findChild<QSpinBox *>("appearance/fontSize")->value(), 13);
    QCOMPARE(Preferences::value("preferences/searchTemplate").toString(), QString("https://search.example/?q=%1"));
    QCOMPARE(data.entries(true).size(), 1);
    QCOMPARE(data.entries(true).first().title, QString("Preservado"));
}

void CoreTests::laysOutLibrariesAtLargeText()
{
    auto &theme = ThemeManager::instance();
    const QString mode = theme.mode();
    QSettings settings;
    const auto font = settings.value("appearance/fontSize");
    const auto restore = qScopeGuard([&] {
        if (font.isValid()) settings.setValue("appearance/fontSize", font); else settings.remove("appearance/fontSize");
        theme.setMode(mode);
    });
    settings.setValue("appearance/fontSize", 18);
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/library.sqlite");
    QVERIFY(data.bookmark(QUrl("https://example.org/"), "Título de teste com acentuação", "Trabalho"));
    for (const QString mode : {QString("dark"), QString("light")}) {
        theme.setMode(mode);
        LibraryPage page(&data, true);
        page.resize(428, 520);
        page.show();
        QTRY_COMPARE(page.findChild<QListWidget *>("detailList")->count(), 1);
        QCoreApplication::processEvents();
        QCOMPARE(page.width(), 428);
        for (auto *button : page.findChildren<QAbstractButton *>()) {
            if (!button->isVisible()) continue;
            QVERIFY(button->mapTo(&page, button->rect().topLeft()).x() >= 0);
            QVERIFY2(button->mapTo(&page, button->rect().bottomRight()).x() < page.width(), qPrintable(button->text()));
        }
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
            QVERIFY(page.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-library-" + mode + "-large-text.png"));
    }
}

void CoreTests::confirmsFullHistoryClearWithActiveSearch()
{
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/history.sqlite");
    QVERIFY(data.visit(QUrl("https://first.example/"), "Primeira visita"));
    QVERIFY(data.visit(QUrl("https://second.example/"), "Segunda visita"));
    LibraryPage page(&data, false);
    page.show();
    auto *list = page.findChild<QListWidget *>("detailList");
    QTRY_COMPARE(list->count(), 2);
    auto *search = page.findChild<QLineEdit *>("librarySearch");
    search->setText("Primeira");
    QTest::keyClick(search, Qt::Key_Return);
    QTRY_COMPARE(list->count(), 1);
    auto *remove = page.findChild<QPushButton *>("libraryRemove");
    QVERIFY(remove->text().contains("todo o histórico"));
    for (auto decision : {QMessageBox::No, QMessageBox::Yes}) {
        QTimer::singleShot(0, &page, [decision] {
            if (auto *prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                prompt->button(decision)->click();
        });
        remove->click();
        if (decision == QMessageBox::No) QCOMPARE(data.entries(false).size(), 2);
        else QTRY_COMPARE(list->count(), 0);
    }
    QVERIFY(data.entries(false).isEmpty());
}

void CoreTests::treatsExternalTooltipContentAsText()
{
    const QString text = "<img src='file:///tmp/private.png'> & título";
    TabItemWidget tab(1);
    tab.setTitle(text);
    QTextDocument document;
    document.setHtml(tab.toolTip());
    QCOMPARE(document.toPlainText(), text);
    QVERIFY(!document.toHtml().contains("<img"));
    tab.setDiscarded(true);
    document.setHtml(tab.toolTip());
    QCOMPARE(document.toPlainText(), text + " (hibernada)");
    document.setHtml(PageWidgets::plainToolTip("Arquivo\n" + text));
    QCOMPARE(document.toPlainText(), "Arquivo\n" + text);
    Sidebar sidebar;
    sidebar.addTab({19, 0, -1, text, {}, false, true, 0});
    QCoreApplication::processEvents();
    auto *essentials = sidebar.findChild<QListWidget *>("essentialsList");
    QCOMPARE(essentials->count(), 1);
    document.setHtml(essentials->item(0)->toolTip());
    QCOMPARE(document.toPlainText(), text);
    QCOMPARE(essentials->item(0)->data(Qt::AccessibleTextRole).toString(), text);
}

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
    QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("Isolated"), 10000);
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
    QWidget host;
    auto *dialog = manager.createPage(&host);
    QVERIFY(!dialog->isWindow());
    auto *list = dialog->findChild<QListWidget *>();
    QVERIFY(list && list->count() == 3);
    list->setCurrentRow(1);
    QPersistentModelIndex selected(list->currentIndex());
    manager.changed();
    const bool retained = selected.isValid() && list->currentIndex() == selected;
    host.resize(760, 540);
    auto *layout = new QVBoxLayout(&host);
    layout->addWidget(dialog);
    host.show();
    if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) host.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-downloads.png");
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
        QTRY_COMPARE_WITH_TIMEOUT(view->title(), QStringLiteral("Discard recovery"), 15000);
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

void CoreTests::destroysProfilesWithPendingCookies()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto released = std::make_shared<int>(0);
    for (int i = 0; i < 8; ++i) {
        WebProfile profile(dir.path() + QString("/data-%1").arg(i),
            dir.path() + QString("/cache-%1").arg(i), nullptr, true);
        auto *manager = webkit_web_context_get_website_data_manager(profile.context());
        g_object_weak_ref(G_OBJECT(manager), +[](gpointer data, GObject *) {
            std::unique_ptr<std::shared_ptr<int>> count(static_cast<std::shared_ptr<int> *>(data));
            ++**count;
        }, new std::shared_ptr<int>(released));
    }
    QTRY_COMPARE_WITH_TIMEOUT(*released, 8, 20000);
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
        QVERIFY(sidebar.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-sidebar.png"));
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
            QVERIFY(window.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-" + name + ".png"));
        window.resize(760, 540);
        QCoreApplication::processEvents();
        QVERIFY(mode->width() > 80);
        QVERIFY(mode->mapTo(&window, mode->rect().bottomRight()).x() < window.width());
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
            QVERIFY(window.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-" + name + "-narrow.png"));
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
    QSignalSpy searchChanged(&preferences, &SettingsPage::searchTemplateChanged);
    auto *customSearch = preferences.findChild<QLineEdit *>("customSearchTemplate");
    customSearch->setText("javascript:alert('%1')");
    preferences.findChild<QPushButton *>("saveCustomSearch")->click();
    QCOMPARE(searchChanged.size(), 0);
    customSearch->setText("https://custom.example/?q=%1");
    preferences.findChild<QPushButton *>("saveCustomSearch")->click();
    QCOMPARE(searchChanged.size(), 1);
    QCOMPARE(search->currentData().toString(), QString("https://custom.example/?q=%1"));
    preferences.beginWorkspaceEdit(0, {});
    auto *workspaceEditor = preferences.findChild<QWidget *>("workspaceEditor");
    QVERIFY(!workspaceEditor->isHidden());
    auto *workspaceName = workspaceEditor->findChild<QLineEdit *>("workspaceName");
    auto *saveWorkspace = workspaceEditor->findChild<QPushButton *>();
    QVERIFY(!saveWorkspace->isEnabled());
    QSignalSpy workspaceRequested(&preferences, &SettingsPage::workspaceNameRequested);
    workspaceName->setText("Trabalho");
    saveWorkspace->click();
    QCOMPARE(workspaceRequested.size(), 1);
    QCOMPARE(workspaceRequested.first().at(1).toString(), QString("Trabalho"));
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
        const QString shortcut = Navigation::shortcutLabel(entry->data().toInt());
        if (!shortcut.isEmpty()) {
            QVERIFY(entry->text().endsWith('\t' + shortcut));
            const QKeySequence sequence(shortcut, QKeySequence::PortableText);
            QVERIFY(!sequence.isEmpty());
            const auto combination = sequence[0];
            QCOMPARE(Navigation::command(combination.key(), combination.keyboardModifiers()), entry->data().toInt());
        }
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
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) QVERIFY(menu.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-menu-" + theme + ".png"));
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
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) popup->grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-workspace-menu.png");
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

void CoreTests::usesNativeStartPage()
{
    StartPage start;
    start.setWorkspaceName("Espaço de teste");
    auto *input = start.findChild<QLineEdit *>("startSearch");
    auto *open = start.findChild<QPushButton *>("startOpen");
    QVERIFY(input && open);
    QVERIFY(!open->isEnabled());
    QSignalSpy navigate(&start, &StartPage::navigateRequested);
    input->setText("https://example.org/page");
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigate.size(), 1);
    QCOMPARE(navigate.last().first().toUrl(), QUrl("https://example.org/page"));
    QVERIFY(input->text().isEmpty());
    start.setSearchTemplate("https://www.google.com/search?q=%1");
    input->setText("texto de teste");
    open->click();
    QCOMPARE(navigate.size(), 2);
    QCOMPARE(navigate.last().first().toUrl().host(), QString("www.google.com"));
    QVERIFY(navigate.last().first().toUrl().query().contains("texto"));
    input->setText("javascript:alert(1)");
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigate.size(), 2);
    QSignalSpy commands(&start, &StartPage::commandRequested);
    for (QPushButton *button : start.findChildren<QPushButton *>("startAction")) button->click();
    QCOMPARE(commands.size(), 3);
    QCOMPARE(commands.first().first().toInt(), int(Navigation::Bookmarks));
}

void CoreTests::defersNativeBlankTabs()
{
    if (!graphicalTests) QSKIP("WebKit integration requires X11");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    QWidget host;
    TabManager manager(&profile, nullptr);
    QList<int> ids;
    for (int i = 0; i < 3; ++i) {
        const int id = manager.createTab(QUrl("about:blank"), 0, &host, false, -1, true);
        manager.activateTab(id, false);
        QVERIFY(!manager.tab(id)->view);
        ids.append(id);
    }
    QVERIFY(manager.ensureView(ids.first()));
    GList *windows = gtk_window_list_toplevels();
    bool undecoratedHost = false;
    for (GList *entry = windows; entry; entry = entry->next) {
        auto *widget = GTK_WIDGET(entry->data);
        if (QString::fromUtf8(gtk_widget_get_name(widget)) == "zenithWebHost")
            undecoratedHost = !gtk_window_get_decorated(GTK_WINDOW(widget)) && gtk_container_get_border_width(GTK_CONTAINER(widget)) == 0;
    }
    g_list_free(windows);
    QVERIFY(undecoratedHost);
    QVERIFY(!manager.tab(ids.at(1))->view);
    QVERIFY(!manager.tab(ids.at(2))->view);
}

void CoreTests::laysOutInternalScreens()
{
    auto &theme = ThemeManager::instance();
    const QString original = theme.mode();
    QSettings appearance;
    const QVariant originalFont = appearance.value("appearance/fontSize");
    const QVariant originalWidth = appearance.value("appearance/sidebarWidth");
    const auto restore = qScopeGuard([&] {
        for (const auto &entry : QList<QPair<QString, QVariant>>{{"appearance/fontSize", originalFont}, {"appearance/sidebarWidth", originalWidth}}) {
            if (entry.second.isValid()) appearance.setValue(entry.first, entry.second);
            else appearance.remove(entry.first);
        }
        theme.setMode(original);
    });
    QMainWindow window(nullptr, Qt::Window | Qt::BypassWindowManagerHint);
    window.setObjectName("browserWindow");
    window.setContentsMargins(6, 6, 6, 6);
    auto *central = new QWidget;
    window.setCentralWidget(central);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *sidebar = new Sidebar(central);
    sidebar->setWorkspaces({"Espaço de teste", "Trabalho (teste)"});
    auto *right = new QWidget(central);
    auto *vertical = new QVBoxLayout(right);
    vertical->setContentsMargins(0, 0, 0, 0); vertical->setSpacing(0);
    vertical->addWidget(new UrlBar(right));
    auto *stack = new QStackedWidget(right);
    auto *start = new StartPage(stack);
    start->setWorkspaceName("Espaço de teste");
    auto *settings = new SettingsPage(stack);
    settings->setWorkspaces({"Espaço de teste", "Trabalho (teste)"}, 0);
    settings->setAdBlockStatus(0, 0, "Listas de teste");
    stack->addWidget(start); stack->addWidget(settings);
    vertical->addWidget(stack, 1);
    layout->addWidget(sidebar); layout->addWidget(right, 1);
    auto *navigation = settings->findChild<QListWidget *>("settingsNavigation");
    QVERIFY(navigation);
    window.show();
    for (const QString name : {QStringLiteral("dark"), QStringLiteral("light")}) {
        theme.setMode(name);
        for (const QSize size : {QSize(1280, 800), QSize(760, 520), QSize(1024, 768), QSize(1920, 1080)}) {
            window.resize(size);
            stack->setCurrentWidget(start);
            QCoreApplication::processEvents();
            QCOMPARE(window.size(), size);
            const QString suffix = name + (size.width() < 1000 ? "-narrow" : size.width() == 1280 ? ""
                : QString("-%1x%2").arg(size.width()).arg(size.height()));
            if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) QVERIFY(window.grab().save(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-start-" + suffix + ".png"));
            const QColor corner = window.grab().toImage().pixelColor(3, 3);
            QCOMPARE(corner.lightness() < 128, name == "dark");
            stack->setCurrentWidget(settings);
            for (int page = 0; page < navigation->count(); ++page) {
                navigation->setCurrentRow(page);
                QCoreApplication::processEvents();
                for (QComboBox *control : settings->findChildren<QComboBox *>()) {
                    if (!control->isVisible()) continue;
                    QVERIFY2(control->mapTo(&window, control->rect().bottomRight()).x() < window.width(), qPrintable(control->objectName()));
                }
                for (QCheckBox *control : settings->findChildren<QCheckBox *>()) {
                    if (!control->isVisible()) continue;
                    QVERIFY2(control->fontMetrics().horizontalAdvance(control->text()) + 28 <= control->width(), qPrintable(control->text()));
                }
                if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) QVERIFY(window.grab().save(QString(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-settings-%1-%2.png").arg(page).arg(suffix)));
            }
        }
    }
    appearance.setValue("appearance/fontSize", 18);
    appearance.setValue("appearance/sidebarWidth", 320);
    theme.apply();
    window.resize(760, 540);
    stack->setCurrentWidget(settings);
    for (int page = 0; page < navigation->count(); ++page) {
        navigation->setCurrentRow(page);
        QCoreApplication::processEvents();
        QCOMPARE(window.size(), QSize(760, 540));
        for (QCheckBox *control : settings->findChildren<QCheckBox *>()) {
            if (!control->isVisible()) continue;
            QVERIFY2(control->fontMetrics().horizontalAdvance(control->text()) + 28 <= control->width(), qPrintable(control->text()));
        }
        if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI")) QVERIFY(window.grab().save(QString(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp") + "/zenith-settings-%1-large-text.png").arg(page)));
    }
    QSignalSpy selected(settings, &SettingsPage::workspaceSelected);
    navigation->setCurrentRow(2);
    auto *spaces = settings->findChild<QListWidget *>("workspaceList");
    QVERIFY(spaces);
    spaces->setCurrentRow(1);
    QTest::keyClick(spaces, Qt::Key_Return);
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected.first().first().toInt(), 1);
    theme.setMode(original);
}

void CoreTests::searchesAndRefreshesPreferences()
{
    QWidget host;
    auto *page = new SettingsPage(&host);
    host.resize(1000, 700);
    page->setGeometry(host.rect());
    host.show();
    QCoreApplication::processEvents();
    auto *navigation = page->findChild<QListWidget *>("settingsNavigation");
    auto *category = page->findChild<QComboBox *>("settingsCategory");
    auto *search = page->findChild<QLineEdit *>("settingsSearch");
    QVERIFY(navigation->isVisible());
    QVERIFY(!category->isVisible());
    search->setText("cookies");
    QCOMPARE(navigation->currentRow(), 3);
    QVERIFY(!navigation->item(3)->isHidden());
    QVERIFY(navigation->item(0)->isHidden());
    search->setText("memoria");
    QCOMPARE(navigation->currentRow(), 4);
    search->setText("nenhuma-preferencia-com-esse-nome");
    QCOMPARE(page->findChild<QStackedWidget *>()->currentIndex(), navigation->count());
    search->clear();
    QCOMPARE(page->findChild<QStackedWidget *>()->currentIndex(), navigation->currentRow());
    for (int i = 0; i < navigation->count(); ++i) QVERIFY(!navigation->item(i)->isHidden());
    host.resize(520, 600);
    page->setGeometry(host.rect());
    QCoreApplication::processEvents();
    QCOMPARE(page->width(), 520);
    QVERIFY(!navigation->isVisible());
    QVERIFY(category->isVisible());
    category->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(category, "activated", Q_ARG(int, 1)));
    QCOMPARE(navigation->currentRow(), 1);
    QSettings settings;
    const QVariant original = settings.value("privacy/blockThirdPartyCookies");
    auto *cookies = page->findChild<QCheckBox *>("thirdPartyCookies");
    QSignalSpy changed(page, &SettingsPage::privacyPolicyChanged);
    settings.setValue("privacy/blockThirdPartyCookies", !cookies->isChecked());
    page->refreshPreferenceControls();
    QCOMPARE(cookies->isChecked(), settings.value("privacy/blockThirdPartyCookies").toBool());
    QCOMPARE(changed.size(), 0);
    if (original.isValid()) settings.setValue("privacy/blockThirdPartyCookies", original);
    else settings.remove("privacy/blockThirdPartyCookies");
    const QString originalMode = ThemeManager::instance().mode();
    ThemeManager::instance().setMode(originalMode == "dark" ? "light" : "dark");
    QCOMPARE(page->findChild<QComboBox *>("browserTheme")->currentData().toString(), ThemeManager::instance().mode());
    ThemeManager::instance().setMode(originalMode);
}

void CoreTests::customizesBrowserDimensions()
{
    QSettings settings;
    const QStringList keys{"appearance/sidebarWidth", "appearance/tabHeight", "appearance/fontSize", "web/defaultZoom"};
    QHash<QString, QVariant> saved;
    for (const QString &key : keys) saved.insert(key, settings.value(key));
    const QString originalAccent = ThemeManager::instance().accent();
    SettingsPage page;
    Sidebar sidebar;
    sidebar.addTab({77, 0, -1, "Aba de teste", {}, false, false, 0});
    sidebar.setWorkspace(0);
    auto *width = page.findChild<QSpinBox *>("appearance/sidebarWidth");
    auto *height = page.findChild<QSpinBox *>("appearance/tabHeight");
    QVERIFY(width && height);
    width->setValue(280);
    height->setValue(44);
    QCoreApplication::processEvents();
    QCOMPARE(sidebar.preferredWidth(), 280);
    QCOMPARE(sidebar.width(), 280);
    const auto rows = sidebar.findChildren<TabItemWidget *>();
    QVERIFY(!rows.isEmpty());
    QCOMPARE(rows.first()->height(), 44);
    ThemeManager::instance().setAccent("#cc5500");
    QCOMPARE(ThemeManager::instance().accent(), QString("#cc5500"));
    QVERIFY(themeColor("#8b7cf6").isValid());
    ThemeManager::instance().setAccent("invalid");
    QCOMPARE(ThemeManager::instance().accent(), QString("#cc5500"));
    QTemporaryDir dir;
    BrowserData data(dir.path() + "/data.sqlite");
    settings.setValue("web/defaultZoom", 125);
    QCOMPARE(data.zoom(QUrl("https://default.example")), 1.25);
    QVERIFY(data.setZoom(QUrl("https://saved.example"), 1.75));
    settings.setValue("web/defaultZoom", 150);
    QCOMPARE(data.zoom(QUrl("https://saved.example")), 1.75);
    QSignalSpy changed(&page, &SettingsPage::browsingPreferencesChanged);
    page.refreshPreferenceControls();
    QCOMPARE(page.findChild<QSpinBox *>("web/defaultZoom")->value(), 150);
    QCOMPARE(changed.size(), 0);
    for (const QString &key : keys) {
        if (saved.value(key).isValid()) settings.setValue(key, saved.value(key));
        else settings.remove(key);
    }
    ThemeManager::instance().setAccent(originalAccent);
}

void CoreTests::embedsFindBar()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *bar = new FindBar(&host);
    layout->addStretch();
    layout->addWidget(bar);
    host.resize(520, 400);
    host.show();
    bar->open(nullptr);
    QVERIFY(!bar->isWindow());
    QVERIFY(bar->isVisible());
    QVERIFY(!QApplication::activeModalWidget());
    auto *input = bar->findChild<QLineEdit *>("findInput");
    QVERIFY(input);
    QTest::keyClicks(input, "teste");
    QSignalSpy closed(bar, &FindBar::closed);
    bar->finish();
    QCOMPARE(closed.size(), 1);
    QVERIFY(bar->isHidden());
    QTest::qWait(180);
    QCOMPARE(closed.size(), 1);
}

void CoreTests::embedsBrowserTools()
{
    if (!graphicalTests) QSKIP("Browser integration requires X11");
    MainWindow window(true);
    window.show();
    window.activateWindow();
    QTest::qWait(200);
    auto *start = window.findChild<StartPage *>();
    QVERIFY(start);
    const auto command = [start](int value) {
        QVERIFY(QMetaObject::invokeMethod(start, "commandRequested", Q_ARG(int, value)));
    };
    const QString clipboardText = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([clipboardText] { QApplication::clipboard()->setText(clipboardText); });
    for (int iteration = 0; iteration < 20; ++iteration) {
        command(Navigation::NewTab);
        QApplication::clipboard()->setText("texto colado na nova aba");
        auto *address = window.findChild<QLineEdit *>("addressInput");
        QVERIFY(address);
        address->selectAll();
        QTest::keyClick(address, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(address->text(), QString("texto colado na nova aba"));
        QVERIFY(window.findChildren<WebView *>().isEmpty());
        command(Navigation::CloseTab);
    }
    for (int iteration = 0; iteration < 3; ++iteration) {
        for (const auto &entry : QList<QPair<int, QString>>{{Navigation::History, "historyPage"}, {Navigation::Bookmarks, "bookmarksPage"}, {Navigation::Downloads, "downloadsPage"}}) {
            command(entry.first);
            auto *page = window.findChild<QDialog *>(entry.second);
            QVERIFY(page && page->isVisible());
            QCOMPARE(page->parentWidget(), window.findChild<QStackedWidget *>());
            QCOMPARE(page->window(), static_cast<QWidget *>(&window));
            QVERIFY(!QApplication::activeModalWidget());
            page->reject();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(!window.findChild<QDialog *>(entry.second));
        }
    }
    command(Navigation::Settings);
    QVERIFY(window.findChild<SettingsPage *>()->isVisible());
    command(Navigation::Downloads);
    auto *downloadsPage = window.findChild<QDialog *>("downloadsPage");
    QVERIFY(downloadsPage && downloadsPage->isVisible());
    downloadsPage->reject();
    QVERIFY(window.findChild<SettingsPage *>()->isVisible());
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!window.findChild<SettingsPage *>()->isVisible());
    QTemporaryDir dir;
    QFile document(dir.path() + "/find.html");
    QVERIFY(document.open(QIODevice::WriteOnly));
    document.write("<title>Find fixture</title><p>needle needle</p>");
    document.close();
    window.openUrls({QUrl::fromLocalFile(document.fileName())});
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<WebView *>(), 10000);
    auto *view = window.findChild<WebView *>();
    QTRY_COMPARE_WITH_TIMEOUT(view->title(), QString("Find fixture"), 15000);
    command(Navigation::Find);
    auto *bar = window.findChild<FindBar *>();
    QVERIFY(bar && bar->isVisible());
    auto *input = bar->findChild<QLineEdit *>("findInput");
    input->setText("needle");
    QTRY_VERIFY_WITH_TIMEOUT(bar->findChild<QLabel *>("findStatus")->text().contains('2'), 5000);
    command(Navigation::FindPrevious);
    bar->finish();
    QVERIFY(!bar->isVisible());
    command(Navigation::FindNext);
    command(Navigation::Downloads);
    QVERIFY(!bar->isVisible());
    window.close();
}

void CoreTests::compactSidebarTracksPointerAfterTabChanges()
{
    if (!graphicalTests) QSKIP("Compact sidebar integration requires X11");
    MainWindow window(true);
    window.show();
    window.activateWindow();
    QTest::qWait(200);
    auto *sidebar = window.findChild<Sidebar *>("sidebar");
    QVERIFY(sidebar);
    auto *urlBar = window.findChild<UrlBar *>();
    QVERIFY(urlBar);
    QVERIFY(QMetaObject::invokeMethod(urlBar, "compactToggled"));
    QTRY_VERIFY_WITH_TIMEOUT(!sidebar->isVisible(), 3000);

    QCursor::setPos(window.mapToGlobal(QPoint(window.width() / 2, window.height() / 2)));
    QTest::keyClick(&window, Qt::Key_T, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!sidebar->isVisible(), 1500);
    QTest::keyClick(&window, Qt::Key_Tab, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!sidebar->isVisible(), 1500);
    window.close();
}

void CoreTests::preservesAutomaticDownloadNames()
{
    QTemporaryDir dir;
    QFile existing(dir.path() + "/archive.tar.gz");
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("preserve");
    existing.close();
    const QString path = DownloadManager::uniqueDestination(dir.path(), "../archive.tar.gz");
    QCOMPARE(QFileInfo(path).fileName(), QString("archive (1).tar.gz"));
    QCOMPARE(QFileInfo(path).absolutePath(), dir.path());
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("preserve"));
}

void CoreTests::handlesFailedClipboardTargets()
{
    struct Result { int calls = 0; int count = -9; bool empty = false; } result;
    const auto received = +[](GtkClipboard *, GdkAtom *atoms, gint count, gpointer data) {
        auto *result = static_cast<Result *>(data);
        ++result->calls;
        result->count = count;
        result->empty = atoms == nullptr;
    };
    const auto failed = +[](GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc callback, gpointer data) {
        callback(clipboard, nullptr, -1, data);
    };
    GtkClipboardGuard::requestTargets(nullptr, received, &result, failed);
    QCOMPARE(result.calls, 1);
    QCOMPARE(result.count, 0);
    QVERIFY(result.empty);
    const auto valid = +[](GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc callback, gpointer data) {
        GdkAtom targets[]{GDK_SELECTION_PRIMARY, GDK_SELECTION_CLIPBOARD};
        callback(clipboard, targets, 2, data);
    };
    GtkClipboardGuard::requestTargets(nullptr, received, &result, valid);
    QCOMPARE(result.calls, 2);
    QCOMPARE(result.count, 2);
    QVERIFY(!result.empty);
    const auto invalid = +[](GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc callback, gpointer data) {
        GdkAtom targets[]{GDK_SELECTION_PRIMARY};
        callback(clipboard, targets, -1, data);
    };
    GtkClipboardGuard::requestTargets(nullptr, received, &result, invalid);
    QCOMPARE(result.count, 0);
    const auto asynchronous = +[](GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc callback, gpointer data) {
        QTimer::singleShot(0, [clipboard, callback, data] { callback(clipboard, nullptr, -1, data); });
    };
    GtkClipboardGuard::requestTargets(nullptr, received, &result, asynchronous);
    QTRY_COMPARE(result.calls, 4);
    QCOMPARE(result.count, 0);
    QVERIFY(result.empty);
    QVERIFY(dlsym(RTLD_DEFAULT, "gtk_clipboard_request_targets") == reinterpret_cast<void *>(&gtk_clipboard_request_targets));
}

void CoreTests::defersWebViewKeySinkDispatch()
{
    QWidget context;
    auto *layout = new QVBoxLayout(&context);
    UrlBar address(&context);
    layout->addWidget(&address);
    context.resize(800, 80);
    context.show();
    address.focusAddress();
    auto *input = address.findChild<QLineEdit *>("addressInput");
    QVERIFY(input);
    QApplication::clipboard()->setText(QStringLiteral("texto colado fora do callback GTK"));
    bool delivered = false;
    int receivedKey = 0;
    Qt::KeyboardModifiers receivedModifiers;
    QString receivedText;
    WebView::setKeySink([&](int key, Qt::KeyboardModifiers modifiers, const QString &text) -> bool {
        delivered = true;
        receivedKey = key;
        receivedModifiers = modifiers;
        receivedText = text;
        address.injectKey(key, modifiers, text);
        return true;
    });
    const auto clearSink = qScopeGuard([] { WebView::setKeySink({}); });

    QVERIFY(WebView::queueKeyToSink(&context, Qt::Key_V, Qt::ControlModifier, QStringLiteral("v")));
    QVERIFY(!delivered);
    QTRY_VERIFY(delivered);
    QCOMPARE(receivedKey, int(Qt::Key_V));
    QCOMPARE(receivedModifiers, Qt::KeyboardModifiers(Qt::ControlModifier));
    QCOMPARE(receivedText, QStringLiteral("v"));
    QCOMPARE(input->text(), QStringLiteral("texto colado fora do callback GTK"));

    delivered = false;
    QVERIFY(WebView::queueKeyToSink(&context, Qt::Key_V, Qt::ControlModifier, QStringLiteral("v")));
    WebView::setKeySink({});
    QCoreApplication::processEvents();
    QVERIFY(!delivered);
}

void CoreTests::pastesIntoNativeInputs()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *address = new UrlBar(&host);
    auto *start = new StartPage(&host);
    layout->addWidget(address);
    layout->addWidget(start);
    host.resize(900, 600);
    host.show();
    const QString original = QApplication::clipboard()->text();
    const auto restore = qScopeGuard([original] { QApplication::clipboard()->setText(original); });
    for (const QString &value : {QString("texto de teste"), QString("https://paste.example/path")}) {
        QApplication::clipboard()->setText(value);
        auto *input = start->findChild<QLineEdit *>("startSearch");
        input->clear();
        input->setFocus();
        QTest::keyClick(input, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(input->text(), value);
        address->focusAddress();
        address->injectKey(Qt::Key_A, Qt::ControlModifier, {});
        address->injectKey(Qt::Key_V, Qt::ControlModifier, {});
        QCOMPARE(address->findChild<QLineEdit *>("addressInput")->text(), value);
    }
}

void CoreTests::opensMiddleClickInBackground()
{
    if (!graphicalTests) QSKIP("Middle-click integration requires X11");
    QTemporaryDir dir;
    WebProfile profile(dir.path() + "/data", dir.path() + "/cache");
    QWidget host;
    host.resize(900, 700);
    TabManager manager(&profile, nullptr);
    const int source = manager.createTab(QUrl("about:blank"), 0, &host);
    auto *view = manager.ensureView(source);
    view->resize(900, 700);
    host.show(); view->show();
    QSignalSpy loaded(view, &WebView::loadFinished);
    QSignalSpy requested(&manager, &TabManager::newTabRequested);
    QSignalSpy created(&manager, &TabManager::tabCreated);
    bool expectedForeground = false;
    connect(&manager, &TabManager::newTabRequested, &manager, [&manager, &host, &expectedForeground](int, const QUrl &url, bool foreground) {
        QCOMPARE(foreground, expectedForeground);
        manager.createTab(url, 0, &host, false, -1, true);
    });
    view->loadHtml("<style>body{margin:0}a{display:block;width:500px;height:200px}</style><a href='https://middle.example/'>Open link</a>", QUrl("https://source.example/"));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
    GtkWidget *native = nullptr;
    GList *windows = gtk_window_list_toplevels();
    for (GList *node = windows; node; node = node->next) {
        if (!GTK_IS_PLUG(node->data)) continue;
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(node->data));
        if (WEBKIT_IS_WEB_VIEW(child) && QString::fromUtf8(webkit_web_view_get_uri(WEBKIT_WEB_VIEW(child))) == "https://source.example/") native = child;
    }
    g_list_free(windows);
    QVERIFY(native);
    GdkWindow *window = gtk_widget_get_window(native);
    QVERIFY(window);
    int expectedRequests = 0;
    for (int iteration = 0; iteration < 20; ++iteration) {
        for (int gesture = 0; gesture < 3; ++gesture) {
            expectedForeground = gesture == 2;
            const guint modifiers = gesture == 0 ? 0 : GDK_CONTROL_MASK | (gesture == 2 ? GDK_SHIFT_MASK : 0);
            for (GdkEventType type : {GDK_BUTTON_PRESS, GDK_BUTTON_RELEASE}) {
                GdkEvent *event = gdk_event_new(type);
                event->button.window = GDK_WINDOW(g_object_ref(window));
                event->button.time = GDK_CURRENT_TIME;
                event->button.x = 20; event->button.y = 20;
                event->button.button = gesture == 0 ? 2 : 1;
                event->button.state = modifiers;
                gdk_event_set_device(event, gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_window_get_display(window))));
                gtk_widget_event(native, event);
                gdk_event_free(event);
            }
            ++expectedRequests;
            QTRY_COMPARE_WITH_TIMEOUT(requested.size(), expectedRequests, 5000);
            QCOMPARE(created.size(), expectedRequests);
            QCOMPARE(view->url(), QUrl("https://source.example/"));
            QCOMPARE(requested.last().at(1).toUrl(), QUrl("https://middle.example/"));
            const int added = manager.allTabIds().last();
            QVERIFY(!manager.tab(added)->view);
            manager.closeTab(added);
        }
    }
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
    QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("light"), 10000);
    theme.setSiteMode("dark");
    loaded.clear();
    view.loadHtml(document, QUrl("https://rendering.example/"));
    QVERIFY(loaded.wait(20000));
    QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("dark"), 10000);
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
    for (const auto &entry : added) QVERIFY(data.removeBookmark(QUrl(entry.url)));
    QCOMPARE(data.entries(true).size(), 1);
    QCOMPARE(data.entries(true).first().title, QString("Original"));
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
    QTranslator qtPortuguese;
    if (qtPortuguese.load("qtbase_pt_BR", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) app.installTranslator(&qtPortuguese);
    QCoreApplication::setApplicationVersion(ZENITH_VERSION);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("ZenithTests");
    QCoreApplication::setApplicationName("CoreTests");
    ThemeManager::instance().apply();
    if (qEnvironmentVariableIsSet("ZENITH_CAPTURE_UI"))
        QDir().mkpath(qEnvironmentVariable("ZENITH_CAPTURE_DIR", "/tmp"));
    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "CoreTests.moc"
