#include "core/crop_geometry.h"

#include <algorithm>
#include <cmath>
#include <pfx/geometry.h>

namespace pixelfx::core {

namespace {

constexpr const char* kRotate = "geometry.rotate90";
constexpr const char* kFlipH = "geometry.flipH";
constexpr const char* kFlipV = "geometry.flipV";
constexpr const char* kAngle = "geometry.angle";
constexpr const char* kCropX = "geometry.crop.x";
constexpr const char* kCropY = "geometry.crop.y";
constexpr const char* kCropW = "geometry.crop.w";
constexpr const char* kCropH = "geometry.crop.h";
constexpr const char* kVertical = "geometry.perspective.vertical";
constexpr const char* kHorizontal = "geometry.perspective.horizontal";
constexpr const char* kKeep = "geometry.perspective.keep";
constexpr std::array<const char*, 4> kCorners = {
    "geometry.perspective.corners.topLeft", "geometry.perspective.corners.topRight",
    "geometry.perspective.corners.bottomRight", "geometry.perspective.corners.bottomLeft"};

void setOrEraseNumber(pfx::Settings& settings, const char* key, double value)
{
    if (value == 0.0) {
        settings.erase(key);
    } else {
        settings.set(key, value);
    }
}
constexpr double kEpsilon = 1e-9;

void setOrErase(pfx::Settings& settings, const char* key, bool on)
{
    if (on) {
        settings.set(key, true);
    } else {
        settings.erase(key);
    }
}

/// `w` by `h` pixels centred on `centre`, moved inside a photo of `size`, as fractions.
QRectF placed(QPointF centre, double w, double h, const QSize& size)
{
    const double width = size.width();
    const double height = size.height();
    const double x = std::clamp(centre.x() - (w / 2), 0.0, width - w);
    const double y = std::clamp(centre.y() - (h / 2), 0.0, height - h);
    return {x / width, y / height, w / width, h / height};
}

} // namespace

QRectF cropOf(const pfx::Settings& settings)
{
    return {settings.number(kCropX, 0.0), settings.number(kCropY, 0.0), settings.number(kCropW, 1.0),
            settings.number(kCropH, 1.0)};
}

void setCrop(pfx::Settings& settings, const QRectF& crop)
{
    const QRectF c = crop.intersected(QRectF(0, 0, 1, 1));
    const bool whole = std::abs(c.x()) < kEpsilon && std::abs(c.y()) < kEpsilon &&
                       std::abs(c.width() - 1) < kEpsilon && std::abs(c.height() - 1) < kEpsilon;
    if (whole || c.isEmpty()) {
        for (const char* key : {kCropX, kCropY, kCropW, kCropH}) {
            settings.erase(key);
        }
        return;
    }
    settings.set(kCropX, c.x());
    settings.set(kCropY, c.y());
    settings.set(kCropW, c.width());
    settings.set(kCropH, c.height());
}

pfx::Settings withoutCrop(const pfx::Settings& settings)
{
    pfx::Settings s = settings;
    setCrop(s, QRectF(0, 0, 1, 1));
    return s;
}

pfx::Settings withoutGeometry(const pfx::Settings& settings)
{
    pfx::Settings out;
    for (const std::string& key : settings.keys()) {
        if (pfx::groupOf(key) != "geometry") {
            out.set(key, *settings.find(key));
        }
    }
    return out;
}

pfx::Settings geometryOnly(const pfx::Settings& settings)
{
    return settings.onlyGroups({"geometry"});
}

Corners ownCorners()
{
    return {{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}};
}

Corners cornersOf(const pfx::Settings& settings)
{
    Corners out = ownCorners();
    for (std::size_t i = 0; i < 4; ++i) {
        const std::string key = kCorners.at(i);
        out.at(i) = {settings.number(key + ".x", out.at(i).x), settings.number(key + ".y", out.at(i).y)};
    }
    return out;
}

bool hasCorners(const pfx::Settings& settings)
{
    const Corners own = ownCorners();
    const Corners set = cornersOf(settings);
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::abs(set.at(i).x - own.at(i).x) > kEpsilon ||
            std::abs(set.at(i).y - own.at(i).y) > kEpsilon) {
            return pfx::cornersUsable(set);
        }
    }
    return false;
}

void setCorners(pfx::Settings& settings, const Corners& corners)
{
    const Corners own = ownCorners();
    bool same = true;
    for (std::size_t i = 0; i < 4; ++i) {
        same = same && std::abs(corners.at(i).x - own.at(i).x) < kEpsilon &&
               std::abs(corners.at(i).y - own.at(i).y) < kEpsilon;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        const std::string key = kCorners.at(i);
        if (same) {
            settings.erase(key + ".x");
            settings.erase(key + ".y");
        } else {
            settings.set(key + ".x", std::clamp(corners.at(i).x, 0.0, 1.0));
            settings.set(key + ".y", std::clamp(corners.at(i).y, 0.0, 1.0));
        }
    }
}

pfx::Settings withoutCornersOrCrop(const pfx::Settings& settings)
{
    pfx::Settings s = withoutCrop(settings);
    setCorners(s, ownCorners());
    return s;
}

bool keepsInside(const pfx::Settings& settings)
{
    return settings.text(kKeep, "whole") == "inside";
}

void setKeepInside(pfx::Settings& settings, bool inside)
{
    if (inside) {
        settings.set(kKeep, std::string("inside"));
    } else {
        settings.erase(kKeep);
    }
}

QSize outputSize(const pfx::Settings& settings, const QSize& photo)
{
    const pfx::Size s = pfx::outputSize(settings, {photo.width(), photo.height()});
    return {s.width, s.height};
}

QSize uncroppedSize(const pfx::Settings& settings, const QSize& photo)
{
    return outputSize(withoutCrop(settings), photo);
}

void turn(pfx::Settings& settings, bool clockwise)
{
    // The library flips after it turns, so with one flip on, a turn of the shown photo is the
    // opposite turn underneath. Both flips together are a half turn and change nothing.
    const bool flipH = settings.boolean(kFlipH, false);
    const bool flipV = settings.boolean(kFlipV, false);
    int delta = clockwise ? 1 : 3;
    if (flipH != flipV) {
        delta = 4 - delta;
    }
    const int turns = (static_cast<int>(std::lround(settings.number(kRotate, 0.0))) + delta) % 4;
    if (turns == 0) {
        settings.erase(kRotate);
    } else {
        settings.set(kRotate, static_cast<double>(turns));
    }
    const QRectF c = cropOf(settings);
    setCrop(settings, clockwise ? QRectF(1 - c.bottom(), c.x(), c.height(), c.width())
                                : QRectF(c.y(), 1 - c.right(), c.height(), c.width()));
    // The corners turn with the photo, and the one now at the top left comes first.
    const Corners before = cornersOf(settings);
    Corners after{};
    for (std::size_t i = 0; i < 4; ++i) {
        const pfx::Corner& p = before.at(clockwise ? (i + 3) % 4 : (i + 1) % 4);
        after.at(i) = clockwise ? pfx::Corner{1.0 - p.y, p.x} : pfx::Corner{p.y, 1.0 - p.x};
    }
    setCorners(settings, after);
    // Perspective turns with the photo: what narrowed the top now narrows the right (clockwise).
    const double vertical = settings.number(kVertical, 0.0);
    const double horizontal = settings.number(kHorizontal, 0.0);
    setOrEraseNumber(settings, kVertical, clockwise ? horizontal : -horizontal);
    setOrEraseNumber(settings, kHorizontal, clockwise ? -vertical : vertical);
}

void flip(pfx::Settings& settings, Qt::Orientation orientation)
{
    const bool horizontal = orientation == Qt::Horizontal;
    const char* key = horizontal ? kFlipH : kFlipV;
    setOrErase(settings, key, !settings.boolean(key, false));
    // A mirror turns the straightening the other way.
    setAngle(settings, -settings.number(kAngle, 0.0));
    const QRectF c = cropOf(settings);
    setCrop(settings, horizontal ? QRectF(1 - c.right(), c.y(), c.width(), c.height())
                                 : QRectF(c.x(), 1 - c.bottom(), c.width(), c.height()));
    // The corners mirror with the photo; swapped in pairs, they still go round clockwise.
    const Corners before = cornersOf(settings);
    static constexpr std::array<std::size_t, 4> kAcross = {1, 0, 3, 2}; // left and right swap
    static constexpr std::array<std::size_t, 4> kUpDown = {3, 2, 1, 0}; // top and bottom swap
    Corners after{};
    for (std::size_t i = 0; i < 4; ++i) {
        const pfx::Corner& p = before.at(horizontal ? kAcross.at(i) : kUpDown.at(i));
        after.at(i) = horizontal ? pfx::Corner{1.0 - p.x, p.y} : pfx::Corner{p.x, 1.0 - p.y};
    }
    setCorners(settings, after);
    // A mirror swaps the sides a keystone narrows.
    const char* mirrored = horizontal ? kHorizontal : kVertical;
    setOrEraseNumber(settings, mirrored, -settings.number(mirrored, 0.0));
}

void setAngle(pfx::Settings& settings, double degrees)
{
    if (std::abs(degrees) < kEpsilon) {
        settings.erase(kAngle);
    } else {
        settings.set(kAngle, std::clamp(degrees, -45.0, 45.0));
    }
}

QRectF fitRatio(const QRectF& crop, const QSize& size, double ratio)
{
    if (ratio <= 0 || size.isEmpty()) {
        return crop;
    }
    const double w = std::min<double>(size.width(), size.height() * ratio);
    const QPointF centre(crop.center().x() * size.width(), crop.center().y() * size.height());
    return placed(centre, w, w / ratio, size);
}

QRectF conformRatio(const QRectF& crop, const QSize& size, double ratio)
{
    if (ratio <= 0 || size.isEmpty()) {
        return crop;
    }
    double w = crop.width() * size.width();
    double h = w / ratio;
    if (h > size.height()) {
        h = size.height();
        w = h * ratio;
    }
    const QPointF centre(crop.center().x() * size.width(), crop.center().y() * size.height());
    return placed(centre, w, h, size);
}

QRectF swapCrop(const QRectF& crop, const QSize& size)
{
    if (size.isEmpty()) {
        return crop;
    }
    double w = crop.height() * size.height();
    double h = crop.width() * size.width();
    const double fit = std::min({1.0, size.width() / w, size.height() / h});
    w *= fit;
    h *= fit;
    const QPointF centre(crop.center().x() * size.width(), crop.center().y() * size.height());
    return placed(centre, w, h, size);
}

namespace {

/// Which sides of the box a handle moves.
struct Sides
{
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;
};

Sides sidesOf(CropHandle handle)
{
    using enum CropHandle;
    return {handle == Left || handle == TopLeft || handle == BottomLeft,
            handle == Right || handle == TopRight || handle == BottomRight,
            handle == Top || handle == TopLeft || handle == TopRight,
            handle == Bottom || handle == BottomLeft || handle == BottomRight};
}

/// A corner with a fixed ratio: the opposite corner stays, the box follows the side dragged
/// further and shrinks to stay inside.
QRectF cornerWithRatio(const QRectF& box, Sides sides, const QRectF& bounds, double ratio)
{
    const double ax = sides.left ? box.right() : box.left();
    const double ay = sides.top ? box.bottom() : box.top();
    double w = box.width();
    double h = box.height();
    if (w / h > ratio) {
        h = w / ratio;
    } else {
        w = h * ratio;
    }
    const double maxW = sides.left ? ax - bounds.left() : bounds.right() - ax;
    const double maxH = sides.top ? ay - bounds.top() : bounds.bottom() - ay;
    const double fit = std::min({1.0, maxW / w, maxH / h});
    w *= fit;
    h *= fit;
    return {sides.left ? ax - w : ax, sides.top ? ay - h : ay, w, h};
}

/// A left or right side with a fixed ratio: the height follows, centred where it was.
QRectF sideWithRatio(const QRectF& box, const QRectF& start, bool left, const QRectF& bounds, double ratio)
{
    double w = box.width();
    double h = w / ratio;
    const double cy = start.center().y();
    const double maxH = 2 * std::min(cy - bounds.top(), bounds.bottom() - cy);
    if (h > maxH) {
        h = maxH;
        w = h * ratio;
    }
    return {left ? box.right() - w : box.left(), cy - (h / 2), w, h};
}

QRectF transposed(const QRectF& r)
{
    return {r.y(), r.x(), r.height(), r.width()};
}

} // namespace

QRectF dragCrop(const QRectF& start, CropHandle handle, QPointF delta, const QRectF& bounds, double ratio,
                double minimum)
{
    if (handle == CropHandle::None) {
        return start;
    }
    if (handle == CropHandle::Move) {
        const double x = std::clamp(start.x() + delta.x(), bounds.left(), bounds.right() - start.width());
        const double y = std::clamp(start.y() + delta.y(), bounds.top(), bounds.bottom() - start.height());
        return {x, y, start.width(), start.height()};
    }
    const Sides sides = sidesOf(handle);
    double l = start.left();
    double r = start.right();
    double t = start.top();
    double b = start.bottom();
    if (sides.left) {
        l = std::clamp(l + delta.x(), bounds.left(), r - minimum);
    }
    if (sides.right) {
        r = std::clamp(r + delta.x(), l + minimum, bounds.right());
    }
    if (sides.top) {
        t = std::clamp(t + delta.y(), bounds.top(), b - minimum);
    }
    if (sides.bottom) {
        b = std::clamp(b + delta.y(), t + minimum, bounds.bottom());
    }
    const QRectF box(QPointF(l, t), QPointF(r, b));
    if (ratio <= 0) {
        return box;
    }
    const bool horizontal = sides.left || sides.right;
    const bool vertical = sides.top || sides.bottom;
    if (horizontal && vertical) {
        return cornerWithRatio(box, sides, bounds, ratio);
    }
    if (horizontal) {
        return sideWithRatio(box, start, sides.left, bounds, ratio);
    }
    // Top or bottom: the same as a side with the axes swapped.
    return transposed(
        sideWithRatio(transposed(box), transposed(start), sides.top, transposed(bounds), 1 / ratio));
}

} // namespace pixelfx::core
