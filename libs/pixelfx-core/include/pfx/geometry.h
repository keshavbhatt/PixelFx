// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/settings.h"

namespace pfx {

struct Size
{
    int width = 0;
    int height = 0;
    bool operator==(const Size&) const = default;
};

// The size the geometry settings (turn, flip, straighten, crop) give an image of `input` size,
// without touching pixels: what the canvas and the export sheet show.
[[nodiscard]] Size outputSize(const Settings& settings, Size input);

// The largest rectangle with the image's own shape that fits inside the image turned by
// `degrees`, as a share of the image's size (1 when not turned): straightening crops to it so
// no empty corners show.
[[nodiscard]] double straightenScale(double degrees, Size input);

} // namespace pfx
