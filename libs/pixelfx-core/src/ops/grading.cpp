// SPDX-License-Identifier: Apache-2.0
// Colour grading, split-toning style (FEATURES E8, DOCS/presets.md "grading"): a colour for the
// shadows and one for the highlights, each with an amount, and a balance that moves where
// shadows end and highlights begin. Tints add colour without changing a pixel's lightness.
#include "ops/ops.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pfx::detail {

namespace {

constexpr double kStrength = 0.3; // largest colour shift at amount 100, perceptual units

struct Tint
{
    std::array<double, 3> chroma{}; // the hue at full saturation, minus its own lightness
    double amount = 0;
};

Tint tintOf(const Settings& s, const std::string& zone)
{
    Tint t;
    t.amount = s.number("grading." + zone + ".amount", 0.0) / 100.0;
    const double h = std::fmod(s.number("grading." + zone + ".hue", 0.0), 360.0) / 60.0;
    const double x = 1.0 - std::abs(std::fmod(h, 2.0) - 1.0);
    std::array<double, 3> rgb{};
    switch (static_cast<int>(h)) {
    case 0:
        rgb = {1, x, 0};
        break;
    case 1:
        rgb = {x, 1, 0};
        break;
    case 2:
        rgb = {0, 1, x};
        break;
    case 3:
        rgb = {0, x, 1};
        break;
    case 4:
        rgb = {x, 0, 1};
        break;
    default:
        rgb = {1, 0, x};
        break;
    }
    const double luma = color::luminance(rgb[0], rgb[1], rgb[2]);
    for (std::size_t c = 0; c < 3; ++c) {
        t.chroma.at(c) = rgb.at(c) - luma;
    }
    return t;
}

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - (2.0 * t));
}

} // namespace

bool gradingActive(const Settings& s)
{
    return s.number("grading.shadows.amount", 0.0) != 0.0 ||
           s.number("grading.highlights.amount", 0.0) != 0.0;
}

void gradingApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const Tint shadows = tintOf(s, "shadows");
    const Tint highlights = tintOf(s, "highlights");
    // Positive balance gives the highlight colour more of the range.
    const double pivot = 0.5 - (0.3 * s.number("grading.balance", 0.0) / 100.0);
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double lightness =
                    color::perceptualEncode(std::max(0.0, color::luminance(p[0], p[1], p[2])));
                const double high = smoothstep(pivot - 0.35, pivot + 0.35, lightness);
                // Tints fade out at pure black and pure white, where colour would look like a cast.
                const double room =
                    smoothstep(0.0, 0.1, lightness) * (1.0 - smoothstep(0.95, 1.1, lightness));
                const double ws = (1.0 - high) * shadows.amount * kStrength * room;
                const double wh = high * highlights.amount * kStrength * room;
                for (std::size_t c = 0; c < 3; ++c) {
                    const double e = color::perceptualEncode(std::max(0.0, double{p[c]}));
                    const double shifted = e + (ws * shadows.chroma.at(c)) + (wh * highlights.chroma.at(c));
                    p[c] = static_cast<float>(color::perceptualDecode(std::max(0.0, shifted)));
                }
            }
        }
    });
}

} // namespace pfx::detail
