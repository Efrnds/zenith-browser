#include "ui/UrlBar.h"
#include <QCoreApplication>
#include <QApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include "ui/SettingsPage.h"
#include "ui/MainWindow.h"
#include "core/BrowserData.h"
#include "core/BookmarkCodec.h"
#include "core/DownloadManager.h"
#include "core/Navigation.h"
#include "core/TabManager.h"
#include "core/WebProfile.h"
#include "core/WebView.h"
#include "ui/SplitView.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QSettings>
#include <QComboBox>
#include <QSet>
#include <algorithm>
#include "ui/Sidebar.h"
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QShortcut>
#include <QVBoxLayout>

int MainWindow::activeTabId() const
{
    return splitEnabled_ && activePanelId_ == secondaryTabId_ ? secondaryTabId_ : currentTabId_;
}

void MainWindow::setPageInputEnabled(bool enabled)
{
    for (const int id : QSet<int>{currentTabId_, secondaryTabId_})
        if (auto *record = tabManager_->tab(id); record && record->view)
            record->view->setPageFocusEnabled(enabled);
}

WebView *MainWindow::currentView() const
{
    const auto *record = tabManager_->tab(activeTabId());
    return record ? record->view.data() : nullptr;
}

void MainWindow::openUrls(const QList<QUrl> &urls)
{
    if (!sessionOpened_) {
        QTimer::singleShot(100, this, [this, urls] { openUrls(urls); });
        return;
    }
    for (const QUrl &url : urls)
        if (url.isValid()) createTab(url, currentWorkspace_, true);
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::executeCommand(int command)
{
    if (command >= Navigation::SelectTab1 && command <= Navigation::SelectLastTab) {
        const auto ids = tabManager_->orderedTabs(currentWorkspace_);
        const int index = command == Navigation::SelectLastTab ? ids.size() - 1 : command - Navigation::SelectTab1;
        if (index >= 0 && index < ids.size()) activateTab(ids.at(index));
        return;
    }
    WebView *view = settingsVisible_ ? nullptr : currentView();
    switch (command) {
    case Navigation::Settings: showSettings(); break;
    case Navigation::BrowserMenu:
        if (auto *popup = QApplication::activePopupWidget()) { popup->close(); break; }
        showAddressBar(false);
        showBrowserMenu(mapToGlobal(QPoint(width() - 40, 58)));
        break;
    case Navigation::ToggleCompact: setCompactMode(!compactMode_); break;
    case Navigation::NewPrivateWindow:
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--private")}); break;
    case Navigation::Credentials: showCredentialManager(); break;
    case Navigation::SiteControls: if (view) showSiteControls(); break;
    case Navigation::ClearData: showClearData(); break;
    case Navigation::ImportBookmarks: importBookmarks(); break;
    case Navigation::ExportBookmarks: exportBookmarks(); break;
    case Navigation::OpenProfile: openProfile(); break;
    case Navigation::DefaultProfile: setDefaultProfile(); break;
    case Navigation::DefaultBrowser: setDefaultBrowser(); break;
    case Navigation::SaveSession: if (!privateMode_) saveSession(); break;
    case Navigation::CloseWindow: close(); break;
    case Navigation::NewTab: createTab(QUrl("about:blank"), currentWorkspace_, true); break;
    case Navigation::Address: revealAddressBar(); break;
    case Navigation::CloseTab:
        if (activeTabId() == currentTabId_) closeCurrentTab();
        else closeTab(activeTabId());
        break;
    case Navigation::NextTab: cycleTabs(1); break;
    case Navigation::PreviousTab: cycleTabs(-1); break;
    case Navigation::ReopenTab: {
        const int id = tabManager_->reopenClosedTab(splitView_);
        if (id >= 0) activateTab(id);
        break;
    }
    case Navigation::Find: if (view) showFind(); break;
    case Navigation::FindNext: case Navigation::FindPrevious:
        if (!view) break;
        if (view && !view->lastFindText().isEmpty()) view->find(view->lastFindText(), command == Navigation::FindPrevious);
        else showFind();
        break;
    case Navigation::HardReload:
        if (view) view->reload(true);
        break;
    case Navigation::Reload:
        if (view) view->reload();
        break;
    case Navigation::Back: if (settingsVisible_) closeSettings(); else if (view) view->back(); break;
    case Navigation::Forward: if (view) view->forward(); break;
    case Navigation::ZoomIn: case Navigation::ZoomOut: case Navigation::ZoomReset:
        if (view) {
            const double value = command == Navigation::ZoomReset ? 1.0 : view->zoom() + (command == Navigation::ZoomIn ? 0.1 : -0.1);
            view->setZoom(value);
            profile_->data()->setZoom(view->url(), view->zoom());
        }
        break;
    case Navigation::History: showLibrary(false); break;
    case Navigation::Bookmarks: showLibrary(true); break;
    case Navigation::Bookmark: if (view) addBookmark(); break;
    case Navigation::Downloads: profile_->downloads()->show(this); break;
    case Navigation::Fullscreen: isFullScreen() ? showNormal() : showFullScreen(); break;
    case Navigation::Print: if (view) view->print(); break;
    case Navigation::Inspector: if (view) view->inspect(); break;
    case Navigation::ToggleSplit: if (view) toggleSplit(!splitEnabled_); break;
    }
}

void MainWindow::showFind()
{
    const QPointer<WebView> view = currentView();
    if (!view) return;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Buscar nesta página"));
    dialog.resize(460, 150);
    auto *layout = new QVBoxLayout(&dialog);
    auto *input = new QLineEdit(&dialog);
    input->setText(view->lastFindText());
    input->setAccessibleName(tr("Texto da busca"));
    input->setPlaceholderText(tr("Texto da busca"));
    auto *status = new QLabel(tr("Digite para buscar."), &dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *next = buttons->addButton(tr("Próxima"), QDialogButtonBox::ActionRole);
    auto *previous = buttons->addButton(tr("Anterior"), QDialogButtonBox::ActionRole);
    const auto updateButtons = [input, next, previous] {
        next->setEnabled(!input->text().isEmpty());
        previous->setEnabled(!input->text().isEmpty());
    };
    updateButtons();
    connect(input, &QLineEdit::textChanged, &dialog, updateButtons);
    for (auto *button : buttons->findChildren<QPushButton *>()) button->setAutoDefault(false);
    for (const QString &key : {QString("F3"), QString("Ctrl+G")})
        connect(new QShortcut(QKeySequence(key), &dialog), &QShortcut::activated, next, &QPushButton::click);
    for (const QString &key : {QString("Shift+F3"), QString("Ctrl+Shift+G")})
        connect(new QShortcut(QKeySequence(key), &dialog), &QShortcut::activated, previous, &QPushButton::click);
    layout->addWidget(input);
    layout->addWidget(status);
    layout->addWidget(buttons);
    auto *timer = new QTimer(&dialog);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(input, &QLineEdit::textChanged, timer, qOverload<>(&QTimer::start));
    connect(timer, &QTimer::timeout, &dialog, [view, input] { if (view) view->find(input->text()); });
    connect(next, &QPushButton::clicked, &dialog, [view, input] { if (view) view->find(input->text()); });
    connect(input, &QLineEdit::returnPressed, next, &QPushButton::click);
    connect(previous, &QPushButton::clicked, &dialog, [view, input] { if (view) view->find(input->text(), true); });
    connect(view, &WebView::findResult, &dialog, [status, input](int matches) {
        status->setText(input->text().isEmpty() ? tr("Digite para buscar.") : matches == 0 ? tr("Nenhuma ocorrência.") : matches > 10000 ? tr("Mais de 10.000 ocorrências.") : tr("%1 ocorrências").arg(matches));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    input->setFocus();
    input->selectAll();
    if (!input->text().isEmpty()) view->find(input->text());
    dialog.exec();
    if (view) view->finishFind();
}

void MainWindow::addBookmark()
{
    const auto *record = tabManager_->tab(activeTabId());
    if (!record || !Navigation::isWebUrl(record->url)) {
        QMessageBox::information(this, tr("Favoritos"), tr("Abra uma página HTTP ou HTTPS para salvar um favorito."));
        return;
    }
    const QUrl url = record->url;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Adicionar favorito"));
    auto *form = new QFormLayout(&dialog);
    auto *title = new QLineEdit(record->title, &dialog);
    auto *folder = new QLineEdit(&dialog);
    form->addRow(tr("Título"), title);
    form->addRow(tr("Pasta"), folder);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted && !profile_->data()->bookmark(url, title->text(), folder->text()))
        QMessageBox::warning(this, tr("Favoritos"), tr("Não foi possível salvar o favorito."));
}

void MainWindow::showLibrary(bool bookmarks)
{
    QDialog dialog(this);
    dialog.setWindowTitle(bookmarks ? tr("Favoritos") : tr("Histórico"));
    dialog.resize(700, 480);
    auto *layout = new QVBoxLayout(&dialog);
    auto *search = new QLineEdit(&dialog);
    search->setPlaceholderText(tr("Pesquisar"));
    search->setAccessibleName(tr("Pesquisar registros"));
    auto *list = new QListWidget(&dialog);
    list->setAccessibleName(dialog.windowTitle());
    auto *status = new QLabel(&dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *open = buttons->addButton(tr("Abrir"), QDialogButtonBox::ActionRole);
    auto *remove = buttons->addButton(bookmarks ? tr("Remover favorito") : tr("Limpar histórico"), QDialogButtonBox::ActionRole);
    auto *more = buttons->addButton(tr("Carregar mais"), QDialogButtonBox::ActionRole);
    auto *edit = bookmarks ? buttons->addButton(tr("Editar"), QDialogButtonBox::ActionRole) : nullptr;
    auto updateSelection = [list, open, remove, edit, bookmarks] {
        open->setEnabled(list->currentItem() != nullptr);
        if (edit) edit->setEnabled(list->currentItem() != nullptr);
        remove->setEnabled(bookmarks ? list->currentItem() != nullptr : list->count() > 0);
    };
    connect(list, &QListWidget::currentRowChanged, &dialog, updateSelection);
    int offset = 0;
    int generation = 0;
    auto load = [this, list, status, more, search, bookmarks, updateSelection, &offset, &generation, &dialog](bool reset) {
        if (reset) { offset = 0; list->clear(); }
        const int request = ++generation;
        more->setEnabled(false);
        status->setText(tr("Carregando registros…"));
        profile_->data()->requestEntries(bookmarks, search->text(), offset, &dialog,
            [list, status, more, updateSelection, request, &offset, &generation](const BrowserData::QueryResult &result) {
                if (request != generation) return;
                if (!result.error.isEmpty()) {
                    status->setText(tr("Não foi possível consultar os registros."));
                    status->setToolTip(result.error);
                    updateSelection();
                    return;
                }
                const auto &entries = result.entries;
                for (const auto &entry : entries) {
                    auto *item = new QListWidgetItem(entry.title + '\n' + entry.url + (entry.folder.isEmpty() ? QString() : " · " + entry.folder), list);
                    item->setData(Qt::UserRole, entry.url);
                    item->setData(Qt::UserRole + 1, entry.title);
                    item->setData(Qt::UserRole + 2, entry.folder);
                }
                offset += entries.size();
                more->setEnabled(entries.size() == 100);
                status->setText(list->count() ? tr("%1 registros").arg(list->count()) : tr("Nenhum registro encontrado."));
                updateSelection();
            });
    };
    auto openSelected = [this, list, &dialog] {
        if (list->currentItem()) {
            const QUrl url(list->currentItem()->data(Qt::UserRole).toString());
            createTab(url, currentWorkspace_, true);
            dialog.accept();
        }
    };
    connect(open, &QPushButton::clicked, &dialog, openSelected);
    if (edit) connect(edit, &QPushButton::clicked, &dialog, [this, list, &load] {
        if (!list->currentItem()) return;
        const QString url = list->currentItem()->data(Qt::UserRole).toString();
        BrowserData::Entry selected{url, list->currentItem()->data(Qt::UserRole + 1).toString(), list->currentItem()->data(Qt::UserRole + 2).toString(), 0};
        QDialog editor(this);
        editor.setWindowTitle(tr("Editar favorito"));
        auto *form = new QFormLayout(&editor);
        auto *title = new QLineEdit(selected.title, &editor);
        auto *folder = new QLineEdit(selected.folder, &editor);
        form->addRow(tr("Título"), title);
        form->addRow(tr("Pasta"), folder);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &editor);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &editor, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &editor, &QDialog::reject);
        if (editor.exec() == QDialog::Accepted) {
            if (!profile_->data()->bookmark(QUrl(url), title->text(), folder->text()))
                QMessageBox::warning(this, tr("Favoritos"), tr("Não foi possível salvar a edição."));
            load(true);
        }
    });
    connect(list, &QListWidget::itemActivated, &dialog, openSelected);
    auto *timer = new QTimer(&dialog);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(search, &QLineEdit::textChanged, timer, qOverload<>(&QTimer::start));
    connect(timer, &QTimer::timeout, &dialog, [&load] { load(true); });
    connect(more, &QPushButton::clicked, &dialog, [&load] { load(false); });
    connect(remove, &QPushButton::clicked, &dialog, [this, list, bookmarks, &load] {
        if (bookmarks) {
            if (list->currentItem()) profile_->data()->removeBookmark(QUrl(list->currentItem()->data(Qt::UserRole).toString()));
        } else if (QMessageBox::question(this, tr("Limpar histórico"), tr("Excluir todas as visitas?"),
                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            profile_->data()->clearHistory();
        }
        load(true);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(search);
    layout->addWidget(list);
    layout->addWidget(status);
    layout->addWidget(buttons);
    load(true);
    dialog.exec();
}

void MainWindow::showSiteControls()
{
    WebView *view = currentView();
    if (!view || !Navigation::isWebUrl(view->url())) return;
    const QUrl url = view->url();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Permissões: %1").arg(Navigation::origin(url)));
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Desmarcar um bloqueio volta a perguntar no próximo pedido.\nAcessos já ativos terminam ao recarregar ou fechar a página."), &dialog));
    const QList<QPair<QString, QString>> permissions {
        {QStringLiteral("notifications"), tr("notificações")},
        {QStringLiteral("geolocation"), tr("localização")},
        {QStringLiteral("camera"), tr("câmera")},
        {QStringLiteral("microphone"), tr("microfone")},
        {QStringLiteral("camera-microphone"), tr("câmera e microfone")},
        {QStringLiteral("display"), tr("compartilhamento de tela")}
    };
    for (const auto &[kind, label] : permissions) {
        auto *check = new QCheckBox(tr("Bloquear %1").arg(label), &dialog);
        check->setChecked(profile_->data()->permission(url, kind) == -1);
        connect(check, &QCheckBox::toggled, &dialog, [this, url, kind](bool block) { profile_->data()->setPermission(url, kind, block ? -1 : 0); });
        layout->addWidget(check);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showClearData()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Limpar dados de navegação"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *history = new QCheckBox(tr("Histórico de visitas"), &dialog);
    auto *period = new QComboBox(&dialog);
    layout->addWidget(new QLabel(tr("Período do histórico de visitas"), &dialog));
    period->setAccessibleName(tr("Período do histórico"));
    period->addItem(tr("Última hora"), 3600);
    period->addItem(tr("Últimas 24 horas"), 86400);
    period->addItem(tr("Últimos sete dias"), 604800);
    period->addItem(tr("Todo o histórico"), 0);
    period->setCurrentIndex(3);
    layout->addWidget(period);
    auto *cookies = new QCheckBox(tr("Cookies e armazenamento dos sites (encerra logins)"), &dialog);
    auto *cache = new QCheckBox(tr("Cache"), &dialog);
    auto *permissions = new QCheckBox(tr("Decisões de permissões"), &dialog);
    auto *certificates = new QCheckBox(tr("Exceções TLS persistidas (exige reiniciar)"), &dialog);
    const QStringList exceptions = profile_->certificateExceptions();
    if (!exceptions.isEmpty()) {
        auto *hosts = new QLabel(tr("Exceções TLS salvas: %1").arg(exceptions.join(", ")), &dialog);
        hosts->setWordWrap(true);
        layout->addWidget(hosts);
    }
    for (auto *box : {history, cookies, cache, permissions, certificates}) layout->addWidget(box);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    bool ok = true;
    if (history->isChecked()) {
        const qint64 seconds = period->currentData().toLongLong();
        ok &= profile_->data()->clearHistory(seconds ? QDateTime::currentMSecsSinceEpoch() - seconds * 1000 : 0);
    }
    if (permissions->isChecked()) ok &= profile_->data()->clearPermissions();
    if (certificates->isChecked()) ok &= profile_->removeCertificateExceptions();
    const QPointer<MainWindow> self(this);
    profile_->clearWebsiteData(cookies->isChecked(), cache->isChecked(), [self, ok](bool webOk) {
        if (self) QMessageBox::information(self, tr("Limpar dados"), ok && webOk ? tr("Limpeza concluída.") : tr("Não foi possível limpar todos os dados."));
    });
}

void MainWindow::exportBookmarks()
{
    if (exportingBookmarks_) {
        QMessageBox::information(this, tr("Exportar favoritos"), tr("Uma exportação já está em andamento."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar favoritos"), "favoritos.html", "HTML (*.html)");
    if (path.isEmpty()) return;
    exportingBookmarks_ = true;
    setCursor(Qt::BusyCursor);
    profile_->data()->requestBookmarkExport(path, this, [this](const QString &error) {
        exportingBookmarks_ = false;
        unsetCursor();
        if (!error.isEmpty()) QMessageBox::warning(this, tr("Exportar favoritos"), tr("Não foi possível exportar: %1").arg(error));
        else QMessageBox::information(this, tr("Exportar favoritos"), tr("Favoritos exportados."));
    });
}

void MainWindow::importBookmarks()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar favoritos"), {}, "HTML (*.html *.htm)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 10 * 1024 * 1024) {
        QMessageBox::warning(this, tr("Importar"), tr("Arquivo ilegível ou maior que 10 MiB."));
        return;
    }
    const auto entries = BookmarkCodec::decode(file.readAll());
    if (QMessageBox::question(this, tr("Importar favoritos"),
            tr("Importar até %1 endereços? Favoritos existentes serão preservados.").arg(entries.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    bool ok = false;
    const auto added = profile_->data()->importBookmarks(entries, &ok);
    if (!ok) {
        QMessageBox::warning(this, tr("Importar"), tr("A importação falhou. Nenhum favorito foi alterado."));
        return;
    }
    QMessageBox result(QMessageBox::Information, tr("Importar"), tr("%1 favoritos adicionados.").arg(added.size()), QMessageBox::Close, this);
    auto *undo = result.addButton(tr("Desfazer importação"), QMessageBox::ActionRole);
    result.exec();
    if (result.clickedButton() == undo)
        for (const auto &entry : added) profile_->data()->removeBookmark(QUrl(entry.url));
}

void MainWindow::manageWorkspace(int action)
{
    if (action == 2) {
        const int last = workspaceNames_.size() - 1;
        if (last < 1 || !tabManager_->tabsInWorkspace(last, false).isEmpty()) {
            QMessageBox::information(this, tr("Excluir espaço"), tr("Mantenha ao menos um espaço. O último espaço precisa estar sem abas."));
            return;
        }
        for (const auto &folder : tabManager_->foldersInWorkspace(last)) tabManager_->removeFolder(folder.id);
        workspaceNames_.removeLast();
        if (currentWorkspace_ == last) switchWorkspace(0);
    } else {
        if (action == 0 && workspaceNames_.size() >= 16) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, action == 0 ? tr("Novo espaço") : tr("Renomear espaço"),
            tr("Nome:"), QLineEdit::Normal, action == 0 ? QString() : workspaceNames_.at(currentWorkspace_), &ok).trimmed().left(80);
        if (!ok || name.isEmpty()) return;
        if (action == 0) workspaceNames_.append(name);
        else workspaceNames_[currentWorkspace_] = name;
    }
    settingsPage_->setWorkspaces(workspaceNames_, currentWorkspace_);
    tabManager_->setWorkspaceCount(workspaceNames_.size());
    sidebar_->setWorkspaces(workspaceNames_);
    sidebar_->setWorkspace(currentWorkspace_);
    syncFolders();
    if (action == 0) {
        if (settingsVisible_) closeSettings();
        switchWorkspace(workspaceNames_.size() - 1);
        settingsPage_->setWorkspaces(workspaceNames_, currentWorkspace_);
    }
    scheduleSessionSave();
}

void MainWindow::closeFollowingTabs(int id)
{
    const auto *record = tabManager_->tab(id);
    if (!record) return;
    const auto order = tabManager_->orderedTabs(record->isEssential ? currentWorkspace_ : record->workspace);
    const int selected = order.indexOf(id);
    if (selected < 0 || selected + 1 >= order.size()) return;
    if (QMessageBox::question(this, tr("Fechar abas seguintes"), tr("Fechar as abas não fixadas que aparecem depois desta no espaço atual?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes || !tabManager_->contains(id)) return;
    activateTab(id);
    for (int next = selected + 1; next < order.size(); ++next) {
        const int candidate = order.at(next);
        if (const auto *tab = tabManager_->tab(candidate); tab && !tab->isEssential) closeTab(candidate);
    }
}

void MainWindow::openProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Abrir perfil"), tr("Nome (letras, números, _ ou -):"), QLineEdit::Normal, {}, &ok);
    if (!ok) return;
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,32}$")).match(name).hasMatch()) {
        QMessageBox::warning(this, tr("Perfil"), tr("Nome inválido. Use entre 1 e 32 letras, números, _ ou -."));
        return;
    }
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--profile"), name});
}

void MainWindow::setDefaultProfile()
{
    if (privateMode_) return;
    const QString app = QCoreApplication::applicationName();
    const QString name = app == QStringLiteral("Zenith Browser") ? QStringLiteral("default") : app.mid(QStringLiteral("Zenith Browser-").size());
    QSettings global(QStringLiteral("Zenith"), QStringLiteral("Zenith Browser"));
    global.setValue(QStringLiteral("profiles/default"), name);
    global.sync();
    if (global.status() != QSettings::NoError) QMessageBox::warning(this, tr("Perfil"), tr("Não foi possível salvar o perfil padrão."));
}

void MainWindow::setDefaultBrowser()
{
    const QString desktop = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("zenith-browser.desktop"));
    if (desktop.isEmpty()) {
        QMessageBox::information(this, tr("Navegador padrão"), tr("Instale o aplicativo e seu arquivo .desktop antes de definir o padrão."));
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        QMessageBox::information(this, tr("Navegador padrão"), code == 0 && status == QProcess::NormalExit
            ? tr("Associação atualizada.") : tr("O ambiente não conseguiu atualizar o navegador padrão."));
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            QMessageBox::warning(this, tr("Navegador padrão"), tr("xdg-settings não está disponível."));
            process->deleteLater();
        }
    });
    process->start(QStringLiteral("xdg-settings"), {QStringLiteral("set"), QStringLiteral("default-web-browser"), QStringLiteral("zenith-browser.desktop")});
}
