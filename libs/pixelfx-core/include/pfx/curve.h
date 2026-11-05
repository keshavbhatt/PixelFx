// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/settings.h"

#include <vector>

namespace pfx {

// The curve exactly as the curves operation applies it (monotone cubic through the points), for
// drawing it in an editor. `samples` values for x evenly spaced over 0..1.
[[nodiscard]] std::vector<double> sampleCurve(const Curve& points, int samples);
// The parametric curve of `settings` (curves.parametric.*), sampled the same way: what the
// curves operation applies before the point curves, in the sRGB encoding.
[[nodiscard]] std::vector<double> sampleParametricCurve(const Settings& settings, int samples);

} // namespace pfx
