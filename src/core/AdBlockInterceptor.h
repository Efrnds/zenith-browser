#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

#include <memory>
#include <string>
#include <vector>

class AdBlockInterceptor final : public QObject
{
    Q_OBJECT

public:
    explicit AdBlockInterceptor(QObject *parent = nullptr);

    [[nodiscard]] bool reject(const QUrl &url, bool mainFrame);
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;
    void setRules(const QStringList &blockedDomains, const QStringList &allowedDomains = {});
    void setCompiledRules(std::vector<quint64> blocked, std::vector<quint64> allowed = {});
    [[nodiscard]] static quint64 hashDomain(QStringView domain);
    [[nodiscard]] quint64 blockedCount() const;
    [[nodiscard]] qsizetype ruleCount() const;
    [[nodiscard]] bool wouldBlock(const QUrl &url, bool mainFrame = false) const;
    [[nodiscard]] QByteArray contentFilter() const;
    [[nodiscard]] static QStringList builtInDomains();

signals:
    void rulesChanged();
    void enabledChanged();

private:
    struct Rules {
        std::vector<quint64> blockedDomains;
        std::vector<quint64> allowedDomains;
        QStringList blockedNames;
        QStringList allowedNames;
    };

    [[nodiscard]] bool isBlockedHost(const std::string &host,
                                     const std::shared_ptr<const Rules> &rules) const;

    std::atomic_bool enabled_ {true};
    std::atomic<quint64> blockedCount_ {0};
    std::shared_ptr<const Rules> rules_;
};
