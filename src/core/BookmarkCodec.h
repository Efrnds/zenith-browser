#pragma once
#include "core/BrowserData.h"

namespace BookmarkCodec {
QByteArray encode(const QList<BrowserData::Entry> &entries);
QList<BrowserData::Entry> decode(const QByteArray &html);
}
