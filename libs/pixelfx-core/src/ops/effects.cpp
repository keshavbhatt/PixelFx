// SPDX-License-Identifier: Apache-2.0
// Effects, applied last (DOCS/presets.md section 5): vignette, grain, fade. Every distance is a
// share of the image, so a preview and the full-size export look the same.
#include "ops/ops.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pfx::detail {

namespace {

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - (2.0 * t));
}

struct VignetteShape
{
    double halfW; // half-axes, in pixels
    double halfH;
    double power; // 2 is an ellipse, higher is closer to a rectangle
    double inner; // where the falloff starts and ends, 1 is the middle of an edge
    double outer;
};

VignetteShape vignetteShape(const Settings& s, int width, int height)
{
    const double roundness = s.number("effects.vignette.roundness", 0.0) / 100.0;
    const double midpoint = s.number("effects.vignette.midpoint", 50.0) / 100.0;
    // Round toward a circle of the image's mean half-size; square toward a rectangle.
    const double circle = std::sqrt(static_cast<double>(width) * static_cast<double>(height)) / 2.0;
    const double toCircle = std::max(0.0, roundness);
    VignetteShape v{};
    v.halfW = (width / 2.0) + (toCircle * (circle - (width / 2.0)));
    v.halfH = (height / 2.0) + (toCircle * (circle - (height / 2.0)));
    v.power = 2.0 * std::pow(4.0, std::max(0.0, -roundness));
    v.inner = 0.2 + (0.8 * midpoint);
    v.outer = v.inner + 0.9;
    return v;
}

/// A repeatable value in [-1, 1] for a lattice point.
double latticeNoise(std::int64_t ix, std::int64_t iy)
{
    auto h = static_cast<std::uint64_t>(ix) * 0x9E3779B97F4A7C15ULL;
    h ^= static_cast<std::uint64_t>(iy) * 0xC2B2AE3D27D4EB4FULL;
    h ^= h >> 29U;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32U;
    return (static_cast<double>(h >> 11U) / static_cast<double>(1ULL << 53U) * 2.0) - 1.0;
}

/// Smoothly interpolated lattice noise at (x, y) in lattice units.
double valueNoise(double x, double y)
{
    const double fx = std::floor(x);
    const double fy = std::floor(y);
    const auto ix = static_cast<std::int64_t>(fx);
    const auto iy = static_cast<std::int64_t>(fy);
    const double tx = smoothstep(0.0, 1.0, x - fx);
    const double ty = smoothstep(0.0, 1.0, y - fy);
    const double top = latticeNoise(ix, iy) + (tx * (latticeNoise(ix + 1, iy) - latticeNoise(ix, iy)));
    const double bottom =
        latticeNoise(ix, iy + 1) + (tx * (latticeNoise(ix + 1, iy + 1) - latticeNoise(ix, iy + 1)));
    return top + (ty * (bottom - top));
}

} // namespace

bool vignetteActive(const Settings& s)
{
    return s.number("effects.vignette.amount", 0.0) != 0.0;
}

void vignetteApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = s.number("effects.vignette.amount", 0.0) / 100.0;
    const VignetteShape v = vignetteShape(s, image.width(), image.height());
    const double cx = image.width() / 2.0;
    const double cy = image.height() / 2.0;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            const double dy = std::abs((y + 0.5 - cy) / v.halfH);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double dx = std::abs((x + 0.5 - cx) / v.halfW);
                const double d = std::pow(std::pow(dx, v.power) + std::pow(dy, v.power), 1.0 / v.power);
                const double f = smoothstep(v.inner, v.outer, d) * amount;
                for (int c = 0; c < 3; ++c) {
                    const auto value = double{p[c]};
                    // Darkening scales toward black; lightening mixes toward white.
                    p[c] = static_cast<float>(f < 0.0 ? value * (1.0 + f) : value + ((1.0 - value) * f));
                }
            }
        }
    });
}

bool grainActive(const Settings& s)
{
    return s.number("effects.grain.amount", 0.0) != 0.0;
}

void grainApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = s.number("effects.grain.amount", 0.0) / 100.0;
    const double size = s.number("effects.grain.size", 25.0) / 100.0;
    // A grain is 0.05% to 0.3% of the long edge. The lattice is tied to the image, so a preview
    // shows the same grains, smaller; where they shrink below a pixel they average out, as
    // they would when the full-size photo is scaled down.
    const double cell = image.longEdge() * (0.0005 + (0.0025 * size));
    const double strength = 0.12 * amount * std::min(1.0, cell);
    constexpr double kTiny = 1e-6;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double luma = std::max(0.0, color::luminance(p[0], p[1], p[2]));
                const double e = color::perceptualEncode(luma);
                const double n = valueNoise((x + 0.5) / cell, (y + 0.5) / cell);
                // Strongest in the midtones, as film grain shows.
                const double shifted =
                    std::max(0.0, e + (strength * n * (0.35 + (2.6 * e * (1.0 - std::min(e, 1.0))))));
                const double mapped = color::perceptualDecode(shifted);
                for (int c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(luma > kTiny ? double{p[c]} * (mapped / luma)
                                                           : double{p[c]} + mapped - luma);
                }
            }
        }
    });
}

bool fadeActive(const Settings& s)
{
    return s.number("effects.fade", 0.0) != 0.0;
}

void fadeApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double f = s.number("effects.fade", 0.0) / 100.0;
    const double lift = 0.15 * f; // blacks rise to a matte grey
    const double drop = 0.04 * f; // whites soften a little
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                for (int c = 0; c < 3; ++c) {
                    const double e = color::perceptualEncode(std::max(0.0, double{p[c]}));
                    p[c] = static_cast<float>(color::perceptualDecode(lift + (e * (1.0 - lift - drop))));
                }
            }
        }
    });
}

} // namespace pfx::detail
