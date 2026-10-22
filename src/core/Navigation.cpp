#include "core/Navigation.h"

#include <QFileInfo>
#include <QKeySequence>
#include <QRegularExpression>

namespace {
struct Binding { int key; Qt::KeyboardModifiers modifiers; Navigation::Command action; };
const Binding bindings[]{
    {Qt::Key_T, Qt::ControlModifier, Navigation::NewTab},
    {Qt::Key_Comma, Qt::ControlModifier, Navigation::Settings},
    {Qt::Key_L, Qt::ControlModifier, Navigation::Address},
    {Qt::Key_W, Qt::ControlModifier, Navigation::CloseTab},
    {Qt::Key_Tab, Qt::ControlModifier, Navigation::NextTab},
    {Qt::Key_F, Qt::ControlModifier, Navigation::Find},
    {Qt::Key_R, Qt::ControlModifier, Navigation::Reload},
    {Qt::Key_Plus, Qt::ControlModifier, Navigation::ZoomIn},
    {Qt::Key_Minus, Qt::ControlModifier, Navigation::ZoomOut},
    {Qt::Key_0, Qt::ControlModifier, Navigation::ZoomReset},
    {Qt::Key_H, Qt::ControlModifier, Navigation::History},
    {Qt::Key_J, Qt::ControlModifier, Navigation::Downloads},
    {Qt::Key_D, Qt::ControlModifier, Navigation::Bookmark},
    {Qt::Key_P, Qt::ControlModifier, Navigation::Print},
    {Qt::Key_Backslash, Qt::ControlModifier, Navigation::ToggleSplit},
    {Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier, Navigation::PreviousTab},
    {Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier, Navigation::ReopenTab},
    {Qt::Key_P, Qt::ControlModifier | Qt::ShiftModifier, Navigation::NewPrivateWindow},
    {Qt::Key_R, Qt::ControlModifier | Qt::ShiftModifier, Navigation::HardReload},
    {Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier, Navigation::Bookmarks},
    {Qt::Key_G, Qt::ControlModifier | Qt::ShiftModifier, Navigation::FindPrevious},
    {Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier, Navigation::PreviousTab},
    {Qt::Key_Plus, Qt::ControlModifier | Qt::ShiftModifier, Navigation::ZoomIn},
    {Qt::Key_Equal, Qt::ControlModifier | Qt::ShiftModifier, Navigation::ZoomIn},
    {Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier, Navigation::Inspector},
    {Qt::Key_F3, Qt::NoModifier, Navigation::FindNext},
    {Qt::Key_F5, Qt::NoModifier, Navigation::Reload},
    {Qt::Key_F11, Qt::NoModifier, Navigation::Fullscreen},
    {Qt::Key_F10, Qt::NoModifier, Navigation::BrowserMenu},
    {Qt::Key_F12, Qt::NoModifier, Navigation::Inspector},
    {Qt::Key_F3, Qt::ShiftModifier, Navigation::FindPrevious},
    {Qt::Key_Left, Qt::AltModifier, Navigation::Back},
    {Qt::Key_Right, Qt::AltModifier, Navigation::Forward},
    {Qt::Key_Backtab, Qt::ControlModifier, Navigation::PreviousTab},
    {Qt::Key_F5, Qt::ControlModifier, Navigation::HardReload},
    {Qt::Key_G, Qt::ControlModifier, Navigation::FindNext},
    {Qt::Key_Equal, Qt::ControlModifier, Navigation::ZoomIn},
};
}

int Navigation::command(int key, Qt::KeyboardModifiers modifiers)
{
    const auto mods = modifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    if (mods == Qt::ControlModifier) {
        if (key >= Qt::Key_1 && key <= Qt::Key_8) return SelectTab1 + key - Qt::Key_1;
        if (key == Qt::Key_9) return SelectLastTab;
    }
    for (const auto &binding : bindings)
        if (binding.key == key && binding.modifiers == mods) return binding.action;
    return None;
}

QString Navigation::shortcutLabel(int action)
{
    for (const auto &binding : bindings)
        if (binding.action == action)
            return QKeySequence(QKeyCombination(binding.modifiers, static_cast<Qt::Key>(binding.key))).toString(QKeySequence::PortableText);
    if (action >= SelectTab1 && action <= SelectLastTab)
        return QStringLiteral("Ctrl+%1").arg(action == SelectLastTab ? 9 : action - SelectTab1 + 1);
    return {};
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
