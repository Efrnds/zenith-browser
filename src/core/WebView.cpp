#include <QSettings>
#include "core/WebView.h"
#include "ui/ThemeManager.h"

#include "core/AdBlockInterceptor.h"
#include "core/WebProfile.h"
#include "core/BrowserData.h"
#include "core/Navigation.h"

#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QWindow>
#include <QDesktopServices>
#include <algorithm>

#undef signals
#undef slots
#include <gtk/gtkx.h>
#include <webkit2/webkit2.h>
#include <gio/gio.h>

#include <cairo.h>

class DesktopNotifications final : public QObject
{
    Q_OBJECT

public:
    static DesktopNotifications &instance()
    {
        static DesktopNotifications object;
        return object;
    }

    void show(WebView *view, WebKitNotification *notification)
    {
        if (!view || !notification)
            return;
        if (pending_ >= 16 || notes_.size() >= 64) {
            webkit_notification_close(notification);
            return;
        }
        ++pending_;
        g_object_ref(notification);
        Note note{view, std::shared_ptr<WebKitNotification>(notification, [](auto *object) { g_object_unref(object); }),
            Navigation::origin(view->url()), ++generation_};
        const QString title = QString::fromUtf8(webkit_notification_get_title(notification)).left(256);
        const QString body = QString::fromUtf8(webkit_notification_get_body(notification)).left(4096).toHtmlEscaped();
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
        message.setArguments({QStringLiteral("Zenith · %1").arg(view->url().host()), uint(0),
            QStringLiteral("zenith-browser"), title.isEmpty() ? QStringLiteral("Zenith") : title,
            body, QStringList{QStringLiteral("default"), QObject::tr("Abrir")}, QVariantMap{}, 8000});
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 3000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, note] {
            --pending_;
            const QDBusPendingReply<uint> reply = *watcher;
            watcher->deleteLater();
            if (reply.isError()) {
                webkit_notification_close(note.notification.get());
                return;
            }
            const uint id = reply.value();
            release(id);
            if (notes_.size() >= 64) {
                closeDesktop(id);
                webkit_notification_close(note.notification.get());
                return;
            }
            notes_.insert(id, note);
            if (!note.view || Navigation::origin(note.view->url()) != note.origin) {
                closeDesktop(id);
                release(id);
                return;
            }
            QTimer::singleShot(30'000, this, [this, id, generation = note.generation] {
                if (notes_.value(id).generation != generation) return;
                closeDesktop(id);
                release(id);
            });
        });
    }

private:
    struct Note {
        QPointer<WebView> view;
        std::shared_ptr<WebKitNotification> notification;
        QString origin;
        quint64 generation = 0;
    };

    DesktopNotifications()
    {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("ActionInvoked"),
            this, SLOT(onAction(uint,QString)));
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("NotificationClosed"),
            this, SLOT(onClosed(uint,uint)));
    }

    void release(const uint id)
    {
        const auto note = notes_.take(id);
        if (note.notification)
            webkit_notification_close(note.notification.get());
    }

    void closeDesktop(uint id)
    {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("CloseNotification"));
        message.setArguments({id});
        QDBusConnection::sessionBus().asyncCall(message, 3000);
    }

    QHash<uint, Note> notes_;
    int pending_ = 0;
    quint64 generation_ = 0;

private Q_SLOTS:
    void onAction(const uint id, const QString &)
    {
        const auto note = notes_.value(id);
        if (!note.notification)
            return;
        if (note.view && Navigation::origin(note.view->url()) == note.origin) {
            webkit_notification_clicked(note.notification.get());
            emit note.view->notificationActivated();
        }
        closeDesktop(id);
        release(id);
    }

    void onClosed(const uint id, const uint)
    {
        release(id);
    }
};

namespace {
WebView::KeySink keySink;
std::function<void()> keySinkCancelled;
quint64 keySinkRevision = 0;

int qtKeyFromGdk(const guint keyval, QString *text)
{
    switch (keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        return Qt::Key_Return;
    case GDK_KEY_BackSpace:
        return Qt::Key_Backspace;
    case GDK_KEY_Delete:
    case GDK_KEY_KP_Delete:
        return Qt::Key_Delete;
    case GDK_KEY_Left:
        return Qt::Key_Left;
    case GDK_KEY_Right:
        return Qt::Key_Right;
    case GDK_KEY_Home:
        return Qt::Key_Home;
    case GDK_KEY_End:
        return Qt::Key_End;
    case GDK_KEY_Escape:
        return Qt::Key_Escape;
    case GDK_KEY_Tab:
        return Qt::Key_Tab;
    case GDK_KEY_ISO_Left_Tab:
        return Qt::Key_Backtab;
    case GDK_KEY_F5: return Qt::Key_F5;
    case GDK_KEY_F3: return Qt::Key_F3;
    case GDK_KEY_F10: return Qt::Key_F10;
    case GDK_KEY_F11: return Qt::Key_F11;
    case GDK_KEY_F12: return Qt::Key_F12;
    default:
        break;
    }
    const guint unicode = gdk_keyval_to_unicode(keyval);
    if (unicode == 0)
        return 0;
    if (text)
        *text = QString(QChar(unicode));
    if (unicode >= 'a' && unicode <= 'z')
        return Qt::Key_A + static_cast<int>(unicode - 'a');
    if (unicode >= 'A' && unicode <= 'Z')
        return Qt::Key_A + static_cast<int>(unicode - 'A');
    if (unicode >= '0' && unicode <= '9')
        return Qt::Key_0 + static_cast<int>(unicode - '0');
    return static_cast<int>(unicode);
}
}

namespace {
QIcon iconFromSurface(cairo_surface_t *surface)
{
    if (!surface)
        return {};
    cairo_surface_flush(surface);

    int width = 0;
    int height = 0;
    if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE) {
        width = cairo_image_surface_get_width(surface);
        height = cairo_image_surface_get_height(surface);
    }
    if (width <= 0 || height <= 0) {
        width = 32;
        height = 32;
    }

    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *context = cairo_create(image);
    cairo_set_source_surface(context, surface, 0, 0);
    cairo_paint(context);
    cairo_destroy(context);
    cairo_surface_flush(image);

    const int stride = cairo_image_surface_get_stride(image);
    auto *source = cairo_image_surface_get_data(image);
    if (!source || stride <= 0) {
        cairo_surface_destroy(image);
        return {};
    }

    QImage copy(width, height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < height; ++y)
        memcpy(copy.scanLine(y), source + static_cast<size_t>(y) * static_cast<size_t>(stride),
               static_cast<size_t>(width) * 4);
    cairo_surface_destroy(image);
    return QIcon(QPixmap::fromImage(copy));
}

QString scriptMessageText(JSCValue *value)
{
    if (!value)
        return {};
    char *text = jsc_value_to_string(value);
    const QString message = QString::fromUtf8(text);
    g_free(text);
    return message;
}
}

struct WebView::Impl {
    WebView *owner = nullptr;
    WebProfile *profile = nullptr;
    AdBlockInterceptor *blocker = nullptr;
    GtkWidget *plug = nullptr;
    WebKitWebView *webView = nullptr;
    QWidget *container = nullptr;
    QIcon icon;
    PopupFactory popupFactory;
    MessageHandler messageHandler;
    bool wasFullScreen = false;
    bool sawLoadFailure = false;
    bool parked = false;
    bool crashed = false;
    bool permissionPending = false;
    QString userAgentOverride;
    QString lastFindText;
    bool findActive = false;
    std::function<void()> pendingLoad;

    void applyUserAgent(const QUrl &url)
    {
        const QString agent = Navigation::outlookUserAgent(url);
        if (!webView || agent == userAgentOverride) return;
        userAgentOverride = agent;
        const QByteArray utf8 = agent.toUtf8();
        webkit_settings_set_user_agent(webkit_web_view_get_settings(webView), agent.isEmpty() ? nullptr : utf8.constData());
    }

    ~Impl();

    static void onUriChanged(WebKitWebView *, GParamSpec *, gpointer user);
    static void onTitleChanged(WebKitWebView *, GParamSpec *, gpointer user);
    static void onProgressChanged(WebKitWebView *, GParamSpec *, gpointer user);
    static void publishFavicon(WebView *view);
    static void onFaviconChanged(WebKitWebView *, GParamSpec *, gpointer user);
    static void onLoadChanged(WebKitWebView *, WebKitLoadEvent event, gpointer user);
    static gboolean onLoadFailed(WebKitWebView *, WebKitLoadEvent, const gchar *, GError *, gpointer user);
    static gboolean onTlsErrors(WebKitWebView *, const gchar *failingUri, GTlsCertificate *certificate,
                                GTlsCertificateFlags errors, gpointer user);
    static gboolean onPermission(WebKitWebView *, WebKitPermissionRequest *request, gpointer user);
    static gboolean onPermissionState(WebKitWebView *, WebKitPermissionStateQuery *query, gpointer user);
    static gboolean onNotification(WebKitWebView *, WebKitNotification *notification, gpointer user);
    static GtkWidget *onCreate(WebKitWebView *, WebKitNavigationAction *action, gpointer user);
    static gboolean onDecidePolicy(WebKitWebView *, WebKitPolicyDecision *decision,
                                   WebKitPolicyDecisionType type, gpointer user);
    static gboolean onScriptDialog(WebKitWebView *, WebKitScriptDialog *dialog, gpointer user);
    static gboolean onFileChooser(WebKitWebView *, WebKitFileChooserRequest *request, gpointer user);
    static gboolean onEnterFullscreen(WebKitWebView *, gpointer user);
    static gboolean onLeaveFullscreen(WebKitWebView *, gpointer user);
    static void onClose(WebKitWebView *, gpointer user);
    static gboolean onScriptMessage(WebKitUserContentManager *, JSCValue *value,
                                    WebKitScriptMessageReply *reply, gpointer user);
    static gboolean onKeyPress(GtkWidget *, GdkEventKey *event, gpointer user);
};

WebView::Impl::~Impl()
{
    messageHandler = nullptr;
    popupFactory = nullptr;
    if (plug && owner)
        g_signal_handlers_disconnect_by_data(plug, owner);
    if (webView && owner) {
        g_signal_handlers_disconnect_by_data(webkit_web_view_get_find_controller(webView), owner);
        g_signal_handlers_disconnect_by_data(webView, owner);
        if (WebKitUserContentManager *content = webkit_web_view_get_user_content_manager(webView))
            g_signal_handlers_disconnect_by_data(content, owner);
    }
    if (container) {
        container->hide();
        container->setParent(nullptr);
        delete container;
        container = nullptr;
    }
    if (plug) {
        gtk_widget_destroy(plug);
        plug = nullptr;
        webView = nullptr;
    }
}

void WebView::Impl::onUriChanged(WebKitWebView *, GParamSpec *, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    view->impl_->applyUserAgent(view->url());
    emit view->urlChanged(view->url());
}

void WebView::Impl::onTitleChanged(WebKitWebView *, GParamSpec *, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    emit view->titleChanged(view->title());
}

void WebView::Impl::onProgressChanged(WebKitWebView *, GParamSpec *, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (!view->impl_->webView)
        return;
    const int progress = qRound(webkit_web_view_get_estimated_load_progress(view->impl_->webView) * 100.0);
    emit view->loadProgress(progress);
}

void WebView::Impl::publishFavicon(WebView *view)
{
    if (!view || !view->impl_ || !view->impl_->webView)
        return;
    const QIcon icon = iconFromSurface(webkit_web_view_get_favicon(view->impl_->webView));
    if (icon.isNull())
        return;
    view->impl_->icon = icon;
    emit view->iconChanged(icon);
}

void WebView::Impl::onFaviconChanged(WebKitWebView *, GParamSpec *, gpointer user)
{
    publishFavicon(static_cast<WebView *>(user));
}

void WebView::Impl::onLoadChanged(WebKitWebView *, const WebKitLoadEvent event, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (event == WEBKIT_LOAD_STARTED) {
        view->impl_->sawLoadFailure = false;
        view->impl_->findActive = false;
        emit view->loadProgress(0);
        return;
    }
    if (event != WEBKIT_LOAD_FINISHED)
        return;
    Impl::publishFavicon(view);
    emit view->loadProgress(100);
    if (view->impl_->sawLoadFailure) {
        view->impl_->sawLoadFailure = false;
        return;
    }
    emit view->loadFinished(true);
}

gboolean WebView::Impl::onLoadFailed(WebKitWebView *, WebKitLoadEvent, const gchar *, GError *error, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    view->impl_->sawLoadFailure = true;
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)) {
        emit view->loadProgress(100);
        return FALSE;
    }
    emit view->loadFinished(false);
    emit view->loadError(QString::fromUtf8(error->message));
    return FALSE;
}

gboolean WebView::Impl::onTlsErrors(WebKitWebView *, const gchar *failingUri, GTlsCertificate *certificate,
                                    const GTlsCertificateFlags errors, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (!view || !view->impl_ || !view->impl_->profile || !certificate || !failingUri)
        return FALSE;

    const QUrl url(QString::fromUtf8(failingUri));
    const QString host = url.host();
    if (host.isEmpty())
        return FALSE;

    gchar *pem = nullptr;
    g_object_get(certificate, "certificate-pem", &pem, nullptr);
    const QByteArray encoded = pem ? QByteArray(pem) : QByteArray();
    g_free(pem);
    if (encoded.isEmpty())
        return FALSE;

    QStringList reasons;
    if (errors & G_TLS_CERTIFICATE_UNKNOWN_CA)
        reasons.append(QObject::tr("Quem assinou o certificado não está no sistema."));
    if (errors & G_TLS_CERTIFICATE_BAD_IDENTITY)
        reasons.append(QObject::tr("O certificado não corresponde a este endereço."));
    if (errors & G_TLS_CERTIFICATE_EXPIRED)
        reasons.append(QObject::tr("O certificado está vencido."));
    if (errors & G_TLS_CERTIFICATE_NOT_ACTIVATED)
        reasons.append(QObject::tr("O certificado ainda não vale."));
    if (errors & G_TLS_CERTIFICATE_REVOKED)
        reasons.append(QObject::tr("O certificado foi revogado."));
    if (errors & G_TLS_CERTIFICATE_INSECURE)
        reasons.append(QObject::tr("O certificado usa criptografia fraca."));
    if (reasons.isEmpty())
        reasons.append(QObject::tr("O certificado foi recusado."));

    QMessageBox box(view);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QObject::tr("Certificado não confiável"));
    box.setText(QObject::tr("A conexão com %1 não usa um certificado reconhecido pelo sistema.").arg(host));
    box.setInformativeText(reasons.join(QStringLiteral("\n")));
    auto *proceed = box.addButton(QObject::tr("Continuar mesmo assim"), QMessageBox::AcceptRole);
    auto *back = box.addButton(QObject::tr("Voltar"), QMessageBox::RejectRole);
    box.setDefaultButton(back);
    box.exec();
    if (box.clickedButton() != proceed)
        return TRUE;

    view->impl_->profile->trustCertificate(encoded, host, false);
    webkit_web_view_load_uri(view->impl_->webView, failingUri);
    return TRUE;
}

gboolean WebView::Impl::onPermission(WebKitWebView *, WebKitPermissionRequest *request, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    QString kind;
    QString label;
    if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
        kind = QStringLiteral("notifications");
        label = QObject::tr("enviar notificações");
    } else if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
        kind = QStringLiteral("geolocation");
        label = QObject::tr("acessar sua localização");
    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        auto *media = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);
        if (webkit_user_media_permission_is_for_display_device(media)) {
            kind = QStringLiteral("display"); label = QObject::tr("compartilhar sua tela");
        } else {
            const bool audio = webkit_user_media_permission_is_for_audio_device(media);
            const bool video = webkit_user_media_permission_is_for_video_device(media);
            kind = audio && video ? QStringLiteral("camera-microphone") : audio ? QStringLiteral("microphone") : QStringLiteral("camera");
            label = audio && video ? QObject::tr("usar câmera e microfone") : audio ? QObject::tr("usar o microfone") : QObject::tr("usar a câmera");
        }
    }
    const QUrl page = view->url();
    auto *data = view->impl_->profile->data();
    bool blocked = data->permission(page, kind) == -1;
    if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        auto *media = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);
        const bool camera = webkit_user_media_permission_is_for_video_device(media);
        const bool microphone = webkit_user_media_permission_is_for_audio_device(media);
        blocked |= camera && data->permission(page, "camera") == -1;
        blocked |= microphone && data->permission(page, "microphone") == -1;
        blocked |= (camera || microphone) && data->permission(page, "camera-microphone") == -1;
    }
    if (kind.isEmpty() || page.scheme() != QStringLiteral("https") || view->impl_->permissionPending
        || blocked) {
        webkit_permission_request_deny(request);
        return TRUE;
    }
    // WebKitGTK 4.1 does not expose a requesting frame origin on these requests.
    // Grants are single-use and the prompt names embedded content explicitly.
    g_object_ref(request);
    view->impl_->permissionPending = true;
    QMessageBox box(QMessageBox::Question, QObject::tr("Permissão do site"),
        QObject::tr("Permitir que a página %1 e seu conteúdo incorporado possam %2?")
            .arg(Navigation::origin(page), label), QMessageBox::NoButton, view);
    auto *allow = box.addButton(QObject::tr("Permitir uma vez"), QMessageBox::AcceptRole);
    auto *deny = box.addButton(QObject::tr("Bloquear neste site"), QMessageBox::DestructiveRole);
    auto *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.exec();
    view->impl_->permissionPending = false;
    if (box.clickedButton() == deny)
        data->setPermission(page, kind, -1);
    if (box.clickedButton() == allow && Navigation::origin(view->url()) == Navigation::origin(page))
        webkit_permission_request_allow(request);
    else
        webkit_permission_request_deny(request);
    g_object_unref(request);
    return TRUE;
}

gboolean WebView::Impl::onPermissionState(WebKitWebView *, WebKitPermissionStateQuery *query, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    const QString name = QString::fromUtf8(webkit_permission_state_query_get_name(query));
    gchar *origin = webkit_security_origin_to_string(webkit_permission_state_query_get_security_origin(query));
    const QUrl url(QString::fromUtf8(origin));
    g_free(origin);
    const bool known = name == "notifications" || name == "geolocation" || name == "camera" || name == "microphone";
    int decision = view->impl_->profile->data()->permission(url, name);
    if ((name == "camera" || name == "microphone") && view->impl_->profile->data()->permission(url, "camera-microphone") == -1)
        decision = -1;
    webkit_permission_state_query_finish(query,
        !known || decision == -1 || url.scheme() != QStringLiteral("https")
            ? WEBKIT_PERMISSION_STATE_DENIED : WEBKIT_PERMISSION_STATE_PROMPT);
    return TRUE;
}

gboolean WebView::Impl::onNotification(WebKitWebView *, WebKitNotification *notification, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    DesktopNotifications::instance().show(view, notification);
    return TRUE;
}

GtkWidget *WebView::Impl::onCreate(WebKitWebView *, WebKitNavigationAction *action, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (!view->impl_->popupFactory)
        return nullptr;

    const char *uri = nullptr;
    if (WebKitURIRequest *request = webkit_navigation_action_get_request(action))
        uri = webkit_uri_request_get_uri(request);
    const QUrl url = uri ? QUrl(QString::fromUtf8(uri)) : QUrl();
    WebView *created = view->impl_->popupFactory(view, url);
    if (!created || !created->impl_ || !created->impl_->webView)
        return nullptr;
    return GTK_WIDGET(created->impl_->webView);
}

gboolean WebView::Impl::onDecidePolicy(WebKitWebView *, WebKitPolicyDecision *decision,
                                       const WebKitPolicyDecisionType type, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION || type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        auto *navigation = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(navigation);
        if (!action) return FALSE;
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        if (!request) return FALSE;
        const QUrl url(QString::fromUtf8(webkit_uri_request_get_uri(request)));
        const bool gesture = webkit_navigation_action_is_user_gesture(action);
        const guint button = webkit_navigation_action_get_mouse_button(action);
        const guint modifiers = webkit_navigation_action_get_modifiers(action);
        const bool newTabClick = button == 2 || (button == 1 && (modifiers & GDK_CONTROL_MASK));
        if (gesture && newTabClick && (Navigation::isWebUrl(url)
                || (url.isLocalFile() && !Navigation::isWebUrl(view->url())))) {
            webkit_policy_decision_ignore(decision);
            const QPointer<WebView> safe(view);
            const bool foreground = (modifiers & GDK_SHIFT_MASK) != 0;
            QMetaObject::invokeMethod(view, [safe, url, foreground] {
                if (safe) Q_EMIT safe->newTabRequested(url, foreground);
            }, Qt::QueuedConnection);
            return TRUE;
        }
        if (!Navigation::outlookUserAgent(url).isEmpty()) view->impl_->applyUserAgent(url);
        if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION && !gesture) {
            webkit_policy_decision_ignore(decision);
            return TRUE;
        }
        if (Navigation::isWebUrl(url) || url == QUrl("about:blank")
            || (url.isLocalFile() && !Navigation::isWebUrl(view->url()))
            || url.scheme() == "blob") return FALSE;
        webkit_policy_decision_ignore(decision);
        if (gesture && (url.scheme() == "mailto" || url.scheme() == "tel")) {
            if (QMessageBox::question(view, QObject::tr("Abrir aplicativo externo"),
                QObject::tr("Abrir um aplicativo para o endereço %1?").arg(url.toDisplayString(QUrl::RemoveUserInfo)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
                QDesktopServices::openUrl(url);
        }
        return TRUE;
    }
    if (type != WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
        return FALSE;

    auto *response = WEBKIT_RESPONSE_POLICY_DECISION(decision);
    WebKitURIRequest *request = webkit_response_policy_decision_get_request(response);
    const char *uri = request ? webkit_uri_request_get_uri(request) : nullptr;
    if (!uri)
        return FALSE;

    const bool mainFrame = webkit_response_policy_decision_is_main_frame_main_resource(response);
    if (view->impl_->blocker && view->impl_->blocker->reject(QUrl(QString::fromUtf8(uri)), mainFrame)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }
    if (mainFrame && !webkit_response_policy_decision_is_mime_type_supported(response)) {
        webkit_policy_decision_download(decision);
        return TRUE;
    }
    return FALSE;
}

gboolean WebView::Impl::onScriptDialog(WebKitWebView *, WebKitScriptDialog *dialog, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    webkit_script_dialog_ref(dialog);
    const QString message = QString::fromUtf8(webkit_script_dialog_get_message(dialog));
    switch (webkit_script_dialog_get_dialog_type(dialog)) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        QMessageBox::information(view, QObject::tr("Página"), message);
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM: {
        const auto answer = QMessageBox::question(view, QObject::tr("Página"), message);
        webkit_script_dialog_confirm_set_confirmed(dialog, answer == QMessageBox::Yes);
        break;
    }
    case WEBKIT_SCRIPT_DIALOG_PROMPT: {
        bool accepted = false;
        const QString text = QInputDialog::getText(
            view, QObject::tr("Página"), message, QLineEdit::Normal,
            QString::fromUtf8(webkit_script_dialog_prompt_get_default_text(dialog)), &accepted);
        if (accepted) {
            const QByteArray encoded = text.toUtf8();
            webkit_script_dialog_prompt_set_text(dialog, encoded.constData());
        } else {
            webkit_script_dialog_close(dialog);
        }
        break;
    }
    }
    webkit_script_dialog_unref(dialog);
    return TRUE;
}

gboolean WebView::Impl::onFileChooser(WebKitWebView *, WebKitFileChooserRequest *request, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    g_object_ref(request);
    QFileDialog dialog(view);
    dialog.setFileMode(webkit_file_chooser_request_get_select_multiple(request)
                           ? QFileDialog::ExistingFiles
                           : QFileDialog::ExistingFile);
    if (dialog.exec() != QDialog::Accepted) {
        webkit_file_chooser_request_cancel(request);
        g_object_unref(request);
        return TRUE;
    }

    QList<QByteArray> encoded;
    encoded.reserve(dialog.selectedFiles().size());
    for (const QString &file : dialog.selectedFiles())
        encoded.append(QFile::encodeName(file));

    QList<const char *> pointers;
    pointers.reserve(encoded.size() + 1);
    for (const QByteArray &file : encoded)
        pointers.append(file.constData());
    pointers.append(nullptr);
    webkit_file_chooser_request_select_files(request, pointers.constData());
    g_object_unref(request);
    return TRUE;
}

gboolean WebView::Impl::onEnterFullscreen(WebKitWebView *, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (QWidget *window = view->window()) {
        view->impl_->wasFullScreen = window->isFullScreen();
        window->showFullScreen();
    }
    return TRUE;
}

gboolean WebView::Impl::onLeaveFullscreen(WebKitWebView *, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    if (QWidget *window = view->window(); window && !view->impl_->wasFullScreen)
        window->showNormal();
    return TRUE;
}

void WebView::Impl::onClose(WebKitWebView *, gpointer user)
{
    emit static_cast<WebView *>(user)->closeRequested();
}

gboolean WebView::Impl::onScriptMessage(WebKitUserContentManager *, JSCValue *value,
                                        WebKitScriptMessageReply *reply, gpointer user)
{
    auto *view = static_cast<WebView *>(user);
    const QString result = view->impl_->messageHandler
        ? view->impl_->messageHandler(scriptMessageText(value))
        : QString();
    JSCContext *context = jsc_value_get_context(value);
    const QByteArray utf8 = result.toUtf8();
    JSCValue *replyValue = result.isEmpty()
        ? jsc_value_new_null(context)
        : jsc_value_new_string(context, utf8.constData());
    webkit_script_message_reply_return_value(reply, replyValue);
    g_object_unref(replyValue);
    return TRUE;
}

gboolean WebView::Impl::onKeyPress(GtkWidget *, GdkEventKey *event, gpointer user)
{
    if (!event || !user)
        return FALSE;
    const guint mods = event->state & gtk_accelerator_get_default_mod_mask();
    const bool ctrl = (mods & GDK_CONTROL_MASK) != 0;
    const bool shift = (mods & GDK_SHIFT_MASK) != 0;
    Qt::KeyboardModifiers commandMods;
    if (ctrl) commandMods |= Qt::ControlModifier;
    if (shift) commandMods |= Qt::ShiftModifier;
    if (mods & GDK_MOD1_MASK) commandMods |= Qt::AltModifier;
    if (mods & GDK_META_MASK) commandMods |= Qt::MetaModifier;
    const int shortcut = Navigation::command(qtKeyFromGdk(event->keyval, nullptr), commandMods);
    if (shortcut != 0) {
        auto *view = static_cast<WebView *>(user);
        const QPointer<WebView> safe(view);
        QMetaObject::invokeMethod(view, [safe, shortcut] {
            if (safe)
                safe->acceleratorPressed(shortcut);
        }, Qt::QueuedConnection);
        return TRUE;
    }

    auto *view = static_cast<WebView *>(user);
    QString text;
    const int key = qtKeyFromGdk(event->keyval, &text);
    if (key == 0)
        return FALSE;
    Qt::KeyboardModifiers modifiers;
    if (ctrl)
        modifiers |= Qt::ControlModifier;
    if (shift)
        modifiers |= Qt::ShiftModifier;
    return WebView::queueKeyToSink(view, key, modifiers, text) ? TRUE : FALSE;
}

void WebView::setKeySink(KeySink sink, std::function<void()> cancelled)
{
    ++keySinkRevision;
    keySink = std::move(sink);
    keySinkCancelled = std::move(cancelled);
}

bool WebView::queueKeyToSink(QObject *context, const int key,
                             const Qt::KeyboardModifiers modifiers, const QString &text)
{
    if (!context || !keySink)
        return false;
    const quint64 revision = keySinkRevision;
    QPointer<QObject> safeContext(context);
    QMetaObject::invokeMethod(context, [safeContext, revision, key, modifiers, text] {
        if (!safeContext || revision != keySinkRevision || !keySink)
            return;
        const auto sink = keySink;
        sink(key, modifiers, text);
    }, Qt::QueuedConnection);
    return true;
}

WebView::WebView(WebProfile *profile, QWidget *parent, WebView *opener)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    Q_ASSERT(profile);
    impl_->owner = this;
    impl_->profile = profile;
    impl_->blocker = profile->requestBlocker();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setFocusPolicy(Qt::StrongFocus);

    gtk_init_check(nullptr, nullptr);
    impl_->plug = gtk_plug_new(0);
    gtk_window_set_decorated(GTK_WINDOW(impl_->plug), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(impl_->plug), 0);
    gtk_widget_set_name(impl_->plug, "zenithWebHost");
    auto *hostStyle = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(impl_->plug), GTK_STYLE_PROVIDER(hostStyle), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_set_data_full(G_OBJECT(impl_->plug), "zenith-host-style", hostStyle, g_object_unref);
    const auto styleHost = [this, hostStyle] {
        const QByteArray css = "#zenithWebHost { background-color: "
            + ThemeManager::instance().color("#121214").name().toUtf8() + "; border: 0; padding: 0; }";
        gtk_css_provider_load_from_data(hostStyle, css.constData(), css.size(), nullptr);
    };
    styleHost();
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, styleHost);
    WebKitUserContentManager *content = webkit_user_content_manager_new();
    if (impl_->blocker && impl_->blocker->isEnabled()) {
        const char *const sites[] = {
            "*://*.youtube.com/*",
            "*://youtube.com/*",
            "*://youtu.be/*",
            nullptr,
        };
        const char *style =
            "ytd-ad-slot-renderer,ytd-display-ad-renderer,ytd-banner-promo-renderer,"
            "ytd-statement-banner-renderer,ytd-in-feed-ad-layout-renderer,"
            "ytd-engagement-panel-section-list-renderer[target-id=\"engagement-panel-ads\"],"
            "#player-ads,#masthead-ad,.ytp-ad-module,.ytp-ad-overlay-slot,.video-ads"
            "{display:none !important;}";
        WebKitUserStyleSheet *sheet = webkit_user_style_sheet_new(
            style, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER,
            sites, nullptr);
        webkit_user_content_manager_add_style_sheet(content, sheet);
        webkit_user_style_sheet_unref(sheet);

        const char *script =
            "setInterval(function(){if(document.hidden)return;"
            "var video=document.querySelector('video.html5-main-video,video');"
            "var ad=document.querySelector('.ad-showing,.ytp-ad-player-overlay,.ytp-ad-text');"
            "if(ad&&video&&isFinite(video.duration)&&video.duration>0)"
            "video.currentTime=video.duration;"
            "document.querySelectorAll('.ytp-ad-skip-button,.ytp-ad-skip-button-modern,"
            ".ytp-skip-ad-button').forEach(function(button){button.click();});"
            "},1000);";
        WebKitUserScript *userScript = webkit_user_script_new(
            script, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, sites, nullptr);
        webkit_user_content_manager_add_script(content, userScript);
        webkit_user_script_unref(userScript);
    }
    WebKitWebsitePolicies *policies = webkit_website_policies_new_with_policies(
        "autoplay", WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND, nullptr);
    WebKitSettings *pageSettings = profile->createSettings();
    if (opener && opener->impl_ && opener->impl_->webView) {
        impl_->webView = WEBKIT_WEB_VIEW(g_object_new(
            WEBKIT_TYPE_WEB_VIEW,
            "related-view", opener->impl_->webView,
            "settings", pageSettings,
            "website-policies", policies,
            "user-content-manager", content,
            nullptr));
    } else {
        impl_->webView = WEBKIT_WEB_VIEW(g_object_new(
            WEBKIT_TYPE_WEB_VIEW,
            "web-context", profile->context(),
            "settings", pageSettings,
            "website-policies", policies,
            "user-content-manager", content,
            nullptr));
    }
    g_object_unref(policies);
    g_object_unref(pageSettings);
    g_object_unref(content);

    const auto applyContentFilter = [this] {
        auto *manager = webkit_web_view_get_user_content_manager(impl_->webView);
        webkit_user_content_manager_remove_filter_by_id(manager, "zenith-adblock");
        if (auto *filter = impl_->profile->contentFilter())
            webkit_user_content_manager_add_filter(manager, filter);
        if (!impl_->profile->contentFilterPending() && impl_->pendingLoad) {
            auto load = std::move(impl_->pendingLoad);
            impl_->pendingLoad = nullptr;
            load();
        }
    };
    connect(profile, &WebProfile::contentFilterChanged, this, applyContentFilter);
    applyContentFilter();

    const GdkRGBA background {1.0, 1.0, 1.0, 1.0};
    webkit_web_view_set_background_color(impl_->webView, &background);
    gtk_widget_set_size_request(impl_->plug, -1, -1);
    gtk_widget_set_size_request(GTK_WIDGET(impl_->webView), -1, -1);
    gtk_container_add(GTK_CONTAINER(impl_->plug), GTK_WIDGET(impl_->webView));
    gtk_widget_show_all(impl_->plug);
    gtk_widget_realize(impl_->plug);

    g_signal_connect(impl_->webView, "notify::uri", G_CALLBACK(Impl::onUriChanged), this);
    g_signal_connect(impl_->webView, "notify::title", G_CALLBACK(Impl::onTitleChanged), this);
    g_signal_connect(impl_->webView, "notify::estimated-load-progress",
                     G_CALLBACK(Impl::onProgressChanged), this);
    g_signal_connect(impl_->webView, "notify::favicon", G_CALLBACK(Impl::onFaviconChanged), this);
    g_signal_connect(impl_->webView, "load-changed", G_CALLBACK(Impl::onLoadChanged), this);
    g_signal_connect(impl_->webView, "web-process-terminated", G_CALLBACK(+[](WebKitWebView *, WebKitWebProcessTerminationReason, gpointer user) {
        auto *view = static_cast<WebView *>(user);
        view->impl_->crashed = true;
        emit view->loadFinished(false);
        emit view->processTerminated();
    }), this);
    g_signal_connect(impl_->webView, "notify::is-playing-audio", G_CALLBACK(+[](WebKitWebView *, GParamSpec *, gpointer user) {
        emit static_cast<WebView *>(user)->audioChanged();
    }), this);
    WebKitFindController *findController = webkit_web_view_get_find_controller(impl_->webView);
    g_signal_connect(findController, "found-text", G_CALLBACK(+[](WebKitFindController *, guint matches, gpointer user) {
        emit static_cast<WebView *>(user)->findResult(matches > 10000 ? 10001 : int(matches));
    }), this);
    g_signal_connect(findController, "failed-to-find-text", G_CALLBACK(+[](WebKitFindController *, gpointer user) {
        emit static_cast<WebView *>(user)->findResult(0);
    }), this);
    g_signal_connect(impl_->webView, "load-failed", G_CALLBACK(Impl::onLoadFailed), this);
    g_signal_connect(impl_->webView, "load-failed-with-tls-errors", G_CALLBACK(Impl::onTlsErrors), this);
    g_signal_connect(impl_->webView, "permission-request", G_CALLBACK(Impl::onPermission), this);
    g_signal_connect(impl_->webView, "query-permission-state", G_CALLBACK(Impl::onPermissionState), this);
    g_signal_connect(impl_->webView, "show-notification", G_CALLBACK(Impl::onNotification), this);
    g_signal_connect(impl_->webView, "create", G_CALLBACK(Impl::onCreate), this);
    g_signal_connect(impl_->webView, "decide-policy", G_CALLBACK(Impl::onDecidePolicy), this);
    g_signal_connect(impl_->webView, "script-dialog", G_CALLBACK(Impl::onScriptDialog), this);
    g_signal_connect(impl_->webView, "run-file-chooser", G_CALLBACK(Impl::onFileChooser), this);
    g_signal_connect(impl_->webView, "enter-fullscreen", G_CALLBACK(Impl::onEnterFullscreen), this);
    g_signal_connect(impl_->webView, "leave-fullscreen", G_CALLBACK(Impl::onLeaveFullscreen), this);
    g_signal_connect(impl_->webView, "close", G_CALLBACK(Impl::onClose), this);
    g_signal_connect(impl_->webView, "key-press-event", G_CALLBACK(Impl::onKeyPress), this);
    g_signal_connect(impl_->webView, "focus-in-event", G_CALLBACK(+[](GtkWidget *, GdkEventFocus *, gpointer user) -> gboolean {
        emit static_cast<WebView *>(user)->pageFocused();
        return FALSE;
    }), this);
    g_signal_connect(impl_->plug, "key-press-event", G_CALLBACK(Impl::onKeyPress), this);
    g_signal_connect(impl_->webView, "button-press-event", G_CALLBACK(+[](GtkWidget *, GdkEventButton *, gpointer user) -> gboolean {
        emit static_cast<WebView *>(user)->pageFocused();
        if (!keySink && !keySinkCancelled)
            return FALSE;
        ++keySinkRevision;
        keySink = {};
        auto cancelled = std::move(keySinkCancelled);
        keySinkCancelled = {};
        if (cancelled)
            cancelled();
        return FALSE;
    }), this);

    const WId plugId = static_cast<WId>(gtk_plug_get_id(GTK_PLUG(impl_->plug)));
    if (plugId == 0) {
        auto *error = new QLabel(tr("Não foi possível embutir o WebKit."), this);
        error->setAlignment(Qt::AlignCenter);
        layout->addWidget(error);
        return;
    }

    auto *foreign = QWindow::fromWinId(plugId);
    impl_->container = QWidget::createWindowContainer(foreign, this);
    impl_->container->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(impl_->container);
    layout->addWidget(impl_->container);
}

WebView::~WebView() = default;

void WebView::setUrl(const QUrl &url)
{
    if (!impl_->webView)
        return;
    if (impl_->profile->contentFilterPending()) {
        impl_->pendingLoad = [this, url] { setUrl(url); };
        return;
    }
    impl_->pendingLoad = nullptr;
    impl_->applyUserAgent(url);
    const QByteArray uri = (url.isValid() ? url : QUrl(QStringLiteral("about:blank")))
                               .toString(QUrl::FullyEncoded)
                               .toUtf8();
    webkit_web_view_load_uri(impl_->webView, uri.constData());
}

void WebView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    if (!impl_->webView)
        return;
    if (impl_->profile->contentFilterPending()) {
        impl_->pendingLoad = [this, html, baseUrl] { loadHtml(html, baseUrl); };
        return;
    }
    impl_->pendingLoad = nullptr;
    impl_->applyUserAgent(baseUrl);
    const QByteArray markup = html.toUtf8();
    const QByteArray base = baseUrl.toString(QUrl::FullyEncoded).toUtf8();
    webkit_web_view_load_html(impl_->webView, markup.constData(), base.constData());
}

bool WebView::engineAlive() const
{
    return impl_->webView && !impl_->crashed && WEBKIT_IS_WEB_VIEW(impl_->webView);
}

QString WebView::userAgent() const
{
    return impl_->webView ? QString::fromUtf8(webkit_settings_get_user_agent(webkit_web_view_get_settings(impl_->webView))) : QString();
}

bool WebView::allowsCredentials() const
{
    return engineAlive() && Navigation::canUseCredentials(url(), impl_->profile->hasCertificateException(url()));
}

QUrl WebView::url() const
{
    if (!engineAlive())
        return {};
    const char *uri = webkit_web_view_get_uri(impl_->webView);
    return uri ? QUrl(QString::fromUtf8(uri)) : QUrl();
}

QString WebView::title() const
{
    if (!impl_->webView)
        return {};
    const char *title = webkit_web_view_get_title(impl_->webView);
    return title ? QString::fromUtf8(title) : QString();
}

QIcon WebView::icon() const
{
    return impl_->icon;
}

bool WebView::canGoBack() const
{
    return impl_->webView && webkit_web_view_can_go_back(impl_->webView);
}

bool WebView::canGoForward() const
{
    return impl_->webView && webkit_web_view_can_go_forward(impl_->webView);
}

void WebView::back()
{
    if (impl_->webView) {
        if (auto *item = webkit_back_forward_list_get_back_item(webkit_web_view_get_back_forward_list(impl_->webView)))
            impl_->applyUserAgent(QUrl(QString::fromUtf8(webkit_back_forward_list_item_get_uri(item))));
        webkit_web_view_go_back(impl_->webView);
    }
}

void WebView::forward()
{
    if (impl_->webView) {
        if (auto *item = webkit_back_forward_list_get_forward_item(webkit_web_view_get_back_forward_list(impl_->webView)))
            impl_->applyUserAgent(QUrl(QString::fromUtf8(webkit_back_forward_list_item_get_uri(item))));
        webkit_web_view_go_forward(impl_->webView);
    }
}

void WebView::reload(bool bypassCache)
{
    if (impl_->webView) {
        impl_->crashed = false;
        if (bypassCache) webkit_web_view_reload_bypass_cache(impl_->webView);
        else webkit_web_view_reload(impl_->webView);
    }
}

void WebView::find(const QString &text, bool backwards)
{
    if (!engineAlive()) return;
    impl_->lastFindText = text;
    if (text.isEmpty()) { finishFind(); emit findResult(0); return; }
    const QByteArray utf8 = text.toUtf8();
    WebKitFindController *controller = webkit_web_view_get_find_controller(impl_->webView);
    const gchar *previous = webkit_find_controller_get_search_text(controller);
    if (impl_->findActive && previous && utf8 == previous) {
        if (backwards) webkit_find_controller_search_previous(controller);
        else webkit_find_controller_search_next(controller);
        return;
    }
    const guint options = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE | WEBKIT_FIND_OPTIONS_WRAP_AROUND
        | (backwards ? WEBKIT_FIND_OPTIONS_BACKWARDS : 0);
    impl_->findActive = true;
    webkit_find_controller_search(webkit_web_view_get_find_controller(impl_->webView), utf8.constData(), options, 10000);
}
QString WebView::lastFindText() const { return impl_->lastFindText; }

void WebView::finishFind()
{
    impl_->findActive = false;
    if (impl_->webView)
        webkit_find_controller_search_finish(webkit_web_view_get_find_controller(impl_->webView));
}
void WebView::setZoom(double value)
{
    if (engineAlive()) webkit_web_view_set_zoom_level(impl_->webView, std::clamp(value, 0.25, 5.0));
}
double WebView::zoom() const { return engineAlive() ? webkit_web_view_get_zoom_level(impl_->webView) : 1.0; }
void WebView::setMuted(bool muted)
{
    if (engineAlive()) webkit_web_view_set_is_muted(impl_->webView, muted);
    emit audioChanged();
}
bool WebView::isMuted() const { return engineAlive() && webkit_web_view_get_is_muted(impl_->webView); }
bool WebView::isPlayingAudio() const { return engineAlive() && webkit_web_view_is_playing_audio(impl_->webView); }
bool WebView::maySuspend() const
{
    return engineAlive() && !webkit_web_view_is_loading(impl_->webView)
        && !isPlayingAudio() && !impl_->permissionPending
        && webkit_web_view_get_camera_capture_state(impl_->webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE
        && webkit_web_view_get_microphone_capture_state(impl_->webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE
        && webkit_web_view_get_display_capture_state(impl_->webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE;
}
void WebView::checkCanSuspend(const std::function<void(bool)> &callback)
{
    if (!maySuspend()) { callback(false); return; }
    auto *done = new std::function<void(bool)>(callback);
    const char *script = R"JS((() => {
        if (document.querySelector('[contenteditable]:not([contenteditable="false"]),iframe,frame')) return false;
        for (const field of document.querySelectorAll('input,textarea,select')) {
            if (field.tagName === 'SELECT') {
                const options = Array.from(field.options);
                const explicitDefault = options.some(option => option.defaultSelected);
                if (!field.multiple && !explicitDefault) {
                    if (field.selectedIndex !== (options.length ? 0 : -1)) return false;
                } else if (options.some(option => option.selected !== option.defaultSelected)) return false;
            } else if (field.type === 'checkbox' || field.type === 'radio') {
                if (field.checked !== field.defaultChecked) return false;
            } else if (field.type === 'file') {
                if (field.files.length) return false;
            } else if (field.type === 'password') {
                if (field.value) return false;
            } else if (field.type !== 'hidden' && field.value !== field.defaultValue) return false;
        }
        return !Array.from(document.querySelectorAll('video,audio')).some(media => !media.paused && !media.ended);
    })())JS";
    webkit_web_view_evaluate_javascript(impl_->webView, script, -1, nullptr, nullptr, nullptr,
        +[](GObject *source, GAsyncResult *result, gpointer user) {
            std::unique_ptr<std::function<void(bool)>> done(static_cast<std::function<void(bool)> *>(user));
            GError *error = nullptr;
            JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &error);
            const bool ok = !error && value && jsc_value_is_boolean(value) && jsc_value_to_boolean(value);
            if (value) g_object_unref(value);
            if (error) g_error_free(error);
            (*done)(ok);
        }, done);
}
void WebView::print()
{
    if (!engineAlive()) return;
    WebKitPrintOperation *operation = webkit_print_operation_new(impl_->webView);
    webkit_print_operation_run_dialog(operation, GTK_WINDOW(impl_->plug));
    g_object_unref(operation);
}
void WebView::inspect()
{
    if (engineAlive()) webkit_web_inspector_show(webkit_web_view_get_inspector(impl_->webView));
}

void WebView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncSurfaceSize();
}

void WebView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this] { syncSurfaceSize(); });
}

void WebView::syncSurfaceSize()
{
    if (!impl_->webView || impl_->parked)
        return;
    const int width = this->width();
    const int height = this->height();
    if (width < 32 || height < 32)
        return;
}

void WebView::parkSurface()
{
    impl_->parked = true;
    if (impl_->webView)
        gtk_widget_hide(GTK_WIDGET(impl_->webView));
    lower();
}

void WebView::reembed()
{
    if (!impl_->plug)
        return;
    const WId plugId = static_cast<WId>(gtk_plug_get_id(GTK_PLUG(impl_->plug)));
    if (plugId == 0)
        return;
    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
    if (!layout)
        return;
    if (impl_->container) {
        setFocusProxy(nullptr);
        layout->removeWidget(impl_->container);
        impl_->container->hide();
        impl_->container->setParent(nullptr);
        impl_->container->deleteLater();
        impl_->container = nullptr;
    }
    gtk_widget_show_all(GTK_WIDGET(impl_->plug));
    auto *foreign = QWindow::fromWinId(plugId);
    impl_->container = QWidget::createWindowContainer(foreign, this);
    impl_->container->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(impl_->container);
    layout->addWidget(impl_->container);
    impl_->container->show();
}

void WebView::setEmbeddedVisible(const bool visible)
{
    impl_->parked = !visible;
    if (!isVisible())
        show();
    if (!visible) {
        parkSurface();
        return;
    }
    if (impl_->webView)
        gtk_widget_show(GTK_WIDGET(impl_->webView));
    raise();
    syncSurfaceSize();
}

void WebView::setPageFocusEnabled(const bool enabled)
{
    if (impl_->webView)
        gtk_widget_set_can_focus(GTK_WIDGET(impl_->webView), enabled);
    if (impl_->plug)
        gtk_widget_set_can_focus(GTK_WIDGET(impl_->plug), enabled);
    if (impl_->container)
        impl_->container->setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);
    if (enabled)
        return;
    if (impl_->plug && GTK_IS_WINDOW(impl_->plug))
        gtk_window_set_focus(GTK_WINDOW(impl_->plug), nullptr);
    if (impl_->container)
        impl_->container->clearFocus();
    clearFocus();
}

void WebView::stop()
{
    impl_->pendingLoad = nullptr;
    if (impl_->webView)
        webkit_web_view_stop_loading(impl_->webView);
}

QByteArray WebView::sessionState() const
{
    if (!engineAlive())
        return {};
    WebKitWebViewSessionState *state = webkit_web_view_get_session_state(impl_->webView);
    if (!state)
        return {};
    GBytes *bytes = webkit_web_view_session_state_serialize(state);
    webkit_web_view_session_state_unref(state);
    if (!bytes)
        return {};
    gsize size = 0;
    const void *data = g_bytes_get_data(bytes, &size);
    const QByteArray serialized(static_cast<const char *>(data), static_cast<qsizetype>(size));
    g_bytes_unref(bytes);
    return serialized;
}

bool WebView::restoreSessionState(const QByteArray &state)
{
    if (!impl_->webView || state.isEmpty())
        return false;
    impl_->pendingLoad = nullptr;
    GBytes *bytes = g_bytes_new(state.constData(), static_cast<gsize>(state.size()));
    WebKitWebViewSessionState *session = webkit_web_view_session_state_new(bytes);
    g_bytes_unref(bytes);
    if (!session)
        return false;
    webkit_web_view_restore_session_state(impl_->webView, session);
    webkit_web_view_session_state_unref(session);
    WebKitBackForwardList *list = webkit_web_view_get_back_forward_list(impl_->webView);
    if (WebKitBackForwardListItem *item = webkit_back_forward_list_get_current_item(list)) {
        if (impl_->profile->contentFilterPending()) {
            impl_->pendingLoad = [this, state] { restoreSessionState(state); };
            return true;
        }
        impl_->applyUserAgent(QUrl(QString::fromUtf8(webkit_back_forward_list_item_get_uri(item))));
        webkit_web_view_go_to_back_forward_list_item(impl_->webView, item);
        return true;
    }
    return false;
}

void WebView::setPopupFactory(PopupFactory factory)
{
    impl_->popupFactory = std::move(factory);
}

void WebView::installPageScript(const QString &source)
{
    if (!impl_->webView)
        return;
    const QByteArray code = source.toUtf8();
    WebKitUserScript *script = webkit_user_script_new_for_world(
        code.constData(),
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
        "zenith-credentials",
        nullptr,
        nullptr);
    webkit_user_content_manager_add_script(
        webkit_web_view_get_user_content_manager(impl_->webView), script);
    webkit_user_script_unref(script);
}

void WebView::setMessageHandler(MessageHandler handler)
{
    if (!impl_->webView)
        return;
    impl_->messageHandler = std::move(handler);
    WebKitUserContentManager *content = webkit_web_view_get_user_content_manager(impl_->webView);
    g_signal_connect(content, "script-message-with-reply-received::credentials",
                     G_CALLBACK(Impl::onScriptMessage), this);
    webkit_user_content_manager_register_script_message_handler_with_reply(
        content, "credentials", "zenith-credentials");
}

#include "WebView.moc"

void WebView::applyBrowsingPreferences()
{
    if (!impl_->webView) return;
    auto *settings = webkit_web_view_get_settings(impl_->webView);
    QSettings preferences;
    webkit_settings_set_enable_javascript(settings, preferences.value("web/javascript", true).toBool());
    webkit_settings_set_enable_smooth_scrolling(settings, preferences.value("web/smoothScrolling", true).toBool());
    webkit_settings_set_default_font_size(settings, qBound(12, preferences.value("web/defaultFontSize", 16).toInt(), 32));
    setZoom(impl_->profile->data()->zoom(url()));
}
