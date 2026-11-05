// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"

namespace pfx {

// Downscales so the long edge is at most `longEdge` pixels (Lanczos 3, in linear light, so
// highlights keep their weight). Images already small enough come back unchanged: never enlarges.
[[nodiscard]] Result<Image> fitLongEdge(const Image& image, int longEdge);

// Exactly `width` by `height` (size presets: Instagram portrait, a print): scaled to cover the
// frame, then the part at `anchorX`, `anchorY` kept: 0 the left or top, 0.5 the middle, 1 the
// right or bottom. May enlarge.
[[nodiscard]] Result<Image> fillSize(const Image& image, int width, int height, float anchorX = 0.5F,
                                     float anchorY = 0.5F);
// Exactly `width` by `height`: scaled to fit inside, centred on a border of grey `border`
// (linear light, 0 black, 1 white). May enlarge.
[[nodiscard]] Result<Image> fitInside(const Image& image, int width, int height, float border);

// Sharpening for where the resized photo is going, on its luminance (no colour fringes), in
// output pixels: it is applied once the image has its final size.
enum class OutputSharpen
{
    None,
    Screen, // gentle, at the pixel level: resizing softens fine detail
    Print,  // stronger and wider, as ink spreads on paper (for about 300 pixels per inch)
};
void sharpenForOutput(Image& image, OutputSharpen kind);

} // namespace pfx
