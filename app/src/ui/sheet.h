#pragma once

#include <QDialog>
#include <QPointer>

namespace pixelfx::ui {

/// A dialog drawn as a sheet, without the system's title bar: a rounded panel with a 1 px
/// border over its dimmed parent window. Dragging an empty part of it moves it; Escape and the
/// close button of its title row (sheetCloseButton) close it.
class Sheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Sheet)

public:
    explicit Sheet(QWidget* parent = nullptr);
    ~Sheet() override;

    static constexpr int kRadius = 14;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QPointer<QWidget> m_scrim;
};

} // namespace pixelfx::ui
