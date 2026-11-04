#pragma once

#include <QRectF>
#include <QSize>

#include <pfx/settings.h>

// The geometry settings (DOCS/presets.md, group "geometry") as the Crop mode edits them. The
// library applies quarter turns, then flips, then straightening, then the crop; these helpers
// keep that order invisible: a turn or a flip acts on the photo as it is shown.
namespace pixelfx::core {

/// The crop as fractions of the straightened photo; (0, 0, 1, 1) when there is none.
[[nodiscard]] QRectF cropOf(const pfx::Settings& settings);
/// Stores `crop`, or removes the crop keys when it covers the whole photo.
void setCrop(pfx::Settings& settings, const QRectF& crop);
[[nodiscard]] pfx::Settings withoutCrop(const pfx::Settings& settings);
/// Only the geometry group: what the original needs to line up with the edit.
[[nodiscard]] pfx::Settings geometryOnly(const pfx::Settings& settings);

/// The size of the rendered photo for `photo` (its upright size), and the size before the crop.
[[nodiscard]] QSize outputSize(const pfx::Settings& settings, const QSize& photo);
[[nodiscard]] QSize uncroppedSize(const pfx::Settings& settings, const QSize& photo);

/// Turns the shown photo a quarter, crop included.
void turn(pfx::Settings& settings, bool clockwise);
/// Mirrors the shown photo left to right (Qt::Horizontal) or top to bottom, crop included.
void flip(pfx::Settings& settings, Qt::Orientation orientation);
/// Straightening in degrees, positive turns the photo clockwise.
void setAngle(pfx::Settings& settings, double degrees);

/// The largest crop of `ratio` (width over height, in pixels) inside a photo of `size`,
/// centred where `crop` is centred. A ratio of 0 or less returns `crop`.
[[nodiscard]] QRectF fitRatio(const QRectF& crop, const QSize& size, double ratio);
/// `crop` brought to `ratio` keeping its width and centre where it can (after straightening
/// changed the photo's shape).
[[nodiscard]] QRectF conformRatio(const QRectF& crop, const QSize& size, double ratio);

/// `crop` turned from landscape to portrait or back about its centre (the X key with a free
/// ratio), shrunk to fit.
[[nodiscard]] QRectF swapCrop(const QRectF& crop, const QSize& size);

/// What a drag on the crop box moves.
enum class CropHandle
{
    None,
    Move,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

/// The box `start`, inside `bounds` (both in one pixel space), after dragging `handle` by
/// `delta`. With `ratio` above 0 the box keeps that width over height. The box never gets
/// smaller than `minimum` on either side.
[[nodiscard]] QRectF dragCrop(const QRectF& start, CropHandle handle, QPointF delta, const QRectF& bounds,
                              double ratio, double minimum);

} // namespace pixelfx::core
