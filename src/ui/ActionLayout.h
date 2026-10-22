#pragma once
#include "ui/UiMetrics.h"
#include <QLayout>
#include <QVector>

class ActionLayout final : public QLayout
{
public:
    explicit ActionLayout() { setContentsMargins(0, 0, 0, 0); setSpacing(UiMetrics::Small); }
    ~ActionLayout() override { while (auto *item = takeAt(0)) delete item; }
    void addItem(QLayoutItem *item) override { items_.append(item); }
    int count() const override { return items_.size(); }
    QLayoutItem *itemAt(int index) const override { return items_.value(index); }
    QLayoutItem *takeAt(int index) override {
        return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), false); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size;
        for (auto *item : items_) if (!item->isEmpty()) size = size.expandedTo(item->minimumSize());
        return size;
    }
    void setGeometry(const QRect &rect) override { QLayout::setGeometry(rect); arrange(rect, true); }
private:
    int arrange(const QRect &rect, bool apply) const {
        int x = rect.x(), y = rect.y(), lineHeight = 0;
        for (auto *item : items_) {
            if (item->isEmpty()) continue;
            const QSize size = item->sizeHint().expandedTo(item->minimumSize());
            if (x > rect.x() && x + size.width() > rect.x() + rect.width()) {
                x = rect.x();
                y += lineHeight + spacing();
                lineHeight = 0;
            }
            if (apply) item->setGeometry(QRect(QPoint(x, y), size));
            x += size.width() + spacing();
            lineHeight = qMax(lineHeight, size.height());
        }
        return y - rect.y() + lineHeight;
    }
    QVector<QLayoutItem *> items_;
};
