#include "core/SessionStore.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <utility>

SessionStore::SessionStore(QString path) : path_(std::move(path)) {}

QVariantMap SessionStore::read(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32 * 1024 * 1024) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto map = document.toVariant().toMap();
    if (map.value("version").toInt() != 1 || !map.value("tabs").canConvert<QVariantList>()
        || map.value("tabs").toList().size() > 1000) return {};
    return map;
}

QVariantMap SessionStore::load() const
{
    auto map = read(path_);
    return map.isEmpty() ? read(path_ + QStringLiteral(".bak")) : map;
}

bool SessionStore::save(const QVariantMap &session) const
{
    QVariantMap map = session;
    map.insert("version", 1);
    if (!map.value("tabs").canConvert<QVariantList>()) return false;
    const QByteArray bytes = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
    if (bytes.size() > 32 * 1024 * 1024 || map.value("tabs").toList().size() > 1000) return false;
    QFile existing(path_);
    if (existing.open(QIODevice::ReadOnly) && existing.size() == bytes.size() && existing.readAll() == bytes) return true;
    if (!read(path_).isEmpty()) {
        QFile old(path_);
        if (!old.open(QIODevice::ReadOnly)) return false;
        const QByteArray previous = old.readAll();
        QSaveFile backup(path_ + QStringLiteral(".bak"));
        if (!backup.open(QIODevice::WriteOnly)) return false;
        backup.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (backup.write(previous) != previous.size() || !backup.commit()) return false;
    }
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return file.write(bytes) == bytes.size() && file.commit();
}
