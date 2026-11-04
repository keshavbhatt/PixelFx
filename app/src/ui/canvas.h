#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

namespace pixelfx::ui {

/// The photo, fitted to the widget on the canvas colour (DESIGN section 2). Shows a status line
/// while opening and the reason when opening failed. Zoom and pan arrive with M3.
class Canvas : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Canvas)

public:
    explicit Canvas(QWidget* parent = nullptr);
    ~Canvas() override;

    /// `fullSize` is the photo's real size: the fit view never enlarges past 100% of it.
    void setImage(const QImage& image, const QSize& fullSize);
    /// Swaps the shown pixels (same photo: original or edited) without touching the full size.
    void showImage(const QImage& image);
    void setStatus(const QString& text);
    void setError(const QString& text);
    [[nodiscard]] const QImage& image() const { return m_image; }
    /// Where the photo is drawn, in widget coordinates.
    [[nodiscard]] QRect photoRect() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
    QSize m_fullSize;
    QString m_status;
    QString m_error;
};

} // namespace pixelfx::ui
