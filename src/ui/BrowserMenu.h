#pragma once
#include <QMenu>

class BrowserMenu final : public QMenu
{
    Q_OBJECT
public:
    struct State {
        bool hasPage = false;
        bool hasSite = false;
        bool privateMode = false;
        bool compact = false;
        bool split = false;
        bool fullscreen = false;
        int zoom = 100;
    };
    explicit BrowserMenu(const State &state, QWidget *parent = nullptr);
signals:
    void commandRequested(int command);
};
