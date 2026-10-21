#pragma once
#include <QStyledItemDelegate>
#include <QWidget>

class QVBoxLayout;
class QLabel;
namespace PageWidgets {
QVBoxLayout *section(QVBoxLayout *parent, const QString &title, const QString &description = {});
void heading(QVBoxLayout *layout, const QString &title, const QString &description);
QLabel *emptyState(QWidget *parent, const QString &title, const QString &description);
}

class DetailListDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class ThemePreview final : public QWidget
{
public:
    explicit ThemePreview(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};
