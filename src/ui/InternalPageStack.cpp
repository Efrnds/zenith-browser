#include "ui/InternalPageStack.h"
#include <QApplication>
#include <QDialog>
#include <QStackedWidget>
#include <QTimer>

InternalPageStack::InternalPageStack(QStackedWidget *stack, QWidget *root)
    : QObject(stack), stack_(stack), root_(root)
{
    keep(root);
}

void InternalPageStack::keep(QWidget *page)
{
    permanent_.insert(page);
    connect(page, &QObject::destroyed, this, [this, page] { permanent_.remove(page); });
}

bool InternalPageStack::active() const { return !entries_.isEmpty(); }

void InternalPageStack::release(QWidget *page)
{
    if (!page || permanent_.contains(page)) return;
    stack_->removeWidget(page);
    page->deleteLater();
}

void InternalPageStack::open(QWidget *page)
{
    if (!page || page == root_) return;
    if (page->isWindow() || page->parentWidget() != stack_)
        page->setParent(stack_, Qt::Widget);
    for (int i = 0; i < entries_.size(); ++i) {
        QWidget *existing = entries_[i].page;
        const bool sameRoute = existing && !page->objectName().isEmpty()
            && page->objectName() == existing->objectName();
        if (existing != page && !sameRoute) continue;
        if (existing != page) release(page);
        while (entries_.size() > i + 1) release(entries_.takeLast().page);
        reveal();
        return;
    }
    QWidget *origin = QApplication::focusWidget();
    if (origin && origin->window() != stack_->window()) origin = nullptr;
    if (!entries_.isEmpty()) entries_.last().focus = origin;
    else rootFocus_ = origin;
    // Retain a bounded native history; opening tools never creates a web view.
    if (entries_.size() == 6) release(entries_.takeFirst().page);
    if (stack_->indexOf(page) < 0) stack_->addWidget(page);
    entries_.append(Entry{page, {}});
    if (auto *dialog = qobject_cast<QDialog *>(page)) {
        connect(dialog, &QDialog::finished, this, [this, dialog] {
            if (!entries_.isEmpty() && entries_.last().page == dialog) back();
        });
    }
    reveal();
}

void InternalPageStack::reveal()
{
    while (!entries_.isEmpty() && !entries_.last().page) entries_.removeLast();
    QWidget *page = active() ? entries_.last().page.data() : root_;
    stack_->setCurrentWidget(page);
    page->show();
    emit changed(active());
    const quint64 generation = ++focusGeneration_;
    const QPointer<QWidget> focus = active() ? entries_.last().focus : rootFocus_;
    const QPointer<QWidget> target(page);
    QTimer::singleShot(0, this, [this, focus, target, generation] {
        if (generation == focusGeneration_ && target && stack_->currentWidget() == target
            && focus && focus->window() == stack_->window() && focus->isVisible() && focus->isEnabled())
            focus->setFocus(Qt::BacktabFocusReason);
    });
}

void InternalPageStack::back()
{
    if (!active()) return;
    release(entries_.takeLast().page);
    reveal();
}

void InternalPageStack::clear()
{
    if (!active()) return;
    while (!entries_.isEmpty()) release(entries_.takeLast().page);
    reveal();
}
