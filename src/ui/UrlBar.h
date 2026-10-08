#pragma once

#include <QUrl>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QWidget;

class UrlBar final : public QWidget
{
    Q_OBJECT

public:
    explicit UrlBar(QWidget *parent = nullptr);

    void setUrl(const QUrl &url);
    void setNavigationState(bool canGoBack, bool canGoForward);
    void setLoading(bool loading);
    void setSplitEnabled(bool enabled);
    void setSearchTemplate(const QString &searchTemplate);
    void focusAddress();

signals:
    void navigateRequested(const QUrl &url);
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void stopRequested();
    void splitToggled(bool enabled);
    void menuRequested(const QPoint &globalPosition);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QUrl urlFromInput() const;

    QToolButton *backButton_;
    QToolButton *forwardButton_;
    QToolButton *reloadButton_;
    QToolButton *splitButton_;
    QToolButton *moreButton_;
    QWidget *addressSurface_ = nullptr;
    QLabel *securityIndicator_;
    QLineEdit *addressInput_;
    QString searchTemplate_ = QStringLiteral("https://duckduckgo.com/?q=%1");
    bool loading_ = false;
};
