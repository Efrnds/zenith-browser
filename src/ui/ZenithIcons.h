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
    Reload,
    Settings,
    Shield,
    Sleep,
    Split,
    Stop
};

[[nodiscard]] QIcon zenithIcon(ZenithIcon type,
                               const QColor &color = QColor(QStringLiteral("#d4d4d8")),
                               int size = 18);
