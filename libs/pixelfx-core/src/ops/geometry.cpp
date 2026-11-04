// SPDX-License-Identifier: Apache-2.0
// Geometry (DOCS/presets.md section 5, first in the pipeline): quarter turns, then flips, then
// straightening by a small angle with the empty corners trimmed, then the crop. The crop is a
// share of the straightened image, so it means the same at any resolution (ADR-004).
#include "pfx/geometry.h"

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
    double angle = 0.0; // degrees, positive turns the photo clockwise
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
    return g.quarterTurns != 0 || g.flipH || g.flipV || g.angle != 0.0 || hasCrop(g);
}

void geometryApply(Image& image, const Settings& s, const OpContext& /*ctx*/)
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
