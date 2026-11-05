#pragma once

#include <QWidget>

class QHBoxLayout;
class QLabel;
class QToolButton;

namespace pixelfx::ui {

/// One row of a list: an icon, a title with a small line under it, and an optional action
/// button at the right end. Hover and selection are drawn by the style sheet ("listRow").
/// Extra controls (a check box, a drag handle) go before the icon with addLeading.
class ListRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ListRow)

public:
    explicit ListRow(QWidget* parent = nullptr);
    ~ListRow() override;

    void setIcon(const QString& glyph);
    void setTitle(const QString& title);
    /// The line under the title; cut with "..." when it does not fit.
    void setSubtitle(const QString& subtitle);
    /// Adds the action button at the right end (for example a × that removes the row).
    void setAction(const QString& glyph, const QString& tip);
    void addLeading(QWidget* widget);
    void setSelected(bool selected);
    [[nodiscard]] bool isSelected() const { return m_selected; }
    [[nodiscard]] QString title() const;
    [[nodiscard]] QToolButton* actionButton() const { return m_action; }

Q_SIGNALS:
    void clicked();
    void actionClicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QHBoxLayout* m_row;
    QLabel* m_icon;
    QLabel* m_title;
    QLabel* m_subtitle;
    QToolButton* m_action = nullptr;
    bool m_selected = false;
    int m_leading = 0;
};

} // namespace pixelfx::ui
