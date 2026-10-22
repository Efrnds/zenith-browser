#pragma once

#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>
#include <QWidget>

class QTabBar;
class QEnterEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QResizeEvent;
class QShowEvent;
class QToolButton;
class TabItemWidget;

class Sidebar final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int slideOffset READ slideOffset WRITE setSlideOffset)

public:
    struct TabVisual {
        int id = -1;
        int workspace = 0;
        int folderId = -1;
        QString title;
        QIcon icon;
        bool discarded = false;
        bool essential = false;
        int position = 0;
    };

    struct FolderVisual {
        int id = -1;
        int workspace = 0;
        QString name;
        int position = 0;
    };

    explicit Sidebar(QWidget *parent = nullptr);

    [[nodiscard]] int slideOffset() const;
    void setSlideOffset(int offset);

    void addTab(const TabVisual &tab);
    void updateTab(const TabVisual &tab);
    void removeTab(int id);
    void selectTab(int id);
    void setWorkspace(int workspace);
    void setWorkspaces(const QStringList &names);
    void setFolders(const QList<FolderVisual> &folders);
    [[nodiscard]] int preferredWidth() const;

signals:
    void newTabRequested();
    void reopenTabRequested();
    void tabActivated(int id);
    void closeTabRequested(int id);
    void workspaceSelected(int workspace);
    void createWorkspaceRequested();
    void workspaceActionRequested(int action);
    void splitTabRequested(int id);
    void tabEssentialRequested(int id, bool essential);
    void tabFolderRequested(int id, int folderId);
    void tabWorkspaceRequested(int id, int workspace);
    void tabMoveRequested(int id, int delta);
    void tabDropRequested(int source, int target);
    void duplicateTabRequested(int id);
    void muteTabRequested(int id);
    void closeOtherTabsRequested(int id);
    void createFolderRequested(const QString &name);
    void folderEditRequested(int id);
    void renameFolderRequested(int id, const QString &name);
    void removeFolderRequested(int id);
    void folderMoveRequested(int id, int delta);
    void folderDropRequested(int source, int target);
    void closeFollowingTabsRequested(int id);
    void pointerEntered();
    void pointerLeft();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void scheduleRebuild();
    void rebuild();
    void rebuildEssentials();
    void rebuildTree();
    void relayoutTabRows();
    void toggleFolder(int id);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showTabMenu(int id, const QPoint &globalPosition);
    void showFolderMenu(int id, const QPoint &globalPosition);
    void showWorkspaceMenu(const QPoint &globalPosition);
    [[nodiscard]] TabItemWidget *makeTabWidget(const TabVisual &tab, QWidget *parent);

    QWidget *body_ = nullptr;
    int slideOffset_ = 0;
    QLabel *essentialsLabel_;
    QLabel *sectionLabel_;
    QToolButton *workspaceMenuButton_;
    QPushButton *newTabButton_;
    QTabBar *workspaceTabs_;
    QStringList workspaceNames_;
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
    bool rebuildPending_ = false;
    int selectedTabId_ = -1;
    int currentWorkspace_ = 0;
};
