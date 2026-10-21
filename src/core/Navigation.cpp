#include "core/Navigation.h"

#include <QFileInfo>
#include <QRegularExpression>

int Navigation::command(int key, Qt::KeyboardModifiers modifiers)
{
    const auto mods = modifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    if (mods == Qt::ControlModifier) {
        if (key >= Qt::Key_1 && key <= Qt::Key_8) return SelectTab1 + key - Qt::Key_1;
        if (key == Qt::Key_9) return SelectLastTab;
        switch (key) {
        case Qt::Key_T: return NewTab;
        case Qt::Key_Comma: return Settings;
        case Qt::Key_L: return Address;
        case Qt::Key_W: return CloseTab;
        case Qt::Key_Tab: return NextTab;
        case Qt::Key_Backtab: return PreviousTab;
        case Qt::Key_F: return Find;
        case Qt::Key_R: return Reload;
        case Qt::Key_F5: return HardReload;
        case Qt::Key_G: return FindNext;
        case Qt::Key_Plus: case Qt::Key_Equal: return ZoomIn;
        case Qt::Key_Minus: return ZoomOut;
        case Qt::Key_0: return ZoomReset;
        case Qt::Key_H: return History;
        case Qt::Key_J: return Downloads;
        case Qt::Key_D: return Bookmark;
        case Qt::Key_P: return Print;
        case Qt::Key_Backslash: return ToggleSplit;
        }
    }
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
        switch (key) {
        case Qt::Key_T: return ReopenTab;
        case Qt::Key_P: return NewPrivateWindow;
        case Qt::Key_R: return HardReload;
        case Qt::Key_G: return FindPrevious;
        case Qt::Key_Tab: case Qt::Key_Backtab: return PreviousTab;
        case Qt::Key_B: return Bookmarks;
        case Qt::Key_Plus: case Qt::Key_Equal: return ZoomIn;
        case Qt::Key_I: return Inspector;
        }
    }
    if (mods == Qt::AltModifier && key == Qt::Key_Left) return Back;
    if (mods == Qt::AltModifier && key == Qt::Key_Right) return Forward;
    if (mods == Qt::NoModifier && key == Qt::Key_F5) return Reload;
    if (mods == Qt::NoModifier && key == Qt::Key_F3) return FindNext;
    if (mods == Qt::ShiftModifier && key == Qt::Key_F3) return FindPrevious;
    if (mods == Qt::NoModifier && key == Qt::Key_F11) return Fullscreen;
    if (mods == Qt::NoModifier && key == Qt::Key_F10) return BrowserMenu;
    if (mods == Qt::NoModifier && key == Qt::Key_F12) return Inspector;
    return None;
}

bool Navigation::isWebUrl(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"));
}

bool Navigation::canUseCredentials(const QUrl &url, bool certificateException)
{
    return isWebUrl(url) && url.scheme() == QStringLiteral("https") && !certificateException;
}

QString Navigation::outlookUserAgent(const QUrl &url)
{
    if (url.scheme() != QStringLiteral("https")) return {};
    const QString host = url.host().toLower();
    if (host != QStringLiteral("outlook.live.com") && host != QStringLiteral("outlook.office.com")
        && host != QStringLiteral("outlook.office365.com")) return {};
    // Outlook rejects WebKitGTK's Linux identity and selects its unsupported basic client.
    return QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.5 Safari/605.1.15");
}

QString Navigation::origin(const QUrl &url)
{
    if (!isWebUrl(url))
        return {};
    QUrl result;
    result.setScheme(url.scheme());
    result.setHost(url.host().toLower());
    const int port = url.port();
    if (port >= 0 && port != (url.scheme() == QStringLiteral("https") ? 443 : 80))
        result.setPort(port);
    return result.toString(QUrl::FullyEncoded);
}

QUrl Navigation::fromInput(const QString &text, const QString &searchTemplate)
{
    const QString input = text.trimmed();
    if (input.isEmpty())
        return {};
    if (input.startsWith('/') && QFileInfo(input).isFile())
        return QUrl::fromLocalFile(input);
    static const QRegularExpression local(QStringLiteral(R"(^(localhost|127(?:\.\d+){3}|\[::1\])(?::\d+)?(?:/|$))"));
    if (local.match(input).hasMatch())
        return QUrl(QStringLiteral("http://") + input);
    static const QRegularExpression hostPort(QStringLiteral(R"(^[^\s/:]+:\d+(?:/.*)?$)"));
    if (hostPort.match(input).hasMatch())
        return QUrl(QStringLiteral("https://") + input);
    const QUrl explicitUrl(input);
    if (!explicitUrl.scheme().isEmpty()) {
        const QString scheme = explicitUrl.scheme();
        if (isWebUrl(explicitUrl) || scheme == QStringLiteral("file")
            || input == QStringLiteral("about:blank"))
            return explicitUrl;
        return {};
    }
    if (!input.contains(QRegularExpression(QStringLiteral("\\s")))
        && (input.contains('.') || input.startsWith('[')))
        return QUrl(QStringLiteral("https://") + input);
    const QUrl search(searchTemplate.arg(QString::fromLatin1(QUrl::toPercentEncoding(input))));
    return isWebUrl(search) ? search : QUrl();
}
