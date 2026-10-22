#include "core/DownloadManager.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <utility>

DownloadManager::DownloadManager(QObject *parent, QString historyPath)
    : QObject(parent), historyPath_(std::move(historyPath)), progressTimer_(new QTimer(this))
{
    progressTimer_->setSingleShot(true);
    progressTimer_->setInterval(150);
    connect(progressTimer_, &QTimer::timeout, this, &DownloadManager::changed);
    QFile file(historyPath_);
    if (!historyPath_.isEmpty() && file.open(QIODevice::ReadOnly) && file.size() < 1024 * 1024) {
        const auto records = QJsonDocument::fromJson(file.readAll()).array();
        for (const auto &record : records) {
            if (items_.size() >= 200) break;
            const auto object = record.toObject();
            const QString path = object.value("path").toString();
            if (path.isEmpty() || !QFileInfo(path).isAbsolute()) continue;
            const bool completed = object.value("completed").toBool();
            items_.append({nextId_++, path, completed ? tr("Concluído") : tr("Interrompido; baixe novamente"),
                completed ? 1.0 : 0.0, 0, 0, true, !completed, nullptr});
        }
    }
}

DownloadManager::~DownloadManager()
{
    const auto cancelers = nativeCancelers_;
    nativeCancelers_.clear();
    for (const auto &cancel : cancelers)
        if (cancel) cancel();
}

void DownloadManager::cancel(const int id)
{
    auto canceler = nativeCancelers_.take(id);
    if (!canceler) return;
    canceler();
    for (Item &item : items_) {
        if (item.id != id) continue;
        item.finished = true;
        item.failed = true;
        item.status = tr("Cancelado");
        break;
    }
    emit changed();
    persist();
}

bool DownloadManager::active() const
{
    for (const Item &item : items_)
        if (!item.finished) return true;
    return false;
}
