#pragma once

#include <QWidget>

class QFrame;

namespace pixelfx::ui {

/// First run and nothing open (mock welcome.html): the drop zone and Open images. Open folder,
/// Batch edit and Recent arrive with the features behind them (M3 to M5).
class WelcomePage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WelcomePage)

public:
    explicit WelcomePage(QWidget* parent = nullptr);
    /// Highlights the drop zone while files are dragged over the window.
    void setDragActive(bool active);

Q_SIGNALS:
    void openRequested();

private:
    QFrame* m_dropZone = nullptr;
};

} // namespace pixelfx::ui
