// SPDX-License-Identifier: Apache-2.0
// Exposure and tone (contrast, highlights, shadows, whites, blacks). Both work on luminance
// and keep each pixel's hue and saturation (applyLuminanceMapping).
#include "ops/curve.h"
#include "ops/ops.h"

#include <cmath>

namespace pfx::detail {

bool exposureActive(const Settings& s)
{
    return s.number("light.exposure", 0.0) != 0.0;
}

void exposureApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const auto gain = static_cast<float>(std::exp2(s.number("light.exposure", 0.0)));
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                p[0] *= gain;
                p[1] *= gain;
                p[2] *= gain;
            }
        }
    });
}

namespace {

struct ToneParams
{
    double contrast;
    double highlights;
    double shadows;
    double whites;
    double blacks;
};

ToneParams toneParams(const Settings& s)
{
    return {s.number("light.contrast", 0) / 100.0, s.number("light.highlights", 0) / 100.0,
            s.number("light.shadows", 0) / 100.0, s.number("light.whites", 0) / 100.0,
            s.number("light.blacks", 0) / 100.0};
}

// Version 1 of the tone operation is global: one curve in the perceptual encoding built from
// five control points (0, 1/4, 1/2, 3/4, 1) that blacks, shadows, highlights and whites move,
// interpolated monotonically, then a contrast S-curve pivoting on middle grey.
MonotoneCurve buildToneCurve(const ToneParams& t)
{
    constexpr double kEndReach = 0.08;
    constexpr double kQuarterReach = 0.12;
    constexpr double kMinStep = 0.02;
    Curve points = {{0.0, kEndReach * t.blacks},
                    {0.25, 0.25 + kQuarterReach * t.shadows},
                    {0.5, 0.5},
                    {0.75, 0.75 + kQuarterReach * t.highlights},
                    {1.0, 1.0 + kEndReach * t.whites}};
    for (std::size_t i = 1; i < points.size(); ++i) {
        points[i].y = std::max(points[i].y, points[i - 1].y + kMinStep);
    }
    return MonotoneCurve(points);
}

double contrastCurve(double p, double amount)
{
    if (amount == 0.0 || p <= 0.0) {
        return p;
    }
    constexpr double kPivot = 0.4633; // middle grey (0.18) in the perceptual encoding
    const double g = std::exp2(amount);
    if (p < kPivot) {
        return kPivot * std::pow(p / kPivot, g);
    }
    if (p <= 1.0) {
        return 1.0 - (1.0 - kPivot) * std::pow((1.0 - p) / (1.0 - kPivot), g);
    }
    return p; // headroom above white passes through
}

} // namespace

bool toneActive(const Settings& s)
{
    const ToneParams t = toneParams(s);
    return t.contrast != 0.0 || t.highlights != 0.0 || t.shadows != 0.0 || t.whites != 0.0 || t.blacks != 0.0;
}

void toneApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const ToneParams t = toneParams(s);
    const CurveLut curve{buildToneCurve(t)};
    applyLuminanceMapping(image, ctx, [&](double luma) {
        const double p = curve(color::perceptualEncode(luma));
        return color::perceptualDecode(contrastCurve(p, t.contrast));
    });
}

} // namespace pfx::detail
