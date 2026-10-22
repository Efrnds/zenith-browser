#include "core/WebProfile.h"

#include "core/AdBlockInterceptor.h"
#include "core/BrowserData.h"
#include "core/DownloadManager.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QUuid>
#include <QPointer>

#undef signals
#undef slots
#import <WebKit/WebKit.h>

struct WebProfile::Native
{
    WKWebsiteDataStore *dataStore = nil;
    WKContentRuleListStore *ruleStore = nil;
    WKContentRuleList *ruleList = nil;
    quint64 generation = 0;
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

    if (ephemeral_) {
        native_->dataStore = [[WKWebsiteDataStore nonPersistentDataStore] retain];
    } else {
        const QByteArray identity = QFile::encodeName(QDir(dataDirectory).absolutePath());
        const QByteArray digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).left(16);
        const QByteArray uuid = QUuid::fromRfc4122(digest).toString(QUuid::WithoutBraces).toLatin1();
        NSString *uuidText = [[NSString alloc] initWithBytes:uuid.constData()
            length:static_cast<NSUInteger>(uuid.size()) encoding:NSASCIIStringEncoding];
        NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidText];
        native_->dataStore = [[WKWebsiteDataStore dataStoreForIdentifier:uuid] retain];
        [uuidText release];
        [uuid release];
    }
    native_->ruleStore = [[WKContentRuleListStore defaultStore] retain];
}

WebProfile::~WebProfile()
{
    delete downloads_;
    if (native_->ruleList) [native_->ruleList release];
    if (native_->ruleStore) [native_->ruleStore release];
    if (native_->dataStore) [native_->dataStore release];
}

void *WebProfile::nativeHandle() const { return native_->dataStore; }
void *WebProfile::nativeContentFilter() const { return native_->ruleList; }

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

AdBlockInterceptor *WebProfile::requestBlocker() const { return requestBlocker_; }
void WebProfile::whenCookiesReady(const std::function<void()> &callback) { callback(); }
void WebProfile::markCookiesReady() { cookiesReady_ = true; }
void WebProfile::trustCertificate(const QByteArray &, const QString &, bool) {}
BrowserData *WebProfile::data() const { return data_.get(); }
DownloadManager *WebProfile::downloads() const { return downloads_; }
bool WebProfile::isPrivate() const { return ephemeral_; }
QStringList WebProfile::certificateExceptions() const { return {}; }
bool WebProfile::removeCertificateExceptions() { return true; }
bool WebProfile::hasCertificateException(const QUrl &) const { return false; }
void WebProfile::setPrivacyPolicy(bool, bool) {}
QString WebProfile::engineVersion() { return QStringLiteral("WebKit WebView (macOS)"); }

void WebProfile::clearWebsiteData(const bool cookies, const bool cache,
                                  const std::function<void(bool)> &done)
{
    NSSet<NSString *> *types = [WKWebsiteDataStore allWebsiteDataTypes];
    if (!cookies || !cache) {
        NSMutableSet<NSString *> *selected = [NSMutableSet set];
        if (cookies) {
            [selected addObject:WKWebsiteDataTypeCookies];
            [selected addObject:WKWebsiteDataTypeLocalStorage];
            [selected addObject:WKWebsiteDataTypeIndexedDBDatabases];
            [selected addObject:WKWebsiteDataTypeServiceWorkerRegistrations];
        }
        if (cache) {
            [selected addObject:WKWebsiteDataTypeMemoryCache];
            [selected addObject:WKWebsiteDataTypeDiskCache];
            [selected addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
        }
        types = selected;
    }
    if (types.count == 0) {
        done(true);
        return;
    }
    [native_->dataStore removeDataOfTypes:types
        modifiedSince:[NSDate dateWithTimeIntervalSince1970:0]
        completionHandler:^{ done(true); }];
}

void WebProfile::rebuildContentFilter()
{
    const quint64 generation = ++native_->generation;
    const QByteArray json = requestBlocker_ && requestBlocker_->isEnabled()
        ? requestBlocker_->contentFilter() : QByteArrayLiteral("[]");
    if (json == "[]") {
        if (native_->ruleList) [native_->ruleList release];
        native_->ruleList = nil;
        contentFilterPending_ = false;
        emit contentFilterChanged();
        return;
    }
    contentFilterPending_ = true;
    NSString *source = [[NSString alloc] initWithBytes:json.constData()
        length:static_cast<NSUInteger>(json.size()) encoding:NSUTF8StringEncoding];
    const QString identifier = QStringLiteral("zenith-adblock-%1").arg(generation);
    NSString *name = [[NSString alloc] initWithCharacters:
        reinterpret_cast<const unichar *>(identifier.utf16()) length:identifier.size()];
    QPointer<WebProfile> guard(this);
    [native_->ruleStore compileContentRuleListForIdentifier:name encodedContentRuleList:source
        completionHandler:^(WKContentRuleList *ruleList, NSError *) {
            if (!guard || generation != guard->native_->generation) {
                [name release];
                [source release];
                return;
            }
            if (guard->native_->ruleList) [guard->native_->ruleList release];
            guard->native_->ruleList = [ruleList retain];
            guard->contentFilterPending_ = false;
            emit guard->contentFilterChanged();
            [name release];
            [source release];
        }];
}
