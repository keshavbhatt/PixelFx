#pragma once

#include "core/histogram.h"

#include <QWidget>

namespace pixelfx::ui {

/// The histogram well at the top of the adjust panel (mock editor.html): red, green and blue
/// as translucent areas, square-root scaled so dark and light tones both stay visible.
class HistogramView : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HistogramView)

public:
    explicit HistogramView(QWidget* parent = nullptr);
    ~HistogramView() override;

    void setHistogram(const core::Histogram& histogram);
    [[nodiscard]] const core::Histogram& histogram() const { return m_histogram; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::Histogram m_histogram;
};

} // namespace pixelfx::ui
