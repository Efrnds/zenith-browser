#pragma once

#include <QString>
#include <QList>

#include <optional>

struct CredentialEntry {
    QString origin;
    QString username;
};

struct StoredCredential {
    QString origin;
    QString username;
    QString password;
};

class CredentialStore
{
public:
    [[nodiscard]] bool save(const StoredCredential &credential, QString *error = nullptr);
    [[nodiscard]] std::optional<StoredCredential> lookup(const QString &origin,
                                                         QString *error = nullptr) const;
    [[nodiscard]] bool remove(const QString &origin, QString *error = nullptr);
    [[nodiscard]] QList<CredentialEntry> entries() const;

private:
    void updateIndex(const QString &origin, const QString &username);
    void removeFromIndex(const QString &origin);
};
