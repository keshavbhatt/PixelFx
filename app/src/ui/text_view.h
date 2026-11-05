#pragma once

#include <QTextBrowser>

namespace pixelfx::ui {

/// Rich text that is as tall as its text, up to `maximumTextHeight`, then scrolls (What's new).
class TextView : public QTextBrowser
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TextView)

public:
    explicit TextView(int maximumTextHeight, QWidget* parent = nullptr);
    ~TextView() override;

    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;

private:
    int m_maximum;
};

} // namespace pixelfx::ui
