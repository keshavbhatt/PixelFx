#pragma once

#include <QWidget>

#include <functional>

class QLabel;
class QSlider;

namespace pixelfx::ui {

/// A labelled slider: the name and the value above the track. The value shows in the accent
/// while it differs from the default, and a double-click on the track goes back to the
/// default. It works in whole ticks; how a tick reads ("+0.35", "4%") is set with setFormat.
class SliderRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SliderRow)

public:
    explicit SliderRow(const QString& label, QWidget* parent = nullptr);
    ~SliderRow() override;

    void setRange(int minimum, int maximum, int defaultTicks);
    void setFormat(std::function<QString(int)> format);
    [[nodiscard]] int ticks() const;
    /// Shows `ticks` without emitting.
    void setTicks(int ticks);
    [[nodiscard]] int defaultTicks() const { return m_default; }
    [[nodiscard]] bool isChanged() const;
    [[nodiscard]] QString text() const;
    [[nodiscard]] const QString& label() const { return m_label; }
    [[nodiscard]] QSlider* slider() const { return m_slider; }

Q_SIGNALS:
    /// Every change, live while dragging.
    void ticksMoved(int ticks);
    /// A change is complete: the drag ended, or a key, a click on the track or a reset.
    void ticksReleased(int ticks);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();

    QString m_label;
    int m_default = 0;
    std::function<QString(int)> m_format;
    QSlider* m_slider;
    QLabel* m_name;
    QLabel* m_value;
};

} // namespace pixelfx::ui
