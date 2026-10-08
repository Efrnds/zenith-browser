#pragma once

#include <QByteArray>
#include <QStringList>

#include <vector>

struct ParsedAdBlockRules {
    QStringList blockedDomains;
    QStringList allowedDomains;
};

struct CompiledAdBlockRules {
    std::vector<quint64> blockedDomains;
    std::vector<quint64> allowedDomains;
};

class AdBlockRuleParser
{
public:
    [[nodiscard]] static ParsedAdBlockRules parse(const QByteArray &content);
    [[nodiscard]] static QByteArray serialize(const ParsedAdBlockRules &rules);
    [[nodiscard]] static ParsedAdBlockRules deserialize(const QByteArray &content);
    [[nodiscard]] static CompiledAdBlockRules compile(const ParsedAdBlockRules &rules);
    [[nodiscard]] static QByteArray serializeCompiled(const CompiledAdBlockRules &rules);
    [[nodiscard]] static CompiledAdBlockRules deserializeCompiled(const QByteArray &content);
};
