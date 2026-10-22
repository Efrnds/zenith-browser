#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QSet>
#include <QUrl>

#include <functional>
#include <memory>

#include <QtGlobal>

class AdBlockInterceptor;
class BrowserData;
class DownloadManager;

#ifdef Q_OS_LINUX
struct _WebKitSettings;
struct _WebKitWebContext;
struct _WebKitUserContentFilter;
struct _WebKitUserContentFilterStore;
#endif

class WebProfile final : public QObject
{
    Q_OBJECT
public:
    WebProfile(const QString &dataDirectory, const QString &cacheDirectory, QObject *parent = nullptr, bool ephemeral = false);
    ~WebProfile() override;

#ifdef Q_OS_LINUX
    [[nodiscard]] _WebKitWebContext *context() const;
    [[nodiscard]] _WebKitSettings *settings() const;
    [[nodiscard]] _WebKitSettings *createSettings() const;
#else
    [[nodiscard]] void *nativeHandle() const;
#endif

    void setRequestBlocker(AdBlockInterceptor *blocker);
    [[nodiscard]] AdBlockInterceptor *requestBlocker() const;
#ifdef Q_OS_LINUX
    [[nodiscard]] _WebKitUserContentFilter *contentFilter() const;
#endif
#if !defined(Q_OS_LINUX)
    [[nodiscard]] void *nativeContentFilter() const;
#endif
    [[nodiscard]] bool contentFilterPending() const { return contentFilterPending_; }
    void whenCookiesReady(const std::function<void()> &callback);
    void markCookiesReady();
    void trustCertificate(const QByteArray &pem, const QString &host, bool persist);
    BrowserData *data() const;
    DownloadManager *downloads() const;
    bool isPrivate() const;
    void clearWebsiteData(bool cookies, bool cache, const std::function<void(bool)> &done);
    QStringList certificateExceptions() const;
    bool removeCertificateExceptions();
    bool hasCertificateException(const QUrl &url) const;
    void setPrivacyPolicy(bool blockThirdParty, bool trackingPrevention);
    static QString engineVersion();

signals:
    void contentFilterChanged();

private:
    void rebuildContentFilter();
#ifdef Q_OS_LINUX
    _WebKitUserContentFilterStore *filterStore_ = nullptr;
    _WebKitUserContentFilter *contentFilter_ = nullptr;
    quint64 filterGeneration_ = 0;
    struct _WebKitWebsiteDataManager *dataManager_ = nullptr;
    _WebKitWebContext *context_ = nullptr;
    _WebKitSettings *settings_ = nullptr;
#else
    struct Native;
    std::unique_ptr<Native> native_;
#endif
    bool contentFilterPending_ = false;
    AdBlockInterceptor *requestBlocker_ = nullptr;
    bool cookiesReady_ = false;
    std::function<void()> cookiesReadyCallback_;
    QString exceptionsFile_;
    bool ephemeral_ = false;
    std::unique_ptr<BrowserData> data_;
    DownloadManager *downloads_ = nullptr;
    QSet<QString> trustedHosts_;
};
