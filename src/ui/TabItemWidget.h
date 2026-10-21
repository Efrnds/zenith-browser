#pragma once

#include <QIcon>
#include <QWidget>

class QEnterEvent;
class QKeyEvent;

class TabItemWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TabItemWidget(int tabId, QWidget *parent = nullptr);

    [[nodiscard]] int tabId() const;
    [[nodiscard]] QSize sizeHint() const override;
    void setTitle(const QString &title);
    void setIcon(const QIcon &icon);
    void setDiscarded(bool discarded);
    void setCollapsed(bool collapsed);
    void setActive(bool active);
    void setLeading(int leading);
    void syncPointer();

signals:
    void activated(int tabId);
    void closeRequested(int tabId);
    void moveRequested(int sourceId, int targetId);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    [[nodiscard]] QRect closeRect() const;
    [[nodiscard]] bool pointerInRow() const;

    int tabId_;
    QString fullTitle_;
    QIcon icon_;
    int leading_ = 0;
    bool discarded_ = false;
    bool collapsed_ = false;
    bool active_ = false;
    bool pointerInside_ = false;
    QPoint dragStart_;
};
