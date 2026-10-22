#include "ui/LibraryPage.h"
#include "ui/PageWidgets.h"
#include "ui/UiMetrics.h"
#include "ui/ZenithIcons.h"
#include "ui/ActionLayout.h"
#include <QAction>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

LibraryPage::LibraryPage(BrowserData *data, bool bookmarks, QWidget *parent)
    : QDialog(parent, Qt::Widget), data_(data), bookmarks_(bookmarks)
{
    setObjectName(bookmarks ? "bookmarksPage" : "historyPage");
    setWindowTitle(bookmarks ? tr("Favoritos") : tr("Histórico"));
    auto *layout = new QVBoxLayout(this);
    PageWidgets::heading(layout, windowTitle(), bookmarks
        ? tr("Abra e organize as páginas que você salvou.") : tr("Encontre uma página que você já visitou."));
    search_ = new QLineEdit(this);
    search_->setObjectName("librarySearch");
    search_->setPlaceholderText(tr("Pesquisar título ou endereço"));
    search_->setAccessibleName(tr("Pesquisar registros"));
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);
    auto *toolbar = new ActionLayout;
    auto *open = new QToolButton(this);
    open->setObjectName("libraryOpen");
    open->setPopupMode(QToolButton::MenuButtonPopup);
    auto *menu = new QMenu(open);
    foreground_ = menu->addAction(tr("Abrir em nova aba"));
    background_ = menu->addAction(tr("Abrir em segundo plano"));
    foreground_->setObjectName("libraryForeground");
    background_->setObjectName("libraryBackground");
    open->setDefaultAction(foreground_);
    open->setMenu(menu);
    toolbar->addWidget(open);
    edit_ = new QPushButton(tr("Editar"), this);
    edit_->setObjectName("libraryEdit");
    edit_->setVisible(bookmarks);
    toolbar->addWidget(edit_);
    remove_ = new QPushButton(bookmarks ? tr("Remover favorito") : tr("Limpar todo o histórico"), this);
    remove_->setObjectName("libraryRemove");
    toolbar->addWidget(remove_);
    layout->addLayout(toolbar);
    list_ = new QListWidget(this);
    list_->setObjectName("detailList");
    list_->setAccessibleName(windowTitle());
    list_->setItemDelegate(new DetailListDelegate(list_));
    layout->addWidget(list_, 1);
    empty_ = PageWidgets::emptyState(this, {}, {});
    layout->addWidget(empty_, 1);
    empty_->hide();
    editor_ = new QWidget(this);
    editor_->setObjectName("bookmarkEditor");
    editor_->setProperty("escapeHidesEditor", true);
    auto *form = new QFormLayout(editor_);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    title_ = new QLineEdit(editor_);
    title_->setObjectName("bookmarkTitle");
    folder_ = new QLineEdit(editor_);
    folder_->setObjectName("bookmarkFolder");
    form->addRow(tr("Título"), title_);
    form->addRow(tr("Pasta"), folder_);
    editStatus_ = new QLabel(editor_);
    editStatus_->setProperty("state", "error");
    editStatus_->setWordWrap(true);
    form->addRow(editStatus_);
    auto *editButtons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, editor_);
    form->addRow(editButtons);
    connect(editButtons, &QDialogButtonBox::accepted, this, &LibraryPage::saveEditor);
    connect(editButtons, &QDialogButtonBox::rejected, editor_, &QWidget::hide);
    connect(title_, &QLineEdit::returnPressed, this, &LibraryPage::saveEditor);
    connect(folder_, &QLineEdit::returnPressed, this, &LibraryPage::saveEditor);
    layout->addWidget(editor_);
    editor_->hide();
    status_ = new QLabel(this);
    status_->setObjectName("libraryStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Voltar"));
    buttons->button(QDialogButtonBox::Close)->setIcon(zenithIcon(ZenithIcon::Back));
    more_ = buttons->addButton(tr("Carregar mais"), QDialogButtonBox::ActionRole);
    more_->setObjectName("libraryMore");
    retry_ = buttons->addButton(tr("Tentar novamente"), QDialogButtonBox::ActionRole);
    retry_->setObjectName("libraryRetry");
    retry_->hide();
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(search_, &QLineEdit::textChanged, this, [this, timer] {
        ++generation_;
        busy_ = true;
        updateActions();
        status_->setText(tr("Pesquisando…"));
        timer->start();
    });
    connect(timer, &QTimer::timeout, this, &LibraryPage::refresh);
    connect(search_, &QLineEdit::returnPressed, this, [this, timer] { timer->stop(); refresh(); });
    connect(more_, &QPushButton::clicked, this, [this] { load(false); });
    connect(retry_, &QPushButton::clicked, this, &LibraryPage::refresh);
    connect(foreground_, &QAction::triggered, this, [this] { openSelected(true); });
    connect(background_, &QAction::triggered, this, [this] { openSelected(false); });
    connect(list_, &QListWidget::itemActivated, this, [this] { openSelected(true); });
    connect(list_, &QListWidget::currentRowChanged, this, &LibraryPage::updateActions);
    connect(edit_, &QPushButton::clicked, this, &LibraryPage::showEditor);
    connect(remove_, &QPushButton::clicked, this, &LibraryPage::removeSelected);
    busy_ = true;
    updateActions();
    QTimer::singleShot(0, this, &LibraryPage::refresh);
}

void LibraryPage::updateActions()
{
    const bool selected = !busy_ && list_->currentItem();
    foreground_->setEnabled(selected);
    background_->setEnabled(selected);
    edit_->setEnabled(selected && bookmarks_);
    remove_->setEnabled(!busy_ && (bookmarks_ ? selected : list_->count() > 0));
    more_->setEnabled(!busy_ && more_->property("hasMore").toBool());
}

void LibraryPage::refresh() { load(true); }

void LibraryPage::load(bool reset)
{
    selected_ = list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString() : QString();
    scroll_ = list_->verticalScrollBar()->value();
    target_ = reset && query_ == search_->text() ? qMax(100, list_->count()) : 100;
    query_ = search_->text();
    reset_ = reset;
    if (reset) offset_ = 0;
    pending_.clear();
    busy_ = true;
    retry_->hide();
    status_->setText(tr("Carregando registros…"));
    updateActions();
    requestChunk(++generation_, offset_);
}

void LibraryPage::requestChunk(int generation, int offset)
{
    data_->requestEntries(bookmarks_, query_, offset, this, [this, generation, offset](const BrowserData::QueryResult &result) {
        if (generation != generation_) return;
        if (!result.error.isEmpty()) {
            status_->setText(tr("Não foi possível consultar os registros. Tente novamente."));
            retry_->show();
            // Keep existing results visible, but disable actions on a stale query.
            busy_ = true;
            updateActions();
            return;
        }
        pending_.append(result.entries);
        if (result.entries.size() == 100 && pending_.size() < target_) {
            requestChunk(generation, offset + result.entries.size());
            return;
        }
        if (reset_) list_->clear();
        for (const auto &entry : std::as_const(pending_)) {
            QString context = entry.folder;
            if (!bookmarks_ && entry.visited > 0)
                context = QLocale().toString(QDateTime::fromMSecsSinceEpoch(entry.visited), QLocale::ShortFormat);
            auto *item = new QListWidgetItem((entry.title.isEmpty() ? entry.url : entry.title) + '\n'
                + entry.url + (context.isEmpty() ? QString() : " · " + context), list_);
            item->setIcon(zenithIcon(bookmarks_ ? ZenithIcon::Bookmark : ZenithIcon::History));
            item->setToolTip(PageWidgets::plainToolTip(entry.title + '\n' + entry.url
                + (context.isEmpty() ? QString() : '\n' + context)));
            item->setData(Qt::UserRole, entry.url);
            item->setData(Qt::UserRole + 1, entry.title);
            item->setData(Qt::UserRole + 2, entry.folder);
            if (entry.url == selected_) list_->setCurrentItem(item);
        }
        offset_ = offset + result.entries.size();
        pending_.clear();
        busy_ = false;
        more_->setProperty("hasMore", result.entries.size() == 100);
        const bool empty = list_->count() == 0;
        list_->setVisible(!empty);
        empty_->setVisible(empty);
        empty_->setText(!query_.trimmed().isEmpty() ? tr("Nenhum resultado\n\nTente outro título ou endereço.")
            : bookmarks_ ? tr("Nenhum favorito ainda\n\nUse Ctrl+D em um site para adicionar seu primeiro favorito.")
            : tr("Nenhuma visita registrada\n\nAs páginas visitadas fora de janelas privadas aparecem nesta lista."));
        status_->setText(empty ? tr("Nenhum registro encontrado.") : list_->count() == 1
            ? tr("1 registro") : tr("%1 registros").arg(list_->count()));
        list_->verticalScrollBar()->setValue(scroll_);
        updateActions();
    });
}

void LibraryPage::openSelected(bool foreground)
{
    if (busy_ || !list_->currentItem()) return;
    emit openRequested(QUrl(list_->currentItem()->data(Qt::UserRole).toString()), foreground);
}

void LibraryPage::showEditor()
{
    if (busy_ || !bookmarks_ || !list_->currentItem()) return;
    editor_->setProperty("bookmarkUrl", list_->currentItem()->data(Qt::UserRole));
    title_->setText(list_->currentItem()->data(Qt::UserRole + 1).toString());
    folder_->setText(list_->currentItem()->data(Qt::UserRole + 2).toString());
    editStatus_->hide();
    editor_->show();
    title_->setFocus(Qt::ShortcutFocusReason);
}

void LibraryPage::saveEditor()
{
    if (!editor_->isVisible()) return;
    if (data_->bookmark(QUrl(editor_->property("bookmarkUrl").toString()), title_->text(), folder_->text())) {
        editor_->hide();
        refresh();
    } else {
        editStatus_->setText(tr("Não foi possível salvar a edição. Seus campos foram preservados."));
        editStatus_->show();
    }
}

void LibraryPage::removeSelected()
{
    if (busy_) return;
    bool removed = false;
    if (bookmarks_) {
        if (!list_->currentItem()) return;
        removed = data_->removeBookmark(QUrl(list_->currentItem()->data(Qt::UserRole).toString()));
    } else {
        if (QMessageBox::question(this, tr("Limpar todo o histórico"),
            tr("Excluir todas as visitas deste perfil, incluindo as que não aparecem na busca atual?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        removed = data_->clearHistory();
    }
    if (removed) refresh();
    else status_->setText(tr("Não foi possível remover os registros. Tente novamente."));
}
