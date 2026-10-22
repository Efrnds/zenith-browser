#include "core/DownloadManager.h"
#include "ui/PageWidgets.h"
#include "ui/ZenithIcons.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QApplication>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QDir>
#include <algorithm>
#include <utility>

#undef signals
#undef slots
#include <webkit2/webkit2.h>

DownloadManager::DownloadManager(WebKitWebContext *context, QObject *parent, QString historyPath)
    : QObject(parent), context_(context), historyPath_(std::move(historyPath)), progressTimer_(new QTimer(this))
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
    if (context_) g_signal_connect(context_, "download-started", G_CALLBACK(+[](WebKitWebContext *, WebKitDownload *download, gpointer user) {
        static_cast<DownloadManager *>(user)->started(download);
    }), this);
}

DownloadManager::~DownloadManager()
{
    if (context_) g_signal_handlers_disconnect_by_data(context_, this);
    for (const Item &item : items_) {
        if (!item.native) continue;
        g_signal_handlers_disconnect_by_data(item.native, this);
        if (!item.finished)
            webkit_download_cancel(item.native);
        g_object_unref(item.native);
    }
}


void DownloadManager::trimHistory()
{
    while (items_.size() > 200) {
        const auto oldest = std::find_if(items_.begin(), items_.end(), [](const Item &item) { return item.finished; });
        if (oldest == items_.end()) break;
        const Item old = *oldest;
        items_.erase(oldest);
        if (old.native) {
            g_signal_handlers_disconnect_by_data(old.native, this);
            g_object_unref(old.native);
        }
    }
}

void DownloadManager::started(WebKitDownload *download)
{
    g_object_ref(download);
    items_.append({nextId_++, {}, tr("Escolhendo destino"), 0, 0, 0, false, false, download});
    trimHistory();
    g_signal_connect(download, "decide-destination", G_CALLBACK(+[](WebKitDownload *download, const gchar *suggestion, gpointer user) -> gboolean {
        auto *self = static_cast<DownloadManager *>(user);
        const QString name = safeName(QString::fromUtf8(suggestion));
        QSettings preferences;
        QString directory = preferences.value("downloads/directory", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
        if (!QDir::isAbsolutePath(directory)) directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        const bool ask = preferences.value("downloads/askDestination", true).toBool();
        QString path;
        if (self->destinationChooser_) path = self->destinationChooser_(name);
        else if (ask) path = QFileDialog::getSaveFileName(QApplication::activeWindow(), tr("Salvar download"), QDir(directory).filePath(name));
        else if (QDir().mkpath(directory)) path = uniqueDestination(directory, name);
        if (path.isEmpty()) {
            webkit_download_cancel(download);
            return TRUE;
        }
        const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
        webkit_download_set_allow_overwrite(download, self->destinationChooser_ || ask);
        webkit_download_set_destination(download, uri.constData());
        for (Item &item : self->items_)
            if (item.native == download) {
                item.path = path;
                item.status = tr("Baixando");
            }
        emit self->changed();
        self->persist();
        return TRUE;
    }), this);
    g_signal_connect(download, "received-data", G_CALLBACK(+[](WebKitDownload *download, guint64, gpointer user) {
        auto *self = static_cast<DownloadManager *>(user);
        for (Item &item : self->items_)
            if (item.native == download) {
                item.progress = webkit_download_get_estimated_progress(download);
                item.bytes = webkit_download_get_received_data_length(download);
                item.seconds = webkit_download_get_elapsed_time(download);
            }
        if (!self->progressTimer_->isActive()) self->progressTimer_->start();
    }), this);
    g_signal_connect(download, "failed", G_CALLBACK(+[](WebKitDownload *download, GError *error, gpointer user) {
        auto *self = static_cast<DownloadManager *>(user);
        for (Item &item : self->items_)
            if (item.native == download) {
                item.failed = true;
                item.status = QString::fromUtf8(error->message);
            }
        emit self->changed();
    }), this);
    g_signal_connect(download, "finished", G_CALLBACK(+[](WebKitDownload *download, gpointer user) {
        auto *self = static_cast<DownloadManager *>(user);
        for (Item &item : self->items_)
            if (item.native == download) {
                item.finished = true;
                item.native = nullptr;
                if (!item.failed) {
                    item.progress = 1;
                    item.status = tr("Concluído");
                }
            }
        g_signal_handlers_disconnect_by_data(download, self);
        g_object_unref(download);
        self->trimHistory();
        self->progressTimer_->stop();
        emit self->changed();
        self->persist();
    }), this);
    emit changed();
}

void DownloadManager::cancel(int id)
{
    for (const Item &item : items_)
        if (item.id == id && !item.finished) {
            webkit_download_cancel(item.native);
            return;
        }
}
bool DownloadManager::active() const
{
    for (const Item &item : items_)
        if (!item.finished)
            return true;
    return false;
}
