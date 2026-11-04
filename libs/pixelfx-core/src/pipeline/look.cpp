// SPDX-License-Identifier: Apache-2.0
#include "pfx/look.h"

#include "core/parallel.h"
#include "ops/ops.h"

#include <algorithm>

namespace pfx {

void applyLook(Image& image, const Look& look, float strength, int threads)
{
    strength = std::clamp(strength, 0.0F, 1.0F);
    if (strength <= 0.0F || look.steps.empty()) {
        return;
    }
    const Image before = strength < 1.0F ? image : Image();
    const detail::OpContext ctx{threads};
    for (const LookStep& step : look.steps) {
        if (!step.enabled) {
            continue;
        }
        if (step.op == "lut") {
            const auto it = look.resources.find(step.params.text("resource", ""));
            if (it != look.resources.end()) {
                applyLut(image, it->second, static_cast<float>(step.params.number("mix", 100.0) / 100.0),
                         threads);
            }
            continue;
        }
        if (step.op != "geometry") {
            detail::applyOperation(step.op, image, step.params, ctx);
        }
    }
    if (strength < 1.0F) {
        // Linear light: a half-strength look is half the change in light, not in encoded values.
        const float* in = before.data();
        float* out = image.data();
        detail::parallelFor(image.height(), threads, [&](int y0, int y1) {
            const auto from = static_cast<std::size_t>(y0) * static_cast<std::size_t>(image.width()) * 4;
            const auto to = static_cast<std::size_t>(y1) * static_cast<std::size_t>(image.width()) * 4;
            for (std::size_t i = from; i < to; ++i) {
                out[i] =
                    in[i] +
                    (strength * (out[i] - in[i])); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            }
        });
    }
}

} // namespace pfx
