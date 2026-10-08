#include "core/AdBlockUpdater.h"

#include "core/AdBlockInterceptor.h"
#include "core/AdBlockRuleParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace {
constexpr qint64 kRefreshAfterDays = 7;
const QList<QUrl> kRuleUrls {
    QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")),
    QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt")),
};
}

AdBlockUpdater::AdBlockUpdater(AdBlockInterceptor *interceptor, QObject *parent)
    : QObject(parent)
    , interceptor_(interceptor)
    , network_(new QNetworkAccessManager(this))
    , statusText_(tr("Usando lista interna"))
{
    loadCache();
    if (!lastUpdated_.isValid()
        || lastUpdated_.daysTo(QDateTime::currentDateTimeUtc()) >= kRefreshAfterDays) {
        QTimer::singleShot(2000, this, &AdBlockUpdater::updateNow);
    }
}

void AdBlockUpdater::updateNow()
{
    if (pendingReplies_ > 0)
        return;
    payloads_.clear();
    pendingReplies_ = kRuleUrls.size();
    statusText_ = tr("Atualizando listas");
    emit statusChanged();

    for (const QUrl &url : kRuleUrls) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("Zenith Browser/0.1"));
        QNetworkReply *reply = network_->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            handleReply(reply);
        });
    }
}

QString AdBlockUpdater::statusText() const
{
    return statusText_;
}

QDateTime AdBlockUpdater::lastUpdated() const
{
    return lastUpdated_;
}

void AdBlockUpdater::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const CompiledAdBlockRules rules = AdBlockRuleParser::deserializeCompiled(file.readAll());
    if (rules.blockedDomains.empty())
        return;
    interceptor_->setCompiledRules(rules.blockedDomains, rules.allowedDomains);
    lastUpdated_ = QFileInfo(file).lastModified().toUTC();
    statusText_ = tr("%1 regras, atualizadas em %2")
        .arg(static_cast<qsizetype>(rules.blockedDomains.size()))
        .arg(lastUpdated_.toLocalTime().toString(QStringLiteral("dd/MM/yyyy")));
}

void AdBlockUpdater::handleReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
        payloads_.append(reply->readAll());
    reply->deleteLater();
    if (--pendingReplies_ == 0)
        finishUpdate();
}

void AdBlockUpdater::finishUpdate()
{
    if (payloads_.isEmpty()) {
        statusText_ = tr("Falha ao atualizar, usando cache");
        emit statusChanged();
        return;
    }

    const QList<QByteArray> payloads = std::exchange(payloads_, {});
    statusText_ = tr("Compilando listas");
    emit statusChanged();
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher] {
        applyCompiledRules(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([payloads] {
        QSet<QString> blocked;
        QSet<QString> allowed;
        for (const QByteArray &payload : payloads) {
            const auto parsed = AdBlockRuleParser::parse(payload);
            for (const QString &domain : parsed.blockedDomains)
                blocked.insert(domain);
            for (const QString &domain : parsed.allowedDomains)
                allowed.insert(domain);
        }
        for (const QString &domain : std::as_const(allowed))
            blocked.remove(domain);
        return AdBlockRuleParser::serializeCompiled(
            AdBlockRuleParser::compile({blocked.values(), allowed.values()}));
    }));
}

void AdBlockUpdater::applyCompiledRules(const QByteArray &compiled)
{
    const CompiledAdBlockRules rules = AdBlockRuleParser::deserializeCompiled(compiled);
    if (rules.blockedDomains.empty()) {
        statusText_ = tr("Listas recebidas sem regras compatíveis");
        emit statusChanged();
        return;
    }

    interceptor_->setCompiledRules(rules.blockedDomains, rules.allowedDomains);
    const QString path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(compiled);
        file.commit();
    }
    lastUpdated_ = QDateTime::currentDateTimeUtc();
    statusText_ = tr("%1 regras ativas")
        .arg(static_cast<qsizetype>(rules.blockedDomains.size()));
    emit statusChanged();
}

QString AdBlockUpdater::cachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/adblock/rules.cache");
}
