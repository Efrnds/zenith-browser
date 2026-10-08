#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;

class SettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setSearchTemplate(const QString &value);
    void setCompactMode(bool enabled);
    void setAdBlockEnabled(bool enabled);
    void setAdBlockStatus(quint64 blocked, qsizetype rules, const QString &status);
    void setSuspendOnMinimize(bool enabled);
    void setRestoreSession(bool enabled);

signals:
    void closeRequested();
    void searchTemplateChanged(const QString &value);
    void compactModeChanged(bool enabled);
    void adBlockChanged(bool enabled);
    void updateAdBlockRequested();
    void manageCredentialsRequested();
    void suspendOnMinimizeChanged(bool enabled);
    void restoreSessionChanged(bool enabled);

private:
    QWidget *makeGeneralPage();
    QWidget *makeAppearancePage();
    QWidget *makePrivacyPage();
    QWidget *makePerformancePage();
    QWidget *makeAboutPage();

    QListWidget *navigation_;
    QStackedWidget *pages_;
    QComboBox *searchEngine_;
    QCheckBox *restoreSession_;
    QCheckBox *compactMode_;
    QCheckBox *adBlock_;
    QCheckBox *suspendOnMinimize_;
    QLabel *adBlockStatus_;
};
