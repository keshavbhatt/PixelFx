// SPDX-License-Identifier: Apache-2.0
// Calibration and profile (FEATURES P8), the colour a camera profile gives before any slider:
// - calibration turns each primary toward its neighbours (hue) and makes it purer or greyer
//   (saturation), as one matrix that keeps white white, and tints the shadows green or magenta;
// - the vivid profile adds the contrast and colour of Lightroom's Adobe Vivid.
#include "ops/hue_bands.h"
#include "ops/ops.h"

#include <array>
#include <cmath>
#include <string>

namespace pfx::detail {

namespace {

constexpr double kHueReach = 0.3;         // share of the neighbour primary mixed in at hue +-100
constexpr double kSaturationReach = 0.5;  // saturation change at +-100
constexpr double kShadowTintReach = 0.05; // perceptual units at the darkest tones, at +-100
constexpr std::array<const char*, 3> kPrimaries = {"red", "green", "blue"};

/// The calibration matrix; it maps (1, 1, 1) to itself.
color::Mat3 calibrationMatrix(const Settings& s)
{
    color::Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::string key = std::string("calibration.") + kPrimaries.at(i);
        const double hue = s.number(key + ".hue", 0.0) / 100.0;
        const double saturation = s.number(key + ".saturation", 0.0) / 100.0;
        // Red turns toward green (+) or blue (-), green toward blue or red, blue toward red or green.
        color::Vec3 column{};
        column.at(i) = 1.0;
        column.at(hue > 0.0 ? (i + 1) % 3 : (i + 2) % 3) += std::abs(hue) * kHueReach;
        const double luma = color::luminance(column[0], column[1], column[2]);
        for (double& v : column) {
            v = luma + ((v - luma) * (1.0 + (saturation * kSaturationReach)));
        }
        for (std::size_t row = 0; row < 3; ++row) {
            m.at(row).at(i) = column.at(row);
        }
    }
    for (auto& row : m) {
        const double white = row[0] + row[1] + row[2];
        for (double& v : row) {
            v /= white;
        }
    }
    return m;
}

bool primariesMoved(const Settings& s)
{
    return std::ranges::any_of(kPrimaries, [&](const char* primary) {
        const std::string key = std::string("calibration.") + primary;
        return s.number(key + ".hue", 0.0) != 0.0 || s.number(key + ".saturation", 0.0) != 0.0;
    });
}

} // namespace

bool calibrationActive(const Settings& s)
{
    return primariesMoved(s) || s.number("calibration.shadows.tint", 0.0) != 0.0;
}

void calibrationApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const bool matrix = primariesMoved(s);
    const color::Mat3 m = calibrationMatrix(s);
    // Positive is magenta, negative green, as Lightroom's Shadows tint.
    const double tint = kShadowTintReach * s.number("calibration.shadows.tint", 0.0) / 100.0;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                color::Vec3 v = {double{p[0]}, double{p[1]}, double{p[2]}};
                if (matrix) {
                    v = color::transform(m, v);
                }
                if (tint != 0.0) {
                    const double lightness =
                        color::perceptualEncode(std::max(0.0, color::luminance(v[0], v[1], v[2])));
                    const double w = tint * (1.0 - smoothstep(0.0, 0.5, lightness));
                    const std::array<double, 3> shift = {w * 0.5, -w, w * 0.5};
                    for (std::size_t c = 0; c < 3; ++c) {
                        v.at(c) = color::perceptualDecode(
                            std::max(0.0, color::perceptualEncode(std::max(0.0, v.at(c))) + shift.at(c)));
                    }
                }
                for (std::size_t c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(v.at(c));
                }
            }
        }
    });
}

bool profileActive(const Settings& s)
{
    return s.text("color.profile", "standard") == "vivid";
}

void profileApply(Image& image, const Settings& /*s*/, const OpContext& ctx)
{
    constexpr double kContrast = 0.18; // a gentle S-curve around middle grey
    constexpr double kChroma = 1.15;   // colour a little purer
    constexpr double kPivot = 0.4633;  // middle grey (0.18) in the perceptual encoding
    const double g = std::exp2(kContrast);
    applyLuminanceMapping(image, ctx, [g](double luma) {
        const double p = color::perceptualEncode(luma);
        double out = p;
        if (p > 0.0 && p < kPivot) {
            out = kPivot * std::pow(p / kPivot, g);
        } else if (p >= kPivot && p <= 1.0) {
            out = 1.0 - ((1.0 - kPivot) * std::pow((1.0 - p) / (1.0 - kPivot), g));
        }
        return color::perceptualDecode(out);
    });
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double luma = color::luminance(p[0], p[1], p[2]);
                for (int c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(std::max(0.0, luma + ((double{p[c]} - luma) * kChroma)));
                }
            }
        }
    });
}

} // namespace pfx::detail
