// SPDX-License-Identifier: Apache-2.0
#include "ops/curve.h"
#include "pfx/curve.h"

namespace pfx {

std::vector<double> sampleCurve(const Curve& points, int samples)
{
    std::vector<double> out;
    if (samples < 2) {
        return out;
    }
    const detail::MonotoneCurve curve(points);
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        out.push_back(curve(static_cast<double>(i) / (samples - 1)));
    }
    return out;
}

} // namespace pfx
