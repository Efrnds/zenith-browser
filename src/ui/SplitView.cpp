#include "ui/SplitView.h"

#include <QLabel>
#include <QSplitter>
#include <QStackedLayout>
#include <QWebEngineView>

SplitView::SplitView(QWidget *parent)
    : QWidget(parent)
    , stack_(new QStackedLayout(this))
    , emptyState_(new QLabel(tr("Nenhuma aba aberta.\nCrie uma aba para começar."), this))
    , splitter_(new QSplitter(Qt::Horizontal, this))
{
    setObjectName(QStringLiteral("contentSurface"));
    stack_->setContentsMargins(0, 0, 0, 0);
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setStyleSheet(QStringLiteral("color: #a1a1aa;"));
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(3);
    stack_->addWidget(emptyState_);
    stack_->addWidget(splitter_);
    stack_->setCurrentWidget(emptyState_);
}

void SplitView::setSingleView(QWebEngineView *view)
{
    hideCurrentViews();
    primary_ = view;
    secondary_.clear();
    split_ = false;

    if (!view) {
        showEmptyState();
        return;
    }

    splitter_->addWidget(view);
    view->show();
    stack_->setCurrentWidget(splitter_);
}

void SplitView::setSplitViews(QWebEngineView *primary, QWebEngineView *secondary)
{
    if (!primary || !secondary || primary == secondary) {
        setSingleView(primary);
        return;
    }

    hideCurrentViews();
    primary_ = primary;
    secondary_ = secondary;
    split_ = true;

    splitter_->addWidget(primary);
    splitter_->addWidget(secondary);
    primary->show();
    secondary->show();
    splitter_->setSizes({1, 1});
    stack_->setCurrentWidget(splitter_);
}

void SplitView::showEmptyState()
{
    hideCurrentViews();
    primary_.clear();
    secondary_.clear();
    split_ = false;
    stack_->setCurrentWidget(emptyState_);
}

bool SplitView::isSplit() const
{
    return split_;
}

void SplitView::hideCurrentViews()
{
    if (primary_) {
        primary_->hide();
    }
    if (secondary_) {
        secondary_->hide();
    }
}
