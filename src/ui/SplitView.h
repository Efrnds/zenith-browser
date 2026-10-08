#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QSplitter;
class QStackedLayout;
class QWebEngineView;

class SplitView final : public QWidget
{
    Q_OBJECT

public:
    explicit SplitView(QWidget *parent = nullptr);

    void setSingleView(QWebEngineView *view);
    void setSplitViews(QWebEngineView *primary, QWebEngineView *secondary);
    void showEmptyState();
    [[nodiscard]] bool isSplit() const;

private:
    void hideCurrentViews();

    QStackedLayout *stack_;
    QLabel *emptyState_;
    QSplitter *splitter_;
    QPointer<QWebEngineView> primary_;
    QPointer<QWebEngineView> secondary_;
    bool split_ = false;
};
