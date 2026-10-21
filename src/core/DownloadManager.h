#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <functional>

struct _WebKitWebContext;
struct _WebKitDownload;
class QWidget;
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
        _WebKitDownload *native = nullptr;
    };
    DownloadManager(_WebKitWebContext *context, QObject *parent, QString historyPath = {});
    ~DownloadManager() override;
    QList<Item> items() const;
    void cancel(int id);
    bool active() const;
    void show(QWidget *parent);
    static QString safeName(const QString &suggestion);
    void setDestinationChooser(std::function<QString(const QString &)> chooser);
signals:
    void changed();
private:
    void started(_WebKitDownload *download);
    void persist() const;
    void trimHistory();
    _WebKitWebContext *context_;
    QList<Item> items_;
    int nextId_ = 1;
    QString historyPath_;
    std::function<QString(const QString &)> destinationChooser_;
    QTimer *progressTimer_;
};
