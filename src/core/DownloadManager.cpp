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

QString DownloadManager::safeName(const QString &suggestion)
{
    QString name = suggestion;
    name.replace('\\', '/');
    name = name.section('/', -1);
    name.remove(QChar::Null);
    for (QChar &ch : name)
        if (ch.category() == QChar::Other_Control || ch.category() == QChar::Other_Format
            || ch.category() == QChar::Separator_Line || ch.category() == QChar::Separator_Paragraph)
            ch = '_';
    if (name.isEmpty() || name == "." || name == "..")
        return QStringLiteral("download");
    return name.left(200);
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

void DownloadManager::persist() const
{
    if (historyPath_.isEmpty()) return;
    QJsonArray records;
    for (const Item &item : items_)
        if (!item.path.isEmpty()) records.append(QJsonObject{{"path", item.path}, {"completed", item.finished && !item.failed}});
    QSaveFile file(historyPath_);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray bytes = QJsonDocument(records).toJson(QJsonDocument::Compact);
    if (file.write(bytes) == bytes.size()) file.commit();
}

QList<DownloadManager::Item> DownloadManager::items() const { return items_; }
void DownloadManager::setDestinationChooser(std::function<QString(const QString &)> chooser)
{
    destinationChooser_ = std::move(chooser);
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

QDialog *DownloadManager::createPage(QWidget *parent)
{
    auto *page = new QDialog(parent, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    QDialog &dialog = *page;
    dialog.setObjectName("downloadsPage");
    dialog.setWindowTitle(tr("Downloads"));
    dialog.resize(680, 420);
    auto *layout = new QVBoxLayout(&dialog);
    PageWidgets::heading(layout, tr("Downloads"), tr("Acompanhe transferências e encontre os arquivos baixados."));
    auto *list = new QListWidget(&dialog);
    list->setObjectName("detailList");
    list->setItemDelegate(new DetailListDelegate(list));
    list->setAccessibleName(tr("Downloads"));
    auto *empty = PageWidgets::emptyState(&dialog, tr("Nenhum download por aqui"), tr("Os arquivos que você baixar aparecerão nesta lista."));
    layout->addWidget(empty, 1);
    layout->addWidget(list, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *cancelButton = buttons->addButton(tr("Cancelar download"), QDialogButtonBox::ActionRole);
    auto *folder = buttons->addButton(tr("Abrir pasta"), QDialogButtonBox::ActionRole);
    auto *open = buttons->addButton(tr("Abrir arquivo"), QDialogButtonBox::ActionRole);
    open->setObjectName("openDownload");
    auto *feedback = new QLabel(&dialog);
    feedback->setWordWrap(true);
    feedback->hide();
    layout->addWidget(feedback);
    auto updateSelection = [this, list, cancelButton, folder, open, feedback] {
        feedback->hide();
        open->setEnabled(false);
        cancelButton->setEnabled(false);
        folder->setEnabled(false);
        if (!list->currentItem()) return;
        for (const auto &item : items_)
            if (item.id == list->currentItem()->data(Qt::UserRole).toInt()) {
                cancelButton->setEnabled(!item.finished);
                folder->setEnabled(!item.path.isEmpty());
                open->setEnabled(item.finished && !item.failed && !item.path.isEmpty());
            }
    };
    auto refresh = [this, list, empty, updateSelection] {
        QHash<int, QListWidgetItem *> rows;
        for (int index = 0; index < list->count(); ++index) {
            auto *row = list->item(index);
            rows.insert(row->data(Qt::UserRole).toInt(), row);
        }
        QSet<int> retained;
        for (const Item &item : items_) {
            auto *row = rows.value(item.id);
            if (!row) {
                row = new QListWidgetItem(list);
                row->setData(Qt::UserRole, item.id);
                row->setIcon(zenithIcon(ZenithIcon::Download));
            }
            retained.insert(item.id);
            const QString detail = item.finished ? item.status : tr("%1 · %2% · %3 KiB/s").arg(item.status)
                .arg(int(item.progress * 100)).arg(item.seconds > 0 ? item.bytes / item.seconds / 1024 : 0, 0, 'f', 0);
            const QString text = (item.path.isEmpty() ? tr("Preparando download") : QFileInfo(item.path).fileName()) + '\n' + detail;
            if (row->text() != text) row->setText(text);
            row->setToolTip(PageWidgets::plainToolTip(item.path));
            row->setData(Qt::UserRole + 4, item.progress);
            row->setData(Qt::UserRole + 5, !item.finished);
        }
        for (auto it = rows.cbegin(); it != rows.cend(); ++it)
            if (!retained.contains(it.key())) delete list->takeItem(list->row(it.value()));
        empty->setVisible(items_.isEmpty());
        list->setVisible(!items_.isEmpty());
        updateSelection();
    };
    connect(this, &DownloadManager::changed, &dialog, refresh);
    connect(list, &QListWidget::currentRowChanged, &dialog, updateSelection);
    connect(cancelButton, &QPushButton::clicked, &dialog, [this, list] {
        if (list->currentItem())
            cancel(list->currentItem()->data(Qt::UserRole).toInt());
    });
    connect(folder, &QPushButton::clicked, &dialog, [this, list] {
        if (!list->currentItem())
            return;
        for (const Item &item : items_)
            if (item.id == list->currentItem()->data(Qt::UserRole).toInt() && !item.path.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(item.path).absolutePath()));
    });
    const auto openFile = [this, list, feedback] {
        if (!list->currentItem()) return;
        for (const Item &item : items_) {
            if (item.id != list->currentItem()->data(Qt::UserRole).toInt()
                || !item.finished || item.failed || item.path.isEmpty()) continue;
            const QFileInfo file(item.path);
            const bool exists = file.isFile();
            if (exists && QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()))) return;
            feedback->setText(exists ? tr("Não foi possível abrir o arquivo. Tente abrir pela pasta.")
                : tr("O arquivo foi movido ou removido. Abra a pasta ou baixe novamente."));
            feedback->show();
            return;
        }
    };
    connect(open, &QPushButton::clicked, &dialog, openFile);
    connect(list, &QListWidget::itemActivated, &dialog, [openFile](QListWidgetItem *) { openFile(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    refresh();
    return page;
}

QString DownloadManager::uniqueDestination(const QString &directory, const QString &suggestion)
{
    const QString name = safeName(suggestion);
    const QFileInfo info(name);
    const QString suffix = info.completeSuffix();
    const QString stem = suffix.isEmpty() ? name : name.left(name.size() - suffix.size() - 1);
    for (int attempt = 0; attempt < 10000; ++attempt) {
        const QString candidate = attempt == 0 ? name
            : stem + QString(" (%1)").arg(attempt) + (suffix.isEmpty() ? QString() : '.' + suffix);
        const QString path = QDir(directory).absoluteFilePath(candidate);
        if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) return path;
    }
    return {};
}
