#pragma once

#include <QLabel>

namespace pixelfx::ui {

/// A one-line label that ends its text with "..." when it is narrower than the text. It asks for
/// no width of its own.
class ElidedLabel : public QLabel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ElidedLabel)

public:
    explicit ElidedLabel(QWidget* parent = nullptr);
    ~ElidedLabel() override;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace pixelfx::ui
