#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>

inline QJsonObject processTreeMemory()
{
    QMap<int, int> parents;
    for (const auto &name : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const int pid = name.toInt(&numeric);
        if (!numeric) continue;
        QFile stat("/proc/" + name + "/stat");
        if (!stat.open(QIODevice::ReadOnly)) continue;
        const QByteArray value = stat.readAll();
        const auto fields = value.mid(value.lastIndexOf(')') + 2).simplified().split(' ');
        if (fields.size() > 1) parents.insert(pid, fields[1].toInt());
    }
    const int root = static_cast<int>(QCoreApplication::applicationPid());
    QSet<int> selected{root};
    for (;;) {
        const int count = selected.size();
        for (auto it = parents.cbegin(); it != parents.cend(); ++it)
            if (selected.contains(it.value())) selected.insert(it.key());
        if (selected.size() == count) break;
    }
    QJsonArray rows;
    qint64 totalRss = 0, totalPss = 0;
    bool complete = true;
    for (int pid : selected) {
        QJsonObject row{{"pid", pid}};
        QFile name(QString("/proc/%1/comm").arg(pid));
        if (name.open(QIODevice::ReadOnly)) row.insert("name", QString::fromUtf8(name.readAll()).trimmed());
        QFile memory(QString("/proc/%1/smaps_rollup").arg(pid));
        qint64 rss = -1, pss = -1;
        if (memory.open(QIODevice::ReadOnly)) {
            const auto lines = memory.readAll().split('\n');
            for (const auto &line : lines) {
                const auto words = line.simplified().split(' ');
                if (words.size() < 2) continue;
                if (words[0] == "Rss:") rss = words[1].toLongLong();
                if (words[0] == "Pss:") pss = words[1].toLongLong();
            }
        }
        if (rss < 0 || pss < 0) {
            complete = false;
            row.insert("error", memory.errorString());
        }
        row.insert("rss_kib", rss < 0 ? QJsonValue() : QJsonValue(rss));
        row.insert("pss_kib", pss < 0 ? QJsonValue() : QJsonValue(pss));
        totalRss += qMax(qint64(0), rss);
        totalPss += qMax(qint64(0), pss);
        rows.append(row);
    }
    return {{"root_pid", root}, {"complete", complete}, {"rss_kib", totalRss},
            {"pss_kib", totalPss}, {"processes", rows}};
}
