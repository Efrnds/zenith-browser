#include "core/WebView.h"
#include "core/WebProfile.h"
#include "core/DownloadManager.h"
#include "core/AdBlockInterceptor.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QPointer>
#include <QJsonArray>
#include <QTimer>
#include <QSettings>

#undef signals
#undef slots
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
WebView::KeySink g_keySink;
std::function<void()> g_keySinkCancelled;

std::wstring wide(const QString &value) { return value.toStdWString(); }
QString string(const LPWSTR value)
{
    if (!value) return {};
    const QString result = QString::fromWCharArray(value);
    CoTaskMemFree(value);
    return result;
}
}

struct WebView::Impl
{
    WebView *owner = nullptr;
    WebProfile *profile = nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webView;
    ComPtr<ICoreWebView2_4> webView4;
    PopupFactory popupFactory;
    MessageHandler messageHandler;
    QString pendingUrl;
    QString findText;
    QUrl currentUrl;
    QString pageTitle;
    QIcon pageIcon;
    bool visible = true;
    bool focusEnabled = true;
    bool loading = false;
    bool muted = false;
    double zoom = 1.0;
    QList<QString> scripts;

    explicit Impl(WebView *view, WebProfile *webProfile) : owner(view), profile(webProfile) {}

    void initialize()
    {
        environment = static_cast<ICoreWebView2Environment *>(profile->nativeHandle());
        if (!environment) {
            emit owner->loadError(QObject::tr("O runtime Microsoft Edge WebView2 não está disponível."));
            return;
        }
        QPointer<WebView> guard(owner);
        const HRESULT result = environment->CreateCoreWebView2Controller(
            reinterpret_cast<HWND>(owner->winId()),
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [this, guard](HRESULT status, ICoreWebView2Controller *created) -> HRESULT {
                    if (!guard) return S_OK;
                    if (FAILED(status) || !created) {
                        emit owner->loadError(QObject::tr("Não foi possível inicializar o WebView2."));
                        return S_OK;
                    }
                    controller = created;
                    controller->get_CoreWebView2(&webView);
                    if (!webView) return S_OK;
                    webView.As(&webView4);
                    ComPtr<ICoreWebView2Settings> settings;
                    if (SUCCEEDED(webView->get_Settings(&settings)) && settings) {
                        settings->put_AreDefaultContextMenusEnabled(TRUE);
                        settings->put_IsScriptEnabled(TRUE);
                        settings->put_AreDevToolsEnabled(TRUE);
                    }
                    attachEvents();
                    updateBounds();
                    applyBrowsingPreferences();
                    for (const QString &source : std::as_const(scripts))
                        webView->AddScriptToExecuteOnDocumentCreated(wide(source).c_str(), nullptr);
                    if (!pendingUrl.isEmpty()) webView->Navigate(wide(pendingUrl).c_str());
                    return S_OK;
                }).Get());
        if (FAILED(result))
            emit owner->loadError(QObject::tr("Falha ao criar o controle WebView2 (%1).")
                .arg(qulonglong(quint32(result)), 8, 16, QLatin1Char('0')));
    }

    void updateBounds()
    {
        if (!controller) return;
        RECT bounds {0, 0, owner->width(), owner->height()};
        controller->put_Bounds(bounds);
        controller->put_IsVisible(visible ? TRUE : FALSE);
    }

    void attachEvents()
    {
        QPointer<WebView> guard(owner);
        EventRegistrationToken token{};
        webView->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2SourceChangedEventArgs *) -> HRESULT {
                if (!guard || !webView) return S_OK;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(webView->get_Source(&uri))) {
                    currentUrl = QUrl(string(uri));
                    emit owner->urlChanged(currentUrl);
                }
                return S_OK;
            }).Get(), &token);
        webView->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
            [this, guard](ICoreWebView2 *, IUnknown *) -> HRESULT {
                if (!guard || !webView) return S_OK;
                LPWSTR title = nullptr;
                if (SUCCEEDED(webView->get_DocumentTitle(&title))) {
                    pageTitle = string(title).left(512);
                    emit owner->titleChanged(pageTitle);
                }
                return S_OK;
            }).Get(), &token);
        webView->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
                if (!guard) return S_OK;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&uri))) {
                    const QUrl target(string(uri));
                    if ((target.scheme() == "javascript" || target.scheme() == "data") && !target.isValid()) {
                        args->put_Cancel(TRUE);
                        return S_OK;
                    }
                    currentUrl = target;
                    emit owner->urlChanged(currentUrl);
                }
                loading = true;
                emit owner->loadProgress(0);
                return S_OK;
            }).Get(), &token);
        webView->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                if (!guard) return S_OK;
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                loading = false;
                emit owner->loadProgress(100);
                emit owner->loadFinished(success);
                if (!success) emit owner->loadError(QObject::tr("A navegação não foi concluída."));
                return S_OK;
            }).Get(), &token);
        webView->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
                if (!guard) return S_OK;
                LPWSTR uri = nullptr;
                args->get_Uri(&uri);
                const QUrl target(string(uri));
                args->put_Handled(TRUE);
                emit owner->newTabRequested(target, true);
                return S_OK;
            }).Get(), &token);
        webView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                if (!guard || !messageHandler) return S_OK;
                LPWSTR json = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&json)))
                    messageHandler(string(json));
                return S_OK;
            }).Get(), &token);
        webView->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        webView->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
                if (!guard || !profile->requestBlocker() || !profile->requestBlocker()->isEnabled()) return S_OK;
                ComPtr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request) return S_OK;
                LPWSTR uri = nullptr;
                if (FAILED(request->get_Uri(&uri))) return S_OK;
                const QUrl target(string(uri));
                if (!profile->requestBlocker()->wouldBlock(target, false)) return S_OK;
                ComPtr<ICoreWebView2WebResourceResponse> response;
                if (SUCCEEDED(environment->CreateWebResourceResponse(nullptr, 403, L"Blocked",
                    L"Content-Type: text/plain", &response)))
                    args->put_Response(response.Get());
                return S_OK;
            }).Get(), &token);
        if (!webView4) return;
        webView4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
            [this, guard](ICoreWebView2 *, ICoreWebView2DownloadStartingEventArgs *args) -> HRESULT {
                if (!guard) return S_OK;
                LPWSTR name = nullptr;
                args->get_ResultFilePath(&name);
                QString suggested = string(name);
                if (suggested.isEmpty()) {
                    ComPtr<ICoreWebView2DownloadOperation> operation;
                    LPWSTR address = nullptr;
                    if (SUCCEEDED(args->get_DownloadOperation(&operation)) && operation
                        && SUCCEEDED(operation->get_Uri(&address)))
                        suggested = QFileInfo(QUrl(string(address)).path()).fileName();
                }
                ComPtr<ICoreWebView2DownloadOperation> operation;
                args->get_DownloadOperation(&operation);
                const int id = profile->downloads()->beginNativeDownload(suggested,
                    [args](const QString &path, bool) {
                        const std::wstring nativePath = wide(path);
                        return SUCCEEDED(args->put_ResultFilePath(nativePath.c_str()));
                    }, [operation] { if (operation) operation->Cancel(); });
                if (id < 0) {
                    args->put_Cancel(TRUE);
                    args->put_Handled(TRUE);
                    return S_OK;
                }
                args->put_Handled(TRUE);
                auto *timer = new QTimer(owner);
                timer->setInterval(300);
                connect(timer, &QTimer::timeout, owner, [this, guard, timer, operation, id] {
                    if (!guard || !operation) { timer->stop(); timer->deleteLater(); return; }
                    INT64 received = 0, total = 0;
                    COREWEBVIEW2_DOWNLOAD_STATE state = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
                    operation->get_BytesReceived(&received);
                    operation->get_TotalBytesToReceive(&total);
                    operation->get_State(&state);
                    const bool done = state != COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
                    profile->downloads()->updateNativeDownload(id, qint64(received), qint64(total), done,
                        state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED ? QObject::tr("Download interrompido") : QString());
                    if (done) { timer->stop(); timer->deleteLater(); }
                });
                timer->start();
                return S_OK;
            }).Get(), nullptr);
    }

    void applyBrowsingPreferences()
    {
        if (!webView) return;
        ComPtr<ICoreWebView2Settings> settings;
        if (FAILED(webView->get_Settings(&settings)) || !settings) return;
        QSettings preferences;
        settings->put_IsScriptEnabled(preferences.value("web/javascript", true).toBool());
        settings->put_IsZoomControlEnabled(TRUE);
    }
};

WebView::WebView(WebProfile *profile, QWidget *parent, WebView *)
    : QWidget(parent), impl_(std::make_unique<Impl>(this, profile))
{
    setAttribute(Qt::WA_NativeWindow);
    setFocusPolicy(Qt::StrongFocus);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    impl_->initialize();
    connect(profile, &WebProfile::contentFilterChanged, this, [this] {
        if (impl_->webView) impl_->webView->Reload();
    });
}

WebView::~WebView() = default;
void WebView::setUrl(const QUrl &url) { if (impl_->webView) impl_->webView->Navigate(wide(url.toString()).c_str()); else impl_->pendingUrl = url.toString(); }
QString WebView::userAgent() const
{
    if (!impl_->webView) return {};
    ComPtr<ICoreWebView2Settings> baseSettings;
    if (FAILED(impl_->webView->get_Settings(&baseSettings)) || !baseSettings) return {};
    ComPtr<ICoreWebView2Settings2> settings;
    if (FAILED(baseSettings.As(&settings)) || !settings) return {};
    LPWSTR value = nullptr;
    if (FAILED(settings->get_UserAgent(&value))) return {};
    return string(value);
}
void WebView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_UNUSED(baseUrl);
    if (impl_->webView) impl_->webView->NavigateToString(wide(html).c_str());
}
bool WebView::engineAlive() const { return bool(impl_->webView); }
bool WebView::allowsCredentials() const { return url().scheme() == QStringLiteral("https"); }
QUrl WebView::url() const { return impl_->currentUrl; }
QString WebView::title() const { return impl_->pageTitle; }
QIcon WebView::icon() const { return impl_->pageIcon; }
bool WebView::canGoBack() const { BOOL result = FALSE; if (impl_->webView) impl_->webView->get_CanGoBack(&result); return result; }
bool WebView::canGoForward() const { BOOL result = FALSE; if (impl_->webView) impl_->webView->get_CanGoForward(&result); return result; }
void WebView::back() { if (impl_->webView) impl_->webView->GoBack(); }
void WebView::forward() { if (impl_->webView) impl_->webView->GoForward(); }
void WebView::reload(bool bypassCache) { if (impl_->webView) bypassCache ? impl_->webView->Reload() : impl_->webView->Reload(); }
void WebView::stop() { if (impl_->webView) impl_->webView->Stop(); }
void WebView::find(const QString &text, bool backwards)
{
    impl_->findText = text;
    if (!impl_->webView || text.isEmpty()) { emit findResult(0); return; }
    const QString escaped = QString::fromLatin1(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact));
    const QString script = QStringLiteral("(()=>{const t=%1.toLocaleLowerCase();const n=(document.body?document.body.innerText:'').toLocaleLowerCase();let c=0,p=0;while((p=n.indexOf(t,p))>=0){c++;p+=Math.max(1,t.length)}return c})()").arg(escaped);
    impl_->webView->ExecuteScript(wide(script).c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [guard = QPointer<WebView>(this)](HRESULT, LPCWSTR json) -> HRESULT {
            if (guard) emit guard->findResult(QString::fromWCharArray(json).toInt());
            return S_OK;
        }).Get());
    if (backwards) { /* Result count is unchanged; the native find UI owns direction. */ }
}
QString WebView::lastFindText() const { return impl_->findText; }
void WebView::finishFind() { impl_->findText.clear(); }
void WebView::applyBrowsingPreferences() { impl_->applyBrowsingPreferences(); }
void WebView::setZoom(double value) { impl_->zoom = std::clamp(value, 0.25, 5.0); if (impl_->controller) impl_->controller->put_ZoomFactor(impl_->zoom); }
double WebView::zoom() const { return impl_->zoom; }
void WebView::setMuted(bool muted) { impl_->muted = muted; if (impl_->webView) { ComPtr<ICoreWebView2_8> v8; if (SUCCEEDED(impl_->webView.As(&v8))) v8->put_IsMuted(muted); } emit audioChanged(); }
bool WebView::isMuted() const { return impl_->muted; }
bool WebView::isPlayingAudio() const
{
    if (!impl_->webView) return false;
    ComPtr<ICoreWebView2_8> media;
    BOOL playing = FALSE;
    return SUCCEEDED(impl_->webView.As(&media)) && SUCCEEDED(media->get_IsDocumentPlayingAudio(&playing)) && playing;
}
bool WebView::maySuspend() const { return engineAlive() && !impl_->loading && !isPlayingAudio(); }
void WebView::checkCanSuspend(const std::function<void(bool)> &callback) { callback(maySuspend()); }
void WebView::print()
{
    if (!impl_->webView) return;
    ComPtr<ICoreWebView2_16> printer;
    if (SUCCEEDED(impl_->webView.As(&printer)))
        printer->ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_BROWSER);
}
void WebView::inspect() { if (impl_->webView) impl_->webView->OpenDevToolsWindow(); }
void WebView::setPageFocusEnabled(bool enabled) { impl_->focusEnabled = enabled; setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus); }
void WebView::setEmbeddedVisible(bool visible) { impl_->visible = visible; impl_->updateBounds(); setVisible(visible); }
void WebView::parkSurface() { setEmbeddedVisible(false); }
void WebView::syncSurfaceSize() { impl_->updateBounds(); }
void WebView::reembed() { impl_->updateBounds(); }
void WebView::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); impl_->updateBounds(); }
void WebView::showEvent(QShowEvent *event) { QWidget::showEvent(event); impl_->updateBounds(); }
void WebView::setKeySink(KeySink sink, std::function<void()> cancelled) { g_keySink = std::move(sink); g_keySinkCancelled = std::move(cancelled); }
bool WebView::queueKeyToSink(QObject *, int key, Qt::KeyboardModifiers modifiers, const QString &text) { return g_keySink && g_keySink(key, modifiers, text); }
QByteArray WebView::sessionState() const { return QJsonDocument(QJsonObject{{"url", url().toString()}}).toJson(QJsonDocument::Compact); }
bool WebView::restoreSessionState(const QByteArray &state) { const auto object = QJsonDocument::fromJson(state).object(); const QUrl target(object.value("url").toString()); if (!target.isValid() || target.isEmpty()) return false; setUrl(target); return true; }
void WebView::setPopupFactory(PopupFactory factory) { impl_->popupFactory = std::move(factory); }
void WebView::installPageScript(const QString &source) { impl_->scripts.append(source); if (impl_->webView) impl_->webView->AddScriptToExecuteOnDocumentCreated(wide(source).c_str(), nullptr); }
void WebView::setMessageHandler(MessageHandler handler)
{
    impl_->messageHandler = std::move(handler);
    if (impl_->webView) impl_->webView->AddScriptToExecuteOnDocumentCreated(
        L"window.webkit=window.webkit||{};window.webkit.messageHandlers=window.webkit.messageHandlers||{};window.webkit.messageHandlers.credentials={postMessage:function(m){window.chrome.webview.postMessage(m);return Promise.resolve(null)}};", nullptr);
}
QString WebView::dispatchPlatformMessage(const QString &message)
{
    return impl_->messageHandler ? impl_->messageHandler(message) : QString();
}
