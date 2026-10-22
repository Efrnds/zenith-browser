#include "core/GtkClipboardGuard.h"
#include <dlfcn.h>
#include <memory>

namespace {
struct Request {
    GtkClipboardTargetsReceivedFunc callback;
    gpointer data;
};

void receivedTargets(GtkClipboard *clipboard, GdkAtom *atoms, gint count, gpointer data)
{
    const std::unique_ptr<Request> request(static_cast<Request *>(data));
    // GTK can report failed TARGETS conversion as (nullptr, -1). WebKitGTK
    // 2.54.1 converts that count to an unsigned span and dereferences nullptr.
    if (!atoms || count <= 0) request->callback(clipboard, nullptr, 0, request->data);
    else request->callback(clipboard, atoms, count, request->data);
}
}

void GtkClipboardGuard::requestTargets(GtkClipboard *clipboard,
    GtkClipboardTargetsReceivedFunc callback, gpointer data, RequestTargets backend)
{
    if (!callback) return;
    if (!backend) {
        callback(clipboard, nullptr, 0, data);
        return;
    }
    backend(clipboard, receivedTargets, new Request{callback, data});
}

extern "C" void gtk_clipboard_request_targets(GtkClipboard *clipboard,
    GtkClipboardTargetsReceivedFunc callback, gpointer data)
{
    static const auto backend = reinterpret_cast<GtkClipboardGuard::RequestTargets>(
        dlsym(RTLD_NEXT, "gtk_clipboard_request_targets"));
    GtkClipboardGuard::requestTargets(clipboard, callback, data, backend);
}
