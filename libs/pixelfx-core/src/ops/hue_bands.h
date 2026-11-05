// SPDX-License-Identifier: Apache-2.0
// The eight colour bands of the colour mixer and the B&W mix, and where a hue lies between
// them. Works on the perceptual encoding in HSV terms.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace pfx::detail {

inline constexpr std::array<const char*, 8> kHueBands = {"red",  "orange", "yellow", "green",
                                                         "aqua", "blue",   "purple", "magenta"};
inline constexpr std::array<double, 8> kHueBandCentres = {0, 30, 60, 120, 180, 240, 270, 300}; // degrees

/// The two bands a hue lies between and how far it is from the first (0) to the second (1).
struct BandPosition
{
    std::size_t lo = 0;
    std::size_t hi = 0;
    double t = 0;
};

[[nodiscard]] inline BandPosition bandAt(double hue)
{
    std::size_t lo = kHueBandCentres.size() - 1;
    for (std::size_t i = 0; i < kHueBandCentres.size(); ++i) {
        if (hue >= kHueBandCentres.at(i)) {
            lo = i;
        }
    }
    const std::size_t hi = (lo + 1) % kHueBandCentres.size();
    const double start = kHueBandCentres.at(lo);
    const double end = hi == 0 ? 360.0 : kHueBandCentres.at(hi);
    return {lo, hi, (hue - start) / (end - start)};
}

/// One value per band, linear between the two bands around `hue` (degrees).
[[nodiscard]] inline double bandValue(const std::array<double, 8>& values, double hue)
{
    const BandPosition at = bandAt(hue);
    return values.at(at.lo) + (at.t * (values.at(at.hi) - values.at(at.lo)));
}

struct Hsv
{
    double hue = 0;        // degrees
    double saturation = 0; // 0 to 1
    double value = 0;
};

/// HSV of perceptually encoded RGB; a grey has saturation 0 and hue 0.
[[nodiscard]] inline Hsv toHsv(const std::array<double, 3>& e)
{
    const double v = std::max({e[0], e[1], e[2]});
    const double chroma = v - std::min({e[0], e[1], e[2]});
    if (v <= 0.0 || chroma <= 0.0) {
        return {0.0, 0.0, std::max(0.0, v)};
    }
    double hue = 0;
    if (v == e[0]) {
        hue = 60.0 * std::fmod(((e[1] - e[2]) / chroma) + 6.0, 6.0);
    } else if (v == e[1]) {
        hue = 60.0 * (((e[2] - e[0]) / chroma) + 2.0);
    } else {
        hue = 60.0 * (((e[0] - e[1]) / chroma) + 4.0);
    }
    return {hue, chroma / v, v};
}

[[nodiscard]] inline double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - (2.0 * t));
}

} // namespace pfx::detail
