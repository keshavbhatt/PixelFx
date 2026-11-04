// SPDX-License-Identifier: Apache-2.0
// White balance, vibrance and saturation.
#include "ops/ops.h"

#include <cmath>

namespace pfx::detail {

namespace {

// Relative white balance: the photo is treated as lit by a white shifted from D65 along the
// Planckian locus (temperature) and across it (tint), and adapted back to D65 with Bradford.
// Positive temperature warms the photo, positive tint makes it more magenta.
color::Mat3 whiteBalanceMatrix(double temperature, double tint)
{
    constexpr double kD65Kelvin = 6504.0;
    constexpr std::array<double, 2> kD65 = {0.31271, 0.32902};
    constexpr double kTintReach = 0.02;
    const double kelvin = kD65Kelvin * std::exp2(temperature / 100.0);
    const auto shifted = color::planckianXy(kelvin);
    const auto reference = color::planckianXy(kD65Kelvin);
    const std::array<double, 2> source = {kD65[0] + (shifted[0] - reference[0]),
                                          kD65[1] + (shifted[1] - reference[1]) + kTintReach * tint / 100.0};
    const color::Mat3 adapt = color::bradford(source, kD65);
    return color::multiply(color::kXyzToRec2020, color::multiply(adapt, color::kRec2020ToXyz));
}

} // namespace

bool whiteBalanceActive(const Settings& s)
{
    return s.number("color.wb.temperature", 0.0) != 0.0 || s.number("color.wb.tint", 0.0) != 0.0;
}

void whiteBalanceApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const color::Mat3 m =
        whiteBalanceMatrix(s.number("color.wb.temperature", 0.0), s.number("color.wb.tint", 0.0));
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const color::Vec3 v = color::transform(m, {double{p[0]}, double{p[1]}, double{p[2]}});
                p[0] = static_cast<float>(v[0]);
                p[1] = static_cast<float>(v[1]);
                p[2] = static_cast<float>(v[2]);
            }
        }
    });
}

bool vibranceSaturationActive(const Settings& s)
{
    return s.number("color.vibrance", 0.0) != 0.0 || s.number("color.saturation", 0.0) != 0.0;
}

// Saturation scales every pixel's distance from its grey. Vibrance does the same with a
// strength that falls as the pixel is already saturated, so muted colours gain most.
void vibranceSaturationApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double vibrance = s.number("color.vibrance", 0.0) / 100.0;
    const double saturation = 1.0 + s.number("color.saturation", 0.0) / 100.0;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double luma = color::luminance(p[0], p[1], p[2]);
                const double hi = double{std::max({p[0], p[1], p[2]})};
                const double lo = double{std::min({p[0], p[1], p[2]})};
                const double existing = hi > 1e-6 ? std::clamp((hi - lo) / hi, 0.0, 1.0) : 0.0;
                const double k = (1.0 + vibrance * (1.0 - existing)) * saturation;
                for (int c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(luma + (double{p[c]} - luma) * k);
                }
            }
        }
    });
}

} // namespace pfx::detail
