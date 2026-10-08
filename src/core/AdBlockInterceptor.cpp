#include "core/AdBlockInterceptor.h"

#include <QUrl>
#include <QWebEngineUrlRequestInfo>

#include <algorithm>
#include <cctype>

AdBlockInterceptor::AdBlockInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    setRules({
        QStringLiteral("2mdn.net"),
        QStringLiteral("adcolony.com"),
        QStringLiteral("adform.net"),
        QStringLiteral("adnxs.com"),
        QStringLiteral("adsrvr.org"),
        QStringLiteral("advertising.com"),
        QStringLiteral("amazon-adsystem.com"),
        QStringLiteral("casalemedia.com"),
        QStringLiteral("criteo.com"),
        QStringLiteral("doubleclick.net"),
        QStringLiteral("google-analytics.com"),
        QStringLiteral("googleadservices.com"),
        QStringLiteral("googlesyndication.com"),
        QStringLiteral("googletagmanager.com"),
        QStringLiteral("media.net"),
        QStringLiteral("openx.net"),
        QStringLiteral("outbrain.com"),
        QStringLiteral("pubmatic.com"),
        QStringLiteral("rubiconproject.com"),
        QStringLiteral("scorecardresearch.com"),
        QStringLiteral("smartadserver.com"),
        QStringLiteral("taboola.com"),
    });
}

void AdBlockInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!enabled_.load(std::memory_order_relaxed)) {
        return;
    }

    if (wouldBlock(info.requestUrl(),
                   info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)) {
        blockedCount_.fetch_add(1, std::memory_order_relaxed);
        info.block(true);
    }
}

void AdBlockInterceptor::setEnabled(const bool enabled)
{
    enabled_.store(enabled, std::memory_order_relaxed);
}

bool AdBlockInterceptor::isEnabled() const
{
    return enabled_.load(std::memory_order_relaxed);
}

void AdBlockInterceptor::setRules(const QStringList &blockedDomains,
                                  const QStringList &allowedDomains)
{
    const auto compile = [](const QStringList &source) {
        std::vector<quint64> destination;
        destination.reserve(static_cast<std::size_t>(source.size()));
        for (QString domain : source) {
            domain = domain.trimmed().toLower();
            while (domain.startsWith('.'))
                domain.remove(0, 1);
            while (domain.endsWith('.'))
                domain.chop(1);
            if (!domain.isEmpty())
                destination.push_back(hashDomain(domain));
        }
        std::ranges::sort(destination);
        destination.erase(std::unique(destination.begin(), destination.end()), destination.end());
        return destination;
    };
    setCompiledRules(compile(blockedDomains), compile(allowedDomains));
}

void AdBlockInterceptor::setCompiledRules(std::vector<quint64> blocked,
                                          std::vector<quint64> allowed)
{
    std::ranges::sort(blocked);
    blocked.erase(std::unique(blocked.begin(), blocked.end()), blocked.end());
    std::ranges::sort(allowed);
    allowed.erase(std::unique(allowed.begin(), allowed.end()), allowed.end());
    auto rules = std::make_shared<Rules>();
    rules->blockedDomains = std::move(blocked);
    rules->allowedDomains = std::move(allowed);
    rules_.store(std::move(rules), std::memory_order_release);
}

quint64 AdBlockInterceptor::hashDomain(const QStringView domain)
{
    const QByteArray bytes = domain.toString().toLower().toUtf8();
    quint64 hash = 14695981039346656037ULL;
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

quint64 AdBlockInterceptor::blockedCount() const
{
    return blockedCount_.load(std::memory_order_relaxed);
}

qsizetype AdBlockInterceptor::ruleCount() const
{
    const auto rules = rules_.load(std::memory_order_acquire);
    return rules ? static_cast<qsizetype>(rules->blockedDomains.size()) : 0;
}

bool AdBlockInterceptor::wouldBlock(const QUrl &url, const bool mainFrame) const
{
    auto host = url.host().toStdString();
    std::ranges::transform(host, host.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    const QString path = url.path().toLower();
    const bool blockedPath = !mainFrame
        && (path.contains(QStringLiteral("/ads/"))
            || path.contains(QStringLiteral("/adserver/"))
            || path.contains(QStringLiteral("/advertisement/"))
            || path.endsWith(QStringLiteral("/ads.js")));
    return isBlockedHost(host, rules_.load(std::memory_order_acquire)) || blockedPath;
}

bool AdBlockInterceptor::isBlockedHost(
    const std::string &host, const std::shared_ptr<const Rules> &rules) const
{
    if (host.empty() || !rules) {
        return false;
    }

    const auto matches = [](const std::string &candidate,
                            const std::vector<quint64> &domains) {
        const quint64 hash = hashDomain(QString::fromStdString(candidate));
        return std::binary_search(domains.begin(), domains.end(), hash);
    };
    for (std::string candidate = host;;) {
        if (matches(candidate, rules->allowedDomains))
            return false;
        const auto dot = candidate.find('.');
        if (dot == std::string::npos)
            break;
        candidate.erase(0, dot + 1);
    }
    if (host.starts_with("ads.") || host.starts_with("adserver.")
        || host.starts_with("analytics.") || host.find(".ads.") != std::string::npos
        || host.find(".adserver.") != std::string::npos
        || host.find(".analytics.") != std::string::npos) {
        return true;
    }
    for (std::string candidate = host;;) {
        if (matches(candidate, rules->blockedDomains))
            return true;
        const auto dot = candidate.find('.');
        if (dot == std::string::npos)
            return false;
        candidate.erase(0, dot + 1);
    }
}
