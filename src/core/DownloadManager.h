#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QHash>
#include <functional>
#include <QtGlobal>

struct _WebKitWebContext;
#ifdef Q_OS_LINUX
struct _WebKitDownload;
#endif
class QWidget;
class QDialog;
class QTimer;

class DownloadManager final : public QObject
{
    Q_OBJECT
public:
    struct Item {
        int id;
        QString path;
        QString status;
        double progress = 0;
        qint64 bytes = 0;
        double seconds = 0;
        bool finished = false;
        bool failed = false;
#ifdef Q_OS_LINUX
        _WebKitDownload *native = nullptr;
#else
        void *native = nullptr;
#endif
    };
#ifdef Q_OS_LINUX
    DownloadManager(_WebKitWebContext *context, QObject *parent, QString historyPath = {});
#else
    DownloadManager(QObject *parent, QString historyPath = {});
#endif
    ~DownloadManager() override;
    QList<Item> items() const;
    void cancel(int id);
    bool active() const;
    QDialog *createPage(QWidget *parent);
    static QString uniqueDestination(const QString &directory, const QString &suggestion);
    static QString safeName(const QString &suggestion);
    void setDestinationChooser(std::function<QString(const QString &)> chooser);
    int beginNativeDownload(const QString &suggestion,
        const std::function<bool(const QString &, bool)> &accept,
        std::function<void()> cancel);
    void updateNativeDownload(int id, qint64 received, qint64 total,
                              bool finished, const QString &error = {});
signals:
    void changed();
private:
#ifdef Q_OS_LINUX
    void started(_WebKitDownload *download);
#endif
    void persist() const;
    void trimHistory();
#ifdef Q_OS_LINUX
    _WebKitWebContext *context_;
#endif
    QList<Item> items_;
    int nextId_ = 1;
    QString historyPath_;
    std::function<QString(const QString &)> destinationChooser_;
    QTimer *progressTimer_;
    QHash<int, std::function<void()>> nativeCancelers_;
};
