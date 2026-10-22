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

int DownloadManager::beginNativeDownload(const QString &suggestion,
    const std::function<bool(const QString &, bool)> &accept,
    std::function<void()> cancel)
{
    const QString name = safeName(suggestion);
    QSettings preferences;
    QString directory = preferences.value("downloads/directory",
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    if (!QDir::isAbsolutePath(directory))
        directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const bool ask = preferences.value("downloads/askDestination", true).toBool();
    QString path;
    if (destinationChooser_)
        path = destinationChooser_(name);
    else if (ask)
        path = QFileDialog::getSaveFileName(QApplication::activeWindow(), tr("Salvar download"),
            QDir(directory).filePath(name));
    else if (QDir().mkpath(directory))
        path = uniqueDestination(directory, name);
    if (path.isEmpty()) {
        if (cancel) cancel();
        return -1;
    }
    if (!accept(path, destinationChooser_ || ask)) {
        if (cancel) cancel();
        return -1;
    }
    const int id = nextId_++;
    items_.append({id, path, tr("Baixando"), 0, 0, 0, false, false, nullptr});
    if (cancel) nativeCancelers_.insert(id, std::move(cancel));
    while (items_.size() > 200) {
        const auto oldest = std::find_if(items_.begin(), items_.end(), [](const Item &item) { return item.finished; });
        if (oldest == items_.end()) break;
        nativeCancelers_.remove(oldest->id);
        items_.erase(oldest);
    }
    emit changed();
    persist();
    return id;
}

void DownloadManager::updateNativeDownload(const int id, const qint64 received,
    const qint64 total, const bool finished, const QString &error)
{
    for (Item &item : items_) {
        if (item.id != id) continue;
        item.bytes = received;
        if (total > 0) item.progress = qBound(0.0, double(received) / double(total), 1.0);
        item.finished = finished;
        item.failed = !error.isEmpty();
        if (item.failed) item.status = error;
        else if (finished) {
            item.progress = 1.0;
            item.status = tr("Concluído");
        }
        if (finished) nativeCancelers_.remove(id);
        break;
    }
    if (!progressTimer_->isActive()) progressTimer_->start();
    persist();
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
