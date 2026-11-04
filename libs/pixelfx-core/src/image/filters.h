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

} // namespace pfx::detail
