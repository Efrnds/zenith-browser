#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>

class AdBlockInterceptor;
class QNetworkAccessManager;
class QNetworkReply;

class AdBlockUpdater final : public QObject
{
    Q_OBJECT

public:
    explicit AdBlockUpdater(AdBlockInterceptor *interceptor, QObject *parent = nullptr,
                            QList<QUrl> sources = {});

    void updateNow();
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QDateTime lastUpdated() const;
    [[nodiscard]] bool isUpdating() const { return pendingReplies_ > 0 || compiling_; }

signals:
    void statusChanged();

private:
    void loadCache();
    void handleReply(QNetworkReply *reply, const QByteArray &payload);
    void finishUpdate();
    void applyCompiledRules(const QByteArray &compiled);
    [[nodiscard]] QString cachePath() const;

    AdBlockInterceptor *interceptor_;
    QNetworkAccessManager *network_;
    QList<QUrl> sources_;
    bool compiling_ = false;
    QList<QByteArray> payloads_;
    int pendingReplies_ = 0;
    QDateTime lastUpdated_;
    QString statusText_;
};
