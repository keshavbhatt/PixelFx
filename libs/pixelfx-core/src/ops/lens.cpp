// SPDX-License-Identifier: Apache-2.0
// Lens corrections that need no lens profile (FEATURES P8):
// - lateral chromatic aberration: the red and blue images are a little larger or smaller than
//   the green one, so edges far from the centre show colour fringes. Each scale is measured on
//   the photo itself (at a fixed analysis size, so a preview and the export agree) and undone;
// - defringe: purple and green fringes along high-contrast edges are made grey.
#include "image/filters.h"
#include "ops/hue_bands.h"
#include "ops/ops.h"
#include "pfx/resample.h"

#include <array>
#include <cmath>

namespace pfx::detail {

namespace {

constexpr int kAnalysisEdge = 1024;
constexpr double kMaxScale = 0.004; // the largest misfit searched, a share of the radius
constexpr double kScaleStep = 0.00025;

Plane channel(const Image& image, int c)
{
    Plane out(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        const float* p = image.row(y);
        for (int x = 0; x < image.width(); ++x, p += 4) {
            out.at(x, y) = static_cast<float>(color::perceptualEncode(std::max(0.0, double{p[c]})));
        }
    }
    return out;
}

/// How badly `moving`, scaled by `scale` about the centre, misses `fixed` on its edges.
double misfit(const Plane& fixed, const Plane& moving, double scale)
{
    const double cx = (fixed.width - 1) / 2.0;
    const double cy = (fixed.height - 1) / 2.0;
    const double inner = 0.3 * std::min(cx, cy); // the middle has almost no aberration to measure
    double sum = 0.0;
    double weight = 0.0;
    for (int y = 1; y < fixed.height - 1; y += 2) {
        for (int x = 1; x < fixed.width - 1; x += 2) {
            const double dx = x - cx;
            const double dy = y - cy;
            if (std::hypot(dx, dy) < inner) {
                continue;
            }
            const double edge = std::abs(double{fixed.at(x + 1, y) - fixed.at(x - 1, y)}) +
                                std::abs(double{fixed.at(x, y + 1) - fixed.at(x, y - 1)});
            if (edge < 0.05) {
                continue;
            }
            const auto d =
                double{fixed.at(x, y) - sampleBilinear(moving, cx + (dx * scale), cy + (dy * scale))};
            sum += edge * d * d;
            weight += edge;
        }
    }
    return weight > 0.0 ? sum / weight : 0.0;
}

/// The scale that fits `moving` best onto `fixed`, refined between the best steps.
double bestScale(const Plane& fixed, const Plane& moving)
{
    const int steps = static_cast<int>(std::lround(kMaxScale / kScaleStep));
    std::vector<double> costs;
    for (int i = -steps; i <= steps; ++i) {
        costs.push_back(misfit(fixed, moving, 1.0 + (i * kScaleStep)));
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i < costs.size(); ++i) {
        if (costs[i] < costs[best]) {
            best = i;
        }
    }
    double offset = 0.0;
    if (best > 0 && best + 1 < costs.size()) {
        const double a = costs[best - 1];
        const double b = costs[best];
        const double c = costs[best + 1];
        const double curvature = a - (2.0 * b) + c;
        if (curvature > 0.0) {
            offset = 0.5 * (a - c) / curvature;
        }
    }
    return 1.0 + ((static_cast<double>(best) - steps + offset) * kScaleStep);
}

} // namespace

bool chromaticAberrationActive(const Settings& s)
{
    return s.boolean("detail.ca.auto", false);
}

void chromaticAberrationApply(Image& image, const Settings& /*s*/, const OpContext& ctx)
{
    if (image.width() < 16 || image.height() < 16) {
        return;
    }
    auto small = fitLongEdge(image, kAnalysisEdge);
    if (!small) {
        return;
    }
    const Plane green = channel(small.value(), 1);
    const std::array<double, 2> scale = {bestScale(green, channel(small.value(), 0)),
                                         bestScale(green, channel(small.value(), 2))};
    const Image source = image;
    const double cx = (image.width() - 1) / 2.0;
    const double cy = (image.height() - 1) / 2.0;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                for (std::size_t k = 0; k < 2; ++k) {
                    const int c = k == 0 ? 0 : 2;
                    const double sx = std::clamp(cx + ((x - cx) * scale.at(k)), 0.0, image.width() - 1.0);
                    const double sy = std::clamp(cy + ((y - cy) * scale.at(k)), 0.0, image.height() - 1.0);
                    const int x0 = static_cast<int>(sx);
                    const int y0s = static_cast<int>(sy);
                    const int x1 = std::min(x0 + 1, image.width() - 1);
                    const int y1s = std::min(y0s + 1, image.height() - 1);
                    const double tx = sx - x0;
                    const double ty = sy - y0s;
                    const auto at = [&](int px, int py) { return double{source.pixel(px, py)[c]}; };
                    const double top = (at(x0, y0s) * (1.0 - tx)) + (at(x1, y0s) * tx);
                    const double bottom = (at(x0, y1s) * (1.0 - tx)) + (at(x1, y1s) * tx);
                    p[c] = static_cast<float>(top + ((bottom - top) * ty));
                }
            }
        }
    });
}

namespace {

/// A hue range of a defringe slider: 0 to 100 across `from` to `to` degrees.
struct FringeRange
{
    double amount = 0; // 0 to 1
    double low = 0;    // degrees
    double high = 0;
};

FringeRange fringeRange(const Settings& s, const std::string& colour, double from, double to,
                        double lowDefault, double highDefault)
{
    const std::string key = "detail.defringe." + colour;
    const double low = s.number(key + ".hueLow", lowDefault) / 100.0;
    const double high = s.number(key + ".hueHigh", highDefault) / 100.0;
    return {s.number(key + ".amount", 0.0) / 20.0, from + ((to - from) * std::min(low, high)),
            from + ((to - from) * std::max(low, high))};
}

double inRange(const FringeRange& r, double hue)
{
    constexpr double kSoft = 10.0; // degrees of soft edge on each side
    return smoothstep(r.low - kSoft, r.low, hue) * (1.0 - smoothstep(r.high, r.high + kSoft, hue));
}

/// How strongly the perceptual lightness changes at each pixel.
Plane edgeStrength(const Image& image)
{
    Plane luma(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        const float* p = image.row(y);
        for (int x = 0; x < image.width(); ++x, p += 4) {
            luma.at(x, y) = static_cast<float>(
                color::perceptualEncode(std::max(0.0, color::luminance(p[0], p[1], p[2]))));
        }
    }
    Plane edges(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int xl = std::max(0, x - 1);
            const int xr = std::min(image.width() - 1, x + 1);
            const int yt = std::max(0, y - 1);
            const int yb = std::min(image.height() - 1, y + 1);
            edges.at(x, y) =
                std::abs(luma.at(xr, y) - luma.at(xl, y)) + std::abs(luma.at(x, yb) - luma.at(x, yt));
        }
    }
    return edges;
}

} // namespace

bool defringeActive(const Settings& s)
{
    return s.number("detail.defringe.purple.amount", 0.0) > 0.0 ||
           s.number("detail.defringe.green.amount", 0.0) > 0.0;
}

void defringeApply(Image& image, const Settings& s, const OpContext& ctx)
{
    // Lightroom's purple slider runs from blue to magenta, its green one from yellow to cyan.
    const FringeRange purple = fringeRange(s, "purple", 230.0, 330.0, 30, 70);
    const FringeRange green = fringeRange(s, "green", 60.0, 180.0, 40, 60);
    constexpr double kEdgeRadius = 0.002; // fringes sit within about this far of an edge
    const Plane near =
        gaussianBlur(edgeStrength(image), std::max(1.0, kEdgeRadius * image.longEdge()), ctx.threads);
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double edge = smoothstep(0.02, 0.12, double{near.at(x, y)});
                if (edge <= 0.0) {
                    continue;
                }
                std::array<double, 3> e{};
                for (std::size_t c = 0; c < 3; ++c) {
                    e.at(c) = color::perceptualEncode(std::max(0.0, double{p[c]}));
                }
                const Hsv hsv = toHsv(e);
                const double grey = edge * std::max(purple.amount * inRange(purple, hsv.hue),
                                                    green.amount * inRange(green, hsv.hue));
                if (grey <= 0.0) {
                    continue;
                }
                const double luminance = color::luminance(p[0], p[1], p[2]);
                for (int c = 0; c < 3; ++c) {
                    p[c] =
                        static_cast<float>(double{p[c]} + ((luminance - double{p[c]}) * std::min(1.0, grey)));
                }
            }
        }
    });
}

} // namespace pfx::detail
