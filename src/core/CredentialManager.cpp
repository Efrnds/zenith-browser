#include "core/CredentialManager.h"

#include <QFile>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace {
const char kCredentialScript[] = R"JS(
(function() {
    if (window.__zenithCredentialsInstalled || typeof qt === "undefined")
        return;
    window.__zenithCredentialsInstalled = true;

    new QWebChannel(qt.webChannelTransport, function(channel) {
        const bridge = channel.objects.credentials;
        const origin = location.origin;
        if (!origin.startsWith("https://"))
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

        bridge.fillReady.connect(function(targetOrigin, username, password) {
            if (targetOrigin !== origin)
                return;
            const passwordField = passwordInput(document);
            if (!passwordField)
                return;
            const scope = passwordField.form || document;
            const usernameField = usernameInput(scope, passwordField);
            if (usernameField && !usernameField.value)
                setNativeValue(usernameField, username);
            if (!passwordField.value)
                setNativeValue(passwordField, password);
        });

        document.addEventListener("submit", function(event) {
            const scope = event.target instanceof HTMLFormElement ? event.target : document;
            const passwordField = passwordInput(scope);
            if (!passwordField || !passwordField.value)
                return;
            const usernameField = usernameInput(scope, passwordField);
            if (!usernameField || !usernameField.value)
                return;
            bridge.submit(origin, usernameField.value, passwordField.value);
        }, true);

        window.setTimeout(function() { bridge.requestFill(origin); }, 250);
    });
})();
)JS";
}

CredentialBridge::CredentialBridge(QWebEnginePage *page, CredentialManager *manager)
    : QObject(page)
    , page_(page)
    , manager_(manager)
{
}

void CredentialBridge::requestFill(const QString &origin)
{
    if (!matchesPageOrigin(origin))
        return;
    if (const auto credential = manager_->lookup(origin))
        emit fillReady(origin, credential->username, credential->password);
}

void CredentialBridge::submit(const QString &origin, const QString &username,
                              const QString &password)
{
    if (!matchesPageOrigin(origin) || username.isEmpty() || password.isEmpty())
        return;
    const auto existing = manager_->lookup(origin);
    if (existing && existing->username == username && existing->password == password)
        return;
    emit manager_->saveRequested(origin, username, password);
}

bool CredentialBridge::matchesPageOrigin(const QString &origin) const
{
    return page_ && CredentialManager::isSameHttpsOrigin(page_->url(), QUrl(origin));
}

CredentialManager::CredentialManager(QObject *parent)
    : QObject(parent)
{
}

bool CredentialManager::isSameHttpsOrigin(const QUrl &left, const QUrl &right)
{
    if (!left.isValid() || !right.isValid())
        return false;
    if (left.scheme() != QStringLiteral("https") || right.scheme() != QStringLiteral("https"))
        return false;
    return left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
        && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
        && left.port(-1) == right.port(-1);
}

void CredentialManager::attachToPage(QWebEnginePage *page)
{
    if (!page)
        return;

    auto *channel = new QWebChannel(page);
    auto *bridge = new CredentialBridge(page, this);
    channel->registerObject(QStringLiteral("credentials"), bridge);
    page->setWebChannel(channel, QWebEngineScript::ApplicationWorld);

    QFile webChannelSource(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    if (!webChannelSource.open(QIODevice::ReadOnly))
        return;

    QWebEngineScript script;
    script.setName(QStringLiteral("zenith-credential-manager"));
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(QString::fromUtf8(webChannelSource.readAll())
                         + QString::fromUtf8(kCredentialScript));
    page->scripts().insert(script);
}

std::optional<StoredCredential> CredentialManager::lookup(const QString &origin) const
{
    return store_.lookup(origin);
}

QList<CredentialEntry> CredentialManager::entries() const
{
    return store_.entries();
}

bool CredentialManager::save(const StoredCredential &credential, QString *error)
{
    return store_.save(credential, error);
}

bool CredentialManager::remove(const QString &origin, QString *error)
{
    return store_.remove(origin, error);
}
