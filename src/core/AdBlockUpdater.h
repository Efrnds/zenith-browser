#pragma once

#include <QDateTime>
#include <QObject>

class AdBlockInterceptor;
class QNetworkAccessManager;
class QNetworkReply;

class AdBlockUpdater final : public QObject
{
    Q_OBJECT

public:
    explicit AdBlockUpdater(AdBlockInterceptor *interceptor, QObject *parent = nullptr);

    void updateNow();
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QDateTime lastUpdated() const;

signals:
    void statusChanged();

private:
    void loadCache();
    void handleReply(QNetworkReply *reply);
    void finishUpdate();
    void applyCompiledRules(const QByteArray &compiled);
    [[nodiscard]] QString cachePath() const;

    AdBlockInterceptor *interceptor_;
    QNetworkAccessManager *network_;
    QList<QByteArray> payloads_;
    int pendingReplies_ = 0;
    QDateTime lastUpdated_;
    QString statusText_;
};
