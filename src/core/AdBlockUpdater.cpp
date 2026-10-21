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
#include <memory>

namespace {
constexpr qint64 kRefreshAfterDays = 7;
constexpr qint64 kMaximumListBytes = 8 * 1024 * 1024;
const QList<QUrl> kRuleUrls {
    QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")),
    QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt")),
};
}

AdBlockUpdater::AdBlockUpdater(AdBlockInterceptor *interceptor, QObject *parent, QList<QUrl> sources)
    : QObject(parent)
    , interceptor_(interceptor)
    , network_(new QNetworkAccessManager(this))
    , sources_(sources.isEmpty() ? kRuleUrls : std::move(sources))
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
    if (pendingReplies_ > 0 || compiling_)
        return;
    payloads_.clear();
    pendingReplies_ = sources_.size();
    statusText_ = tr("Atualizando listas");
    emit statusChanged();

    for (const QUrl &url : sources_) {
        QNetworkRequest request(url);
        request.setTransferTimeout(10'000);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("Zenith Browser"));
        QNetworkReply *reply = network_->get(request);
        reply->setReadBufferSize(64 * 1024);
        auto payload = std::make_shared<QByteArray>();
        connect(reply, &QIODevice::readyRead, reply, [reply, payload] {
            payload->append(reply->readAll());
            if (payload->size() > kMaximumListBytes) reply->abort();
        });
        QTimer::singleShot(30'000, reply, [reply] {
            if (reply->isRunning()) reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, payload] {
            handleReply(reply, *payload);
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
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaximumListBytes * 2)
        return;
    const ParsedAdBlockRules rules = AdBlockRuleParser::deserialize(file.readAll());
    if (rules.blockedDomains.isEmpty())
        return;
    interceptor_->setRules(rules.blockedDomains, rules.allowedDomains);
    lastUpdated_ = QFileInfo(file).lastModified().toUTC();
    statusText_ = tr("%1 regras, atualizadas em %2")
        .arg(static_cast<qsizetype>(rules.blockedDomains.size()))
        .arg(lastUpdated_.toLocalTime().toString(QStringLiteral("dd/MM/yyyy")));
}

void AdBlockUpdater::handleReply(QNetworkReply *reply, const QByteArray &payload)
{
    const QByteArray tail = reply->readAll();
    if (reply->error() == QNetworkReply::NoError && payload.size() + tail.size() <= kMaximumListBytes)
        payloads_.append(payload + tail);
    reply->deleteLater();
    if (--pendingReplies_ == 0)
        finishUpdate();
}

void AdBlockUpdater::finishUpdate()
{
    if (payloads_.size() != sources_.size()) {
        payloads_.clear();
        statusText_ = tr("Falha ao atualizar, usando cache");
        emit statusChanged();
        return;
    }

    const QList<QByteArray> payloads = std::exchange(payloads_, {});
    compiling_ = true;
    statusText_ = tr("Compilando listas");
    emit statusChanged();
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher] {
        applyCompiledRules(watcher->result());
        compiling_ = false;
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([payloads] {
        QSet<QString> blocked;
        QSet<QString> allowed;
        for (const QString &domain : AdBlockInterceptor::builtInDomains())
            blocked.insert(domain);
        for (const QByteArray &payload : payloads) {
            const auto parsed = AdBlockRuleParser::parse(payload);
            if (parsed.blockedDomains.isEmpty()) return QByteArray{};
            for (const QString &domain : parsed.blockedDomains)
                blocked.insert(domain);
            for (const QString &domain : parsed.allowedDomains)
                allowed.insert(domain);
        }
        for (const QString &domain : std::as_const(allowed))
            blocked.remove(domain);
        return AdBlockRuleParser::serialize({blocked.values(), allowed.values()});
    }));
}

void AdBlockUpdater::applyCompiledRules(const QByteArray &compiled)
{
    const ParsedAdBlockRules rules = AdBlockRuleParser::deserialize(compiled);
    if (rules.blockedDomains.isEmpty()) {
        statusText_ = tr("Listas recebidas sem regras compatíveis");
        emit statusChanged();
        return;
    }

    interceptor_->setRules(rules.blockedDomains, rules.allowedDomains);
    const QString path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const bool saved = file.open(QIODevice::WriteOnly)
        && file.write(compiled) == compiled.size() && file.commit();
    lastUpdated_ = QDateTime::currentDateTimeUtc();
    statusText_ = tr("%1 regras ativas")
        .arg(static_cast<qsizetype>(rules.blockedDomains.size()));
    if (!saved) statusText_ += tr(" (cache não foi salvo)");
    emit statusChanged();
}

QString AdBlockUpdater::cachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/adblock/domains-v2.cache");
}
