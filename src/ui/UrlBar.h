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

    void setUrl(const QUrl &url, bool certificateException = false);
    void setNavigationState(bool canGoBack, bool canGoForward);
    void setPageAvailable(bool available);
    void setLoading(bool loading);
    void setDownloadActivity(bool active);
    void setCompactEnabled(bool enabled);
    QPoint menuPosition() const;
    void setSearchTemplate(const QString &searchTemplate);
    void setSuggestions(const QStringList &urls);
    void focusAddress();
    void clearAddressFocus();
    void injectKey(int key, Qt::KeyboardModifiers modifiers, const QString &text);
    [[nodiscard]] bool addressHasFocus() const;

signals:
    void navigateRequested(const QUrl &url);
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void stopRequested();
    void compactToggled();
    void bookmarkRequested();
    void downloadsRequested();
    void siteControlsRequested();
    void menuRequested(const QPoint &globalPosition);
    void addressFocusChanged(bool focused);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QUrl urlFromInput() const;
    void centerClearButton();

    QToolButton *backButton_;
    QToolButton *forwardButton_;
    QToolButton *reloadButton_;
    QToolButton *compactButton_;
    QToolButton *bookmarkButton_;
    QToolButton *downloadsButton_;
    QToolButton *moreButton_;
    QWidget *addressSurface_ = nullptr;
    QToolButton *securityIndicator_;
    QLineEdit *addressInput_;
    QString searchTemplate_ = QStringLiteral("https://duckduckgo.com/?q=%1");
    bool loading_ = false;
    bool pageAvailable_ = true;
    QUrl currentUrl_;
};
