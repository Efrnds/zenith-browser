#pragma once
#include <QList>
#include <QString>
#include <QVariant>

namespace Preferences {
enum class Effect { Immediate, Reload, NextNavigation, NextDownload, NextStartup, NextCycle };
struct Entry {
    QString key;
    QString category;
    QString label;
    QVariant fallback;
    Effect effect = Effect::Immediate;
};
const QList<Entry> &entries();
void resetCategory(const QString &category);
QVariant value(const QString &key);
QString effectDescription(const QString &key);
}
