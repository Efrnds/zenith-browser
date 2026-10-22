#pragma once

#include <QWidget>
#include <QPointer>
#include <QHash>

class QCheckBox;
class QVBoxLayout;
class QFormLayout;
class QLineEdit;
class QResizeEvent;
class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;

class SettingsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    void refreshPreferenceControls();
    void beginWorkspaceEdit(int action, const QString &name, int target = -1);

    void setWorkspaces(const QStringList &names, int current);
    void setSearchTemplate(const QString &value);
    void setCompactMode(bool enabled);
    void setAdBlockEnabled(bool enabled);
    void setAdBlockStatus(quint64 blocked, qsizetype rules, const QString &status);
    void setSuspendOnMinimize(bool enabled);
    void setRestoreSession(bool enabled);

signals:
    void closeRequested();
    void commandRequested(int command);
    void browsingPreferencesChanged();
    void preferencesReset();
    void defaultBrowserRequested();
    void workspaceActionRequested(int action);
    void workspaceSelected(int workspace);
    void workspaceNameRequested(int action, const QString &name, int target);
    void workspaceTargetActionRequested(int action, int target);
    void searchTemplateChanged(const QString &value);
    void compactModeChanged(bool enabled);
    void adBlockChanged(bool enabled);
    void updateAdBlockRequested();
    void manageCredentialsRequested();
    void suspendOnMinimizeChanged(bool enabled);
    void restoreSessionChanged(bool enabled);
    void privacyPolicyChanged(bool blockThirdParty, bool trackingPrevention);
    void reducedMotionChanged(bool enabled);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void filterPreferences(const QString &query);
    QLineEdit *search_;
    QComboBox *category_;
    QWidget *makeGeneralPage();
    void addCommand(QVBoxLayout *section, const QString &label, int command);
    void addPreference(QVBoxLayout *section, const QString &label, const QString &key, bool fallback);
    void addNumber(QFormLayout *form, const QString &label, const QString &key, int fallback, int minimum, int maximum, bool appearance);
    QWidget *makeAppearancePage();
    QWidget *makeWorkspacesPage();
    QWidget *makePrivacyPage();
    QWidget *makePerformancePage();
    QWidget *makeAboutPage();
    QWidget *makeDownloadsPage();
    QWidget *makeProfilesPage();
    void addReset(QVBoxLayout *layout, const QString &category);
    int categoryBeforeSearch_ = 0;
    bool searching_ = false;
    quint64 filterGeneration_ = 0;
    QPointer<QWidget> searchTarget_;
    QHash<int, int> scrollBeforeSearch_;

    QListWidget *workspaceList_ = nullptr;
    QListWidget *navigation_;
    QStackedWidget *pages_;
    QComboBox *searchEngine_;
    QCheckBox *restoreSession_;
    QCheckBox *compactMode_;
    QCheckBox *adBlock_;
    QCheckBox *suspendOnMinimize_;
    QLabel *adBlockStatus_;
};
