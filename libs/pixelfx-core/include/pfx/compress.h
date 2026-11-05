// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/io.h"
#include "pfx/result.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace pfx {

// Making image files smaller without editing them (FEATURES M10). Every step runs in-process
// through linked libraries (ADR-002, ADR-010); the file keeps its format.

enum class JpegCompression
{
    Lossless, // the same pixels: Huffman tables optimised, progressive when smaller
    Quality,  // encoded again at CompressOptions::jpegQuality
};

enum class PngCompression
{
    Lossless,     // the same pixels: smallest colour type and bit depth, best filters and deflate
    FewerColours, // a palette of at most 256 colours, dithered, unless that falls below pngQuality
};

struct CompressOptions
{
    JpegCompression jpeg = JpegCompression::Lossless;
    int jpegQuality = 80; // 1..100
    PngCompression png = PngCompression::Lossless;
    // The lowest quality (0..100) a palette may have; below it the PNG stays lossless.
    int pngQuality = 85;
    // Removes the location (Exif GPS, XMP exif:GPS) and embedded previews (the Exif thumbnail,
    // XMP and Photoshop thumbnails, JFIF extension thumbnails, FlashPix and MPF previews).
    // Camera details, copyright, the colour profile and comments stay.
    bool removeExtras = true;
};

// How the new bytes were made.
enum class CompressMethod
{
    JpegLossless,
    JpegQuality,
    PngLossless,
    PngFewerColours,
    TiffLossless,
    WebpLossless, // a lossless WebP encoded again, without loss
    WebpMetadata, // a lossy or animated WebP: only its metadata changed
    GifLossless,  // frames stored again without loss
    Unchanged,    // the format is not compressed here, or nothing could be made
};

// Why the result is what it is, when that needs saying.
enum class CompressNote
{
    None,
    BelowQuality,   // fewer colours would fall below pngQuality: stored without loss instead
    Animated,       // an animated PNG: left as it is
    LossyWebp,      // a lossy or animated WebP is not encoded again (that would lose quality)
    NotSupported,   // a format this build does not compress (AVIF, HEIC, JPEG XL, RAW, BMP)
    TooManyColours, // a GIF frame needs more than 256 colours: left as it is
};

struct Compressed
{
    std::vector<std::uint8_t> bytes; // the new file; the original's bytes for Unchanged
    std::uintmax_t originalSize = 0;
    CompressMethod method = CompressMethod::Unchanged;
    CompressNote note = CompressNote::None;
    // The quality of a palette (PngFewerColours), 0..100.
    int quality = 100;

    [[nodiscard]] bool smaller() const { return bytes.size() < originalSize; }
};

// Compresses the file at `path` in memory; nothing is written.
[[nodiscard]] Result<Compressed> compress(const std::filesystem::path& path,
                                          const CompressOptions& options = {});
// The same for a file already read into memory; `name` gives the extension where it matters
// (TIFF is compressed only as .tif or .tiff, so camera RAW files built on TIFF are left alone).
[[nodiscard]] Result<Compressed> compress(const std::vector<std::uint8_t>& file,
                                          const std::filesystem::path& name,
                                          const CompressOptions& options = {});

// A place on a photo, in pixels of the upright image.
struct Region
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// The `width` by `height` region of the upright photo with the most detail, where a
// difference made by compression shows first; smaller when the photo is.
[[nodiscard]] Result<Region> detailRegion(const std::filesystem::path& path, int width, int height);
// `region` of the upright image, at full resolution, as 8-bit sRGB, from a file or its bytes.
[[nodiscard]] Result<Rgba8Image> loadRegion(const std::filesystem::path& path, const Region& region);
[[nodiscard]] Result<Rgba8Image> loadRegion(const std::vector<std::uint8_t>& file, const Region& region);

} // namespace pfx
