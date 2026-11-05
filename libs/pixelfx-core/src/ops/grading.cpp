// SPDX-License-Identifier: Apache-2.0
// Colour grading (FEATURES E8, DOCS/presets.md "grading"): a colour for the shadows, the
// midtones and the highlights and one for the whole picture, each with an amount and a
// luminance. Balance moves where shadows end and highlights begin; blending widens or narrows
// the overlap between the ranges. Tints add colour without changing a pixel's lightness; the
// luminance of a range lightens or darkens it.
#include "ops/ops.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace pfx::detail {

namespace {

constexpr double kStrength = 0.3; // largest colour shift at amount 100, perceptual units

constexpr double kLuminanceReach = 0.15; // lightness shift at luminance +-100, perceptual units

struct Tint
{
    std::array<double, 3> chroma{}; // the hue at full saturation, minus its own lightness
    double amount = 0;
    double luminance = 0;
};

Tint tintOf(const Settings& s, const std::string& zone)
{
    Tint t;
    t.amount = s.number("grading." + zone + ".amount", 0.0) / 100.0;
    t.luminance = s.number("grading." + zone + ".luminance", 0.0) / 100.0;
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
    constexpr std::array<const char*, 4> kZones = {"shadows", "midtones", "highlights", "global"};
    return std::ranges::any_of(kZones, [&](const char* zone) {
        const std::string key = std::string("grading.") + zone;
        return s.number(key + ".amount", 0.0) != 0.0 || s.number(key + ".luminance", 0.0) != 0.0;
    });
}

void gradingApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const Tint shadows = tintOf(s, "shadows");
    const Tint midtones = tintOf(s, "midtones");
    const Tint highlights = tintOf(s, "highlights");
    const Tint global = tintOf(s, "global");
    // Positive balance gives the highlight colour more of the range.
    const double pivot = 0.5 - (0.3 * s.number("grading.balance", 0.0) / 100.0);
    // Blending 50 is the even overlap; 0 makes hard ranges, 100 lets them run into each other.
    const double reach = 0.35 * (0.3 + (1.4 * s.number("grading.blending", 50.0) / 100.0));
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double lightness =
                    color::perceptualEncode(std::max(0.0, color::luminance(p[0], p[1], p[2])));
                const double high = smoothstep(pivot - reach, pivot + reach, lightness);
                const double mid = 4.0 * high * (1.0 - high); // 1 where shadows meet highlights
                // Tints fade out at pure black and pure white, where colour would look like a cast.
                const double room =
                    smoothstep(0.0, 0.1, lightness) * (1.0 - smoothstep(0.95, 1.1, lightness));
                const std::array<std::pair<const Tint*, double>, 4> zones = {
                    {{&shadows, 1.0 - high}, {&midtones, mid}, {&highlights, high}, {&global, 1.0}}};
                std::array<double, 3> shift{};
                double lift = 0.0;
                for (const auto& [tint, weight] : zones) {
                    const double w = weight * tint->amount * kStrength * room;
                    for (std::size_t c = 0; c < 3; ++c) {
                        shift.at(c) += w * tint->chroma.at(c);
                    }
                    lift += weight * tint->luminance * kLuminanceReach;
                }
                for (std::size_t c = 0; c < 3; ++c) {
                    const double e = color::perceptualEncode(std::max(0.0, double{p[c]}));
                    p[c] = static_cast<float>(color::perceptualDecode(std::max(0.0, e + shift.at(c) + lift)));
                }
            }
        }
    });
}

} // namespace pfx::detail
