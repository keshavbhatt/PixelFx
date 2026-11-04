// SPDX-License-Identifier: Apache-2.0
// Geometry (DOCS/presets.md section 5, first in the pipeline): quarter turns, then flips, then
// straightening by a small angle with the empty corners trimmed, then the crop. The crop is a
// share of the straightened image, so it means the same at any resolution (ADR-004).
#include "pfx/geometry.h"

#include "core/parallel.h"
#include "ops/ops.h"

#include <algorithm>
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
};

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
    const auto sample = [&in](double x, double y, float* dst) {
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
    };
    detail::parallelFor(in.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const double ty = (y + 0.5) / in.height();
            const double rowScale = topScale + (ty * (bottomScale - topScale));
            for (int x = 0; x < in.width(); ++x) {
                const double tx = (x + 0.5) / in.width();
                const double columnScale = leftScale + (tx * (rightScale - leftScale));
                const double sx = cx + (((x + 0.5) - cx) * rowScale) - 0.5;
                const double sy = cy + (((y + 0.5) - cy) * columnScale) - 0.5;
                sample(sx, sy, out.pixel(x, y));
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
    const Size straight = straightenedSize(g, turned);
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
           g.horizontal != 0.0 || hasCrop(g);
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
    if (hasCrop(g)) {
        const Rect r = cropRect(g, {v.width(), v.height()});
        v = v.extract_area(r.left, r.top, r.width, r.height);
    }
    image = fromVips(v);
}

} // namespace detail
} // namespace pfx
