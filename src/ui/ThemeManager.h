#pragma once
#include <QObject>
#include <QColor>

class ThemeManager final : public QObject
{
    Q_OBJECT
public:
    static bool initializeGtk();
    static ThemeManager &instance();
    QString mode() const { return mode_; }
    QString accent() const { return accent_; }
    QString siteMode() const { return siteMode_; }
    QString textRendering() const { return textRendering_; }
    bool dark() const;
    QColor color(const QString &legacy) const;
    void apply();
    void setMode(const QString &value);
    void setAccent(const QString &value);
    void setSiteMode(const QString &value);
    void setTextRendering(const QString &value);
signals:
    void changed();
private:
    ThemeManager();
    void applyWebAppearance();
    QString mode_, accent_, siteMode_, textRendering_;
    bool systemDark_ = false;
    bool gtkDark_ = false;
    QString gtkTheme_;
    int antialias_ = -1, hinting_ = -1;
    QString hintStyle_, rgba_;
};

inline QColor themeColor(const QString &value) { return ThemeManager::instance().color(value); }
