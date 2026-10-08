#include "core/CredentialStore.h"

#include <QSettings>

#ifdef Q_OS_LINUX
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_MACOS)
#include <Security/Security.h>
#endif

namespace {
constexpr auto kService = "Zenith Browser";

#ifdef Q_OS_LINUX
const SecretSchema *credentialSchema()
{
    static const SecretSchema schema = {
        "org.zenith.browser.credentials",
        SECRET_SCHEMA_NONE,
        {
            {"origin", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
        },
    };
    return &schema;
}
#endif

void setError(QString *destination, const QString &message)
{
    if (destination)
        *destination = message;
}
}

bool CredentialStore::save(const StoredCredential &credential, QString *error)
{
    if (credential.origin.isEmpty() || credential.username.isEmpty() || credential.password.isEmpty()) {
        setError(error, QStringLiteral("Credencial incompleta."));
        return false;
    }

    bool saved = false;
#ifdef Q_OS_LINUX
    GError *nativeError = nullptr;
    const QByteArray origin = credential.origin.toUtf8();
    const QByteArray password = credential.password.toUtf8();
    saved = secret_password_store_sync(
        credentialSchema(), SECRET_COLLECTION_DEFAULT,
        QStringLiteral("%1: %2").arg(QString::fromLatin1(kService), credential.origin)
            .toUtf8().constData(),
        password.constData(), nullptr, &nativeError,
        "origin", origin.constData(),
        "application", "zenith-browser",
        nullptr);
    if (nativeError) {
        setError(error, QString::fromUtf8(nativeError->message));
        g_error_free(nativeError);
    }
#elif defined(Q_OS_WIN)
    const QString target = QStringLiteral("ZenithBrowser:") + credential.origin;
    const QByteArray password = credential.password.toUtf8();
    CREDENTIALW native {};
    native.Type = CRED_TYPE_GENERIC;
    native.TargetName = const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(target.utf16()));
    native.UserName = const_cast<wchar_t *>(
        reinterpret_cast<const wchar_t *>(credential.username.utf16()));
    native.CredentialBlobSize = static_cast<DWORD>(password.size());
    native.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char *>(password.constData()));
    native.Persist = CRED_PERSIST_LOCAL_MACHINE;
    saved = CredWriteW(&native, 0);
    if (!saved)
        setError(error, QStringLiteral("Credential Manager error: %1").arg(GetLastError()));
#elif defined(Q_OS_MACOS)
    const QByteArray service = credential.origin.toUtf8();
    const QByteArray account = credential.username.toUtf8();
    const QByteArray password = credential.password.toUtf8();
    SecKeychainItemRef item = nullptr;
    UInt32 oldLength = 0;
    void *oldData = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(
        nullptr, service.size(), service.constData(), account.size(), account.constData(),
        &oldLength, &oldData, &item);
    if (oldData)
        SecKeychainItemFreeContent(nullptr, oldData);
    if (status == errSecSuccess) {
        status = SecKeychainItemModifyAttributesAndData(
            item, nullptr, password.size(), password.constData());
        CFRelease(item);
    } else {
        status = SecKeychainAddGenericPassword(
            nullptr, service.size(), service.constData(), account.size(), account.constData(),
            password.size(), password.constData(), nullptr);
    }
    saved = status == errSecSuccess;
    if (!saved)
        setError(error, QStringLiteral("Keychain error: %1").arg(status));
#else
    setError(error, QStringLiteral("Cofre do sistema indisponível."));
#endif

    if (saved)
        updateIndex(credential.origin, credential.username);
    return saved;
}

std::optional<StoredCredential> CredentialStore::lookup(const QString &origin,
                                                        QString *error) const
{
    QString username;
    for (const auto &entry : entries()) {
        if (entry.origin == origin) {
            username = entry.username;
            break;
        }
    }
    if (username.isEmpty())
        return std::nullopt;

    QString password;
#ifdef Q_OS_LINUX
    GError *nativeError = nullptr;
    const QByteArray encodedOrigin = origin.toUtf8();
    gchar *secret = secret_password_lookup_sync(
        credentialSchema(), nullptr, &nativeError,
        "origin", encodedOrigin.constData(),
        "application", "zenith-browser",
        nullptr);
    if (secret) {
        password = QString::fromUtf8(secret);
        secret_password_free(secret);
    }
    if (nativeError) {
        setError(error, QString::fromUtf8(nativeError->message));
        g_error_free(nativeError);
    }
#elif defined(Q_OS_WIN)
    PCREDENTIALW native = nullptr;
    const QString target = QStringLiteral("ZenithBrowser:") + origin;
    if (CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &native)) {
        password = QString::fromUtf8(
            reinterpret_cast<const char *>(native->CredentialBlob),
            static_cast<int>(native->CredentialBlobSize));
        CredFree(native);
    }
#elif defined(Q_OS_MACOS)
    const QByteArray service = origin.toUtf8();
    const QByteArray account = username.toUtf8();
    UInt32 length = 0;
    void *data = nullptr;
    if (SecKeychainFindGenericPassword(
            nullptr, service.size(), service.constData(), account.size(), account.constData(),
            &length, &data, nullptr) == errSecSuccess) {
        password = QString::fromUtf8(static_cast<const char *>(data), length);
        SecKeychainItemFreeContent(nullptr, data);
    }
#endif

    if (password.isEmpty())
        return std::nullopt;
    return StoredCredential {origin, username, password};
}

bool CredentialStore::remove(const QString &origin, QString *error)
{
    bool removed = false;
#ifdef Q_OS_LINUX
    GError *nativeError = nullptr;
    const QByteArray encodedOrigin = origin.toUtf8();
    removed = secret_password_clear_sync(
        credentialSchema(), nullptr, &nativeError,
        "origin", encodedOrigin.constData(),
        "application", "zenith-browser",
        nullptr);
    if (nativeError) {
        setError(error, QString::fromUtf8(nativeError->message));
        g_error_free(nativeError);
    }
#elif defined(Q_OS_WIN)
    const QString target = QStringLiteral("ZenithBrowser:") + origin;
    removed = CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0);
#elif defined(Q_OS_MACOS)
    const auto credential = lookup(origin, error);
    if (credential) {
        const QByteArray service = origin.toUtf8();
        const QByteArray account = credential->username.toUtf8();
        SecKeychainItemRef item = nullptr;
        if (SecKeychainFindGenericPassword(
                nullptr, service.size(), service.constData(), account.size(), account.constData(),
                nullptr, nullptr, &item) == errSecSuccess) {
            removed = SecKeychainItemDelete(item) == errSecSuccess;
            CFRelease(item);
        }
    }
#endif
    if (removed)
        removeFromIndex(origin);
    return removed;
}

QList<CredentialEntry> CredentialStore::entries() const
{
    QSettings settings;
    QList<CredentialEntry> result;
    const int count = settings.beginReadArray(QStringLiteral("credentials/index"));
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        const CredentialEntry entry {
            settings.value(QStringLiteral("origin")).toString(),
            settings.value(QStringLiteral("username")).toString(),
        };
        if (!entry.origin.isEmpty() && !entry.username.isEmpty())
            result.append(entry);
    }
    settings.endArray();
    return result;
}

void CredentialStore::updateIndex(const QString &origin, const QString &username)
{
    auto current = entries();
    bool found = false;
    for (auto &entry : current) {
        if (entry.origin == origin) {
            entry.username = username;
            found = true;
            break;
        }
    }
    if (!found)
        current.append({origin, username});

    QSettings settings;
    settings.beginWriteArray(QStringLiteral("credentials/index"));
    for (int index = 0; index < current.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("origin"), current.at(index).origin);
        settings.setValue(QStringLiteral("username"), current.at(index).username);
    }
    settings.endArray();
    settings.sync();
}

void CredentialStore::removeFromIndex(const QString &origin)
{
    auto current = entries();
    current.removeIf([&origin](const CredentialEntry &entry) {
        return entry.origin == origin;
    });

    QSettings settings;
    settings.beginWriteArray(QStringLiteral("credentials/index"));
    for (int index = 0; index < current.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("origin"), current.at(index).origin);
        settings.setValue(QStringLiteral("username"), current.at(index).username);
    }
    settings.endArray();
    settings.sync();
}
