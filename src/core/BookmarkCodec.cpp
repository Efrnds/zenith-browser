#include "core/BookmarkCodec.h"
#include "core/Navigation.h"
#include <QMap>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>

QByteArray BookmarkCodec::encode(const QList<BrowserData::Entry> &entries)
{
    QString html = QStringLiteral("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<meta charset=\"utf-8\"><title>Favoritos Zenith</title>\n<DL><p>\n");
    QMap<QString, QList<BrowserData::Entry>> folders;
    for (const auto &entry : entries) folders[entry.folder].append(entry);
    for (auto folder = folders.cbegin(); folder != folders.cend(); ++folder) {
        if (!folder.key().isEmpty()) html += QStringLiteral("<DT><H3>%1</H3>\n<DL><p>\n").arg(folder.key().toHtmlEscaped());
        for (const auto &entry : folder.value()) {
            if (!Navigation::isWebUrl(QUrl(entry.url))) continue;
            html += QStringLiteral("<DT><A HREF=\"%1\">%2</A>\n").arg(entry.url.toHtmlEscaped(), entry.title.toHtmlEscaped());
        }
        if (!folder.key().isEmpty()) html += QStringLiteral("</DL><p>\n");
    }
    return (html + QStringLiteral("</DL><p>\n")).toUtf8();
}

QList<BrowserData::Entry> BookmarkCodec::decode(const QByteArray &html)
{
    if (html.size() > 10 * 1024 * 1024) return {};
    QTextDocument document;
    document.setHtml(QString::fromUtf8(html));
    QMap<QString, BrowserData::Entry> entries;
    QString folder;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() == 3) folder = block.text().left(256);
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            const QUrl url(fragment.charFormat().anchorHref());
            if (!Navigation::isWebUrl(url)) continue;
            const QString key = url.toString(QUrl::RemoveUserInfo);
            auto &entry = entries[key];
            entry.url = key;
            entry.title += fragment.text();
            entry.folder = folder;
        }
    }
    return entries.values();
}
