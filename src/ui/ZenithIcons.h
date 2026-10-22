#pragma once

#include <QColor>
#include <QIcon>

enum class ZenithIcon {
    Add,
    Back,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    Close,
    Folder,
    Document,
    Forward,
    More,
    Menu,
    Sidebar,
    Download,
    Bookmark,
    History,
    Reload,
    Settings,
    Shield,
    Sleep,
    Split,
    Stop
};

[[nodiscard]] QIcon zenithIcon(ZenithIcon type,
                               const QColor &color = QColor(),
                               int size = 18);
