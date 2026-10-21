#include "core/CredentialManager.h"

#include "core/WebView.h"
#include "core/Navigation.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QInputDialog>

namespace {
const char kCredentialScript[] = R"JS(
(function() {
    if (window.__zenithCredentialsInstalled)
        return;
    window.__zenithCredentialsInstalled = true;

    const handler = window.webkit && window.webkit.messageHandlers
        && window.webkit.messageHandlers.credentials;
    const origin = location.origin;
    if (!handler || !origin.startsWith("https://"))
        return;

    const setNativeValue = function(input, value) {
        const descriptor = Object.getOwnPropertyDescriptor(
            window.HTMLInputElement.prototype, "value");
        descriptor.set.call(input, value);
        input.dispatchEvent(new Event("input", { bubbles: true }));
        input.dispatchEvent(new Event("change", { bubbles: true }));
    };

    const passwordInput = function(root) {
        return root.querySelector('input[type="password"]:not([disabled])');
    };
    const usernameInput = function(root, password) {
        const candidates = Array.from(root.querySelectorAll(
            'input[type="email"],input[autocomplete="username"],input[type="text"]'));
        return candidates.filter(input => !input.disabled && input !== password).pop();
    };

    const fill = function(payload) {
        if (!payload)
            return;
        let data;
        try {
            data = JSON.parse(payload);
        } catch (error) {
            return;
        }
        if (!data || data.origin !== origin)
            return;
        const passwordField = passwordInput(document);
        if (!passwordField)
            return;
        const scope = passwordField.form || document;
        const usernameField = usernameInput(scope, passwordField);
        if (usernameField && !usernameField.value)
            setNativeValue(usernameField, data.username);
        if (!passwordField.value)
            setNativeValue(passwordField, data.password);
    };

    document.addEventListener("submit", function(event) {
        if (!event.isTrusted)
            return;
        const scope = event.target instanceof HTMLFormElement ? event.target : document;
        const passwordField = passwordInput(scope);
        if (!passwordField || !passwordField.value)
            return;
        const usernameField = usernameInput(scope, passwordField);
        if (!usernameField || !usernameField.value)
            return;
        handler.postMessage(JSON.stringify({
            type: "submit",
            origin: origin,
            username: usernameField.value,
            password: passwordField.value
        }));
    }, true);

    let fillRequested = false;
    const requestFill = function() {
        if (fillRequested || !passwordInput(document))
            return;
        fillRequested = true;
        handler.postMessage(JSON.stringify({type: "requestFill", origin: origin}))
            .then(function(payload) {
                if (!payload) fillRequested = false;
                else fill(payload);
            }, function() { fillRequested = false; });
    };
    document.addEventListener("focusin", function(event) {
        if (event.isTrusted && event.target instanceof HTMLInputElement && event.target.type === "password")
            requestFill();
    }, true);
    window.setTimeout(requestFill, 250);
})();
)JS";
}

CredentialBridge::CredentialBridge(WebView *view, CredentialManager *manager)
    : QObject(view)
    , view_(view)
    , manager_(manager)
{
}

QString CredentialBridge::handleMessage(const QString &payload)
{
    if (payload.size() > 65536) return {};
    const QJsonObject object = QJsonDocument::fromJson(payload.toUtf8()).object();
    const QString type = object.value(QStringLiteral("type")).toString();
    const QString origin = object.value(QStringLiteral("origin")).toString();
    if (type == QStringLiteral("requestFill")) {
        if (!matchesPageOrigin(origin))
            return {};
        QStringList accounts;
        for (const auto &entry : manager_->entries())
            if (entry.origin == origin) accounts.append(entry.username);
        QString account;
        if (accounts.size() > 1) {
            bool ok = false;
            account = QInputDialog::getItem(view_, QObject::tr("Preencher credencial"), origin, accounts, 0, false, &ok);
            if (!ok || !matchesPageOrigin(origin)) return {};
        }
        const auto credential = manager_->lookup(origin, account);
        if (!credential || !matchesPageOrigin(origin))
            return {};
        QJsonObject reply;
        reply.insert(QStringLiteral("origin"), origin);
        reply.insert(QStringLiteral("username"), credential->username);
        reply.insert(QStringLiteral("password"), credential->password);
        return QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }
    if (type == QStringLiteral("submit")) {
        submit(origin, object.value(QStringLiteral("username")).toString(),
               object.value(QStringLiteral("password")).toString());
    }
    return {};
}

void CredentialBridge::submit(const QString &origin, const QString &username,
                              const QString &password)
{
    if (!matchesPageOrigin(origin) || username.isEmpty() || password.isEmpty())
        return;
    const auto existing = manager_->lookup(origin, username);
    if (existing && existing->username == username && existing->password == password)
        return;
    emit manager_->saveRequested(origin, username, password);
}

bool CredentialBridge::matchesPageOrigin(const QString &origin) const
{
    return view_ && view_->allowsCredentials() && Navigation::origin(QUrl(origin)) == origin
        && CredentialManager::isSameHttpsOrigin(view_->url(), QUrl(origin));
}

CredentialManager::CredentialManager(QObject *parent)
    : QObject(parent)
{
}

bool CredentialManager::isSameHttpsOrigin(const QUrl &left, const QUrl &right)
{
    if (!left.isValid() || !right.isValid())
        return false;
    const auto webScheme = [](const QString &scheme) {
        return scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
            || scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0;
    };
    if (!webScheme(left.scheme()) || !webScheme(right.scheme())
        || left.scheme() != QStringLiteral("https") || right.scheme() != QStringLiteral("https")
        || left.host().isEmpty() || right.host().isEmpty())
        return false;
    return left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
        && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
        && left.port(443) == right.port(443);
}

void CredentialManager::attachToView(WebView *view)
{
    if (!view)
        return;

    auto *bridge = new CredentialBridge(view, this);
    view->installPageScript(QString::fromUtf8(kCredentialScript));
    view->setMessageHandler([bridge](const QString &message) {
        return bridge->handleMessage(message);
    });
}

std::optional<StoredCredential> CredentialManager::lookup(const QString &origin, const QString &username) const
{
    return store_.lookup(origin, nullptr, username);
}

QList<CredentialEntry> CredentialManager::entries() const
{
    return store_.entries();
}

bool CredentialManager::save(const StoredCredential &credential, QString *error)
{
    return store_.save(credential, error);
}

bool CredentialManager::remove(const QString &origin, QString *error, const QString &username)
{
    return store_.remove(origin, error, username);
}
