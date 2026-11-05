// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/io.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace pfx {

// An image of palette indexes: what PNG palettes and GIF frames store.
struct IndexedImage
{
    int width = 0;
    int height = 0;
    std::vector<std::array<std::uint8_t, 4>> palette; // straight R, G, B, A
    std::vector<std::uint8_t> indexes;                // one per pixel, row by row
};

// The image's own colours as a palette, when it has at most `maxColours` (no change at all).
[[nodiscard]] std::optional<IndexedImage> exactPalette(const Rgba8Image& image, int maxColours = 256);

struct QuantizeOptions
{
    int maxColours = 256; // 2..256
    bool dither = true;   // Floyd-Steinberg error diffusion
};

struct Quantized
{
    IndexedImage image;
    // How close the palette comes to the image, 0..100 on the scale pngquant uses: 100 is no
    // difference, 80 hard to see in a photo, under 50 visibly posterised.
    int quality = 100;
};

// Reduces the image to a palette (ADR-010): colours are binned, split by median cut on the
// largest error, refined by k-means, then mapped with error diffusion. Fully transparent pixels
// share one entry. Images with few enough colours get exactPalette's result at quality 100.
[[nodiscard]] Quantized quantize(const Rgba8Image& image, const QuantizeOptions& options = {});

// The quality figure of a mean squared error per channel (values 0..1), and back.
[[nodiscard]] int qualityOfError(double meanSquaredError);
[[nodiscard]] double errorOfQuality(int quality);

} // namespace pfx
