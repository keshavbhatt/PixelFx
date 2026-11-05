#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QCheckBox;
class QVBoxLayout;

namespace pixelfx::ui {

class ListRow;

/// An ordered list of steps that can be switched off and reordered (the look editor's steps):
/// each row has a drag handle, a check box, an icon, a name with a summary under it, and a menu
/// button. Dragging the handle moves the row live; the move is reported once it is let go.
class StepList : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StepList)

public:
    struct Row
    {
        QString glyph;
        QString title;
        QString subtitle;
        bool on = true;
    };

    explicit StepList(QWidget* parent = nullptr);
    ~StepList() override;

    void setRows(const QList<Row>& rows);
    void setSubtitle(int row, const QString& subtitle);
    [[nodiscard]] int count() const { return static_cast<int>(m_rows.size()); }
    /// The height that shows every row, up to `rows` of them; past that, half of the next row
    /// shows too, so the list reads as one that scrolls.
    [[nodiscard]] int heightForRows(int rows) const;
    [[nodiscard]] int current() const { return m_current; }
    /// Selects `row` (or nothing with -1) and reports it.
    void setCurrent(int row);
    [[nodiscard]] bool isOn(int row) const;
    /// Switches `row` on or off as its check box would.
    void setOn(int row, bool on);
    /// Moves `from` to `to` as a drag would, and reports it.
    void moveRow(int from, int to);

Q_SIGNALS:
    void currentChanged(int row);
    void toggled(int row, bool on);
    void moved(int from, int to);
    /// The row's menu button was pressed; `at` is where the menu opens, in global coordinates.
    void menuRequested(int row, const QPoint& at);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Entry
    {
        ListRow* row;
        QWidget* handle;
        QCheckBox* check;
    };

    [[nodiscard]] int indexOf(const QObject* part) const;
    void place(int from, int to);

    QVBoxLayout* m_column;
    QList<Entry> m_rows;
    int m_current = -1;
    int m_dragFrom = -1;
    int m_dragAt = -1;
};

} // namespace pixelfx::ui
