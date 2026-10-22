#include "ui/PageWidgets.h"
#include "ui/LibraryPage.h"
#include "ui/InternalPageStack.h"
#include <QStatusBar>
#include "ui/ZenithIcons.h"
#include "ui/UrlBar.h"
#include "ui/FindBar.h"
#include <QStackedWidget>
#include <memory>
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
#include <QScrollBar>
#include <QMenu>
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
#include <QHBoxLayout>

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
        if (internalNavigation_->active()) { closeSettings(); break; }
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
    case Navigation::Find: showFind(); break;
    case Navigation::FindNext: case Navigation::FindPrevious:
        if (!view) break;
        if (findBar_->isVisible()) { findBar_->advance(command == Navigation::FindPrevious); break; }
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
    case Navigation::Downloads: presentInternalPage(profile_->downloads()->createPage(contentStack_)); break;
    case Navigation::Fullscreen: isFullScreen() ? showNormal() : showFullScreen(); break;
    case Navigation::Print: if (view) view->print(); break;
    case Navigation::Inspector: if (view) view->inspect(); break;
    case Navigation::ToggleSplit: if (view) toggleSplit(!splitEnabled_); break;
    }
}

void MainWindow::showFind()
{
    if (settingsVisible_) {
        if (auto *search = contentStack_->currentWidget()->findChild<QLineEdit *>()) {
            search->setFocus(Qt::ShortcutFocusReason);
            search->selectAll();
        }
        return;
    }
    if (!currentView()) return;
    WebView::setKeySink({});
    ++addressFocusToken_;
    urlBar_->clearAddressFocus();
    setPageInputEnabled(false);
    findBar_->open(currentView());
}

void MainWindow::addBookmark()
{
    const auto *record = tabManager_->tab(activeTabId());
    if (!record || !Navigation::isWebUrl(record->url)) {
        statusBar()->showMessage(tr("Abra uma página HTTP ou HTTPS para salvar um favorito."), 7000);
        return;
    }
    const QUrl url = record->url;
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    QDialog &dialog = *page;
    dialog.setWindowTitle(tr("Adicionar favorito"));
    auto *layout = new QVBoxLayout(&dialog);
    PageWidgets::heading(layout, tr("Adicionar favorito"), url.toDisplayString());
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addLayout(form);
    auto *title = new QLineEdit(record->title, &dialog);
    auto *folder = new QLineEdit(&dialog);
    form->addRow(tr("Título"), title);
    form->addRow(tr("Pasta"), folder);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    auto *status = new QLabel(&dialog);
    layout->addWidget(status);
    layout->addStretch();
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, page, url, title, folder, status] {
        if (profile_->data()->bookmark(url, title->text(), folder->text())) page->accept();
        else status->setText(tr("Não foi possível salvar o favorito."));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    presentInternalPage(page);
}

void MainWindow::showLibrary(bool bookmarks)
{
    auto *page = new LibraryPage(profile_->data(), bookmarks, contentStack_);
    connect(page, &LibraryPage::openRequested, this, [this](const QUrl &url, bool foreground) {
        createTab(url, currentWorkspace_, foreground);
    });
    presentInternalPage(page);
}

void MainWindow::showSiteControls()
{
    WebView *view = currentView();
    if (!view || !Navigation::isWebUrl(view->url())) return;
    const QUrl url = view->url();
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    QDialog &dialog = *page;
    dialog.setWindowTitle(tr("Permissões: %1").arg(Navigation::origin(url)));
    auto *layout = new QVBoxLayout(&dialog);
    PageWidgets::heading(layout, tr("Permissões do site"), Navigation::origin(url));
    auto *choices = PageWidgets::section(layout, tr("Acessos bloqueados"), tr("Desmarcar volta a perguntar no próximo pedido. Acessos já ativos terminam ao recarregar ou fechar a página."));
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
        choices->addWidget(check);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    presentInternalPage(page);
}

void MainWindow::showClearData()
{
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    QDialog &dialog = *page;
    dialog.setWindowTitle(tr("Limpar dados de navegação"));
    auto *layout = new QVBoxLayout(&dialog);
    PageWidgets::heading(layout, tr("Limpar dados"), tr("Escolha o que remover. Apagar cookies encerra sessões nos sites."));
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

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    auto *status = new QLabel(&dialog);
    status->setWordWrap(true);
    layout->addWidget(status);
    layout->addStretch();
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Limpar selecionados"));
    connect(buttons, &QDialogButtonBox::accepted, page, [this, history, period, cookies, cache, permissions, certificates, buttons, status, page] {
        buttons->setEnabled(false);
        bool ok = true;
        if (history->isChecked()) {
            const qint64 seconds = period->currentData().toLongLong();
            ok &= profile_->data()->clearHistory(seconds ? QDateTime::currentMSecsSinceEpoch() - seconds * 1000 : 0);
        }
        if (permissions->isChecked()) ok &= profile_->data()->clearPermissions();
        if (certificates->isChecked()) ok &= profile_->removeCertificateExceptions();
        const QPointer<QDialog> guard(page);
        profile_->clearWebsiteData(cookies->isChecked(), cache->isChecked(), [guard, status, buttons, ok](bool webOk) {
            if (!guard) return;
            status->setText(ok && webOk ? tr("Limpeza concluída.") : tr("Não foi possível limpar todos os dados."));
            buttons->setEnabled(true);
        });
    });
    presentInternalPage(page);
}

void MainWindow::exportBookmarks()
{
    if (exportingBookmarks_) {
        statusBar()->showMessage(tr("Uma exportação já está em andamento."), 5000);
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
        else statusBar()->showMessage(tr("Favoritos exportados."), 5000);
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
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowTitle(tr("Importação de favoritos"));
    auto *layout = new QVBoxLayout(page);
    PageWidgets::heading(layout, tr("Importação concluída"),
        tr("%1 favoritos adicionados. Os favoritos existentes foram preservados.").arg(added.size()));
    auto *status = new QLabel(tr("Você pode desfazer a importação antes de sair desta tela."), page);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, page);
    auto *undo = buttons->addButton(tr("Desfazer importação"), QDialogButtonBox::ActionRole);
    undo->setEnabled(!added.isEmpty());
    connect(undo, &QPushButton::clicked, page, [this, added, undo, status] {
        bool ok = true;
        for (const auto &entry : added) ok &= profile_->data()->removeBookmark(QUrl(entry.url));
        undo->setEnabled(false);
        status->setText(ok ? tr("Importação desfeita. Favoritos anteriores foram preservados.")
                           : tr("Não foi possível desfazer todos os itens. Confira a biblioteca de favoritos."));
    });
    connect(internalNavigation_, &InternalPageStack::changed, page, [page, undo, status] {
        if (page->isVisible()) return;
        undo->setEnabled(false);
        status->setText(tr("A importação foi concluída. O desfazer foi encerrado ao sair desta tela."));
    });
    connect(buttons, &QDialogButtonBox::rejected, page, &QDialog::reject);
    layout->addStretch();
    layout->addWidget(buttons);
    presentInternalPage(page);
}

void MainWindow::manageWorkspace(int action, const QString &requestedName, int target)
{
    if (target < 0) target = currentWorkspace_;
    if (target < 0 || target >= workspaceNames_.size()) return;
    if (action == 2) {
        if (workspaceNames_.size() <= 1) return;
        auto *page = new QDialog(contentStack_, Qt::Widget);
        page->setWindowFlags(Qt::Widget);
        page->setWindowTitle(tr("Excluir espaço"));
        auto *layout = new QVBoxLayout(page);
        PageWidgets::heading(layout, tr("Excluir %1").arg(workspaceNames_.at(target)),
            tr("As abas e pastas serão preservadas. Escolha para onde transferir o conteúdo antes de excluir este espaço."));
        auto *destination = new QComboBox(page);
        destination->setObjectName("workspaceDestination");
        destination->setAccessibleName(tr("Espaço de destino"));
        destination->addItem(tr("Escolha o destino"), -1);
        for (int i = 0; i < workspaceNames_.size(); ++i)
            if (i != target) destination->addItem(workspaceNames_.at(i), i);
        bool occupied = false;
        for (int id : tabManager_->tabsInWorkspace(target, false)) {
            const auto *tab = tabManager_->tab(id);
            if (tab && (tab->url != QUrl("about:blank") || !tab->historyState.isEmpty() || tab->view)) occupied = true;
        }
        layout->addWidget(destination);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, page);
        auto *remove = buttons->addButton(tr("Excluir espaço"), QDialogButtonBox::DestructiveRole);
        remove->setEnabled(!occupied);
        connect(destination, &QComboBox::currentIndexChanged, page, [destination, remove, occupied] {
            remove->setEnabled(!occupied || destination->currentData().toInt() >= 0);
        });
        layout->addStretch(); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, page, &QDialog::reject);
        connect(remove, &QPushButton::clicked, page, [this, target, destination, page] {
            removeWorkspace(target, destination->currentData().toInt());
            page->accept();
        });
        presentInternalPage(page);
        return;
    }
    if (action == 0 && workspaceNames_.size() >= 16) return;
    if (requestedName.trimmed().isEmpty()) {
        showSettings();
        settingsPage_->beginWorkspaceEdit(action, action == 0 ? QString() : workspaceNames_.at(target), target);
        return;
    }
    const QString name = requestedName.trimmed().left(80);
    if (action == 0) workspaceNames_.append(name);
    else workspaceNames_[target] = name;
    settingsPage_->setWorkspaces(workspaceNames_, currentWorkspace_);
    tabManager_->setWorkspaceCount(workspaceNames_.size());
    sidebar_->setWorkspaces(workspaceNames_);
    sidebar_->setWorkspace(currentWorkspace_);
    syncFolders();
    if (action == 0) {
        if (settingsVisible_) leaveInternalPages();
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
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    page->setWindowTitle(tr("Perfis"));
    auto *layout = new QVBoxLayout(page);
    PageWidgets::heading(layout, tr("Abrir outro perfil"), tr("Cada perfil mantém abas, favoritos e dados separados."));
    auto *name = new QLineEdit(page);
    name->setPlaceholderText(tr("Nome do perfil"));
    name->setAccessibleName(tr("Nome do perfil"));
    name->setMaxLength(32);
    auto *status = new QLabel(page);
    status->setWordWrap(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, page);
    auto *open = buttons->addButton(tr("Abrir perfil"), QDialogButtonBox::ActionRole);
    layout->addWidget(name);
    layout->addWidget(status);
    layout->addStretch();
    layout->addWidget(buttons);
    connect(open, &QPushButton::clicked, page, [name, status] {
        if (!QRegularExpression("^[A-Za-z0-9_-]{1,32}$").match(name->text()).hasMatch()) {
            status->setText(tr("Use entre 1 e 32 letras, números, _ ou -."));
            return;
        }
        const bool opened = QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--profile", name->text()});
        status->setText(opened ? tr("Perfil aberto em outra janela do navegador.") : tr("Não foi possível abrir o perfil."));
    });
    connect(buttons, &QDialogButtonBox::rejected, page, &QDialog::reject);
    presentInternalPage(page);
    name->setFocus();
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
    else statusBar()->showMessage(tr("Perfil padrão atualizado."), 5000);
}

void MainWindow::setDefaultBrowser()
{
    const QString desktop = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("zenith-browser.desktop"));
    if (desktop.isEmpty()) {
        statusBar()->showMessage(tr("Instale o aplicativo e seu arquivo .desktop antes de definir o padrão."), 10000);
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        statusBar()->showMessage(code == 0 && status == QProcess::NormalExit
            ? tr("Associação atualizada.") : tr("O ambiente não conseguiu atualizar o navegador padrão."), 7000);
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

void MainWindow::removeWorkspace(int target, int destination)
{
    if (target < 0 || target >= workspaceNames_.size() || workspaceNames_.size() <= 1) return;
    const int movedTo = destination >= 0 ? destination : (target == 0 ? 1 : target - 1);
    if (!tabManager_->removeWorkspace(target, destination)) return;
    const auto remap = [target, movedTo](int value) {
        const int moved = value == target ? movedTo : value;
        return moved - (moved > target ? 1 : 0);
    };
    currentWorkspace_ = remap(currentWorkspace_);
    QHash<int, int> remembered;
    for (auto it = lastWorkspaceTab_.cbegin(); it != lastWorkspaceTab_.cend(); ++it)
        if (tabManager_->contains(it.value())) remembered.insert(remap(it.key()), it.value());
    lastWorkspaceTab_ = remembered;
    workspaceNames_.removeAt(target);
    settingsPage_->setWorkspaces(workspaceNames_, currentWorkspace_);
    sidebar_->setWorkspaces(workspaceNames_);
    sidebar_->setWorkspace(currentWorkspace_);
    syncFolders();
    applyViewLayout();
    scheduleSessionSave();
}

void MainWindow::editFolder(int id)
{
    QString initial;
    if (id >= 0) {
        if (!tabManager_->folderExists(id)) return;
        for (const auto &folder : tabManager_->foldersInWorkspace(currentWorkspace_))
            if (folder.id == id) initial = folder.name;
        if (initial.isEmpty()) return;
    }
    const int workspace = currentWorkspace_;
    auto *page = new QDialog(contentStack_, Qt::Widget);
    page->setWindowFlags(Qt::Widget);
    page->setWindowTitle(id < 0 ? tr("Nova pasta") : tr("Renomear pasta"));
    auto *layout = new QVBoxLayout(page);
    PageWidgets::heading(layout, page->windowTitle(), tr("Organize as abas do espaço %1.").arg(workspaceNames_.at(workspace)));
    auto *name = new QLineEdit(initial, page);
    name->setAccessibleName(tr("Nome da pasta"));
    name->setPlaceholderText(tr("Nome da pasta"));
    name->setMaxLength(128);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, page);
    auto *save = buttons->button(QDialogButtonBox::Save);
    save->setEnabled(!initial.isEmpty());
    connect(name, &QLineEdit::textChanged, page, [save](const QString &text) { save->setEnabled(!text.trimmed().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, page, [this, id, workspace, name, page] {
        if (workspace >= workspaceNames_.size()) return;
        if (id < 0) tabManager_->createFolder(workspace, name->text());
        else if (tabManager_->folderExists(id)) tabManager_->renameFolder(id, name->text());
        page->accept();
    });
    connect(name, &QLineEdit::returnPressed, save, &QPushButton::click);
    connect(buttons, &QDialogButtonBox::rejected, page, &QDialog::reject);
    layout->addWidget(name); layout->addStretch(); layout->addWidget(buttons);
    presentInternalPage(page);
    name->setFocus(); name->selectAll();
}
