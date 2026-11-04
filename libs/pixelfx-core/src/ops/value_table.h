// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace pfx::detail {

// A smooth function of a linear-light value (0 and up), tabulated once and read with a square
// root and a linear interpolation: far cheaper per pixel than the pow() calls inside most
// operations. Indexing by the square root keeps the steps fine near black, where encodings are
// steep. Values above kMax (bright highlights) call the function itself.
class ValueTable
{
public:
    static constexpr double kMax = 4.0;
    static constexpr int kSize = 8192;

    explicit ValueTable(std::function<double(double)> function)
        : m_function(std::move(function))
        , m_table(kSize + 1)
    {
        for (int i = 0; i <= kSize; ++i) {
            const double root = std::sqrt(kMax) * i / kSize;
            m_table[static_cast<std::size_t>(i)] = m_function(root * root);
        }
    }

    [[nodiscard]] double operator()(double x) const
    {
        if (x >= kMax) {
            return m_function(x);
        }
        const double position = std::sqrt(std::max(0.0, x)) * (kSize / std::sqrt(kMax));
        const auto i = static_cast<std::size_t>(position);
        const double t = position - static_cast<double>(i);
        return m_table[i] + (t * (m_table[i + 1] - m_table[i]));
    }

private:
    std::function<double(double)> m_function;
    std::vector<double> m_table;
};

} // namespace pfx::detail
