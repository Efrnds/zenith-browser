#include "core/AdBlockRuleParser.h"
#include "core/AdBlockInterceptor.h"
#include "core/CredentialManager.h"

#include <QDataStream>
#include <QSignalSpy>
#include <QTest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void parsesNetworkRules();
    void matchesAdDomainsAndExceptions();
    void rejectsCrossOriginCredentials();
    void serializesWebHistory();
};

void CoreTests::parsesNetworkRules()
{
    const auto rules = AdBlockRuleParser::parse(
        "||ads.example.com^\n"
        "||tracker.example.org^$third-party\n"
        "@@||allowed.example.com^\n"
        "example.com##.advert\n"
        "! comment\n");

    QCOMPARE(rules.blockedDomains,
             QStringList({QStringLiteral("ads.example.com"),
                          QStringLiteral("tracker.example.org")}));
    QCOMPARE(rules.allowedDomains,
             QStringList({QStringLiteral("allowed.example.com")}));

    const auto restored = AdBlockRuleParser::deserialize(
        AdBlockRuleParser::serialize(rules));
    QCOMPARE(restored.blockedDomains, rules.blockedDomains);
    QCOMPARE(restored.allowedDomains, rules.allowedDomains);

    const auto compiled = AdBlockRuleParser::compile(rules);
    const auto compiledRestored = AdBlockRuleParser::deserializeCompiled(
        AdBlockRuleParser::serializeCompiled(compiled));
    QCOMPARE(compiledRestored.blockedDomains, compiled.blockedDomains);
    QCOMPARE(compiledRestored.allowedDomains, compiled.allowedDomains);
}

void CoreTests::matchesAdDomainsAndExceptions()
{
    AdBlockInterceptor interceptor;
    interceptor.setRules({QStringLiteral("doubleclick.net"),
                          QStringLiteral("ads.example.com")},
                         {QStringLiteral("allowed.ads.example.com")});
    QVERIFY(interceptor.wouldBlock(
        QUrl(QStringLiteral("https://securepubads.g.doubleclick.net/script.js"))));
    QVERIFY(interceptor.wouldBlock(
        QUrl(QStringLiteral("https://cdn.example.org/ads/banner.js"))));
    QVERIFY(!interceptor.wouldBlock(
        QUrl(QStringLiteral("https://allowed.ads.example.com/content.js"))));
    QVERIFY(!interceptor.wouldBlock(
        QUrl(QStringLiteral("https://example.com/advertising")), true));
}

void CoreTests::rejectsCrossOriginCredentials()
{
    QVERIFY(CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("https://example.com/login")),
        QUrl(QStringLiteral("https://example.com"))));
    QVERIFY(!CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("https://example.com")),
        QUrl(QStringLiteral("https://evil.example"))));
    QVERIFY(!CredentialManager::isSameHttpsOrigin(
        QUrl(QStringLiteral("http://example.com")),
        QUrl(QStringLiteral("http://example.com"))));
}

void CoreTests::serializesWebHistory()
{
    QWebEngineProfile profile;
    QWebEnginePage source(&profile);
    QSignalSpy loaded(&source, &QWebEnginePage::loadFinished);
    source.setHtml(QStringLiteral("<title>History test</title>"),
                   QUrl(QStringLiteral("https://example.com/login")));
    QVERIFY(loaded.wait(5000));

    QByteArray state;
    QDataStream output(&state, QIODevice::WriteOnly);
    output << *source.history();
    QCOMPARE(output.status(), QDataStream::Ok);

    QWebEnginePage restored(&profile);
    QDataStream input(state);
    input >> *restored.history();
    QCOMPARE(input.status(), QDataStream::Ok);
    QCOMPARE(restored.history()->currentItem().url(),
             source.history()->currentItem().url());
}

QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
