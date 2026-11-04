// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>

namespace pfx::detail {

// A single-channel float plane.
struct Plane
{
    int width = 0;
    int height = 0;
    std::vector<float> values;

    Plane() = default;
    Plane(int w, int h);
    [[nodiscard]] float& at(int x, int y);
    [[nodiscard]] float at(int x, int y) const;
};

// Gaussian blur with edge clamping. Exact kernel for small sigma, three box passes (within
// about 3 percent of a true Gaussian) for large sigma, so cost does not grow with the radius.
[[nodiscard]] Plane gaussianBlur(const Plane& source, double sigma, int threads);

// The smallest value within `radius` (a square window), edges clamped (van Herk/Gil-Werman:
// three comparisons per value whatever the radius).
[[nodiscard]] Plane minFilter(const Plane& source, int radius, int threads);

// Edge-preserving smoothing of `input` steered by `guide` (He, Sun and Tang's guided filter),
// smoothing at `sigma`; `epsilon` sets how strong an edge in the guide must be to be kept.
[[nodiscard]] Plane guidedFilter(const Plane& guide, const Plane& input, double sigma, double epsilon,
                                 int threads);

// The guided filter's smoothed coefficients: the result is slope * guide + offset. Computed on a
// small copy and sampled at full size, they give the full-size result at a fraction of the cost
// (He and Sun's fast guided filter).
struct GuidedCoefficients
{
    Plane slope;
    Plane offset;
};
[[nodiscard]] GuidedCoefficients guidedCoefficients(const Plane& guide, const Plane& input, double sigma,
                                                    double epsilon, int threads);

// Bilinear sample of `plane` at (x, y) in its own pixel units (centres at whole numbers).
[[nodiscard]] float sampleBilinear(const Plane& plane, double x, double y);

} // namespace pfx::detail
