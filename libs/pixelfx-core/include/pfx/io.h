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
};

// 8-bit sRGB pixels with straight alpha, interleaved R, G, B, A, for display.
struct Rgba8Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// Decodes any format libvips reads, applies the orientation tag, converts through the
// embedded ICC profile (sRGB when there is none) into the working space.
[[nodiscard]] Result<LoadedImage> load(const std::filesystem::path& path, const LoadOptions& options = {});
[[nodiscard]] Result<ImageInfo> probe(const std::filesystem::path& path);

// Converts to sRGB, embeds the sRGB profile, writes the format the extension names.
[[nodiscard]] Status save(const Image& image, const std::filesystem::path& path,
                          const SaveOptions& options = {}, const Metadata* metadata = nullptr);

// File extensions (lower case, with the dot) this build can write: always .jpg, .png, .tif and
// .webp; .avif, .heic and .jxl when libvips was built with them.
[[nodiscard]] std::vector<std::string> writableExtensions();

// Converts working-space pixels to 8-bit sRGB for display, through the same colour engine as
// save(). Values outside 0..1 are clipped.
[[nodiscard]] Result<Rgba8Image> toSrgb8(const Image& image);

} // namespace pfx
