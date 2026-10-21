#include "core/WebProfile.h"
#include "core/BrowserData.h"
#include "core/DownloadManager.h"
#include "core/AdBlockInterceptor.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QPointer>

#undef signals
#undef slots
#include <webkit2/webkit2.h>
#include <gio/gio.h>

void zenithCookiesLoaded(GObject *source, GAsyncResult *result, gpointer user);

WebProfile::WebProfile(const QString &dataDirectory, const QString &cacheDirectory, QObject *parent, bool ephemeral)
    : QObject(parent)
    , exceptionsFile_(dataDirectory + QStringLiteral("/tls-exceptions.conf"))
    , ephemeral_(ephemeral)
{
    gtk_init_check(nullptr, nullptr);
    const QByteArray filterPath = QFile::encodeName(cacheDirectory + QStringLiteral("/content-filters"));
    filterStore_ = webkit_user_content_filter_store_new(filterPath.constData());
    if (!ephemeral_) {
        QDir().mkpath(dataDirectory);
        QDir().mkpath(cacheDirectory);
    }
    data_ = std::make_unique<BrowserData>(ephemeral_ ? QStringLiteral(":memory:") : dataDirectory + QStringLiteral("/browser.sqlite"));

    const QByteArray dataPath = QFile::encodeName(dataDirectory);
    const QByteArray cachePath = QFile::encodeName(cacheDirectory);
    dataManager_ = ephemeral_ ? webkit_website_data_manager_new_ephemeral() : webkit_website_data_manager_new(
        "base-data-directory", dataPath.constData(),
        "base-cache-directory", cachePath.constData(),
        nullptr);
    context_ = webkit_web_context_new_with_website_data_manager(dataManager_);
    webkit_web_context_set_cache_model(context_, WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER);
    webkit_web_context_set_sandbox_enabled(context_, TRUE);
    downloads_ = new DownloadManager(context_, this, ephemeral_ ? QString() : dataDirectory + QStringLiteral("/downloads.json"));
    const QString gstPlugins = QDir::homePath() + QStringLiteral("/.local/lib/zenith/gstreamer-1.0");
    if (QDir(gstPlugins).exists())
        webkit_web_context_add_path_to_sandbox(context_, QFile::encodeName(gstPlugins).constData(), TRUE);
    const QString runtime = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const auto allowRuntime = [&](const QString &name) {
        if (runtime.isEmpty())
            return;
        const QString path = runtime + QLatin1Char('/') + name;
        if (QFileInfo::exists(path))
            webkit_web_context_add_path_to_sandbox(context_, QFile::encodeName(path).constData(), FALSE);
    };
    allowRuntime(QStringLiteral("pulse"));
    allowRuntime(QStringLiteral("pipewire-0"));
    if (QFileInfo::exists(QStringLiteral("/dev/snd")))
        webkit_web_context_add_path_to_sandbox(context_, "/dev/snd", FALSE);
    const QString faviconDirectory = dataDirectory + QStringLiteral("/favicons");
    if (!ephemeral_)
        QDir().mkpath(faviconDirectory);
    const QByteArray faviconPath = QFile::encodeName(faviconDirectory);
    webkit_web_context_set_favicon_database_directory(context_, ephemeral_ ? nullptr : faviconPath.constData());

    const QByteArray cookiePath = QFile::encodeName(dataDirectory + QStringLiteral("/cookies.sqlite"));
    WebKitCookieManager *cookies = webkit_website_data_manager_get_cookie_manager(dataManager_);
    if (!ephemeral_)
        webkit_cookie_manager_set_persistent_storage(
            cookies, cookiePath.constData(), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    webkit_cookie_manager_set_accept_policy(cookies, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    webkit_website_data_manager_set_tls_errors_policy(dataManager_, WEBKIT_TLS_ERRORS_POLICY_FAIL);
    webkit_website_data_manager_set_itp_enabled(dataManager_, FALSE);
    QSettings privacy;
    setPrivacyPolicy(privacy.value(QStringLiteral("privacy/blockThirdPartyCookies"), false).toBool(),
        privacy.value(QStringLiteral("privacy/trackingPrevention"), false).toBool());
    webkit_website_data_manager_set_persistent_credential_storage_enabled(dataManager_, !ephemeral_);
    webkit_cookie_manager_get_all_cookies(cookies, nullptr, zenithCookiesLoaded, new QPointer<WebProfile>(this));

    settings_ = createSettings();

    if (ephemeral_)
        return;
    QSettings store(exceptionsFile_, QSettings::IniFormat);
    const QStringList hosts = store.childKeys();
    for (const QString &host : hosts) {
        const QByteArray pem = QByteArray::fromBase64(store.value(host).toString().toLatin1());
        trustCertificate(pem, host, false);
    }
}

_WebKitSettings *WebProfile::createSettings() const
{
    auto *settings_ = webkit_settings_new();
    webkit_settings_set_enable_site_specific_quirks(settings_, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(settings_, FALSE);
    webkit_settings_set_media_playback_requires_user_gesture(settings_, TRUE);
    webkit_settings_set_enable_developer_extras(settings_, TRUE);
    webkit_settings_set_enable_fullscreen(settings_, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings_, TRUE);
    webkit_settings_set_enable_webgl(settings_, TRUE);
    webkit_settings_set_enable_webaudio(settings_, TRUE);
    webkit_settings_set_enable_mediasource(settings_, TRUE);
    webkit_settings_set_enable_page_cache(settings_, FALSE);
    // The release build paints through the accelerated backing store whenever
    // compositing is on, and that store is null when WebKit refuses acceleration.
    // NEVER sticks. ALWAYS is ignored in that case, so compositing stays off.
    webkit_settings_set_hardware_acceleration_policy(
        settings_, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    webkit_settings_set_hardware_acceleration_policy(
        settings_, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);

    return settings_;
}

void WebProfile::trustCertificate(const QByteArray &pem, const QString &host, const bool persist)
{
    if (!context_ || pem.isEmpty() || host.isEmpty())
        return;
    GError *error = nullptr;
    GTlsCertificate *certificate = g_tls_certificate_new_from_pem(pem.constData(), pem.size(), &error);
    if (!certificate) {
        if (error)
            g_error_free(error);
        return;
    }
    const QByteArray hostBytes = host.toUtf8();
    webkit_web_context_allow_tls_certificate_for_host(context_, certificate, hostBytes.constData());
    trustedHosts_.insert(host.toLower());
    g_object_unref(certificate);
    if (!persist || ephemeral_)
        return;
    QSettings store(exceptionsFile_, QSettings::IniFormat);
    store.setValue(host, QString::fromLatin1(pem.toBase64()));
    store.sync();
}

WebProfile::~WebProfile()
{
    if (contentFilter_) webkit_user_content_filter_unref(contentFilter_);
    if (filterStore_) g_object_unref(filterStore_);
    delete downloads_;
    if (settings_)
        g_object_unref(settings_);
    if (context_)
        g_object_unref(context_);
    if (dataManager_)
        g_object_unref(dataManager_);
}

BrowserData *WebProfile::data() const { return data_.get(); }
QString WebProfile::engineVersion()
{
    return QStringLiteral("%1.%2.%3").arg(webkit_get_major_version()).arg(webkit_get_minor_version()).arg(webkit_get_micro_version());
}
DownloadManager *WebProfile::downloads() const { return downloads_; }
bool WebProfile::isPrivate() const { return ephemeral_; }
bool WebProfile::hasCertificateException(const QUrl &url) const { return trustedHosts_.contains(url.host().toLower()); }
void WebProfile::setPrivacyPolicy(bool blockThirdParty, bool trackingPrevention)
{
    webkit_cookie_manager_set_accept_policy(webkit_website_data_manager_get_cookie_manager(dataManager_),
        blockThirdParty ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY : WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    webkit_website_data_manager_set_itp_enabled(dataManager_, trackingPrevention);
}

QStringList WebProfile::certificateExceptions() const
{
    if (ephemeral_)
        return {};
    QSettings store(exceptionsFile_, QSettings::IniFormat);
    return store.childKeys();
}

bool WebProfile::removeCertificateExceptions()
{
    // WebKit cannot revoke certificates on a live context; the UI requests a restart.
    return ephemeral_ || !QFile::exists(exceptionsFile_) || QFile::remove(exceptionsFile_);
}

void WebProfile::clearWebsiteData(bool cookies, bool cache, const std::function<void(bool)> &done)
{
    WebKitWebsiteDataTypes types = static_cast<WebKitWebsiteDataTypes>(0);
    if (cookies)
        types = static_cast<WebKitWebsiteDataTypes>(types | WEBKIT_WEBSITE_DATA_COOKIES | WEBKIT_WEBSITE_DATA_LOCAL_STORAGE | WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES | WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS);
    if (cache)
        types = static_cast<WebKitWebsiteDataTypes>(types | WEBKIT_WEBSITE_DATA_MEMORY_CACHE | WEBKIT_WEBSITE_DATA_DISK_CACHE | WEBKIT_WEBSITE_DATA_DOM_CACHE);
    if (!types) {
        done(true);
        return;
    }
    auto *callback = new std::function<void(bool)>(done);
    webkit_website_data_manager_clear(dataManager_, types, 0, nullptr,
        +[](GObject *source, GAsyncResult *result, gpointer user) {
            std::unique_ptr<std::function<void(bool)>> callback(static_cast<std::function<void(bool)> *>(user));
            GError *error = nullptr;
            const bool ok = webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, &error);
            if (error)
                g_error_free(error);
            (*callback)(ok);
        }, callback);
}

_WebKitWebContext *WebProfile::context() const
{
    return context_;
}

_WebKitSettings *WebProfile::settings() const
{
    return settings_;
}

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

_WebKitUserContentFilter *WebProfile::contentFilter() const
{
    return requestBlocker_ && requestBlocker_->isEnabled() ? contentFilter_ : nullptr;
}

void WebProfile::rebuildContentFilter()
{
    ++filterGeneration_;
    contentFilterPending_ = requestBlocker_ && requestBlocker_->isEnabled();
    if (!contentFilterPending_) {
        emit contentFilterChanged();
        return;
    }
    const QByteArray json = requestBlocker_->contentFilter();
    if (json == "[]") {
        if (contentFilter_) webkit_user_content_filter_unref(contentFilter_);
        contentFilter_ = nullptr;
        contentFilterPending_ = false;
        emit contentFilterChanged();
        return;
    }
    struct Pending { QPointer<WebProfile> profile; quint64 generation; };
    GBytes *source = g_bytes_new(json.constData(), json.size());
    webkit_user_content_filter_store_save(filterStore_, "zenith-adblock", source, nullptr,
        +[](GObject *store, GAsyncResult *result, gpointer user) {
            std::unique_ptr<Pending> pending(static_cast<Pending *>(user));
            GError *error = nullptr;
            auto *filter = webkit_user_content_filter_store_save_finish(
                WEBKIT_USER_CONTENT_FILTER_STORE(store), result, &error);
            if (auto *profile = pending->profile.data(); profile && profile->filterGeneration_ == pending->generation) {
                profile->contentFilterPending_ = false;
                if (filter) {
                    if (profile->contentFilter_) webkit_user_content_filter_unref(profile->contentFilter_);
                    profile->contentFilter_ = filter;
                    filter = nullptr;
                }
                if (error) qWarning("Adblock: native filter compilation failed: %s", error->message);
                emit profile->contentFilterChanged();
            }
            if (filter) webkit_user_content_filter_unref(filter);
            if (error) g_error_free(error);
        }, new Pending{this, filterGeneration_});
    g_bytes_unref(source);
}

AdBlockInterceptor *WebProfile::requestBlocker() const
{
    return requestBlocker_;
}

void zenithCookiesLoaded(GObject *source, GAsyncResult *result, gpointer user)
{
    GError *error = nullptr;
    GList *cookies = webkit_cookie_manager_get_all_cookies_finish(
        WEBKIT_COOKIE_MANAGER(source), result, &error);
    if (error)
        g_error_free(error);
    if (cookies)
        g_list_free_full(cookies, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
    std::unique_ptr<QPointer<WebProfile>> profile(static_cast<QPointer<WebProfile> *>(user));
    if (*profile) (*profile)->markCookiesReady();
}

void WebProfile::markCookiesReady()
{
    if (cookiesReady_)
        return;
    cookiesReady_ = true;
    if (cookiesReadyCallback_) {
        const auto callback = std::move(cookiesReadyCallback_);
        cookiesReadyCallback_ = nullptr;
        callback();
    }
}

void WebProfile::whenCookiesReady(const std::function<void()> &callback)
{
    if (cookiesReady_) {
        callback();
        return;
    }
    cookiesReadyCallback_ = callback;
}
