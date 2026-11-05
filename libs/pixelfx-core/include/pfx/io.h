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
    bool raw = false;     // a camera RAW file
    int asShotKelvin = 0; // a RAW's as-shot white balance, when it says (DNG); 0 otherwise
};

struct LoadedImage
{
    Image image; // working space
    ImageInfo info;
    Metadata metadata;
    std::vector<std::uint8_t> profile; // the file's embedded ICC profile, as stored; empty when none
};

struct LoadOptions
{
    int maxLongEdge = 0; // 0: full size; otherwise downscaled on load (previews, thumbnails)
    // A RAW's own tone (a DNG's profile gain table map): the Camera profile; false for PixelFx's
    // renderings.
    bool rawTone = true;
    // A RAW's embedded JPEG instead of developing it, when it is at least maxLongEdge large:
    // thumbnails.
    bool embeddedPreview = false;
};

// The colour space a file is saved in; its ICC profile is embedded. Display P3 and Adobe RGB
// hold more saturated colours than sRGB, for screens and printers that show them.
enum class OutputSpace
{
    Srgb,
    DisplayP3,
    AdobeRgb,
    Original, // the photo's own profile (SaveOptions::originalProfile)
};

struct SaveOptions
{
    int quality = 90;      // JPEG, WebP, AVIF, HEIC, JPEG XL
    int bitsPerSample = 8; // 8 or 16 where the format allows
    bool keepMetadata = true;
    bool keepLocation = true; // with keepMetadata: false strips GPS from Exif and XMP
    int dpi = 0;              // pixels per inch written into the file (prints); 0 leaves it out
    OutputSpace space = OutputSpace::Srgb;
    // With OutputSpace::Original: the photo's profile (LoadedImage::profile). An empty, unreadable
    // or not RGB profile saves sRGB.
    std::vector<std::uint8_t> originalProfile;
};

// Metadata with the location taken out: the Exif GPS block unlinked and zeroed (no trace of the
// coordinates left in the bytes), XMP's exif:GPS properties removed. IPTC (city and country
// written by people) is kept. Malformed blocks are returned as they are.
[[nodiscard]] Metadata withoutLocation(const Metadata& metadata);
[[nodiscard]] std::vector<std::uint8_t> exifWithoutLocation(std::vector<std::uint8_t> exif);
// Exif without its thumbnail (IFD1): unlinked, and cut off when it sits at the end, as writers
// put it.
[[nodiscard]] std::vector<std::uint8_t> exifWithoutThumbnail(std::vector<std::uint8_t> exif);

// What the camera wrote in Exif, for the photo's details and watermark tokens. Fields the block
// does not have stay empty or zero; a malformed block gives an empty summary.
struct ExifSummary
{
    std::string make;
    std::string model;
    std::string lens;
    std::string dateTaken; // as written: "YYYY:MM:DD HH:MM:SS"
    int iso = 0;
    double exposureTime = 0.0; // seconds
    double fNumber = 0.0;
    double focalLength = 0.0; // millimetres
};
[[nodiscard]] ExifSummary readExif(const std::vector<std::uint8_t>& exif);

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
// The JPEG preview inside a camera RAW file, upright, when PixelFx cannot develop the RAW itself.
[[nodiscard]] Result<LoadedImage> loadPreview(const std::filesystem::path& path,
                                              const LoadOptions& options = {});
// Whether the file's extension is a camera RAW format.
[[nodiscard]] bool isRawFile(const std::filesystem::path& path);
// The Exif, XMP and IPTC of a file, from its header, without decoding the pixels.
[[nodiscard]] Result<Metadata> readMetadata(const std::filesystem::path& path);

// Converts to the options' colour space, embeds its profile, writes the format the extension
// names.
[[nodiscard]] Status save(const Image& image, const std::filesystem::path& path,
                          const SaveOptions& options = {}, const Metadata* metadata = nullptr);
// The bytes save() would write for `extension` (".jpg"), in memory: for a size estimate.
[[nodiscard]] Result<std::vector<std::uint8_t>> encode(const Image& image, const std::string& extension,
                                                       const SaveOptions& options = {},
                                                       const Metadata* metadata = nullptr);

// File extensions (lower case, with the dot) this build can write: always .jpg, .png, .tif and
// .webp; .avif, .heic and .jxl when libvips has a working encoder for them (each is tried once).
[[nodiscard]] std::vector<std::string> writableExtensions();

// File extensions (lower case, with the dot, sorted) this build can read as photos: JPEG, PNG,
// TIFF and WebP always; GIF, HEIC, HEIF and AVIF, JPEG XL, camera RAW and BMP when libvips has
// the loader. Documents, vector and scientific formats libvips also reads are left out.
[[nodiscard]] std::vector<std::string> readableExtensions();

// The engines this build runs on, for an About box or a bug report: {"libvips", "8.18.7"},
// {"Little CMS", "2.16"}. Versions are those of the libraries loaded at run time.
struct EngineVersion
{
    std::string name;
    std::string version;
};
[[nodiscard]] std::vector<EngineVersion> engineVersions();

// Converts working-space pixels to 8-bit sRGB for display, through the same colour engine as
// save(). Values outside 0..1 are clipped.
[[nodiscard]] Result<Rgba8Image> toSrgb8(const Image& image);
// The reverse: 8-bit sRGB pixels into the working space (a picture from the screen or the
// clipboard).
[[nodiscard]] Result<Image> fromSrgb8(const Rgba8Image& image);

} // namespace pfx
