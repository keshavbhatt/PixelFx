// SPDX-License-Identifier: Apache-2.0
#include "ops/curve.h"
#include "ops/parametric_curve.h"
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

std::vector<double> sampleParametricCurve(const Settings& settings, int samples)
{
    std::vector<double> out;
    if (samples < 2) {
        return out;
    }
    const detail::ParametricCurve curve(settings);
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const double x = static_cast<double>(i) / (samples - 1);
        out.push_back(x <= 0.0 || x >= 1.0 ? x : curve(x));
    }
    return out;
}

} // namespace pfx
