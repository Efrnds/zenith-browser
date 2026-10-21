#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QUrl>
#include <functional>

class BrowserData
{
public:
    struct Entry {
        QString url;
        QString title;
        QString folder;
        qint64 visited = 0;
    };
    struct QueryResult {
        QList<Entry> entries;
        QString error;
    };
    explicit BrowserData(const QString &path, bool readOnly = false);
    ~BrowserData();
    BrowserData(const BrowserData &) = delete;
    BrowserData &operator=(const BrowserData &) = delete;
    bool ready() const;
    QString error() const;
    bool visit(const QUrl &url, const QString &title);
    bool bookmark(const QUrl &url, const QString &title, const QString &folder = {});
    bool removeBookmark(const QUrl &url);
    QList<Entry> importBookmarks(const QList<Entry> &entries, bool *ok);
    QList<Entry> entries(bool bookmarks, const QString &search = {}, int offset = 0, QString *error = nullptr) const;
    void requestEntries(bool bookmarks, const QString &search, int offset, QObject *context,
                        std::function<void(QueryResult)> callback) const;
    void requestSuggestions(QObject *context, std::function<void(QueryResult)> callback) const;
    void requestBookmarkExport(const QString &destination, QObject *context,
                               std::function<void(QString)> callback) const;
    bool clearHistory(qint64 since = 0);
    int permission(const QUrl &url, const QString &kind) const;
    bool setPermission(const QUrl &url, const QString &kind, int decision);
    bool clearPermissions();
    double zoom(const QUrl &url) const;
    bool setZoom(const QUrl &url, double value);

private:
    QueryResult suggestions() const;
    QString writeBookmarks(const QString &destination) const;
    QSqlDatabase db_;
    QString connection_;
    QString error_;
};
