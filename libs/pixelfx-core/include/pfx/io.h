// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pfx {

struct Metadata
{
    std::vector<std::uint8_t> exif;
    std::vector<std::uint8_t> xmp;
    std::vector<std::uint8_t> iptc;
};

struct ImageInfo
{
    int width = 0; // after orientation
    int height = 0;
    std::string format; // loader name, for example "jpeg", "png"
    int bitsPerSample = 8;
    bool hasAlpha = false;
    bool hasProfile = false;
};

struct LoadedImage
{
    Image image; // working space
    ImageInfo info;
    Metadata metadata;
};

struct LoadOptions
{
    int maxLongEdge = 0; // 0: full size; otherwise downscaled on load (previews, thumbnails)
};

struct SaveOptions
{
    int quality = 90;      // JPEG, WebP, AVIF, HEIC, JPEG XL
    int bitsPerSample = 8; // 8 or 16 where the format allows
    bool keepMetadata = true;
    bool keepLocation = true; // with keepMetadata: false strips GPS from Exif and XMP
};

// Metadata with the location taken out: the Exif GPS block unlinked and zeroed (no trace of the
// coordinates left in the bytes), XMP's exif:GPS properties removed. IPTC (city and country
// written by people) is kept. Malformed blocks are returned as they are.
[[nodiscard]] Metadata withoutLocation(const Metadata& metadata);
[[nodiscard]] std::vector<std::uint8_t> exifWithoutLocation(std::vector<std::uint8_t> exif);

// 8-bit sRGB pixels with straight alpha, interleaved R, G, B, A, for display.
struct Rgba8Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// Decodes any format libvips reads, applies the orientation tag, converts through the
// embedded ICC profile (sRGB when there is none) into the working space.
// Images over 512 megapixels (after any downscale) are refused with a reason, so a damaged
// header cannot exhaust memory.
[[nodiscard]] Result<LoadedImage> load(const std::filesystem::path& path, const LoadOptions& options = {});
[[nodiscard]] Result<ImageInfo> probe(const std::filesystem::path& path);

// Converts to sRGB, embeds the sRGB profile, writes the format the extension names.
[[nodiscard]] Status save(const Image& image, const std::filesystem::path& path,
                          const SaveOptions& options = {}, const Metadata* metadata = nullptr);

// File extensions (lower case, with the dot) this build can write: always .jpg, .png, .tif and
// .webp; .avif, .heic and .jxl when libvips has a working encoder for them (each is tried once).
[[nodiscard]] std::vector<std::string> writableExtensions();

// File extensions (lower case, with the dot, sorted) this build can read as photos: JPEG, PNG,
// TIFF and WebP always; GIF, HEIC, HEIF and AVIF, JPEG XL, camera RAW and BMP when libvips has
// the loader. Documents, vector and scientific formats libvips also reads are left out.
[[nodiscard]] std::vector<std::string> readableExtensions();

// Converts working-space pixels to 8-bit sRGB for display, through the same colour engine as
// save(). Values outside 0..1 are clipped.
[[nodiscard]] Result<Rgba8Image> toSrgb8(const Image& image);

} // namespace pfx
