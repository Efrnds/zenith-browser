#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QResizeEvent;
class QSplitter;
class QStackedLayout;
class WebView;

class SplitView final : public QWidget
{
    Q_OBJECT

public:
    explicit SplitView(QWidget *parent = nullptr);
    ~SplitView() override;

    void setSingleView(WebView *view);
    void resizeEvent(QResizeEvent *event) override;
    void setSplitViews(WebView *primary, WebView *secondary);
    void showEmptyState();
    [[nodiscard]] bool isSplit() const;

private:
    void hideCurrentViews();
    void parkView(WebView *view);

    QStackedLayout *stack_;
    QLabel *emptyState_;
    QSplitter *splitter_;
    QWidget *page_ = nullptr;
    QPointer<WebView> primary_;
    QPointer<WebView> secondary_;
    bool split_ = false;
};
