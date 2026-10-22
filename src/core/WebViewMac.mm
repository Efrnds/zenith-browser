#include "core/WebView.h"
#include "core/WebProfile.h"
#include "core/DownloadManager.h"
#include "core/AdBlockInterceptor.h"

#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWindow>
#include <QVBoxLayout>
#include <QSettings>
#include <QTimer>
#include <utility>

#undef signals
#undef slots
#import <AppKit/AppKit.h>
#import <WebKit/WKContentRuleList.h>
#import <WebKit/WKContentWorld.h>
#import <WebKit/WKNavigationAction.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKScriptMessage.h>
#import <WebKit/WKScriptMessageHandlerWithReply.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKUserContentController.h>
#import <WebKit/WKUserScript.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebsiteDataStore.h>

#include <algorithm>
#include <memory>

@interface ZenithWKDelegate : NSObject <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandlerWithReply>
{
@public
    QPointer<WebView> owner;
}
@end

@implementation ZenithWKDelegate
- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    Q_UNUSED(navigation);
    if (owner) {
        owner->urlChanged(QUrl(QString::fromUtf8(webView.URL.absoluteString.UTF8String ?: "")));
        owner->loadProgress(0);
    }
}
- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    Q_UNUSED(navigation);
    if (owner) owner->urlChanged(QUrl(QString::fromUtf8(webView.URL.absoluteString.UTF8String ?: "")));
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    Q_UNUSED(navigation);
    if (!owner) return;
    owner->urlChanged(QUrl(QString::fromUtf8(webView.URL.absoluteString.UTF8String ?: "")));
    owner->loadProgress(100);
    owner->loadFinished(true);
    owner->titleChanged(QString::fromUtf8(webView.title.UTF8String ?: "").left(512));
}
- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    Q_UNUSED(webView); Q_UNUSED(navigation);
    if (owner) {
        owner->loadFinished(false);
        owner->loadError(QString::fromUtf8(error.localizedDescription.UTF8String ?: "Falha de navegação"));
    }
}
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [self webView:webView didFailNavigation:navigation withError:error];
}
- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration
       forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    Q_UNUSED(configuration); Q_UNUSED(windowFeatures);
    if (owner) {
        const QString address = QString::fromUtf8(navigationAction.request.URL.absoluteString.UTF8String ?: "");
        emit owner->newTabRequested(QUrl(address), true);
    }
    return nil;
}
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message
initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(webView); Q_UNUSED(frame);
    if (owner) QMessageBox::information(owner, QObject::tr("Alerta do site"), QString::fromUtf8(message.UTF8String ?: ""));
    completionHandler();
}
- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message
initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler
{
    Q_UNUSED(webView); Q_UNUSED(frame);
    const bool accepted = owner && QMessageBox::question(owner, QObject::tr("Confirmação do site"),
        QString::fromUtf8(message.UTF8String ?: "")) == QMessageBox::Yes;
    completionHandler(accepted);
}
- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame
completionHandler:(void (^)(NSString *))completionHandler
{
    Q_UNUSED(webView); Q_UNUSED(frame);
    bool accepted = false;
    const QString value = owner ? QInputDialog::getText(owner, QObject::tr("Entrada do site"),
        QString::fromUtf8(prompt.UTF8String ?: ""), QLineEdit::Normal,
        QString::fromUtf8(defaultText.UTF8String ?: ""), &accepted) : QString();
    completionHandler(accepted ? [NSString stringWithUTF8String:value.toUtf8().constData()] : nil);
}
- (void)userContentController:(WKUserContentController *)userContentController
 didReceiveScriptMessage:(WKScriptMessage *)message replyHandler:(void (^)(id, NSString *))replyHandler
{
    Q_UNUSED(userContentController);
    if (!owner) { replyHandler(nil, nil); return; }
    const QString payload = QString::fromUtf8(message.body.description.UTF8String ?: "");
    const QString reply = owner->dispatchPlatformMessage(payload);
    if (reply.isEmpty()) replyHandler(nil, nil);
    else replyHandler([NSString stringWithUTF8String:reply.toUtf8().constData()], nil);
}
@end

namespace {
WebView::KeySink g_keySink;
std::function<void()> g_keySinkCancelled;
}

struct WebView::Impl
{
    WebView *owner = nullptr;
    WebProfile *profile = nullptr;
    WKWebView *webView = nil;
    ZenithWKDelegate *delegate = nil;
    QWidget *container = nullptr;
    PopupFactory popupFactory;
    MessageHandler messageHandler;
    QString findText;
    bool loading = false;
    bool visible = true;
    bool focusEnabled = true;
    double zoom = 1.0;
    QStringList scripts;

    Impl(WebView *view, WebProfile *webProfile) : owner(view), profile(webProfile) {}

    void applyContentRules()
    {
        if (!webView) return;
        WKUserContentController *controller = webView.configuration.userContentController;
        WKContentRuleList *rule = (__bridge WKContentRuleList *)profile->nativeContentFilter();
        if (rule) [controller addContentRuleList:rule];
    }

    void create()
    {
        WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
        configuration.websiteDataStore = (__bridge WKWebsiteDataStore *)profile->nativeHandle();
        WKUserContentController *content = [[WKUserContentController alloc] init];
        configuration.userContentController = content;
        [content release];
        webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, owner->width(), owner->height())
            configuration:configuration];
        [configuration release];
        delegate = [[ZenithWKDelegate alloc] init];
        delegate->owner = owner;
        webView.navigationDelegate = delegate;
        webView.UIDelegate = delegate;
        webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        webView.allowsBackForwardNavigationGestures = true;
        if (@available(macOS 13.3, *)) webView.inspectable = true;
        for (const QString &script : std::as_const(scripts)) install(script);
        applyContentRules();
        const WId nativeId = reinterpret_cast<WId>((__bridge void *)webView);
        QWindow *foreign = QWindow::fromWinId(nativeId);
        container = QWidget::createWindowContainer(foreign, owner);
        container->setFocusPolicy(Qt::StrongFocus);
        owner->setFocusProxy(container);
        owner->layout()->addWidget(container);
        QObject::connect(profile, &WebProfile::contentFilterChanged, owner, [this] {
            if (!webView) return;
            WKContentRuleList *rule = (__bridge WKContentRuleList *)profile->nativeContentFilter();
            if (rule) [webView.configuration.userContentController addContentRuleList:rule];
            [webView reload];
        });
    }

    void install(const QString &source)
    {
        NSString *code = [[NSString alloc] initWithCharacters:
            reinterpret_cast<const unichar *>(source.utf16()) length:source.size()];
        WKUserScript *script = [[WKUserScript alloc] initWithSource:code
            injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES];
        [webView.configuration.userContentController addUserScript:script];
        [script release];
        [code release];
    }
};

WebView::WebView(WebProfile *profile, QWidget *parent, WebView *)
    : QWidget(parent), impl_(std::make_unique<Impl>(this, profile))
{
    setFocusPolicy(Qt::StrongFocus);
    setLayout(new QVBoxLayout);
    layout()->setContentsMargins(0, 0, 0, 0);
    impl_->create();
}

WebView::~WebView()
{
    if (impl_->delegate) impl_->delegate->owner = nullptr;
    if (impl_->container) {
        layout()->removeWidget(impl_->container);
        delete impl_->container;
        impl_->container = nullptr;
    }
    if (impl_->webView) {
        impl_->webView.navigationDelegate = nil;
        impl_->webView.UIDelegate = nil;
        [impl_->webView stopLoading];
        [impl_->webView release];
    }
    if (impl_->delegate) [impl_->delegate release];
}

void WebView::setUrl(const QUrl &url) { if (impl_->webView) [impl_->webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:url.toString().toUtf8().constData()]]]]; }
QString WebView::userAgent() const { return impl_->webView ? QString::fromUtf8(impl_->webView.customUserAgent.UTF8String ?: "") : QString(); }
void WebView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    NSString *markup = [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar *>(html.utf16()) length:html.size()];
    NSURL *base = baseUrl.isValid() ? [NSURL URLWithString:[NSString stringWithUTF8String:baseUrl.toString().toUtf8().constData()]] : nil;
    [impl_->webView loadHTMLString:markup baseURL:base];
    [markup release];
}
bool WebView::engineAlive() const { return impl_->webView != nil; }
bool WebView::allowsCredentials() const { return url().scheme() == QStringLiteral("https"); }
QUrl WebView::url() const { return impl_->webView.URL ? QUrl(QString::fromUtf8(impl_->webView.URL.absoluteString.UTF8String ?: "")) : QUrl(); }
QString WebView::title() const { return QString::fromUtf8(impl_->webView.title.UTF8String ?: "").left(512); }
QIcon WebView::icon() const { return {}; }
bool WebView::canGoBack() const { return impl_->webView.canGoBack; }
bool WebView::canGoForward() const { return impl_->webView.canGoForward; }
void WebView::back() { [impl_->webView goBack]; }
void WebView::forward() { [impl_->webView goForward]; }
void WebView::reload(bool bypassCache) { Q_UNUSED(bypassCache); [impl_->webView reload]; }
void WebView::stop() { [impl_->webView stopLoading]; }
void WebView::find(const QString &text, bool backwards)
{
    Q_UNUSED(backwards);
    impl_->findText = text;
    if (text.isEmpty()) { emit findResult(0); return; }
    const QString js = QStringLiteral("(()=>{const t=%1;const n=document.body?document.body.innerText:'';return (n.match(new RegExp(t.replace(/[.*+?^${}()|[\\]\\\\]/g,'\\\\$&'),'gi'))||[]).length})()")
        .arg(QString::fromLatin1(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact)));
    NSString *script = [NSString stringWithUTF8String:js.toUtf8().constData()];
    [impl_->webView evaluateJavaScript:script completionHandler:^(id result, NSError *) {
        if (result && [result respondsToSelector:@selector(integerValue)]) emit findResult([result integerValue]);
        else emit findResult(0);
    }];
}
QString WebView::lastFindText() const { return impl_->findText; }
void WebView::finishFind() { impl_->findText.clear(); }
void WebView::applyBrowsingPreferences()
{
    QSettings preferences;
    impl_->webView.pageZoom = qBound(0.25, impl_->profile->data()->zoom(url()), 5.0);
    impl_->zoom = impl_->webView.pageZoom;
    impl_->webView.configuration.preferences.javaScriptEnabled = preferences.value("web/javascript", true).toBool();
}
void WebView::setZoom(double value) { impl_->zoom = std::clamp(value, 0.25, 5.0); impl_->webView.pageZoom = impl_->zoom; }
double WebView::zoom() const { return impl_->webView ? impl_->webView.pageZoom : impl_->zoom; }
void WebView::setMuted(bool muted) { const QString js = QStringLiteral("document.querySelectorAll('audio,video').forEach(x=>x.muted=%1)").arg(muted ? "true" : "false"); [impl_->webView evaluateJavaScript:[NSString stringWithUTF8String:js.toUtf8().constData()] completionHandler:nil]; emit audioChanged(); }
bool WebView::isMuted() const { return false; }
bool WebView::isPlayingAudio() const { return false; }
bool WebView::maySuspend() const { return engineAlive() && !impl_->loading && !isPlayingAudio(); }
void WebView::checkCanSuspend(const std::function<void(bool)> &callback) { callback(maySuspend()); }
void WebView::print() { NSPrintOperation *operation = [impl_->webView printOperationWithPrintInfo:[NSPrintInfo sharedPrintInfo]]; [operation runOperation]; }
void WebView::inspect() { impl_->webView.inspectable = true; }
void WebView::setPageFocusEnabled(bool enabled) { impl_->focusEnabled = enabled; setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus); if (impl_->container) impl_->container->setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus); }
void WebView::setEmbeddedVisible(bool visible) { impl_->visible = visible; if (impl_->container) impl_->container->setVisible(visible); }
void WebView::parkSurface() { setEmbeddedVisible(false); }
void WebView::syncSurfaceSize() { if (impl_->container) impl_->container->setGeometry(rect()); }
void WebView::reembed() { syncSurfaceSize(); }
void WebView::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); syncSurfaceSize(); }
void WebView::showEvent(QShowEvent *event) { QWidget::showEvent(event); syncSurfaceSize(); }
void WebView::setKeySink(KeySink sink, std::function<void()> cancelled) { g_keySink = std::move(sink); g_keySinkCancelled = std::move(cancelled); }
bool WebView::queueKeyToSink(QObject *, int key, Qt::KeyboardModifiers modifiers, const QString &text) { return g_keySink && g_keySink(key, modifiers, text); }
QByteArray WebView::sessionState() const { return QJsonDocument(QJsonObject{{"url", url().toString()}}).toJson(QJsonDocument::Compact); }
bool WebView::restoreSessionState(const QByteArray &state) { const QUrl target(QJsonDocument::fromJson(state).object().value("url").toString()); if (!target.isValid() || target.isEmpty()) return false; setUrl(target); return true; }
void WebView::setPopupFactory(PopupFactory factory) { impl_->popupFactory = std::move(factory); }
void WebView::installPageScript(const QString &source) { impl_->scripts.append(source); impl_->install(source); }
void WebView::setMessageHandler(MessageHandler handler)
{
    impl_->messageHandler = std::move(handler);
    [impl_->webView.configuration.userContentController addScriptMessageHandlerWithReply:impl_->delegate contentWorld:WKContentWorld.pageWorld name:@"credentials"];
}
