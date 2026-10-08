#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

namespace {
void configureChromium()
{
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    const QByteArray resourceFlags(
        " --process-per-site --renderer-process-limit=4");
    if (!flags.contains("--renderer-process-limit")) {
        flags += resourceFlags;
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.trimmed());
    }
}

void applyStyleSheet(QApplication &application)
{
    QFile styleFile(QStringLiteral(":/style.qss"));
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&styleFile);
    application.setStyleSheet(stream.readAll());
}
}

int main(int argc, char *argv[])
{
    configureChromium();
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Zenith"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zenith.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Zenith Browser"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    applyStyleSheet(application);

    MainWindow window;
    window.show();
    return application.exec();
}
