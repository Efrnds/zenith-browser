#include "ui/SplitView.h"

#include "core/WebView.h"

#include <algorithm>

#include <QLabel>
#include <QResizeEvent>
#include <QSplitter>
#include <QStackedLayout>
#include <QSettings>

SplitView::~SplitView() = default;

SplitView::SplitView(QWidget *parent)
    : QWidget(parent)
    , stack_(new QStackedLayout(this))
    , emptyState_(new QLabel(tr("Nenhuma aba aberta.\nCrie uma aba para começar."), this))
    , splitter_(new QSplitter(Qt::Horizontal, this))
{
    setObjectName(QStringLiteral("contentSurface"));
    stack_->setContentsMargins(0, 0, 0, 0);
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setObjectName(QStringLiteral("settingsDescription"));
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(3);
    const QByteArray state = QSettings().value(QStringLiteral("window/splitter")).toByteArray();
    if (!state.isEmpty()) splitter_->restoreState(state);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] {
        QSettings settings;
        settings.setValue(QStringLiteral("window/splitter"), splitter_->saveState());
    });
    page_ = new QWidget;
    stack_->addWidget(emptyState_);
    stack_->addWidget(page_);
    stack_->addWidget(splitter_);
    stack_->setCurrentWidget(emptyState_);
}

void SplitView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (split_ || !primary_ || !page_)
        return;
    primary_->setGeometry(0, 0, page_->width(), page_->height());
    primary_->syncSurfaceSize();
}

void SplitView::setSingleView(WebView *view)
{
    if (primary_ && primary_ != view)
        parkView(primary_);
    if (secondary_ && secondary_ != view)
        parkView(secondary_);
    secondary_.clear();
    primary_ = view;
    split_ = false;

    if (!view) {
        showEmptyState();
        return;
    }

    const int width = std::max(page_->width(), this->width());
    const int height = std::max(page_->height(), this->height());
    view->setParent(page_);
    view->setGeometry(0, 0, width, height);
    view->setEmbeddedVisible(true);
    view->raise();
    stack_->setCurrentWidget(page_);
}

void SplitView::setSplitViews(WebView *primary, WebView *secondary)
{
    if (split_ && primary_ == primary && secondary_ == secondary) return;
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
    primary->setEmbeddedVisible(true);
    secondary->setEmbeddedVisible(true);
    const QByteArray state = QSettings().value(QStringLiteral("window/splitter")).toByteArray();
    if (state.isEmpty() || !splitter_->restoreState(state)) splitter_->setSizes({1, 1});
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

void SplitView::parkView(WebView *view)
{
    if (!view || !page_)
        return;
    view->setParent(page_);
    view->parkSurface();
    view->lower();
}

void SplitView::hideCurrentViews()
{
    parkView(primary_);
    parkView(secondary_);
}
