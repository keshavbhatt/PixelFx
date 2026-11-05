#pragma once

#include <QLayout>
#include <QList>

namespace pixelfx::ui {

/// Lays its items out left to right and wraps onto the next row when the width runs out.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = 6);
    ~FlowLayout() override;
    Q_DISABLE_COPY_MOVE(FlowLayout)

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override { return static_cast<int>(m_items.size()); }
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    [[nodiscard]] Qt::Orientations expandingDirections() const override { return {}; }
    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override;
    void setGeometry(const QRect& rect) override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSize() const override;

private:
    /// Places the items in `rect` (when `apply`); returns the height they take.
    [[nodiscard]] int arrange(const QRect& rect, bool apply) const;

    QList<QLayoutItem*> m_items;
    int m_spacing;
};

} // namespace pixelfx::ui
