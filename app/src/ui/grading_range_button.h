#pragma once

#include <QAbstractButton>
#include <QImage>

namespace pixelfx::ui {

/// One colour grading range to choose (Shadows, Midtones, Highlights, Global): a small wheel
/// showing the range's colour, its name under it. Checkable and exclusive in its row, so the
/// arrow keys move between ranges as between radio buttons.
class GradingRangeButton : public QAbstractButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GradingRangeButton)

public:
    GradingRangeButton(const QString& name, QWidget* parent = nullptr);
    ~GradingRangeButton() override;

    /// Shows the range's colour on the small wheel.
    void setColor(double hue, double amount);
    [[nodiscard]] QSize sizeHint() const override;

    static constexpr int kWheel = 44;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_name;
    double m_hue = 0.0;
    double m_amount = 0.0;
    QImage m_disc;
};

} // namespace pixelfx::ui
