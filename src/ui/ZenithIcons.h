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
    Forward,
    More,
    Menu,
    Sidebar,
    Download,
    Bookmark,
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
