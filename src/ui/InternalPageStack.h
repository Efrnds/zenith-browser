#pragma once
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVector>

class QWidget;
class QStackedWidget;

class InternalPageStack final : public QObject
{
    Q_OBJECT
public:
    InternalPageStack(QStackedWidget *stack, QWidget *root);
    void keep(QWidget *page);
    void open(QWidget *page);
    void back();
    void clear();
    bool active() const;
    int depth() const { return entries_.size(); }
signals:
    void changed(bool active);
private:
    struct Entry { QPointer<QWidget> page; QPointer<QWidget> focus; };
    void release(QWidget *page);
    void reveal();
    QStackedWidget *stack_;
    QWidget *root_;
    QSet<QWidget *> permanent_;
    QVector<Entry> entries_;
    QPointer<QWidget> rootFocus_;
    quint64 focusGeneration_ = 0;
};
