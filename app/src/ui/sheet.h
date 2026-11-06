#pragma once

#include <QDialog>
#include <QPointer>
#include <QScrollArea>

class QVBoxLayout;

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
        Editor,  ///< 980 by 640 px at least, resizable from its edges: controls beside a preview
    };
    /// Gives the sheet its size; call once its content is built. Sheets other than Editor then
    /// follow their content's height as it changes, no taller than the window they cover.
    void setSize(Size size);
    /// Makes the sheet as tall as its content at its width, within the window it covers.
    void fitHeight();
    /// The tallest the sheet may be: the window under it, less a margin.
    [[nodiscard]] int heightLimit() const;

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    /// The edges of the sheet within reach of `global` (a point on the screen), for resizing.
    [[nodiscard]] Qt::Edges edgesAt(const QPoint& global) const;
    void showResizeCursor(Qt::Edges edges);

    QPointer<QWidget> m_scrim;
    bool m_resizable = false;
    bool m_followsContent = false;
    bool m_fitting = false;
    Qt::Edges m_cursorEdges;
};

/// The part of a sheet that scrolls when the sheet is held shorter than its content (a short
/// window): as tall as its content otherwise, so the sheet shows all of it.
class SheetScroll : public QScrollArea
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SheetScroll)

public:
    /// Scrolls `content`, which it takes over. `edge` is the sheet's padding on the right, laid
    /// inside the scroll area so the scroll bar takes it and the content keeps its width.
    SheetScroll(QWidget* content, int edge, QWidget* parent);
    ~SheetScroll() override;

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// Adds `heading` (the title row) to `column` (the body of sheetLayout) and `content` under it
/// in a SheetScroll: the content scrolls in a window too short for it while the title and the
/// buttons stay, its scroll bar in the sheet's right padding.
void addScrollingContent(QVBoxLayout* column, QLayout* heading, QLayout* content, QWidget* sheet);

} // namespace pixelfx::ui
