#pragma once

#include <QBrush>
#include <QToolButton>

namespace pixelfx::ui {

/// A round, checkable colour swatch: a colour mixer band, a watermark colour. Checked, it has a
/// ring in the text colour.
class ColorSwatch : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ColorSwatch)

public:
    /// `fill` is a colour or a gradient (the "any colour" swatch); `name` is its tooltip.
    ColorSwatch(const QBrush& fill, const QString& name, QWidget* parent = nullptr);
    ~ColorSwatch() override;

    /// A brush of every hue around the circle, for a swatch that opens a colour picker.
    [[nodiscard]] static QBrush allHues();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QBrush m_fill;
};

} // namespace pixelfx::ui
