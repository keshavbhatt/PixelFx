// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/settings.h"

#include <array>

namespace pfx {

struct Size
{
    int width = 0;
    int height = 0;
    bool operator==(const Size&) const = default;
};

// A point as shares of an image's width and height.
struct Corner
{
    double x = 0.0;
    double y = 0.0;
};

// Whether four corners (top left first, clockwise, as shares of an image) make a shape the
// four-corner perspective can square: convex, clockwise as the image's own corners, every side
// at least 1% long.
[[nodiscard]] bool cornersUsable(const std::array<Corner, 4>& corners);

// The size the geometry settings (turn, flip, straighten, perspective, crop) give an image of
// `input` size, without touching pixels: what the canvas and the export sheet show.
[[nodiscard]] Size outputSize(const Settings& settings, Size input);

// The largest rectangle with the image's own shape that fits inside the image turned by
// `degrees`, as a share of the image's size (1 when not turned): straightening crops to it so
// no empty corners show.
[[nodiscard]] double straightenScale(double degrees, Size input);

} // namespace pfx
