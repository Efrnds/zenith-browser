#pragma once
#include "core/BrowserData.h"
#include <QDialog>

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QAction;

class LibraryPage final : public QDialog
{
    Q_OBJECT
public:
    LibraryPage(BrowserData *data, bool bookmarks, QWidget *parent = nullptr);
    void refresh();
signals:
    void openRequested(const QUrl &url, bool foreground);
private:
    void load(bool reset);
    void requestChunk(int generation, int offset);
    void updateActions();
    void openSelected(bool foreground);
    void removeSelected();
    void showEditor();
    void saveEditor();
    BrowserData *data_;
    bool bookmarks_, busy_ = false, reset_ = true;
    int generation_ = 0, offset_ = 0, target_ = 100, scroll_ = 0;
    QString query_, selected_;
    QList<BrowserData::Entry> pending_;
    QListWidget *list_;
    QLineEdit *search_, *title_, *folder_;
    QLabel *status_, *empty_, *editStatus_;
    QWidget *editor_;
    QPushButton *remove_, *edit_, *more_, *retry_;
    QAction *foreground_, *background_;
};
