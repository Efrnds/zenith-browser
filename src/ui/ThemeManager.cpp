#include "ui/ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QRegularExpression>
#include <QWidget>
#undef signals
#undef slots
#include <gtk/gtk.h>

bool ThemeManager::initializeGtk()
{
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
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    QSettings settings;
    mode_ = settings.value("appearance/theme", "dark").toString();
    accent_ = settings.value("appearance/accent", "violet").toString();
    siteMode_ = settings.value("appearance/sites", "system").toString();
    textRendering_ = settings.value("appearance/textRendering", "smooth").toString();
    if (!QStringList{"system", "light", "dark"}.contains(mode_)) mode_ = "dark";
    if (!QStringList{"violet", "blue", "teal", "amber"}.contains(accent_)) accent_ = "violet";
    if (!QStringList{"system", "light", "dark"}.contains(siteMode_)) siteMode_ = "system";
    if (!QStringList{"system", "smooth", "sharp"}.contains(textRendering_)) textRendering_ = "smooth";
    systemDark_ = qApp->palette().color(QPalette::Window).lightness() < 128;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp->styleHints()->colorScheme() != Qt::ColorScheme::Unknown)
        systemDark_ = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        systemDark_ = scheme == Qt::ColorScheme::Dark;
        apply();
    });
#endif
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
}

bool ThemeManager::dark() const { return mode_ == "dark" || (mode_ == "system" && systemDark_); }

QColor ThemeManager::color(const QString &key) const
{
    const bool d = dark();
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
}

void ThemeManager::apply()
{
    QPalette palette;
    palette.setColor(QPalette::Window, color("#121214"));
    palette.setColor(QPalette::Base, color("#202023"));
    palette.setColor(QPalette::AlternateBase, color("#18181b"));
    palette.setColor(QPalette::Button, color("#18181b"));
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
void ThemeManager::setAccent(const QString &value)
{
    if (value != "violet" && value != "blue" && value != "teal" && value != "amber") return;
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
