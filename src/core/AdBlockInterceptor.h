#pragma once

#include <QStringList>
#include <QUrl>
#include <QWebEngineUrlRequestInterceptor>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class AdBlockInterceptor final : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit AdBlockInterceptor(QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;
    void setRules(const QStringList &blockedDomains, const QStringList &allowedDomains = {});
    void setCompiledRules(std::vector<quint64> blocked, std::vector<quint64> allowed = {});
    [[nodiscard]] static quint64 hashDomain(QStringView domain);
    [[nodiscard]] quint64 blockedCount() const;
    [[nodiscard]] qsizetype ruleCount() const;
    [[nodiscard]] bool wouldBlock(const QUrl &url, bool mainFrame = false) const;

private:
    struct Rules {
        std::vector<quint64> blockedDomains;
        std::vector<quint64> allowedDomains;
    };

    [[nodiscard]] bool isBlockedHost(const std::string &host,
                                     const std::shared_ptr<const Rules> &rules) const;

    std::atomic_bool enabled_ {true};
    std::atomic<quint64> blockedCount_ {0};
    std::atomic<std::shared_ptr<const Rules>> rules_;
};
