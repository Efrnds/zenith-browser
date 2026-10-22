#pragma once
#include <gtk/gtk.h>

namespace GtkClipboardGuard {
using RequestTargets = void (*)(GtkClipboard *, GtkClipboardTargetsReceivedFunc, gpointer);
void requestTargets(GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc callback,
                    gpointer data, RequestTargets backend);
}
