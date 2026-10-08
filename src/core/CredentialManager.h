#pragma once

#include "core/CredentialStore.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

class QWebEnginePage;

class CredentialManager;

class CredentialBridge final : public QObject
{
    Q_OBJECT

public:
    CredentialBridge(QWebEnginePage *page, CredentialManager *manager);

public slots:
    void requestFill(const QString &origin);
    void submit(const QString &origin, const QString &username, const QString &password);

signals:
    void fillReady(const QString &origin, const QString &username, const QString &password);

private:
    [[nodiscard]] bool matchesPageOrigin(const QString &origin) const;

    QPointer<QWebEnginePage> page_;
    CredentialManager *manager_;
};

class CredentialManager final : public QObject
{
    Q_OBJECT

public:
    explicit CredentialManager(QObject *parent = nullptr);

    [[nodiscard]] static bool isSameHttpsOrigin(const QUrl &left, const QUrl &right);
    void attachToPage(QWebEnginePage *page);
    [[nodiscard]] std::optional<StoredCredential> lookup(const QString &origin) const;
    [[nodiscard]] QList<CredentialEntry> entries() const;
    bool save(const StoredCredential &credential, QString *error = nullptr);
    bool remove(const QString &origin, QString *error = nullptr);

signals:
    void saveRequested(const QString &origin, const QString &username, const QString &password);

private:
    CredentialStore store_;
};
