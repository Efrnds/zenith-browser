#pragma once
#include <QWidget>
#include <QUrl>

class QLineEdit;
class QLabel;

class StartPage final : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget *parent = nullptr);
    void setSearchTemplate(const QString &value);
    void setWorkspaceName(const QString &value);
signals:
    void navigateRequested(const QUrl &url);
    void commandRequested(int command);
private:
    QLineEdit *search_;
    QLabel *engine_;
    QLabel *workspace_;
    QString searchTemplate_;
};
