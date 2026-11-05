#pragma once

#include <QWidget>

class QTimer;

namespace pixelfx::ui {

/// A small turning ring for work that does not say how far it is (the full-size picture
/// loading). Turns only while shown; stands still, three quarters drawn, when the desktop asks
/// for no animation (animationsWanted).
class BusyRing : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BusyRing)

public:
    explicit BusyRing(QWidget* parent = nullptr);
    ~BusyRing() override;

    [[nodiscard]] QSize sizeHint() const override { return {kSize, kSize}; }
    static constexpr int kSize = 14;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QTimer* m_timer;
    int m_angle = 0;
};

} // namespace pixelfx::ui
