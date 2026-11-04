// SPDX-License-Identifier: Apache-2.0
#include "ops/curve.h"

#include <algorithm>
#include <cmath>

namespace pfx::detail {

MonotoneCurve::MonotoneCurve(const Curve& points)
{
    for (const CurvePoint& p : points) {
        m_x.push_back(p.x);
        m_y.push_back(p.y);
    }
    if (m_x.size() < 2) {
        m_x = {0.0, 1.0};
        m_y = {0.0, 1.0};
    }
    m_identity = std::ranges::all_of(points, [](const CurvePoint& p) { return p.x == p.y; });
    const std::size_t n = m_x.size();
    std::vector<double> secant(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        secant[i] = (m_y[i + 1] - m_y[i]) / (m_x[i + 1] - m_x[i]);
    }
    m_slope.assign(n, 0.0);
    m_slope[0] = secant[0];
    m_slope[n - 1] = secant[n - 2];
    for (std::size_t i = 1; i + 1 < n; ++i) {
        m_slope[i] = secant[i - 1] * secant[i] <= 0.0 ? 0.0 : (secant[i - 1] + secant[i]) / 2.0;
    }
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (secant[i] == 0.0) {
            m_slope[i] = 0.0;
            m_slope[i + 1] = 0.0;
            continue;
        }
        const double a = m_slope[i] / secant[i];
        const double b = m_slope[i + 1] / secant[i];
        const double h = a * a + b * b;
        if (h > 9.0) {
            const double t = 3.0 / std::sqrt(h);
            m_slope[i] = t * a * secant[i];
            m_slope[i + 1] = t * b * secant[i];
        }
    }
}

double MonotoneCurve::operator()(double x) const
{
    const std::size_t n = m_x.size();
    if (x <= m_x.front()) {
        return m_y.front() + (x - m_x.front()) * m_slope.front();
    }
    if (x >= m_x.back()) {
        return m_y.back() + (x - m_x.back()) * m_slope.back();
    }
    const auto upper = std::ranges::upper_bound(m_x, x);
    const auto i = static_cast<std::size_t>(std::distance(m_x.begin(), upper) - 1);
    const std::size_t j = std::min(i + 1, n - 1);
    const double h = m_x[j] - m_x[i];
    const double t = (x - m_x[i]) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * m_y[i] + (t3 - 2 * t2 + t) * h * m_slope[i] + (-2 * t3 + 3 * t2) * m_y[j] +
           (t3 - t2) * h * m_slope[j];
}

CurveLut::CurveLut(const MonotoneCurve& curve)
    : m_table(kSize)
{
    for (int i = 0; i < kSize; ++i) {
        m_table[static_cast<std::size_t>(i)] =
            static_cast<float>(curve(static_cast<double>(i) / (kSize - 1)));
    }
    constexpr double kStep = 1.0 / (kSize - 1);
    m_endSlope = (curve(1.0) - curve(1.0 - kStep)) / kStep;
}

double CurveLut::operator()(double x) const
{
    if (x >= 1.0) {
        return double{m_table.back()} + (x - 1.0) * m_endSlope;
    }
    if (x <= 0.0) {
        return double{m_table.front()};
    }
    const double pos = x * (kSize - 1);
    const auto i = static_cast<std::size_t>(pos);
    const double f = pos - static_cast<double>(i);
    return double{m_table[i]} + f * (double{m_table[i + 1]} - double{m_table[i]});
}

} // namespace pfx::detail
