// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/settings.h"

namespace pfx {

// Settings for Light and Color worked out from the image's statistics (FEATURES E10): exposure
// toward middle grey, blacks and whites stretched when the image does not reach them, a little
// contrast when tones are bunched, a mild white-balance correction for strong casts, a little
// vibrance for muted images. Conservative: an image that already looks right changes little.
// Values are rounded to what the sliders show; keys left at their default are not set.
[[nodiscard]] Settings autoAdjust(const Image& image);

} // namespace pfx
