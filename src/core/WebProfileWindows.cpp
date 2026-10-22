#include "core/WebProfile.h"

#include "core/AdBlockInterceptor.h"
#include "core/BrowserData.h"
#include "core/DownloadManager.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#undef signals
#undef slots
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct WebProfile::Native
{
    ComPtr<ICoreWebView2Environment> environment;
};

WebProfile::WebProfile(const QString &dataDirectory, const QString &cacheDirectory,
                       QObject *parent, const bool ephemeral)
    : QObject(parent)
    , native_(std::make_unique<Native>())
    , exceptionsFile_(dataDirectory + QStringLiteral("/tls-exceptions.conf"))
    , ephemeral_(ephemeral)
{
    if (!ephemeral_) {
        QDir().mkpath(dataDirectory);
        QDir().mkpath(cacheDirectory);
    }
    data_ = std::make_unique<BrowserData>(ephemeral_ ? QStringLiteral(":memory:")
        : dataDirectory + QStringLiteral("/browser.sqlite"));
    downloads_ = new DownloadManager(this,
        ephemeral_ ? QString() : dataDirectory + QStringLiteral("/downloads.json"));

    const QString userData = ephemeral_ ? QString()
        : QDir(dataDirectory).absoluteFilePath(QStringLiteral("webview2"));
    const std::wstring userDataPath = QDir::toNativeSeparators(userData).toStdWString();
    QEventLoop loop;
    HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, ephemeral_ ? nullptr : userDataPath.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, &loop](HRESULT status, ICoreWebView2Environment *environment) -> HRESULT {
                if (SUCCEEDED(status) && environment)
                    native_->environment = environment;
                loop.quit();
                return S_OK;
            }).Get());
    if (FAILED(result))
        return;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(30000);
    loop.exec();
}

WebProfile::~WebProfile()
{
    delete downloads_;
}

void *WebProfile::nativeHandle() const { return native_->environment.Get(); }
void WebProfile::setRequestBlocker(AdBlockInterceptor *blocker)
{
    if (requestBlocker_) disconnect(requestBlocker_, nullptr, this, nullptr);
    requestBlocker_ = blocker;
    if (blocker) {
        connect(blocker, &AdBlockInterceptor::rulesChanged, this, &WebProfile::rebuildContentFilter);
        connect(blocker, &AdBlockInterceptor::enabledChanged, this, &WebProfile::rebuildContentFilter);
    }
    rebuildContentFilter();
}
void *WebProfile::nativeContentFilter() const { return nullptr; }
void WebProfile::rebuildContentFilter() { emit contentFilterChanged(); }
AdBlockInterceptor *WebProfile::requestBlocker() const { return requestBlocker_; }
void WebProfile::whenCookiesReady(const std::function<void()> &callback) { callback(); }
void WebProfile::markCookiesReady() { cookiesReady_ = true; }
void WebProfile::trustCertificate(const QByteArray &, const QString &, bool) {}
BrowserData *WebProfile::data() const { return data_.get(); }
DownloadManager *WebProfile::downloads() const { return downloads_; }
bool WebProfile::isPrivate() const { return ephemeral_; }
void WebProfile::clearWebsiteData(bool, bool, const std::function<void(bool)> &done) { done(false); }
QStringList WebProfile::certificateExceptions() const { return {}; }
bool WebProfile::removeCertificateExceptions() { return true; }
bool WebProfile::hasCertificateException(const QUrl &) const { return false; }
void WebProfile::setPrivacyPolicy(bool, bool) {}
QString WebProfile::engineVersion() { return QStringLiteral("WebView2 Evergreen"); }
