// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"

namespace pfx {

// Downscales so the long edge is at most `longEdge` pixels (Lanczos 3, in linear light, so
// highlights keep their weight). Images already small enough come back unchanged: never enlarges.
[[nodiscard]] Result<Image> fitLongEdge(const Image& image, int longEdge);

// Exactly `width` by `height` (size presets: Instagram portrait, a print): scaled to cover the
// frame, then the middle kept. May enlarge.
[[nodiscard]] Result<Image> fillSize(const Image& image, int width, int height);
// Exactly `width` by `height`: scaled to fit inside, centred on a border of grey `border`
// (linear light, 0 black, 1 white). May enlarge.
[[nodiscard]] Result<Image> fitInside(const Image& image, int width, int height, float border);

} // namespace pfx
