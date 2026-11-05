#pragma once

#include <QPushButton>

namespace pixelfx::ui {

/// A push button with an icon and text kGap apart. The style sheet draws the button and Qt
/// leaves only 4 px between icon and text, so this button draws its own label. A button with
/// only an icon or only text looks like any other.
class IconTextButton : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IconTextButton)

public:
    IconTextButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);
    ~IconTextButton() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    static constexpr int kGap = 6;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] bool hasIconAndText() const;
    [[nodiscard]] int extraWidth() const;
};

} // namespace pixelfx::ui
