// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>

namespace pfx {

// Where a colour sits among the colour mixer's eight bands (red, orange, yellow, green, aqua,
// blue, purple, magenta, in the order of the mixer.* settings): between `first` and `second`,
// `secondShare` (0 to 1) of the way to the second. `weight` is how much the mixer acts on it
// (0 for a grey, 1 for a clear colour), as the mixer itself weighs it.
struct HueBands
{
    std::size_t first = 0;
    std::size_t second = 0;
    double secondShare = 0.0;
    double weight = 0.0;
};

// The bands of a colour in the working space (linear Rec.2020), as the mixer and the B&W mix see it.
[[nodiscard]] HueBands hueBands(double r, double g, double b);

} // namespace pfx
