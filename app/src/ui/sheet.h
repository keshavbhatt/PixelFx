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

    /// The sheet sizes: widths for sheets as tall as their content, and one fixed size for the
    /// sheets that are mostly a live preview (Watermark, the look editor).
    enum class Size
    {
        Narrow,  ///< 440 px: a short question (Paste, Custom size)
        Regular, ///< 560 px: a form (Export, Save as preset, About)
        Wide,    ///< 820 px: two columns or a sidebar (Settings, Keyboard shortcuts)
        Editor,  ///< 980 by 640 px: controls beside a preview
    };
    /// Gives the sheet its size; call once its content is built.
    void setSize(Size size);
    /// Makes the sheet as tall as its content at its width (after the content changes).
    void fitHeight();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QPointer<QWidget> m_scrim;
};

} // namespace pixelfx::ui
