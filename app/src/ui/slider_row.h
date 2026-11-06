#pragma once

#include <QWidget>

#include <functional>
#include <optional>

class QLabel;
class QLineEdit;
class QSlider;

namespace pixelfx::ui {

/// A labelled slider: the name and the value above the track. The value shows in the accent
/// while it differs from the default, and a double-click on the track goes back to the
/// default. It works in whole ticks; how a tick reads ("+0.35", "4%") is set with setFormat.
/// A click on the value (or Enter on the slider) turns it into a field to type a value in the
/// unit shown; setTicksPerUnit says how many ticks make one of those units.
class SliderRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SliderRow)

public:
    explicit SliderRow(const QString& label, QWidget* parent = nullptr);
    ~SliderRow() override;

    void setRange(int minimum, int maximum, int defaultTicks);
    void setFormat(std::function<QString(int)> format);
    /// A typed number is in a unit of its own (Kelvin): this turns it into the slider's unit.
    void setTypedUnit(std::function<double(double)> toUnit) { m_typedUnit = std::move(toUnit); }
    void setTicksPerUnit(double ticks);
    [[nodiscard]] int ticks() const;
    /// Shows `ticks` without emitting.
    void setTicks(int ticks);
    [[nodiscard]] int defaultTicks() const { return m_default; }
    [[nodiscard]] bool isChanged() const;
    [[nodiscard]] QString text() const;
    /// The full name: what screen readers hear and history steps say ("Sharpening detail").
    [[nodiscard]] const QString& label() const { return m_label; }
    /// A full name for a row whose shown label is short because a sub-heading gives the rest
    /// ("Detail" under "Sharpening").
    void setFullName(const QString& name);
    /// What the slider does, for its tooltip in place of its name.
    void setHelp(const QString& text);
    /// No room above the name, for a slider that is a form's field.
    void setFlush();
    [[nodiscard]] QSlider* slider() const { return m_slider; }
    /// The field the value is typed in; hidden unless typing.
    [[nodiscard]] QLineEdit* valueField() const { return m_field; }
    /// Turns the value into a field with the number selected.
    void beginTyping();
    /// The ticks a typed value stands for, held in the range, or nothing if it is not a number.
    [[nodiscard]] std::optional<int> parse(const QString& typed) const;

Q_SIGNALS:
    /// Every change, live while dragging.
    void ticksMoved(int ticks);
    /// A change is complete: the drag ended, or a key, a click on the track or a reset.
    void ticksReleased(int ticks);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// Off (disabled), the row is dimmed as a whole: its name, value and track keep their colours.
    void changeEvent(QEvent* event) override;

private:
    void refresh();
    void endTyping(bool keep);
    bool fieldEvent(QEvent* event);

    QString m_label;
    int m_default = 0;
    std::function<QString(int)> m_format;
    double m_ticksPerUnit = 1.0;
    std::function<double(double)> m_typedUnit;
    QSlider* m_slider;
    QLabel* m_name;
    QLabel* m_value;
    QLineEdit* m_field;
};

} // namespace pixelfx::ui
