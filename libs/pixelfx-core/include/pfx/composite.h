// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"

#include <cstddef>
#include <cstdint>

namespace pfx {

// An 8-bit sRGB picture with premultiplied alpha (bytes R, G, B, A), placed with its top left
// corner at (x, y) on an image: a watermark drawn by the caller, for example.
struct OverlayView
{
    const std::uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    std::ptrdiff_t stride = 0; // bytes from one row to the next
    int x = 0;
    int y = 0;
};

// Lays the overlay over the image in linear light, scaled by `opacity` (0 to 1). The parts
// outside the image are left out.
void blendOverlay(Image& image, const OverlayView& overlay, float opacity = 1.0F);

// Mean luminance (linear Rec.2020) of a rectangle, clipped to the image; 0 when nothing is left.
[[nodiscard]] double meanLuminance(const Image& image, int x, int y, int width, int height);

} // namespace pfx
