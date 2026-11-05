// SPDX-License-Identifier: Apache-2.0
// Colour mixer: hue, saturation and luminance for eight colour bands (FEATURES E3). Works on
// the perceptual encoding in HSV terms; each pixel's change is blended from the two bands its
// hue lies between, and fades out toward grey so neutral tones never shift.
#include "ops/hue_bands.h"
#include "ops/ops.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace pfx::detail {

namespace {

constexpr double kHueReach = 30.0;      // degrees at +-100
constexpr double kLuminanceReach = 0.4; // share of the value at +-100

struct Band
{
    double hue = 0;
    double saturation = 0;
    double luminance = 0;
};

std::array<Band, 8> bandsOf(const Settings& s)
{
    std::array<Band, 8> bands{};
    for (std::size_t i = 0; i < kHueBands.size(); ++i) {
        const std::string name = kHueBands.at(i);
        bands.at(i) = {s.number("mixer.hue." + name, 0) / 100.0,
                       s.number("mixer.saturation." + name, 0) / 100.0,
                       s.number("mixer.luminance." + name, 0) / 100.0};
    }
    return bands;
}

/// The change at `hue` (degrees): linear between the two neighbouring band centres.
Band blend(const std::array<Band, 8>& bands, double hue)
{
    const BandPosition at = bandAt(hue);
    const double t = at.t;
    const Band& a = bands.at(at.lo);
    const Band& b = bands.at(at.hi);
    return {a.hue + (t * (b.hue - a.hue)), a.saturation + (t * (b.saturation - a.saturation)),
            a.luminance + (t * (b.luminance - a.luminance))};
}

void hsvToRgb(double h, double s, double v, double* rgb)
{
    const double c = v * s;
    const double hp = std::fmod(h, 360.0) / 60.0;
    const double x = c * (1.0 - std::abs(std::fmod(hp, 2.0) - 1.0));
    double r = 0;
    double g = 0;
    double b = 0;
    switch (static_cast<int>(hp)) {
    case 0:
        r = c;
        g = x;
        break;
    case 1:
        r = x;
        g = c;
        break;
    case 2:
        g = c;
        b = x;
        break;
    case 3:
        g = x;
        b = c;
        break;
    case 4:
        r = x;
        b = c;
        break;
    default:
        r = c;
        b = x;
        break;
    }
    const double m = v - c;
    rgb[0] = r + m;
    rgb[1] = g + m;
    rgb[2] = b + m;
}

} // namespace

bool mixerActive(const Settings& s)
{
    return std::ranges::any_of(
        bandsOf(s), [](const Band& b) { return b.hue != 0.0 || b.saturation != 0.0 || b.luminance != 0.0; });
}

void mixerApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const std::array<Band, 8> bands = bandsOf(s);
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                std::array<double, 3> e{};
                for (std::size_t c = 0; c < 3; ++c) {
                    e.at(c) = color::perceptualEncode(std::max(0.0, double{p[c]}));
                }
                const Hsv hsv = toHsv(e);
                if (hsv.saturation <= 0.0) {
                    continue; // grey: no hue to act on
                }
                const double hue = hsv.hue;
                const double sat = hsv.saturation;
                const double v = hsv.value;
                const Band change = blend(bands, hue);
                const double weight = smoothstep(0.0, 0.2, sat); // near-grey pixels barely move
                const double newHue = std::fmod(hue + (change.hue * kHueReach * weight) + 360.0, 360.0);
                const double newSat = std::clamp(sat * (1.0 + (change.saturation * weight)), 0.0, 1.0);
                const double newV = v * (1.0 + (change.luminance * kLuminanceReach * weight));
                std::array<double, 3> out{};
                hsvToRgb(newHue, newSat, std::max(0.0, newV), out.data());
                for (std::size_t c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(color::perceptualDecode(out.at(c)));
                }
            }
        }
    });
}

} // namespace pfx::detail
