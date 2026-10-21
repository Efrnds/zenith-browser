#pragma once

#include <QString>
#include <QUrl>

namespace Navigation {
enum Command {
    None = 0, NewTab = 1, Address = 2, CloseTab = 3, NextTab = 4, PreviousTab = 5,
    ReopenTab, Find, Reload, ZoomIn, ZoomOut, ZoomReset, History, Downloads,
    Bookmark, Bookmarks, Back, Forward, Fullscreen, Print, Inspector, ToggleSplit,
    SelectTab1, SelectTab2, SelectTab3, SelectTab4, SelectTab5, SelectTab6, SelectTab7, SelectTab8, SelectLastTab,
    HardReload, FindNext, FindPrevious,
    Settings, BrowserMenu, ToggleCompact, NewPrivateWindow, Credentials,
    SiteControls, ClearData, ImportBookmarks, ExportBookmarks,
    OpenProfile, DefaultProfile, DefaultBrowser, SaveSession, CloseWindow
};
int command(int key, Qt::KeyboardModifiers modifiers);
QString origin(const QUrl &url);
bool isWebUrl(const QUrl &url);
bool canUseCredentials(const QUrl &url, bool certificateException);
QString outlookUserAgent(const QUrl &url);
QUrl fromInput(const QString &input, const QString &searchTemplate);
}
