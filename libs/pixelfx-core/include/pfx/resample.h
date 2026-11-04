// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"

namespace pfx {

// Downscales so the long edge is at most `longEdge` pixels (Lanczos 3, in linear light, so
// highlights keep their weight). Images already small enough come back unchanged: never enlarges.
[[nodiscard]] Result<Image> fitLongEdge(const Image& image, int longEdge);

} // namespace pfx
