// SPDX-License-Identifier: Apache-2.0
// The encoders behind pfx::compress (ADR-010).
#pragma once

#include "pfx/io.h"
#include "pfx/quantize.h"
#include "pfx/result.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pfx::detail {

// Starts libvips once (io.cpp).
void startVips();

// ---- PNG (png_writer.cpp)

struct PngChunk
{
    std::string type; // four letters
    std::vector<std::uint8_t> data;
};

// Every chunk of a PNG file in order, or nothing when it is not one.
[[nodiscard]] std::optional<std::vector<PngChunk>> readPngChunks(const std::vector<std::uint8_t>& file);

struct PngRaster
{
    int width = 0;
    int height = 0;
    int colourType = 6; // 0 grey, 2 RGB, 3 palette, 4 grey and alpha, 6 RGBA
    int bitDepth = 8;   // 8 or 16; a palette may be 1, 2, 4 or 8
    // Samples row by row, unpacked: a byte each at 8 bits, two (big-endian) at 16, one index
    // per pixel for a palette.
    std::vector<std::uint8_t> samples;
    std::vector<std::array<std::uint8_t, 4>> palette;
};

// Writes a PNG: `early` chunks go before the palette (colour chunks: gAMA, cHRM, sRGB, iCCP),
// `late` ones after it, before the image data. `thorough` tries more filter and deflate
// combinations and keeps the smallest.
[[nodiscard]] std::vector<std::uint8_t> writePng(PngRaster raster, const std::vector<PngChunk>& early,
                                                 const std::vector<PngChunk>& late, bool thorough);

// A palette raster of the smallest bit depth for `image`.
[[nodiscard]] PngRaster paletteRaster(const IndexedImage& image);

// zlib's inflate, for compressed text chunks; nothing when the stream is damaged.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> inflateAll(const std::uint8_t* data, std::size_t size);

// ---- GIF (gif_writer.cpp)

struct GifAnimation
{
    int width = 0;
    int height = 0;
    std::vector<Rgba8Image> frames; // composited, as shown
    std::vector<int> delays;        // milliseconds, one per frame
    int loop = 0;                   // 0 for ever; 1 plays once (no loop extension)
};

// The animation stored without loss, each frame as only what changed when that is valid;
// nothing when a frame needs more than 256 colours.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> writeGif(const GifAnimation& animation);

// ---- JPEG (jpeg_lossless.cpp)

// The same JPEG with optimised Huffman tables, baseline or progressive, whichever is smaller;
// `removeExtras` drops the location and embedded previews from its markers. Trailing data after
// the image (MPF previews) never survives.
[[nodiscard]] Result<std::vector<std::uint8_t>> jpegLossless(const std::vector<std::uint8_t>& file,
                                                             bool removeExtras);

// ---- WebP (riff.cpp)

struct WebpKind
{
    bool lossless = false; // the image is VP8L
    bool animated = false;
};
[[nodiscard]] std::optional<WebpKind> webpKind(const std::vector<std::uint8_t>& file);
// The WebP with its EXIF and XMP chunks cleaned of the location and previews; nothing when the
// file is not a WebP this understands.
[[nodiscard]] std::optional<std::vector<std::uint8_t>>
webpWithoutExtras(const std::vector<std::uint8_t>& file);

// ---- Metadata (metadata.cpp)

// XMP without exif:GPS properties and without embedded thumbnails (xmp:Thumbnails).
[[nodiscard]] std::vector<std::uint8_t> xmpWithoutExtras(const std::vector<std::uint8_t>& xmp);
// A Photoshop APP13 payload ("Photoshop 3.0") without its thumbnail resources.
[[nodiscard]] std::vector<std::uint8_t> photoshopWithoutPreviews(const std::vector<std::uint8_t>& payload);
// Exif without the location and without the thumbnail.
[[nodiscard]] std::vector<std::uint8_t> exifWithoutExtras(std::vector<std::uint8_t> exif);

} // namespace pfx::detail
