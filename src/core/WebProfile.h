#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QSet>
#include <QUrl>

#include <functional>
#include <memory>

class AdBlockInterceptor;
class BrowserData;
class DownloadManager;

struct _WebKitSettings;
struct _WebKitWebContext;
struct _WebKitUserContentFilter;
struct _WebKitUserContentFilterStore;

class WebProfile final : public QObject
{
    Q_OBJECT
public:
    WebProfile(const QString &dataDirectory, const QString &cacheDirectory, QObject *parent = nullptr, bool ephemeral = false);
    ~WebProfile() override;

    [[nodiscard]] _WebKitWebContext *context() const;
    [[nodiscard]] _WebKitSettings *settings() const;
    [[nodiscard]] _WebKitSettings *createSettings() const;

    void setRequestBlocker(AdBlockInterceptor *blocker);
    [[nodiscard]] AdBlockInterceptor *requestBlocker() const;
    [[nodiscard]] _WebKitUserContentFilter *contentFilter() const;
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
    _WebKitUserContentFilterStore *filterStore_ = nullptr;
    _WebKitUserContentFilter *contentFilter_ = nullptr;
    quint64 filterGeneration_ = 0;
    bool contentFilterPending_ = false;
    struct _WebKitWebsiteDataManager *dataManager_ = nullptr;
    _WebKitWebContext *context_ = nullptr;
    _WebKitSettings *settings_ = nullptr;
    AdBlockInterceptor *requestBlocker_ = nullptr;
    bool cookiesReady_ = false;
    std::function<void()> cookiesReadyCallback_;
    QString exceptionsFile_;
    bool ephemeral_ = false;
    std::unique_ptr<BrowserData> data_;
    DownloadManager *downloads_ = nullptr;
    QSet<QString> trustedHosts_;
};
