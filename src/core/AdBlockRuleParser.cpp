#include "core/AdBlockRuleParser.h"
#include "core/AdBlockInterceptor.h"

#include <QDataStream>
#include <QIODevice>
#include <QSet>

#include <algorithm>
#include <utility>

namespace {
bool validDomain(const QString &domain)
{
    if (domain.isEmpty() || !domain.contains('.') || domain.size() > 253)
        return false;
    for (const QChar character : domain) {
        if (!character.isLetterOrNumber() && character != '.' && character != '-')
            return false;
    }
    return true;
}

QString domainFromRule(QString line)
{
    if (!line.startsWith(QStringLiteral("||")))
        return {};
    // Conditional ABP rules cannot be widened into global domain decisions.
    if (line.contains('$')) return {};
    line.remove(0, 2);
    if (line.contains('^') && !line.endsWith('^')) return {};
    qsizetype end = line.indexOf('^');
    const qsizetype option = line.indexOf('$');
    if (end < 0 || (option >= 0 && option < end))
        end = option;
    if (end >= 0)
        line.truncate(end);
    line = line.trimmed().toLower();
    return validDomain(line) ? line : QString();
}
}

ParsedAdBlockRules AdBlockRuleParser::parse(const QByteArray &content)
{
    QSet<QString> blocked;
    QSet<QString> allowed;
    const QList<QByteArray> lines = content.split('\n');
    for (QByteArray rawLine : lines) {
        QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty() || line.startsWith('!') || line.startsWith('[')
            || line.contains(QStringLiteral("##")) || line.contains(QStringLiteral("#@#"))) {
            continue;
        }

        const bool exception = line.startsWith(QStringLiteral("@@"));
        if (exception)
            line.remove(0, 2);
        const QString domain = domainFromRule(line);
        if (domain.isEmpty())
            continue;
        (exception ? allowed : blocked).insert(domain);
    }

    for (const QString &domain : std::as_const(allowed))
        blocked.remove(domain);

    ParsedAdBlockRules result {blocked.values(), allowed.values()};
    std::ranges::sort(result.blockedDomains);
    std::ranges::sort(result.allowedDomains);
    return result;
}

QByteArray AdBlockRuleParser::serialize(const ParsedAdBlockRules &rules)
{
    QByteArray output;
    for (const QString &domain : rules.blockedDomains)
        output += '+' + domain.toUtf8() + '\n';
    for (const QString &domain : rules.allowedDomains)
        output += '-' + domain.toUtf8() + '\n';
    return output;
}

ParsedAdBlockRules AdBlockRuleParser::deserialize(const QByteArray &content)
{
    ParsedAdBlockRules result;
    for (const QByteArray &rawLine : content.split('\n')) {
        if (rawLine.size() < 2)
            continue;
        const QString domain = QString::fromUtf8(rawLine.sliced(1)).trimmed();
        if (!validDomain(domain))
            continue;
        if (rawLine.front() == '+')
            result.blockedDomains.append(domain);
        else if (rawLine.front() == '-')
            result.allowedDomains.append(domain);
    }
    result.blockedDomains.removeDuplicates();
    result.allowedDomains.removeDuplicates();
    std::ranges::sort(result.blockedDomains);
    std::ranges::sort(result.allowedDomains);
    return result;
}

CompiledAdBlockRules AdBlockRuleParser::compile(const ParsedAdBlockRules &rules)
{
    CompiledAdBlockRules compiled;
    compiled.blockedDomains.reserve(static_cast<std::size_t>(rules.blockedDomains.size()));
    compiled.allowedDomains.reserve(static_cast<std::size_t>(rules.allowedDomains.size()));
    for (const QString &domain : rules.blockedDomains)
        compiled.blockedDomains.push_back(AdBlockInterceptor::hashDomain(domain));
    for (const QString &domain : rules.allowedDomains)
        compiled.allowedDomains.push_back(AdBlockInterceptor::hashDomain(domain));
    std::ranges::sort(compiled.blockedDomains);
    std::ranges::sort(compiled.allowedDomains);
    return compiled;
}

QByteArray AdBlockRuleParser::serializeCompiled(const CompiledAdBlockRules &rules)
{
    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32(0x5A414442) << quint16(1);
    stream << quint32(rules.blockedDomains.size());
    for (const quint64 hash : rules.blockedDomains)
        stream << hash;
    stream << quint32(rules.allowedDomains.size());
    for (const quint64 hash : rules.allowedDomains)
        stream << hash;
    return stream.status() == QDataStream::Ok ? output : QByteArray {};
}

CompiledAdBlockRules AdBlockRuleParser::deserializeCompiled(const QByteArray &content)
{
    QDataStream stream(content);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    stream >> magic >> version;
    if (magic != 0x5A414442 || version != 1)
        return {};

    CompiledAdBlockRules rules;
    quint32 blockedCount = 0;
    stream >> blockedCount;
    if (blockedCount > 1'000'000)
        return {};
    rules.blockedDomains.reserve(blockedCount);
    for (quint32 index = 0; index < blockedCount; ++index) {
        quint64 hash = 0;
        stream >> hash;
        rules.blockedDomains.push_back(hash);
    }
    quint32 allowedCount = 0;
    stream >> allowedCount;
    if (allowedCount > 1'000'000)
        return {};
    rules.allowedDomains.reserve(allowedCount);
    for (quint32 index = 0; index < allowedCount; ++index) {
        quint64 hash = 0;
        stream >> hash;
        rules.allowedDomains.push_back(hash);
    }
    if (stream.status() != QDataStream::Ok)
        return {};
    return rules;
}
