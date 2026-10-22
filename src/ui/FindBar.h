#pragma once
#include <QWidget>
#include <QPointer>

class QLineEdit;
class QLabel;
class QTimer;
class WebView;

class FindBar final : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    void open(WebView *view);
    void finish();
    void advance(bool backwards) { search(backwards); }
signals:
    void closed();
private:
    QLineEdit *input_;
    QLabel *status_;
    QTimer *timer_;
    QPointer<WebView> view_;
    QMetaObject::Connection resultConnection_;
    void search(bool backwards = false);
};
