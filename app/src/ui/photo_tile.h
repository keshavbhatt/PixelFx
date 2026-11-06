#pragma once

#include <QImage>
#include <QWidget>

#include <vector>

class QGridLayout;

namespace pixelfx::ui {

/// A look or a preset as a picture of the photo with it, its name under it on one line (cut
/// with "..." and complete in the tooltip), an optional badge, and a ring when selected.
/// Used by the Looks and Presets tabs, so both browse the same way.
class PhotoTile : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PhotoTile)

public:
    enum class Badge
    {
        None,
        Mine,     // the user's own look
        Favorite, // a preset or a look in Favorites
    };

    PhotoTile(QString id, QString name, Badge badge, QWidget* parent = nullptr);
    ~PhotoTile() override;

    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] const QString& name() const { return m_name; }
    void setImage(const QImage& image);
    [[nodiscard]] bool hasImage() const { return !m_image.isNull(); }
    void setSelected(bool selected);
    [[nodiscard]] bool isSelected() const { return m_selected; }
    void setBadge(Badge badge);
    /// A longer line for the tooltip, under the name ("Warm skin, soft greens").
    void setDescription(const QString& description);
    /// Lets the name take two lines, for names whose end tells similar ones apart
    /// ("Portrait 400, pushed twice"); one line by default.
    void setTwoLineName(bool on);
    /// The bar along the bottom of the picture while a preview waits: 0 to 1, or below 0 for none.
    void setWaiting(double progress);

    static constexpr int kImageHeight = 77;
    static constexpr int kHeight = kImageHeight + 24;
    static constexpr int kTwoLineHeight = kImageHeight + 40;

Q_SIGNALS:
    void clicked();
    void menuRequested(const QPoint& globalPos);
    /// The pointer came onto the tile, or the keyboard focus did (true), or left it.
    void hovered(bool inside);
    /// F on a focused tile.
    void favoriteRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void paintBadge(QPainter& p, const QRectF& box) const;
    void refreshText();

    QString m_id;
    QString m_name;
    QString m_description;
    Badge m_badge = Badge::None;
    bool m_selected = false;
    bool m_twoLines = false;
    double m_waiting = -1.0;
    QImage m_image;
};

/// `tiles` in `columns` that share the width (two in the Looks and Presets tabs); blanks after
/// the last tiles keep them as wide as the others.
[[nodiscard]] QGridLayout* tileGrid(const std::vector<PhotoTile*>& tiles, QWidget* parent, int columns = 2);

} // namespace pixelfx::ui
