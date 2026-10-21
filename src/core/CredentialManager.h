#pragma once

#include "core/CredentialStore.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

class WebView;
class CredentialManager;

class CredentialBridge final : public QObject
{
public:
    CredentialBridge(WebView *view, CredentialManager *manager);

    [[nodiscard]] QString handleMessage(const QString &payload);

private:
    [[nodiscard]] bool matchesPageOrigin(const QString &origin) const;
    void submit(const QString &origin, const QString &username, const QString &password);

    QPointer<WebView> view_;
    CredentialManager *manager_;
};

class CredentialManager final : public QObject
{
    Q_OBJECT

public:
    explicit CredentialManager(QObject *parent = nullptr);

    [[nodiscard]] static bool isSameHttpsOrigin(const QUrl &left, const QUrl &right);
    void attachToView(WebView *view);
    [[nodiscard]] std::optional<StoredCredential> lookup(const QString &origin, const QString &username = {}) const;
    [[nodiscard]] QList<CredentialEntry> entries() const;
    bool save(const StoredCredential &credential, QString *error = nullptr);
    bool remove(const QString &origin, QString *error = nullptr, const QString &username = {});

signals:
    void saveRequested(const QString &origin, const QString &username, const QString &password);

private:
    CredentialStore store_;
};
