#pragma once
#include <QString>
#include <QVariantMap>

class SessionStore
{
public:
    explicit SessionStore(QString path);
    QVariantMap load() const;
    bool save(const QVariantMap &session) const;
private:
    QVariantMap read(const QString &path) const;
    QString path_;
};
