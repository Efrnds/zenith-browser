#include "ui/FindBar.h"
#include "ui/ZenithIcons.h"
#include "core/WebView.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>

FindBar::FindBar(QWidget *parent) : QWidget(parent), input_(new QLineEdit(this)),
    status_(new QLabel(this)), timer_(new QTimer(this))
{
    setObjectName("findBar");
    setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(8);
    input_->setObjectName("findInput");
    input_->setPlaceholderText(tr("Buscar nesta página"));
    input_->setAccessibleName(tr("Texto da busca nesta página"));
    input_->setClearButtonEnabled(true);
    layout->addWidget(input_, 1);
    status_->setObjectName("findStatus");
    status_->setAccessibleName(tr("Resultado da busca"));
    layout->addWidget(status_);
    const auto button = [this, layout](const QString &label, ZenithIcon icon) {
        auto *result = new QPushButton(zenithIcon(icon), QString(), this);
        result->setToolTip(label);
        result->setAccessibleName(label);
        result->setFixedSize(32, 32);
        layout->addWidget(result);
        return result;
    };
    auto *previous = button(tr("Anterior (Shift+Enter)"), ZenithIcon::ChevronLeft);
    auto *next = button(tr("Próxima (Enter)"), ZenithIcon::ChevronRight);
    auto *close = button(tr("Fechar busca (Esc)"), ZenithIcon::Close);
    timer_->setSingleShot(true);
    timer_->setInterval(150);
    connect(input_, &QLineEdit::textChanged, this, [this, previous, next] {
        status_->setText(input_->text().isEmpty() ? tr("Digite para buscar") : tr("Buscando…"));
        previous->setEnabled(!input_->text().isEmpty());
        next->setEnabled(!input_->text().isEmpty());
        timer_->start();
    });
    connect(timer_, &QTimer::timeout, this, [this] { search(); });
    connect(next, &QPushButton::clicked, this, [this] { search(); });
    connect(previous, &QPushButton::clicked, this, [this] { search(true); });
    connect(input_, &QLineEdit::returnPressed, this, [this] { search(); });
    auto *backwards = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), this);
    backwards->setContext(Qt::WidgetWithChildrenShortcut);
    connect(backwards, &QShortcut::activated, this, [this] { search(true); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &FindBar::finish);
    connect(close, &QPushButton::clicked, this, &FindBar::finish);
    previous->setEnabled(false);
    next->setEnabled(false);
    hide();
}

void FindBar::open(WebView *view)
{
    if (isVisible() && view_ == view) {
        input_->setFocus(Qt::ShortcutFocusReason);
        input_->selectAll();
        return;
    }
    if (view_ != view && view_) view_->finishFind();
    disconnect(resultConnection_);
    view_ = view;
    input_->setText(view ? view->lastFindText() : QString());
    status_->setText(tr("Digite para buscar"));
    if (view) resultConnection_ = connect(view, &WebView::findResult, this, [this](int matches) {
        status_->setText(input_->text().isEmpty() ? tr("Digite para buscar")
            : matches == 0 ? tr("Nenhum resultado")
            : matches > 10000 ? tr("10.000+ resultados") : tr("%1 resultados").arg(matches));
    });
    show();
    input_->setFocus(Qt::ShortcutFocusReason);
    input_->selectAll();
    if (!input_->text().isEmpty()) search();
}

void FindBar::search(bool backwards)
{
    timer_->stop();
    status_->setText(input_->text().isEmpty() ? tr("Digite para buscar") : tr("Buscando…"));
    if (view_) view_->find(input_->text(), backwards);
}

void FindBar::finish()
{
    if (isHidden() && !view_) return;
    timer_->stop();
    disconnect(resultConnection_);
    if (view_) view_->finishFind();
    view_.clear();
    hide();
    emit closed();
}
