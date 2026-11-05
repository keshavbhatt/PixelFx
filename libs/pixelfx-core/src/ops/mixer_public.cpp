// SPDX-License-Identifier: Apache-2.0
#include "ops/hue_bands.h"
#include "pfx/color.h"
#include "pfx/mixer.h"

#include <algorithm>
#include <array>

namespace pfx {

HueBands hueBands(double r, double g, double b)
{
    const std::array<double, 3> e = {color::perceptualEncode(std::max(0.0, r)),
                                     color::perceptualEncode(std::max(0.0, g)),
                                     color::perceptualEncode(std::max(0.0, b))};
    const detail::Hsv hsv = detail::toHsv(e);
    const detail::BandPosition at = detail::bandAt(hsv.hue);
    // The mixer's own fade toward grey (mixer.cpp).
    return {at.lo, at.hi, at.t, detail::smoothstep(0.0, 0.2, hsv.saturation)};
}

} // namespace pfx
