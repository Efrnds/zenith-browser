#pragma once

#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QEnterEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QToolButton;
class TabItemWidget;

class Sidebar final : public QWidget
{
    Q_OBJECT

public:
    struct TabVisual {
        int id = -1;
        int workspace = 0;
        int folderId = -1;
        QString title;
        QIcon icon;
        bool discarded = false;
        bool essential = false;
    };

    struct FolderVisual {
        int id = -1;
        int workspace = 0;
        QString name;
    };

    explicit Sidebar(QWidget *parent = nullptr);

    void addTab(const TabVisual &tab);
    void updateTab(const TabVisual &tab);
    void removeTab(int id);
    void selectTab(int id);
    void setWorkspace(int workspace);
    void setFolders(const QList<FolderVisual> &folders);
    void setAdBlockEnabled(bool enabled);
    void setCompactEnabled(bool enabled);
    void setSearchTemplate(const QString &searchTemplate);
    [[nodiscard]] int preferredWidth() const;

signals:
    void newTabRequested();
    void tabActivated(int id);
    void closeTabRequested(int id);
    void workspaceSelected(int workspace);
    void tabEssentialRequested(int id, bool essential);
    void tabFolderRequested(int id, int folderId);
    void createFolderRequested(const QString &name);
    void renameFolderRequested(int id, const QString &name);
    void removeFolderRequested(int id);
    void adBlockToggled(bool enabled);
    void compactModeToggled(bool enabled);
    void searchTemplateChanged(const QString &searchTemplate);
    void pointerEntered();
    void pointerLeft();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void rebuild();
    void rebuildEssentials();
    void rebuildTree();
    void relayoutTabRows();
    void toggleFolder(int id);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showTabMenu(int id, const QPoint &globalPosition);
    void showFolderMenu(int id, const QPoint &globalPosition);
    void showPreferencesMenu();
    [[nodiscard]] TabItemWidget *makeTabWidget(const TabVisual &tab, QWidget *parent);

    QLabel *brandLabel_;
    QLabel *essentialsLabel_;
    QLabel *sectionLabel_;
    QToolButton *collapseButton_;
    QToolButton *folderButton_;
    QToolButton *settingsButton_;
    QPushButton *newTabButton_;
    QButtonGroup *workspaceButtons_;
    QListWidget *essentialsList_;
    QScrollArea *tabScroll_;
    QWidget *tabHost_;
    QList<QWidget *> tabRows_;
    QHash<int, TabVisual> tabs_;
    QHash<int, FolderVisual> folders_;
    QHash<int, bool> folderExpanded_;
    QHash<int, QListWidgetItem *> essentialItems_;
    QHash<int, TabItemWidget *> widgets_;
    bool layingOutTabs_ = false;
    bool adBlockEnabled_ = true;
    bool compactEnabled_ = false;
    QString searchTemplate_;
    int currentWorkspace_ = 0;
};
