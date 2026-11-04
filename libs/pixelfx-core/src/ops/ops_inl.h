// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/parallel.h"
#include "ops/value_table.h"
#include "pfx/color.h"

#include <algorithm>

namespace pfx::detail {

template <typename Mapping>
void applyLuminanceMapping(Image& image, const OpContext& ctx, const Mapping& mapping)
{
    constexpr double kTiny = 1e-5;
    const ValueTable table([&mapping](double luma) { return mapping(luma); });
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double luma = std::max(0.0, color::luminance(p[0], p[1], p[2]));
                const double mapped = table(luma);
                if (luma > kTiny) {
                    const double ratio = mapped / luma;
                    for (int c = 0; c < 3; ++c) {
                        p[c] = static_cast<float>(double{p[c]} * ratio);
                    }
                } else {
                    const double lift = mapped - luma;
                    for (int c = 0; c < 3; ++c) {
                        p[c] = static_cast<float>(double{p[c]} + lift);
                    }
                }
            }
        }
    });
}

} // namespace pfx::detail
