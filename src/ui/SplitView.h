#pragma once

#include <QPointer>
#include <QWidget>
#include <QUrl>

class QLabel;
class QResizeEvent;
class QSplitter;
class QStackedLayout;
class WebView;
class StartPage;

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
    void showStartPage(const QString &workspace, const QString &searchTemplate);
    [[nodiscard]] bool isSplit() const;

signals:
    void navigateRequested(const QUrl &url);
    void commandRequested(int command);

private:
    void hideCurrentViews();
    void parkView(WebView *view);

    QStackedLayout *stack_;
    QLabel *emptyState_;
    StartPage *startPage_;
    QSplitter *splitter_;
    QWidget *page_ = nullptr;
    QPointer<WebView> primary_;
    QPointer<WebView> secondary_;
    bool split_ = false;
};
