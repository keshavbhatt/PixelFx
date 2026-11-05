#pragma once

#include <QColor>
#include <QDate>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QPainter;

namespace pfx {
struct ExifSummary;
}

// Watermarks (FEATURES X10): a line of text or a logo laid on exported
// photos, never on the editor view. Size and margin are shares of the photo's long edge, so a
// whole batch of portrait, square and landscape photos gets the same-looking mark.
namespace pixelfx::core {

struct Watermark
{
    enum class Kind
    {
        Text,
        Logo,
    };
    enum class ColorMode
    {
        Auto, ///< light or dark per photo, from the area under the mark
        White,
        Black,
        Custom,
    };

    QString name;
    Kind kind = Kind::Text;
    QString text = QStringLiteral("© {year}");
    QString logo; ///< a PNG or SVG, inside the watermarks folder once saved
    QString font; ///< family; empty: the system's sans
    ColorMode colorMode = ColorMode::Auto;
    QColor color = QColor(Qt::white); ///< with Custom
    double size = 0.03;               ///< the mark's height, share of the long edge (0.01 to 0.2)
    double opacity = 0.7;             ///< 0 to 1
    double margin = 0.02;             ///< from the photo's edges, share of the long edge (0 to 0.1)
    int anchor = 8;                   ///< 0 to 8, rows of three from the top left; 8 is bottom right
    std::optional<QPointF> position;  ///< dragged freely: the mark's centre, shares of width and height
    bool repeat = false;              ///< tiled across the photo, for proofs
    double angle = -30.0;             ///< degrees, with repeat
    bool shadow = true;               ///< a soft shadow in the opposite tone

    bool operator==(const Watermark&) const = default;
};

/// What the text's tokens fill in with, per photo.
struct WatermarkTokens
{
    QDate date;     ///< taken, or today when the photo does not say
    QString camera; ///< "Nikon Z6", empty when unknown
};

[[nodiscard]] WatermarkTokens tokensFrom(const pfx::ExifSummary& exif, const QDate& fallback);
/// {year}, {date} (ISO) and {camera} filled in.
[[nodiscard]] QString expandTokens(const QString& text, const WatermarkTokens& tokens);

/// Mean linear luminance of a rectangle of the photo (in the photo's own pixels).
using LuminanceUnder = std::function<double(const QRect&)>;
/// The same for an 8-bit sRGB picture (previews).
[[nodiscard]] double meanLuminance(const QImage& image, const QRect& rect);
/// Auto's choice: white over dark areas, black over light ones; the tone with the higher
/// contrast ratio against the mean.
[[nodiscard]] QColor autoColor(double luminance);

/// The mark's rectangles on a photo of `photo` pixels, for a mark of `mark` pixels (one, or a
/// grid when repeating; the grid's rectangles are before the turn by `angle`).
[[nodiscard]] std::vector<QRectF> placements(const Watermark& w, const QSizeF& photo, const QSizeF& mark);

/// Draws the mark on a painter whose device covers a photo of `photo` pixels.
void paintWatermark(QPainter& painter, const Watermark& w, const QSize& photo, const WatermarkTokens& tokens,
                    const LuminanceUnder& luminance);

/// The mark alone, on a transparent picture covering only the part of the photo it touches.
struct WatermarkOverlay
{
    QImage image; ///< RGBA8888 premultiplied, sRGB
    QPoint origin;
};
[[nodiscard]] WatermarkOverlay renderWatermark(const Watermark& w, const QSize& photo,
                                               const WatermarkTokens& tokens,
                                               const LuminanceUnder& luminance);

/// The .pfxwatermark file (presets.md section 7), version 1.
[[nodiscard]] QByteArray watermarkToJson(const Watermark& w);
[[nodiscard]] std::optional<Watermark> watermarkFromJson(const QByteArray& json, QString* error = nullptr);

/// Saved watermarks, one .pfxwatermark each in a folder (<AppData>/watermarks), logos copied
/// next to them so a watermark keeps working when the original logo file moves.
class WatermarkLibrary
{
public:
    explicit WatermarkLibrary(QString folder);
    [[nodiscard]] static QString defaultFolder();
    [[nodiscard]] const QString& folder() const { return m_folder; }

    struct Entry
    {
        QString path;
        Watermark watermark;
    };
    /// Sorted by name.
    [[nodiscard]] std::vector<Entry> entries() const;
    /// Saves the watermark: into `path` when given (editing a saved one), otherwise a new file
    /// named after it that replaces nothing; copies an outside logo in. The path, or empty.
    [[nodiscard]] QString save(const Watermark& w, const QString& path = {}) const;
    [[nodiscard]] static std::optional<Watermark> load(const QString& path);
    [[nodiscard]] bool remove(const QString& path) const;

private:
    QString m_folder;
};

} // namespace pixelfx::core
