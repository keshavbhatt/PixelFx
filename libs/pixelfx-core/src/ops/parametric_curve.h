// SPDX-License-Identifier: Apache-2.0
// The parametric curve (FEATURES P8): four tone regions, as Lightroom's, lifted or lowered and
// split at three movable points. Shared by the curves operation and the public sampler.
#pragma once

#include "pfx/settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace pfx::detail {

inline constexpr std::array<std::string_view, 4> kRegions = {"shadows", "darks", "lights", "highlights"};

inline bool parametricActive(const Settings& s)
{
    return std::ranges::any_of(kRegions, [&](std::string_view region) {
        return s.number("curves.parametric." + std::string(region), 0.0) != 0.0;
    });
}

/// The parametric curve, sampled on [0, 1] and kept rising.
class ParametricCurve
{
public:
    explicit ParametricCurve(const Settings& s)
    {
        constexpr double kReach = 0.25; // largest lift of a region at +-100
        constexpr double kEnds = 0.05;  // black and white stay where they are
        std::array<double, 3> split = {s.number("curves.parametric.split.shadows", 25.0) / 100.0,
                                       s.number("curves.parametric.split.midtones", 50.0) / 100.0,
                                       s.number("curves.parametric.split.highlights", 75.0) / 100.0};
        std::ranges::sort(split);
        const std::array<double, 5> edges = {0.0, split[0], split[1], split[2], 1.0};
        std::array<double, 4> amount{};
        for (std::size_t k = 0; k < kRegions.size(); ++k) {
            amount.at(k) = kReach * s.number("curves.parametric." + std::string(kRegions.at(k)), 0.0) / 100.0;
        }
        m_samples.resize(kSamples + 1);
        double highest = 0.0;
        for (std::size_t i = 0; i <= kSamples; ++i) {
            const double x = static_cast<double>(i) / kSamples;
            double lift = 0.0;
            for (std::size_t k = 0; k < 4; ++k) {
                // A raised cosine over the region, spreading half a region into each neighbour.
                const double centre = (edges.at(k) + edges.at(k + 1)) / 2.0;
                const double half = std::max(0.02, edges.at(k + 1) - edges.at(k));
                const double t = std::abs(x - centre) / half;
                if (t < 1.0) {
                    lift += amount.at(k) * 0.5 * (1.0 + std::cos(std::numbers::pi * t));
                }
            }
            const double ends = std::min({1.0, x / kEnds, (1.0 - x) / kEnds});
            highest = std::max(highest, std::clamp(x + (lift * ends), 0.0, 1.0));
            m_samples[i] = highest;
        }
    }

    double operator()(double v) const
    {
        if (v <= 0.0 || v >= 1.0) {
            return v; // headroom passes through
        }
        const double at = v * kSamples;
        const auto i = static_cast<std::size_t>(at);
        const double t = at - static_cast<double>(i);
        return m_samples[i] + (t * (m_samples[std::min(i + 1, kSamples)] - m_samples[i]));
    }

private:
    static constexpr std::size_t kSamples = 1024;
    std::vector<double> m_samples;
};

} // namespace pfx::detail
