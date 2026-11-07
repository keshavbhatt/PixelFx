// SPDX-License-Identifier: Apache-2.0
// Geometry (DOCS/presets.md section 5, first in the pipeline): quarter turns, then flips, then
// the keystone sliders, then straightening by a small angle with the empty corners trimmed, then
// the four-corner perspective, then the crop. The corners are shares of the straightened image
// and the crop a share of what comes out, so they mean the same at any resolution (ADR-004).
#include "pfx/geometry.h"

#include "core/parallel.h"
#include "ops/ops.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <numbers>
#include <vips/vips8>

namespace pfx {

namespace {

struct Geometry
{
    int quarterTurns = 0; // clockwise
    bool flipH = false;
    bool flipV = false;
    double angle = 0.0;      // degrees, positive turns the photo clockwise
    double vertical = 0.0;   // -1..1: positive widens the top (converging verticals fixed)
    double horizontal = 0.0; // -1..1: positive widens the left side
    double x = 0.0;
    double y = 0.0;
    double w = 1.0;
    double h = 1.0;
    // The four corners of what should be a rectangle, top left first, clockwise, as shares of
    // the straightened image; the image's own corners when unset.
    std::array<Corner, 4> corners{{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}};
    bool keepInside = false; // only the shape, squared; otherwise the whole image, trimmed
};

constexpr std::array<const char*, 4> kCornerKeys = {
    "geometry.perspective.corners.topLeft", "geometry.perspective.corners.topRight",
    "geometry.perspective.corners.bottomRight", "geometry.perspective.corners.bottomLeft"};

bool hasCorners(const Geometry& g)
{
    constexpr std::array<Corner, 4> kOwn{{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}};
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::abs(g.corners.at(i).x - kOwn.at(i).x) > 1e-9 ||
            std::abs(g.corners.at(i).y - kOwn.at(i).y) > 1e-9) {
            return true;
        }
    }
    return false;
}

Geometry geometryOf(const Settings& s)
{
    Geometry g;
    g.quarterTurns = ((static_cast<int>(std::lround(s.number("geometry.rotate90", 0.0))) % 4) + 4) % 4;
    g.flipH = s.boolean("geometry.flipH", false);
    g.flipV = s.boolean("geometry.flipV", false);
    g.angle = s.number("geometry.angle", 0.0);
    g.vertical = std::clamp(s.number("geometry.perspective.vertical", 0.0) / 100.0, -1.0, 1.0);
    g.horizontal = std::clamp(s.number("geometry.perspective.horizontal", 0.0) / 100.0, -1.0, 1.0);
    g.x = std::clamp(s.number("geometry.crop.x", 0.0), 0.0, 1.0);
    g.y = std::clamp(s.number("geometry.crop.y", 0.0), 0.0, 1.0);
    g.w = std::clamp(s.number("geometry.crop.w", 1.0), 0.0, 1.0 - g.x);
    g.h = std::clamp(s.number("geometry.crop.h", 1.0), 0.0, 1.0 - g.y);
    for (std::size_t i = 0; i < 4; ++i) {
        const std::string key = kCornerKeys.at(i);
        Corner& c = g.corners.at(i);
        c.x = std::clamp(s.number(key + ".x", c.x), 0.0, 1.0);
        c.y = std::clamp(s.number(key + ".y", c.y), 0.0, 1.0);
    }
    g.keepInside = s.text("geometry.perspective.keep", "whole") == "inside";
    if (!cornersUsable(g.corners)) {
        g.corners = Geometry{}.corners; // a shape that folds over cannot be squared
    }
    if (hasCorners(g)) {
        g.vertical = 0.0; // the corners set the perspective; the sliders are off
        g.horizontal = 0.0;
    }
    return g;
}

bool hasCrop(const Geometry& g)
{
    return g.x > 0.0 || g.y > 0.0 || g.w < 1.0 || g.h < 1.0;
}

// The pixel rectangle of the crop inside an image of `size`, at least one pixel.
struct Rect
{
    int left;
    int top;
    int width;
    int height;
};

Rect cropRect(const Geometry& g, Size size)
{
    const int left = std::clamp(static_cast<int>(std::lround(g.x * size.width)), 0, size.width - 1);
    const int top = std::clamp(static_cast<int>(std::lround(g.y * size.height)), 0, size.height - 1);
    const int width = std::clamp(static_cast<int>(std::lround(g.w * size.width)), 1, size.width - left);
    const int height = std::clamp(static_cast<int>(std::lround(g.h * size.height)), 1, size.height - top);
    return {left, top, width, height};
}

Size straightenedSize(const Geometry& g, Size turned)
{
    if (g.angle == 0.0) {
        return turned;
    }
    // Two pixels inside the exact rectangle: its corners touch the turned edge, where the
    // interpolation would blend in the empty outside.
    constexpr int kInset = 2;
    const double s = straightenScale(g.angle, turned);
    return {std::max(1, static_cast<int>(std::floor(turned.width * s)) - kInset),
            std::max(1, static_cast<int>(std::floor(turned.height * s)) - kInset)};
}

void ensureVips()
{
    static std::once_flag once;
    std::call_once(once, [] { VIPS_INIT("pixelfx"); });
}

// The colour at (x, y) of `in`, between its pixels, the edge pixels repeated outside.
void sampleBilinear(const Image& in, double x, double y, float* dst)
{
    x = std::clamp(x, 0.0, in.width() - 1.0);
    y = std::clamp(y, 0.0, in.height() - 1.0);
    const auto x0 = static_cast<int>(x);
    const auto y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, in.width() - 1);
    const int y1 = std::min(y0 + 1, in.height() - 1);
    const double fx = x - x0;
    const double fy = y - y0;
    const float* a = in.pixel(x0, y0);
    const float* b = in.pixel(x1, y0);
    const float* c = in.pixel(x0, y1);
    const float* d = in.pixel(x1, y1);
    for (int k = 0; k < 4; ++k) {
        const double top = double{a[k]} + (fx * (double{b[k]} - double{a[k]}));
        const double bottom = double{c[k]} + (fx * (double{d[k]} - double{c[k]}));
        dst[k] = static_cast<float>(top + (fy * (bottom - top)));
    }
}

// Keystone correction. Each output row (vertical) or column (horizontal) samples a centred
// stretch of the input that narrows toward one edge, so the side that leaned away widens. Only
// ever narrower than the input: no empty corners and no extra crop. Lines stay straight.
constexpr double kKeystoneReach = 0.4; // the narrowed edge samples 60% of the width at 100

Image keystone(const Image& in, double vertical, double horizontal, int threads)
{
    Image out(in.width(), in.height());
    const double cx = in.width() / 2.0;
    const double cy = in.height() / 2.0;
    const double topScale = 1.0 - (kKeystoneReach * std::max(0.0, vertical));
    const double bottomScale = 1.0 - (kKeystoneReach * std::max(0.0, -vertical));
    const double leftScale = 1.0 - (kKeystoneReach * std::max(0.0, horizontal));
    const double rightScale = 1.0 - (kKeystoneReach * std::max(0.0, -horizontal));
    detail::parallelFor(in.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const double ty = (y + 0.5) / in.height();
            const double rowScale = topScale + (ty * (bottomScale - topScale));
            for (int x = 0; x < in.width(); ++x) {
                const double tx = (x + 0.5) / in.width();
                const double columnScale = leftScale + (tx * (rightScale - leftScale));
                const double sx = cx + (((x + 0.5) - cx) * rowScale) - 0.5;
                const double sy = cy + (((y + 0.5) - cy) * columnScale) - 0.5;
                sampleBilinear(in, sx, sy, out.pixel(x, y));
            }
        }
    });
    return out;
}

// A projective map of the plane, row by row: (x, y) goes to ((a x + b y + c) / (g x + h y + i),
// (d x + e y + f) / (g x + h y + i)).
using Projection = std::array<double, 9>;

Projection multiply(const Projection& l, const Projection& r)
{
    Projection out{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            for (std::size_t k = 0; k < 3; ++k) {
                out.at((row * 3) + col) += l.at((row * 3) + k) * r.at((k * 3) + col);
            }
        }
    }
    return out;
}

Projection inverse(const Projection& m)
{
    const auto at = [&m](std::size_t r, std::size_t c) { return m.at((r * 3) + c); };
    Projection adj = {at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1), at(0, 2) * at(2, 1) - at(0, 1) * at(2, 2),
                      at(0, 1) * at(1, 2) - at(0, 2) * at(1, 1), at(1, 2) * at(2, 0) - at(1, 0) * at(2, 2),
                      at(0, 0) * at(2, 2) - at(0, 2) * at(2, 0), at(0, 2) * at(1, 0) - at(0, 0) * at(1, 2),
                      at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0), at(0, 1) * at(2, 0) - at(0, 0) * at(2, 1),
                      at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)};
    const double det = at(0, 0) * adj.at(0) + at(0, 1) * adj.at(3) + at(0, 2) * adj.at(6);
    for (double& v : adj) {
        v /= det;
    }
    return adj;
}

// Where `m` takes (x, y), and the divisor: a point the map sends past infinity has it at 0 or
// below.
struct Mapped
{
    double x;
    double y;
    double w;
};

Mapped apply(const Projection& m, double x, double y)
{
    const double w = (m.at(6) * x) + (m.at(7) * y) + m.at(8);
    return {((m.at(0) * x) + (m.at(1) * y) + m.at(2)) / w, ((m.at(3) * x) + (m.at(4) * y) + m.at(5)) / w, w};
}

// The map that takes the unit square's corners (0, 0), (1, 0), (1, 1), (0, 1) to `q` (Heckbert,
// "Fundamentals of Texture Mapping", 1989).
Projection squareTo(const std::array<Corner, 4>& q)
{
    const double dx1 = q[1].x - q[2].x;
    const double dx2 = q[3].x - q[2].x;
    const double dx3 = q[0].x - q[1].x + q[2].x - q[3].x;
    const double dy1 = q[1].y - q[2].y;
    const double dy2 = q[3].y - q[2].y;
    const double dy3 = q[0].y - q[1].y + q[2].y - q[3].y;
    const double det = (dx1 * dy2) - (dx2 * dy1);
    const double g = det == 0.0 ? 0.0 : ((dx3 * dy2) - (dx2 * dy3)) / det;
    const double h = det == 0.0 ? 0.0 : ((dx1 * dy3) - (dx3 * dy1)) / det;
    return {q[1].x - q[0].x + (g * q[1].x),
            q[3].x - q[0].x + (h * q[3].x),
            q[0].x,
            q[1].y - q[0].y + (g * q[1].y),
            q[3].y - q[0].y + (h * q[3].y),
            q[0].y,
            g,
            h,
            1.0};
}

double distance(const Corner& a, const Corner& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

// The four-corner perspective on an image of `size`: how big the result is, and the map from a
// result pixel's centre to the image's pixel coordinates.
struct CornerWarp
{
    Size size;
    Projection toSource{};
};

// Half the height of the largest rectangle of `aspect` (width over height) centred on `middle`
// that fits inside the convex, clockwise `outline`, up to `limit`.
double largestInside(const std::array<Corner, 4>& outline, Corner middle, double aspect, double limit)
{
    const auto contains = [&outline](double x, double y) {
        for (std::size_t i = 0; i < 4; ++i) {
            const Corner& a = outline.at(i);
            const Corner& b = outline.at((i + 1) % 4);
            if (((b.x - a.x) * (y - a.y)) - ((b.y - a.y) * (x - a.x)) < 0.0) {
                return false;
            }
        }
        return true;
    };
    double low = 0.0;
    double high = limit;
    for (int step = 0; step < 40; ++step) { // the half height, to a fraction of a pixel
        const double half = (low + high) / 2.0;
        const double halfWidth = half * aspect;
        const bool fits = contains(middle.x - halfWidth, middle.y - half) &&
                          contains(middle.x + halfWidth, middle.y - half) &&
                          contains(middle.x + halfWidth, middle.y + half) &&
                          contains(middle.x - halfWidth, middle.y + half);
        (fits ? low : high) = half;
    }
    return low;
}

CornerWarp cornerWarp(const Geometry& g, Size size)
{
    std::array<Corner, 4> q{};
    for (std::size_t i = 0; i < 4; ++i) {
        q.at(i) = {g.corners.at(i).x * size.width, g.corners.at(i).y * size.height};
    }
    // The rectangle the shape becomes: its sides' average lengths.
    const double width = std::max(1.0, (distance(q[0], q[1]) + distance(q[3], q[2])) / 2.0);
    const double height = std::max(1.0, (distance(q[0], q[3]) + distance(q[1], q[2])) / 2.0);
    const Projection fromRect = multiply(squareTo(q), {1.0 / width, 0, 0, 0, 1.0 / height, 0, 0, 0, 1});
    const auto inside = [&] {
        return CornerWarp{{static_cast<int>(std::lround(width)), static_cast<int>(std::lround(height))},
                          fromRect};
    };
    if (g.keepInside) {
        return inside();
    }
    // The whole image squared: its outline in the rectangle's plane, then the largest rectangle of
    // the image's own shape inside it, around the shape's middle: what the corners were put on
    // stays in the picture as far as it can.
    const Projection toRect = inverse(fromRect);
    std::array<Corner, 4> outline{};
    const std::array<Corner, 4> own{{{0.0, 0.0},
                                     {static_cast<double>(size.width), 0.0},
                                     {static_cast<double>(size.width), static_cast<double>(size.height)},
                                     {0.0, static_cast<double>(size.height)}}};
    const Corner middle{width / 2.0, height / 2.0};
    for (std::size_t i = 0; i < 4; ++i) {
        const Mapped m = apply(toRect, own.at(i).x, own.at(i).y);
        if (m.w <= 0.0) {
            return inside(); // part of the image would go past infinity: only the shape can be kept
        }
        outline.at(i) = {m.x, m.y};
    }
    const double aspect = static_cast<double>(size.width) / size.height;
    const double low = largestInside(outline, middle, aspect, 2.0 * std::max(size.width, size.height));
    const int outHeight = std::max(1, static_cast<int>(std::floor(2.0 * low)) - 2);
    const int outWidth = std::max(1, static_cast<int>(std::floor(2.0 * low * aspect)) - 2);
    const Projection shift = {1, 0, middle.x - (outWidth / 2.0), 0, 1, middle.y - (outHeight / 2.0), 0, 0, 1};
    return {{outWidth, outHeight}, multiply(fromRect, shift)};
}

Image warpCorners(const Image& in, const Geometry& g, int threads)
{
    const CornerWarp warp = cornerWarp(g, {in.width(), in.height()});
    Image out(warp.size.width, warp.size.height);
    detail::parallelFor(out.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = 0; x < out.width(); ++x) {
                const Mapped m = apply(warp.toSource, x + 0.5, y + 0.5);
                sampleBilinear(in, m.x - 0.5, m.y - 0.5, out.pixel(x, y));
            }
        }
    });
    return out;
}

vips::VImage toVips(const Image& image)
{
    return vips::VImage::new_from_memory(const_cast<float*>(image.data()), // NOLINT: read only
                                         image.pixelCount() * 4 * sizeof(float), image.width(),
                                         image.height(), 4, VIPS_FORMAT_FLOAT);
}

Image fromVips(const vips::VImage& in)
{
    Image out(in.width(), in.height());
    std::size_t size = 0;
    void* memory = in.write_to_memory(&size);
    std::copy_n(static_cast<const float*>(memory), std::min(size / sizeof(float), out.pixelCount() * 4),
                out.data());
    g_free(memory);
    return out;
}

} // namespace

bool cornersUsable(const std::array<Corner, 4>& corners)
{
    constexpr double kShortest = 0.01;
    for (std::size_t i = 0; i < 4; ++i) {
        const Corner& a = corners.at(i);
        const Corner& b = corners.at((i + 1) % 4);
        const Corner& c = corners.at((i + 2) % 4);
        if (!std::isfinite(a.x) || !std::isfinite(a.y) || distance(a, b) < kShortest) {
            return false;
        }
        // Clockwise on the screen (y down), as the image's own corners: a shape turning the
        // other way would mirror the photo.
        if (((b.x - a.x) * (c.y - b.y)) - ((b.y - a.y) * (c.x - b.x)) <= 0.0) {
            return false;
        }
    }
    return true;
}

double straightenScale(double degrees, Size input)
{
    const double a = std::abs(degrees) * std::numbers::pi / 180.0;
    if (a == 0.0 || input.width <= 0 || input.height <= 0) {
        return 1.0;
    }
    const double w = input.width;
    const double h = input.height;
    const double c = std::cos(a);
    const double s = std::sin(a);
    return std::min(w / (w * c + h * s), h / (w * s + h * c));
}

Size outputSize(const Settings& settings, Size input)
{
    const Geometry g = geometryOf(settings);
    const Size turned = (g.quarterTurns % 2 == 1) ? Size{input.height, input.width} : input;
    Size straight = straightenedSize(g, turned);
    if (hasCorners(g)) {
        straight = cornerWarp(g, straight).size;
    }
    if (!hasCrop(g)) {
        return straight;
    }
    const Rect r = cropRect(g, straight);
    return {r.width, r.height};
}

namespace detail {

bool geometryActive(const Settings& s)
{
    const Geometry g = geometryOf(s);
    return g.quarterTurns != 0 || g.flipH || g.flipV || g.angle != 0.0 || g.vertical != 0.0 ||
           g.horizontal != 0.0 || hasCorners(g) || hasCrop(g);
}

void geometryApply(Image& image, const Settings& s, const OpContext& ctx)
{
    ensureVips();
    const Geometry g = geometryOf(s);
    vips::VImage v = toVips(image);
    static constexpr VipsAngle kTurns[] = {VIPS_ANGLE_D0, VIPS_ANGLE_D90, VIPS_ANGLE_D180, VIPS_ANGLE_D270};
    if (g.quarterTurns != 0) {
        v = v.rot(kTurns[g.quarterTurns]);
    }
    if (g.flipH) {
        v = v.flip(VIPS_DIRECTION_HORIZONTAL);
    }
    if (g.flipV) {
        v = v.flip(VIPS_DIRECTION_VERTICAL);
    }
    Image warped; // lives until the end: libvips reads it in place
    if (g.vertical != 0.0 || g.horizontal != 0.0) {
        warped = keystone(fromVips(v), g.vertical, g.horizontal, ctx.threads);
        v = toVips(warped);
    }
    if (g.angle != 0.0) {
        const Size turned{v.width(), v.height()};
        const Size inner = straightenedSize(g, turned);
        const vips::VImage rotated =
            v.rotate(g.angle, vips::VImage::option()->set("interpolate",
                                                          vips::VInterpolate::new_from_name("bicubic")));
        const int left = (rotated.width() - inner.width) / 2;
        const int top = (rotated.height() - inner.height) / 2;
        v = rotated.extract_area(left, top, inner.width, inner.height);
    }
    Image squared; // lives until the end, as `warped`
    if (hasCorners(g)) {
        squared = warpCorners(fromVips(v), g, ctx.threads);
        v = toVips(squared);
    }
    if (hasCrop(g)) {
        const Rect r = cropRect(g, {v.width(), v.height()});
        v = v.extract_area(r.left, r.top, r.width, r.height);
    }
    image = fromVips(v);
}

} // namespace detail
} // namespace pfx
