#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"
#include "core/Navigation.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QFile>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QSettings>
#include <QTranslator>
#include <QLibraryInfo>

#ifdef Q_OS_LINUX
#undef signals
#undef slots
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#endif

namespace {
bool prepareWebKitHost()
{
#ifdef Q_OS_LINUX
    // WebKitGTK is a GTK widget. Qt on Wayland cannot reparent it, so the
    // window goes through XWayland, where a GtkPlug can sit inside the view.
    XInitThreads();
    qputenv("GDK_BACKEND", "x11");
    qputenv("QT_QPA_PLATFORM", "xcb");
    const QString gstPlugins = QDir::homePath() + QStringLiteral("/.local/lib/zenith/gstreamer-1.0");
    if (QDir(gstPlugins).exists()) {
        QByteArray path = QFile::encodeName(gstPlugins);
        const QByteArray existing = qgetenv("GST_PLUGIN_PATH");
        if (!existing.isEmpty())
            path += ':' + existing;
        qputenv("GST_PLUGIN_PATH", path);
    }
    const bool available = ThemeManager::initializeGtk();
    if (!available) qputenv("QT_QPA_PLATFORM", "offscreen");
    return available;
#else
    return true;
#endif
}

}

int main(int argc, char *argv[])
{
    const bool displayAvailable = prepareWebKitHost();
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication application(argc, argv);
    QTranslator qtPortuguese;
    if (qtPortuguese.load("qtbase_pt_BR", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        application.installTranslator(&qtPortuguese);
    QCoreApplication::setOrganizationName(QStringLiteral("Zenith"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zenith.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Zenith Browser"));
    const QString flatpakDesktopId = qEnvironmentVariable("FLATPAK_ID");
    application.setDesktopFileName(flatpakDesktopId.isEmpty()
        ? QStringLiteral("zenith-browser") : flatpakDesktopId);
    QCoreApplication::setApplicationVersion(QStringLiteral(ZENITH_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Zenith Browser"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"private", QStringLiteral("Janela privada sem persistência de navegação")});
    parser.addOption({"profile", QStringLiteral("Perfil persistente separado"), "name", "default"});
    parser.addOption({"set-default-profile", QStringLiteral("Define o perfil padrão para próximas execuções"), "name"});
    QCommandLineOption smokeOption(QStringLiteral("smoke-test"), QStringLiteral("Encerra após inicializar a janela, para testes de integração"));
    smokeOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(smokeOption);
    parser.addPositionalArgument("urls", QStringLiteral("Endereços ou pesquisas para abrir"), "[urls...]");
    parser.process(application);
    QSettings globalSettings(QStringLiteral("Zenith"), QStringLiteral("Zenith Browser"));
    const QString profile = parser.isSet("set-default-profile") ? parser.value("set-default-profile")
        : parser.isSet("profile") ? parser.value("profile")
        : globalSettings.value(QStringLiteral("profiles/default"), QStringLiteral("default")).toString();
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,32}$")).match(profile).hasMatch()) {
        fprintf(stderr, "Perfil inválido: use até 32 letras, números, _ ou -.\n");
        return 2;
    }
    if (parser.isSet("set-default-profile")) {
        globalSettings.setValue(QStringLiteral("profiles/default"), profile);
        globalSettings.sync();
        return globalSettings.status() == QSettings::NoError ? 0 : 1;
    }
    if (!displayAvailable) {
        fprintf(stderr, "Zenith requer uma sessão X11/XWayland acessível.\n");
        return 1;
    }
    if (profile != "default")
        QCoreApplication::setApplicationName(QStringLiteral("Zenith Browser-%1").arg(profile));
    const bool privateMode = parser.isSet("private");
    application.setProperty("zenithProfile", profile);
    application.setProperty("zenithPrivate", privateMode);
    if (parser.positionalArguments().size() > 100) {
        fprintf(stderr, "Abra no máximo 100 endereços por chamada.\n");
        return 2;
    }
    QList<QUrl> urls;
    QJsonArray arguments;
    for (const QString &input : parser.positionalArguments()) {
        const QUrl url = Navigation::fromInput(input, QStringLiteral("https://duckduckgo.com/?q=%1"));
        if (!url.isEmpty()) { urls.append(url); arguments.append(url.toString()); }
    }
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataRoot);
    QLockFile lock(dataRoot + QStringLiteral("/instance.lock"));
    const QString serverName = QStringLiteral("zenith-") + QString::fromLatin1(QCryptographicHash::hash(dataRoot.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!privateMode) {
        if (!lock.tryLock(0)) {
            QLocalSocket socket;
            socket.connectToServer(serverName);
            if (!socket.waitForConnected(1500)) {
                fprintf(stderr, "Perfil em uso; não foi possível contatar a instância.\n");
                return 1;
            }
            socket.write(QJsonDocument(arguments).toJson(QJsonDocument::Compact) + '\n');
            if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(1500)) return 1;
            return 0;
        }
        QLocalServer::removeServer(serverName);
        if (!server.listen(serverName)) {
            fprintf(stderr, "Não foi possível abrir o canal da instância.\n");
            return 1;
        }
    }

    ThemeManager::instance().apply();
    application.setWindowIcon(QIcon(QStringLiteral(":/icons/zenith.png")));

    MainWindow window(privateMode);
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            socket->setReadBufferSize(65537);
            auto consume = [socket, &window] {
                if (socket->bytesAvailable() > 65536) { socket->abort(); return; }
                if (!socket->canReadLine()) return;
                const QJsonDocument document = QJsonDocument::fromJson(socket->readLine(65537));
                QList<QUrl> incoming;
                if (document.isArray() && document.array().size() <= 100)
                    for (const auto &value : document.array()) {
                        const QUrl url = Navigation::fromInput(value.toString(), "https://duckduckgo.com/?q=%1");
                        if (!url.isEmpty()) incoming.append(url);
                    }
                window.openUrls(incoming);
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &window, consume);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(3000, socket, [socket] { socket->abort(); socket->deleteLater(); });
            consume();
        }
    });
    window.show();
    if (parser.isSet("smoke-test")) QTimer::singleShot(4000, &window, &QWidget::close);
    if (!urls.isEmpty()) window.openUrls(urls);
    return application.exec();
}
