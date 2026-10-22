#pragma once

#include <QIcon>
#include <QUrl>
#include <QWidget>

#include <functional>
#include <memory>

class QString;

class AdBlockInterceptor;
class QResizeEvent;
class QShowEvent;
class WebProfile;

class WebView final : public QWidget
{
    Q_OBJECT

public:
    using PopupFactory = std::function<WebView *(WebView *opener, const QUrl &url)>;
    using MessageHandler = std::function<QString(const QString &message)>;

    explicit WebView(WebProfile *profile, QWidget *parent = nullptr, WebView *opener = nullptr);
    ~WebView() override;

    void setUrl(const QUrl &url);
    QString userAgent() const;
    void loadHtml(const QString &html, const QUrl &baseUrl);
    [[nodiscard]] bool engineAlive() const;
    [[nodiscard]] bool allowsCredentials() const;
    [[nodiscard]] QUrl url() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QIcon icon() const;
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;
    void back();
    void forward();
    void reload(bool bypassCache = false);
    void stop();
    void find(const QString &text, bool backwards = false);
    QString lastFindText() const;
    void finishFind();
    void applyBrowsingPreferences();
    void setZoom(double value);
    double zoom() const;
    void setMuted(bool muted);
    bool isMuted() const;
    bool isPlayingAudio() const;
    bool maySuspend() const;
    void checkCanSuspend(const std::function<void(bool)> &callback);
    void print();
    void inspect();
    void setPageFocusEnabled(bool enabled);
    void setEmbeddedVisible(bool visible);
    void parkSurface();
    void syncSurfaceSize();
    void reembed();

    using KeySink = std::function<bool(int key, Qt::KeyboardModifiers modifiers, const QString &text)>;
    static void setKeySink(KeySink sink, std::function<void()> cancelled = {});

    [[nodiscard]] QByteArray sessionState() const;
    bool restoreSessionState(const QByteArray &state);

    void setPopupFactory(PopupFactory factory);
    void installPageScript(const QString &source);
    void setMessageHandler(MessageHandler handler);

signals:
    void urlChanged(const QUrl &url);
    void titleChanged(const QString &title);
    void iconChanged(const QIcon &icon);
    void loadProgress(int progress);
    void loadFinished(bool ok);
    void closeRequested();
    void acceleratorPressed(int shortcut);
    void notificationActivated();
    void findResult(int matches);
    void processTerminated();
    void loadError(const QString &message);
    void audioChanged();
    void pageFocused();
    void newTabRequested(const QUrl &url, bool foreground);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
