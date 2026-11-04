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

// The white balance (color.wb.temperature and color.wb.tint) that makes a colour neutral grey:
// the white balance picker. `r`, `g`, `b` are working-space values (linear Rec.2020) of the
// photo before its own white balance. Clamped to the sliders' range; empty for black.
[[nodiscard]] Settings neutralWhiteBalance(double r, double g, double b);

} // namespace pfx
