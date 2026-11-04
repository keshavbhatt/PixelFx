// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/settings.h"

#include <vector>

namespace pfx::detail {

// Monotone cubic interpolation through control points (Fritsch and Carlson), so a curve never
// reverses even with points placed close together. Beyond the last point it continues with
// the end slope; before the first, with the start slope.
class MonotoneCurve
{
public:
    explicit MonotoneCurve(const Curve& points);
    [[nodiscard]] double operator()(double x) const;
    [[nodiscard]] bool isIdentity() const { return m_identity; }

private:
    std::vector<double> m_x;
    std::vector<double> m_y;
    std::vector<double> m_slope;
    bool m_identity = false;
};

// A lookup table of a curve on [0, 1], linear interpolation between entries.
class CurveLut
{
public:
    static constexpr int kSize = 4096;
    explicit CurveLut(const MonotoneCurve& curve);
    [[nodiscard]] double operator()(double x) const;

private:
    std::vector<float> m_table;
    double m_endSlope = 1.0;
};

} // namespace pfx::detail
