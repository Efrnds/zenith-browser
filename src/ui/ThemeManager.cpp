#include "ui/ThemeManager.h"
#include "ui/UiMetrics.h"
#include "core/Preferences.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QRegularExpression>
#include <QWidget>
#include <QFocusEvent>
#include <QStyle>
#include <cmath>
#ifdef Q_OS_LINUX
#undef signals
#undef slots
#include <gtk/gtk.h>
#endif

bool ThemeManager::initializeGtk()
{
#ifdef Q_OS_LINUX
    // WebKit reads GTK_THEME ahead of prefer-dark, so keep the override as a
    // GtkSettings value instead. Do this before GTK or Qt starts worker threads.
    const QByteArray override = qgetenv("GTK_THEME");
    if (!override.isEmpty()) qunsetenv("GTK_THEME");
    const bool available = gtk_init_check(nullptr, nullptr);
    if (available && !override.isEmpty()) {
        QString name = QString::fromUtf8(override);
        const bool dark = name.endsWith(":dark", Qt::CaseInsensitive) || name.endsWith("-dark", Qt::CaseInsensitive);
        const int variant = name.indexOf(':');
        if (variant >= 0) name.truncate(variant);
        g_object_set(gtk_settings_get_default(), "gtk-theme-name", name.toUtf8().constData(),
            "gtk-application-prefer-dark-theme", dark, nullptr);
    }
    return available;
#else
    return true;
#endif
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    qApp->installEventFilter(this);
    readPreferences();
    systemDark_ = qApp->palette().color(QPalette::Window).lightness() < 128;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp->styleHints()->colorScheme() != Qt::ColorScheme::Unknown)
        systemDark_ = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        systemDark_ = scheme == Qt::ColorScheme::Dark;
        apply();
    });
#endif
#ifdef Q_OS_LINUX
    if (auto *gtk = gtk_settings_get_default()) {
        gboolean dark = FALSE;
        gchar *theme = nullptr, *hint = nullptr, *rgba = nullptr;
        g_object_get(gtk, "gtk-application-prefer-dark-theme", &dark, "gtk-theme-name", &theme,
            "gtk-xft-antialias", &antialias_, "gtk-xft-hinting", &hinting_,
            "gtk-xft-hintstyle", &hint, "gtk-xft-rgba", &rgba, nullptr);
        gtkTheme_ = QString::fromUtf8(theme);
        gtkDark_ = dark || gtkTheme_.endsWith("dark", Qt::CaseInsensitive);
        hintStyle_ = QString::fromUtf8(hint); rgba_ = QString::fromUtf8(rgba);
        g_free(theme); g_free(hint); g_free(rgba);
    }
#endif
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            const auto *focus = static_cast<QFocusEvent *>(event);
            const bool keyboard = event->type() == QEvent::FocusIn
                && (focus->reason() == Qt::TabFocusReason || focus->reason() == Qt::BacktabFocusReason
                    || focus->reason() == Qt::ShortcutFocusReason);
            widget->setProperty("keyboardFocus", keyboard);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }
    }
    return QObject::eventFilter(watched, event);
}

bool ThemeManager::dark() const { return mode_ == "dark" || (mode_ == "system" && systemDark_); }

QColor ThemeManager::color(Role role) const
{
    switch (role) {
    case Role::Content: return color("#121214");
    case Role::Sidebar: return color("#18181b");
    case Role::Field: return color("#202023");
    case Role::Raised: return color("#27272a");
    case Role::Divider: return color("#323238");
    case Role::Border: return color("#3f3f46");
    case Role::Text: return color("#f4f4f5");
    case Role::Secondary: return color("#a1a1aa");
    case Role::AccentText: case Role::Focus: return color("#8b7cf6");
    case Role::AccentFill: return color("#5b4fc4");
    case Role::Error: case Role::Destructive: return QColor(dark() ? "#ffb4ab" : "#9c252c");
    }
    return {};
}

QColor ThemeManager::color(const QString &key) const
{
    const bool d = dark();
    if (accent_.startsWith('#') && (key == "#8b7cf6" || key == "#5b4fc4")) {
        const QColor custom(accent_);
        const bool foreground = key == "#8b7cf6";
        const QColor background = foreground ? color("#27272a") : QColor(Qt::white);
        const auto luminance = [](const QColor &value) {
            const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
            return 0.2126 * linear(value.redF()) + 0.7152 * linear(value.greenF()) + 0.0722 * linear(value.blueF());
        };
        double lightness = foreground && d ? 0.76 : 0.30;
        QColor result;
        for (int step = 0; step < 51; ++step) {
            result = QColor::fromHslF(qMax(0.0f, custom.hslHueF()), custom.hslSaturationF(), lightness);
            const double a = luminance(result), b = luminance(background);
            if ((qMax(a, b) + 0.05) / (qMin(a, b) + 0.05) >= 4.5) break;
            lightness = qBound(0.0, lightness + (foreground && d ? 0.02 : -0.02), 1.0);
        }
        return result;
    }
    if (key == "#8b7cf6") {
        if (accent_ == "blue") return QColor(d ? "#91c5ff" : "#175ca5");
        if (accent_ == "teal") return QColor(d ? "#76d9c0" : "#08745e");
        if (accent_ == "amber") return QColor(d ? "#f1c17d" : "#865300");
        return QColor(d ? "#bcaaff" : "#6840b0");
    }
    if (key == "#5b4fc4") {
        if (accent_ == "blue") return QColor(d ? "#1e5a8c" : "#175ca5");
        if (accent_ == "teal") return QColor(d ? "#16604f" : "#08745e");
        if (accent_ == "amber") return QColor(d ? "#785000" : "#865300");
        return QColor(d ? "#574184" : "#6840b0");
    }
    if (key == "#121214") return QColor(d ? "#10151c" : "#f3f5f8");
    if (key == "#18181b") return QColor(d ? "#171e28" : "#e8edf3");
    if (key == "#202023" || key == "#1c1c1f") return QColor(d ? "#202b38" : "#ffffff");
    if (key == "#27272a" || key == "#353438") return QColor(d ? "#2c3a4b" : "#d6dfeb");
    if (key == "#323236" || key == "#323238") return QColor(d ? "#354457" : "#cbd5e2");
    if (key == "#3f3f46" || key == "#46454b" || key == "#52525b") return QColor(d ? "#6b7d94" : "#7e8fa5");
    if (key == "#a1a1aa") return QColor(d ? "#abb9ca" : "#506176");
    if (key == "#f4f4f5" || key == "#fafafa" || key == "#ffffff" || key == "#d4d4d8")
        return QColor(d ? "#edf2f8" : "#1c2a3b");
    return QColor(key);
}

void ThemeManager::applyWebAppearance()
{
#ifdef Q_OS_LINUX
    auto *gtk = gtk_settings_get_default();
    if (!gtk) return;
    if (siteMode_ == "system") {
        g_object_set(gtk, "gtk-theme-name", gtkTheme_.toUtf8().constData(),
            "gtk-application-prefer-dark-theme", gtkDark_, nullptr);
    } else {
        // A dark GTK theme name overrides prefer-dark=false inside WebKit.
        g_object_set(gtk, "gtk-theme-name", "Adwaita",
            "gtk-application-prefer-dark-theme", siteMode_ == "dark", nullptr);
    }
    if (textRendering_ == "system") {
        g_object_set(gtk, "gtk-xft-antialias", antialias_, "gtk-xft-hinting", hinting_,
            "gtk-xft-hintstyle", hintStyle_.toUtf8().constData(), "gtk-xft-rgba", rgba_.toUtf8().constData(), nullptr);
    } else {
        // Grayscale avoids RGB fringes on composited or fractionally scaled surfaces.
        g_object_set(gtk, "gtk-xft-antialias", 1, "gtk-xft-hinting", 1,
            "gtk-xft-hintstyle", textRendering_ == "sharp" ? "hintfull" : "hintslight",
            "gtk-xft-rgba", "none", nullptr);
    }
#endif
}

void ThemeManager::apply()
{
    QPalette palette;
    palette.setColor(QPalette::Window, color(Role::Content));
    palette.setColor(QPalette::Base, color(Role::Field));
    palette.setColor(QPalette::AlternateBase, color(Role::Sidebar));
    palette.setColor(QPalette::Button, color(Role::Sidebar));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::ToolTipText})
        palette.setColor(role, color("#f4f4f5"));
    palette.setColor(QPalette::PlaceholderText, color("#a1a1aa"));
    palette.setColor(QPalette::Highlight, color("#5b4fc4"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, color("#18181b"));
    palette.setColor(QPalette::Link, color("#8b7cf6"));
    palette.setColor(QPalette::Disabled, QPalette::Text, color("#a1a1aa"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, color("#a1a1aa"));
    qApp->setPalette(palette);
    QFile file(":/style.qss");
    if (file.open(QIODevice::ReadOnly)) {
        QString sheet = QString::fromUtf8(file.readAll());
        sheet.replace("@hit@", QString::number(UiMetrics::Hit));
        sheet.replace("@page@", QString::number(UiMetrics::Page));
        sheet.replace("@tiny@", QString::number(UiMetrics::Tiny));
        sheet.replace("@small@", QString::number(UiMetrics::Small));
        sheet.replace("@related@", QString::number(UiMetrics::Related));
        sheet.replace("@section@", QString::number(UiMetrics::Section));
        sheet.replace("@large@", QString::number(UiMetrics::Large));
        sheet.replace("@error@", color(Role::Error).name());
        sheet.replace("@danger@", color(Role::Destructive).name());
        QSettings preferences;
        sheet.replace("font-size: 13px;", QString("font-size: %1px;").arg(qBound(11, preferences.value("appearance/fontSize", 13).toInt(), 18)));
        sheet.replace("border-radius: 10px;", QString("border-radius: %1px;").arg(qBound(0, preferences.value("appearance/cornerRadius", 10).toInt(), 20)));
        QRegularExpression expression("#[0-9a-fA-F]{6}\\b");
        auto matches = expression.globalMatch(sheet);
        QString rendered; qsizetype offset = 0;
        while (matches.hasNext()) {
            const auto match = matches.next();
            rendered += sheet.mid(offset, match.capturedStart() - offset) + color(match.captured().toLower()).name();
            offset = match.capturedEnd();
        }
        rendered += sheet.mid(offset);
        qApp->setStyleSheet(rendered);
    }
    applyWebAppearance();
    emit changed();
    for (QWidget *widget : QApplication::allWidgets()) widget->update();
}

void ThemeManager::setMode(const QString &value)
{
    if (value != "system" && value != "light" && value != "dark") return;
    mode_ = value; QSettings().setValue("appearance/theme", value); apply();
}
void ThemeManager::reloadPreferences()
{
    readPreferences();
    apply();
}

void ThemeManager::readPreferences()
{
    mode_ = Preferences::value("appearance/theme").toString();
    accent_ = Preferences::value("appearance/accent").toString();
    siteMode_ = Preferences::value("appearance/sites").toString();
    textRendering_ = Preferences::value("appearance/textRendering").toString();
    if (!QStringList{"system", "light", "dark"}.contains(mode_)) mode_ = "dark";
    if (!QStringList{"violet", "blue", "teal", "amber"}.contains(accent_)
        && !QRegularExpression("^#[0-9a-fA-F]{6}$").match(accent_).hasMatch()) accent_ = "violet";
    if (!QStringList{"system", "light", "dark"}.contains(siteMode_)) siteMode_ = "system";
    if (!QStringList{"system", "smooth", "sharp"}.contains(textRendering_)) textRendering_ = "smooth";
}
void ThemeManager::setAccent(const QString &value)
{
    if (value != "violet" && value != "blue" && value != "teal" && value != "amber"
        && !QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch()) return;
    accent_ = value; QSettings().setValue("appearance/accent", value); apply();
}
void ThemeManager::setSiteMode(const QString &value)
{
    if (value != "system" && value != "light" && value != "dark") return;
    siteMode_ = value; QSettings().setValue("appearance/sites", value); apply();
}
void ThemeManager::setTextRendering(const QString &value)
{
    if (value != "system" && value != "smooth" && value != "sharp") return;
    textRendering_ = value; QSettings().setValue("appearance/textRendering", value); apply();
}
