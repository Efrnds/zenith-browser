#include "core/BrowserData.h"
#include "core/Navigation.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <QFutureWatcher>
#include <QtConcurrent>

BrowserData::BrowserData(const QString &path, bool readOnly)
    : connection_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_);
    db_.setDatabaseName(path);
    if (readOnly) db_.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000"));
    if (!db_.open()) {
        error_ = db_.lastError().text();
        return;
    }
    if (readOnly) return;
    QSqlQuery q(db_);
    if (path != QStringLiteral(":memory:")) QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    for (const QString &sql : {
             QStringLiteral("PRAGMA journal_mode=WAL"),
             QStringLiteral("PRAGMA busy_timeout=1000"),
             QStringLiteral("CREATE TABLE IF NOT EXISTS visits(url TEXT PRIMARY KEY,title TEXT NOT NULL,visited INTEGER NOT NULL)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS visits_time ON visits(visited DESC)"),
             QStringLiteral("CREATE TABLE IF NOT EXISTS bookmarks(url TEXT PRIMARY KEY,title TEXT NOT NULL,folder TEXT NOT NULL DEFAULT '')"),
             QStringLiteral("CREATE TABLE IF NOT EXISTS permissions(origin TEXT,kind TEXT,decision INTEGER NOT NULL,PRIMARY KEY(origin,kind))"),
             QStringLiteral("CREATE TABLE IF NOT EXISTS zoom(origin TEXT PRIMARY KEY,value REAL NOT NULL)")}) {
        if (!q.exec(sql)) {
            error_ = q.lastError().text();
            break;
        }
    }
}

BrowserData::~BrowserData()
{
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_);
}

bool BrowserData::ready() const { return db_.isOpen() && error_.isEmpty(); }
QString BrowserData::error() const { return error_; }

bool BrowserData::visit(const QUrl &url, const QString &title)
{
    if (!Navigation::isWebUrl(url))
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO visits VALUES(?,?,?)"));
    q.addBindValue(url.toString(QUrl::RemoveUserInfo));
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return q.exec();
}

bool BrowserData::bookmark(const QUrl &url, const QString &title, const QString &folder)
{
    if (!Navigation::isWebUrl(url))
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO bookmarks VALUES(?,?,?)"));
    q.addBindValue(url.toString(QUrl::RemoveUserInfo));
    q.addBindValue(title);
    q.addBindValue(folder.isNull() ? QStringLiteral("") : folder);
    return q.exec();
}

bool BrowserData::removeBookmark(const QUrl &url)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE url=?"));
    q.addBindValue(url.toString(QUrl::RemoveUserInfo));
    return q.exec();
}

QList<BrowserData::Entry> BrowserData::importBookmarks(const QList<Entry> &entries, bool *ok)
{
    *ok = false;
    if (!db_.transaction()) return {};
    QList<Entry> added;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO bookmarks VALUES(?,?,?)"));
    for (const auto &entry : entries) {
        if (!Navigation::isWebUrl(QUrl(entry.url))) continue;
        q.bindValue(0, entry.url);
        q.bindValue(1, entry.title);
        q.bindValue(2, entry.folder.isNull() ? QStringLiteral("") : entry.folder);
        if (!q.exec()) { db_.rollback(); return {}; }
        if (q.numRowsAffected() > 0) added.append(entry);
    }
    *ok = db_.commit();
    if (!*ok) { db_.rollback(); return {}; }
    return added;
}

QList<BrowserData::Entry> BrowserData::entries(bool bookmarks, const QString &search, int offset, QString *error) const
{
    QSqlQuery q(db_);
    q.prepare(bookmarks
        ? QStringLiteral("SELECT url,title,folder,0 FROM bookmarks WHERE instr(lower(url || ' ' || title || ' ' || folder),lower(?))>0 ORDER BY title LIMIT 100 OFFSET ?")
        : QStringLiteral("SELECT url,title,'',visited FROM visits WHERE instr(lower(url || ' ' || title),lower(?))>0 ORDER BY visited DESC LIMIT 100 OFFSET ?"));
    q.addBindValue(search.isNull() ? QStringLiteral("") : search);
    q.addBindValue(std::max(0, offset));
    QList<Entry> result;
    if (q.exec())
        while (q.next())
            result.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toLongLong()});
    if (error) *error = q.lastError().text();
    return result;
}

void BrowserData::requestEntries(bool bookmarks, const QString &search, int offset, QObject *context,
                                std::function<void(QueryResult)> callback) const
{
    const QString path = db_.databaseName();
    if (path == QStringLiteral(":memory:")) {
        QueryResult result;
        result.entries = entries(bookmarks, search, offset, &result.error);
        callback(result);
        return;
    }
    auto *watcher = new QFutureWatcher<QueryResult>(context);
    QObject::connect(watcher, &QFutureWatcher<QueryResult>::finished, context, [watcher, callback = std::move(callback)] {
        const auto result = watcher->result();
        watcher->deleteLater();
        callback(result);
    });
    watcher->setFuture(QtConcurrent::run([path, bookmarks, search, offset] {
        BrowserData reader(path, true);
        QueryResult result;
        if (!reader.ready()) result.error = reader.error();
        else result.entries = reader.entries(bookmarks, search, offset, &result.error);
        return result;
    }));
}

BrowserData::QueryResult BrowserData::suggestions() const
{
    QueryResult result;
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral(
            "SELECT url,MAX(title),'',MAX(visited) FROM ("
            "SELECT url,title,0 AS visited,1 AS priority FROM bookmarks UNION ALL "
            "SELECT url,title,visited,0 AS priority FROM visits) GROUP BY url "
            "ORDER BY MAX(priority) DESC,MAX(visited) DESC,url LIMIT 100"))) {
        result.error = query.lastError().text();
        return result;
    }
    while (query.next())
        result.entries.append({query.value(0).toString(), query.value(1).toString(), {}, query.value(3).toLongLong()});
    return result;
}

void BrowserData::requestSuggestions(QObject *context, std::function<void(QueryResult)> callback) const
{
    const QString path = db_.databaseName();
    if (path == QStringLiteral(":memory:")) {
        callback(suggestions());
        return;
    }
    auto *watcher = new QFutureWatcher<QueryResult>(context);
    QObject::connect(watcher, &QFutureWatcher<QueryResult>::finished, context,
        [watcher, callback = std::move(callback)] {
            const auto result = watcher->result();
            watcher->deleteLater();
            callback(result);
        });
    watcher->setFuture(QtConcurrent::run([path] {
        BrowserData reader(path, true);
        if (!reader.ready()) return QueryResult{{}, reader.error()};
        return reader.suggestions();
    }));
}

QString BrowserData::writeBookmarks(const QString &destination) const
{
    const QString target = QFileInfo(destination).canonicalFilePath();
    const QString absoluteTarget = QFileInfo(destination).absoluteFilePath();
    const QString source = QFileInfo(db_.databaseName()).canonicalFilePath();
    const QStringList protectedPaths{source, source + "-wal", source + "-shm"};
    if (!source.isEmpty() && (protectedPaths.contains(target) || protectedPaths.contains(absoluteTarget)))
        return QStringLiteral("O destino não pode substituir o banco do navegador.");
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT url,title,folder FROM bookmarks ORDER BY folder,title,url")))
        return query.lastError().text();
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) return file.errorString();
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const auto write = [&file](const QString &text) {
        const QByteArray bytes = text.toUtf8();
        return file.write(bytes) == bytes.size();
    };
    if (!write(QStringLiteral("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<meta charset=\"utf-8\"><title>Favoritos Zenith</title>\n<DL><p>\n")))
        return file.errorString();
    QString folder;
    while (query.next()) {
        const QUrl url(query.value(0).toString());
        if (!Navigation::isWebUrl(url)) continue;
        const QString nextFolder = query.value(2).toString();
        if (nextFolder != folder) {
            if (!folder.isEmpty() && !write(QStringLiteral("</DL><p>\n"))) return file.errorString();
            folder = nextFolder;
            if (!folder.isEmpty() && !write(QStringLiteral("<DT><H3>%1</H3>\n<DL><p>\n").arg(folder.toHtmlEscaped())))
                return file.errorString();
        }
        if (!write(QStringLiteral("<DT><A HREF=\"%1\">%2</A>\n")
                .arg(url.toString(QUrl::RemoveUserInfo).toHtmlEscaped(), query.value(1).toString().toHtmlEscaped())))
            return file.errorString();
    }
    if (query.lastError().isValid()) return query.lastError().text();
    if (!folder.isEmpty() && !write(QStringLiteral("</DL><p>\n"))) return file.errorString();
    if (!write(QStringLiteral("</DL><p>\n")) || !file.commit()) return file.errorString();
    return {};
}

void BrowserData::requestBookmarkExport(const QString &destination, QObject *context,
                                      std::function<void(QString)> callback) const
{
    const QString path = db_.databaseName();
    if (path == QStringLiteral(":memory:")) {
        callback(writeBookmarks(destination));
        return;
    }
    auto *watcher = new QFutureWatcher<QString>(context);
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, context,
        [watcher, callback = std::move(callback)] {
            const QString error = watcher->result();
            watcher->deleteLater();
            callback(error);
        });
    watcher->setFuture(QtConcurrent::run([path, destination] {
        BrowserData reader(path, true);
        return reader.ready() ? reader.writeBookmarks(destination) : reader.error();
    }));
}

bool BrowserData::clearHistory(qint64 since)
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM visits WHERE visited>=?"));
    q.addBindValue(since);
    return q.exec();
}

int BrowserData::permission(const QUrl &url, const QString &kind) const
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT decision FROM permissions WHERE origin=? AND kind=?"));
    q.addBindValue(Navigation::origin(url));
    q.addBindValue(kind);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

bool BrowserData::setPermission(const QUrl &url, const QString &kind, int decision)
{
    const QString origin = Navigation::origin(url);
    if (origin.isEmpty() || decision < -1 || decision > 1)
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO permissions VALUES(?,?,?)"));
    q.addBindValue(origin);
    q.addBindValue(kind);
    q.addBindValue(decision);
    return q.exec();
}

bool BrowserData::clearPermissions()
{
    QSqlQuery q(db_);
    return q.exec(QStringLiteral("DELETE FROM permissions"));
}

double BrowserData::zoom(const QUrl &url) const
{
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT value FROM zoom WHERE origin=?"));
    q.addBindValue(Navigation::origin(url));
    return q.exec() && q.next() ? std::clamp(q.value(0).toDouble(), 0.25, 5.0) : 1.0;
}

bool BrowserData::setZoom(const QUrl &url, double value)
{
    const QString origin = Navigation::origin(url);
    if (origin.isEmpty())
        return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO zoom VALUES(?,?)"));
    q.addBindValue(origin);
    q.addBindValue(std::clamp(value, 0.25, 5.0));
    return q.exec();
}
