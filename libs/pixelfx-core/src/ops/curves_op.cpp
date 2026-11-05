// SPDX-License-Identifier: Apache-2.0
// Curves work on sRGB-encoded values of the working space, the way users expect a curve to
// read. First the parametric curve (four tone regions, as Lightroom's, split at three movable
// points), then the RGB point curve on every channel, then each channel's own curve.
#include "ops/curve.h"
#include "ops/ops.h"
#include "ops/parametric_curve.h"
#include "ops/value_table.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace pfx::detail {

namespace {

std::optional<CurveLut> lutFor(const Settings& s, std::string_view key)
{
    const Curve points = s.curve(key);
    if (points.size() < 2) {
        return std::nullopt;
    }
    const MonotoneCurve curve(points);
    if (curve.isIdentity()) {
        return std::nullopt;
    }
    return CurveLut(curve);
}

} // namespace

bool curvesActive(const Settings& s)
{
    constexpr std::array<std::string_view, 4> kKeys = {"curves.rgb", "curves.r", "curves.g", "curves.b"};
    return parametricActive(s) || std::ranges::any_of(kKeys, [&](std::string_view key) {
               const Curve c = s.curve(key);
               return c.size() >= 2 && !MonotoneCurve(c).isIdentity();
           });
}

void curvesApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const std::optional<ParametricCurve> parametric =
        parametricActive(s) ? std::optional<ParametricCurve>(s) : std::nullopt;
    const auto rgb = lutFor(s, "curves.rgb");
    const std::optional<CurveLut> channel[3] = {lutFor(s, "curves.r"), lutFor(s, "curves.g"),
                                                lutFor(s, "curves.b")};
    // Each channel's whole path (encode, curves, decode) is one function of its value: tabulate it.
    std::vector<ValueTable> tables;
    tables.reserve(3);
    for (const auto& own : channel) {
        tables.emplace_back([&parametric, &rgb, &own](double linear) {
            double v = color::srgbEncode(linear);
            if (parametric) {
                v = (*parametric)(v);
            }
            if (rgb) {
                v = (*rgb)(v);
            }
            if (own) {
                v = (*own)(v);
            }
            return color::srgbDecode(std::max(0.0, v));
        });
    }
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                for (std::size_t c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(tables[c](double{std::max(0.0F, p[c])}));
                }
            }
        }
    });
}

} // namespace pfx::detail
