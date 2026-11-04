// SPDX-License-Identifier: Apache-2.0
// Decoding and encoding through libvips; colour conversion through LittleCMS2 in float
// (ADR-003). The working space is linear Rec.2020; files without a profile are sRGB.
#include "pfx/io.h"

#include "core/log_internal.h"
#include "core/parallel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <lcms2.h>
#include <memory>
#include <mutex>
#include <string_view>
#include <vips/vips8>

namespace pfx {
namespace {

using ProfilePtr = std::unique_ptr<void, decltype(&cmsCloseProfile)>;
using TransformPtr = std::unique_ptr<void, decltype(&cmsDeleteTransform)>;

void ensureVips()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (VIPS_INIT("pixelfx") != 0) {
            detail::log(LogLevel::Error, "libvips failed to start");
        }
    });
}

ProfilePtr workingProfile()
{
    // Rec.2020 primaries, D65 white, linear transfer.
    const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
    const cmsCIExyYTRIPLE primaries = {{0.708, 0.292, 1.0}, {0.170, 0.797, 1.0}, {0.131, 0.046, 1.0}};
    cmsToneCurve* linear = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve* const curves[3] = {linear, linear, linear};
    cmsHPROFILE profile = cmsCreateRGBProfile(&d65, &primaries, curves);
    cmsFreeToneCurve(linear);
    return {profile, &cmsCloseProfile};
}

ProfilePtr srgbProfile()
{
    return {cmsCreate_sRGBProfile(), &cmsCloseProfile};
}

std::vector<std::uint8_t> profileBytes(cmsHPROFILE profile)
{
    cmsUInt32Number size = 0;
    cmsSaveProfileToMem(profile, nullptr, &size);
    std::vector<std::uint8_t> bytes(size);
    cmsSaveProfileToMem(profile, bytes.data(), &size);
    return bytes;
}

// Runs a float RGBA transform over the image in place, in parallel. The transform is created
// with cmsFLAGS_NOCACHE so it is safe to share between threads.
void transformInPlace(cmsHTRANSFORM transform, Image& image)
{
    detail::parallelFor(image.height(), 0, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            cmsDoTransform(transform, image.row(y), image.row(y),
                           static_cast<cmsUInt32Number>(image.width()));
        }
    });
}

TransformPtr makeTransform(cmsHPROFILE from, cmsHPROFILE to)
{
    return {cmsCreateTransform(from, TYPE_RGBA_FLT, to, TYPE_RGBA_FLT, INTENT_PERCEPTUAL,
                               cmsFLAGS_NOCACHE | cmsFLAGS_COPY_ALPHA),
            &cmsDeleteTransform};
}

std::vector<std::uint8_t> blob(const vips::VImage& image, const char* name)
{
    if (image.get_typeof(name) == 0) {
        return {};
    }
    std::size_t size = 0;
    const void* data = image.get_blob(name, &size);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return {bytes, bytes + size};
}

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

// Clamps to 0..1 and scales to the output's integer range in place; reports whether every
// pixel is fully opaque (the alpha band is then dropped).
bool quantize(Image& image, double scale)
{
    bool opaque = true;
    float* p = image.data();
    const std::size_t count = image.pixelCount() * 4;
    for (std::size_t i = 0; i < count; ++i) {
        const auto v = double{std::clamp(p[i], 0.0F, 1.0F)};
        if (i % 4 == 3 && v < 1.0) {
            opaque = false;
        }
        p[i] = static_cast<float>(std::round(v * scale));
    }
    return opaque;
}

void attachMetadata(vips::VImage& img, const Metadata& metadata)
{
    const auto put = [&img](const char* name, const std::vector<std::uint8_t>& bytes) {
        if (!bytes.empty()) {
            vips_image_set_blob_copy(img.get_image(), name, bytes.data(), bytes.size());
        }
    };
    put(VIPS_META_EXIF_NAME, metadata.exif);
    put(VIPS_META_XMP_NAME, metadata.xmp);
    put(VIPS_META_IPTC_NAME, metadata.iptc);
}

vips::VOption* saverOptions(const std::string& ext, const SaveOptions& options)
{
    vips::VOption* save = vips::VImage::option();
    const bool lossy = ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".avif" ||
                       ext == ".heic" || ext == ".heif" || ext == ".jxl";
    if (lossy) {
        save->set("Q", std::clamp(options.quality, 1, 100));
    } else if (ext == ".png") {
        save->set("compression", 6);
    } else if (ext == ".tif" || ext == ".tiff") {
        save->set("compression", VIPS_FOREIGN_TIFF_COMPRESSION_DEFLATE);
    }
    if (!options.keepMetadata) {
        save->set("keep", VIPS_FOREIGN_KEEP_ICC);
    }
    return save;
}

int bitsOf(const vips::VImage& image)
{
    return (image.format() == VIPS_FORMAT_USHORT || image.format() == VIPS_FORMAT_SHORT) ? 16 : 8;
}

double rangeOf(const vips::VImage& image)
{
    if (image.format() == VIPS_FORMAT_FLOAT || image.format() == VIPS_FORMAT_DOUBLE) {
        return 1.0;
    }
    return bitsOf(image) == 16 ? 65535.0 : 255.0;
}

// Brings any decoded image to 4 bands of 0..1 float in its own encoding.
vips::VImage normalizeBands(vips::VImage in, double& scale)
{
    const VipsInterpretation kind = in.interpretation();
    if (kind == VIPS_INTERPRETATION_CMYK || kind == VIPS_INTERPRETATION_LAB ||
        kind == VIPS_INTERPRETATION_XYZ) {
        in = in.colourspace(VIPS_INTERPRETATION_sRGB);
    }
    scale = rangeOf(in);
    int bands = in.bands();
    if (bands > 4) {
        in = in.extract_band(0, vips::VImage::option()->set("n", 4));
        bands = 4;
    }
    // Note: VImage::bandjoin with a list is the static overload; the list is every band.
    if (bands == 1) {
        in = vips::VImage::bandjoin({in, in, in, in.new_from_image(scale)});
    } else if (bands == 2) {
        const vips::VImage grey = in.extract_band(0);
        in = vips::VImage::bandjoin({grey, grey, grey, in.extract_band(1)});
    } else if (bands == 3) {
        in = in.bandjoin_const({scale});
    }
    return in.cast(VIPS_FORMAT_FLOAT);
}

// libvips caches loaded files by name; `revalidate` makes it check the file again, so a photo
// overwritten on disk (an export over an existing file) is never served from the stale cache.
vips::VImage openFile(const std::filesystem::path& path)
{
    return vips::VImage::new_from_file(path.string().c_str(),
                                       vips::VImage::option()->set("revalidate", true));
}

} // namespace

Result<ImageInfo> probe(const std::filesystem::path& path)
{
    ensureVips();
    try {
        const vips::VImage in = openFile(path);
        ImageInfo info;
        const bool swap = in.get_typeof(VIPS_META_ORIENTATION) != 0 && in.get_int(VIPS_META_ORIENTATION) >= 5;
        info.width = swap ? in.height() : in.width();
        info.height = swap ? in.width() : in.height();
        info.format = in.get_typeof("vips-loader") != 0 ? in.get_string("vips-loader") : "";
        info.bitsPerSample = bitsOf(in);
        info.hasAlpha = in.has_alpha();
        info.hasProfile = in.get_typeof(VIPS_META_ICC_NAME) != 0;
        return info;
    } catch (const vips::VError& e) {
        return Error{ErrorCode::DecodeFailed, std::string("Could not read the image: ") + e.what()};
    }
}

Result<LoadedImage> load(const std::filesystem::path& path, const LoadOptions& options)
{
    ensureVips();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Error{ErrorCode::NotFound, "The file " + path.string() + " does not exist."};
    }
    LoadedImage result;
    std::vector<std::uint8_t> icc;
    double scale = 255.0;
    try {
        vips::VImage in = openFile(path);
        result.info.format = in.get_typeof("vips-loader") != 0 ? in.get_string("vips-loader") : "";
        result.info.bitsPerSample = bitsOf(in);
        result.info.hasAlpha = in.has_alpha();
        icc = blob(in, VIPS_META_ICC_NAME);
        result.info.hasProfile = !icc.empty();
        result.metadata = {blob(in, VIPS_META_EXIF_NAME), blob(in, VIPS_META_XMP_NAME),
                           blob(in, VIPS_META_IPTC_NAME)};
        in = in.autorot();
        if (options.maxLongEdge > 0 && std::max(in.width(), in.height()) > options.maxLongEdge) {
            in = in.resize(static_cast<double>(options.maxLongEdge) / std::max(in.width(), in.height()));
        }
        in = normalizeBands(in, scale);
        result.info.width = in.width();
        result.info.height = in.height();
        result.image = Image(in.width(), in.height());
        std::size_t size = 0;
        void* memory = in.write_to_memory(&size);
        const std::unique_ptr<void, decltype(&g_free)> guard(memory, &g_free);
        const auto* src = static_cast<const float*>(memory);
        const std::size_t count = size / sizeof(float);
        const auto inv = static_cast<float>(1.0 / scale);
        std::transform(src, src + count, result.image.data(), [inv](float v) { return v * inv; });
    } catch (const vips::VError& e) {
        return Error{ErrorCode::DecodeFailed, std::string("Could not read the image: ") + e.what()};
    }

    ProfilePtr source(
        icc.empty() ? nullptr : cmsOpenProfileFromMem(icc.data(), static_cast<cmsUInt32Number>(icc.size())),
        &cmsCloseProfile);
    if (source && cmsGetColorSpace(source.get()) != cmsSigRgbData) {
        detail::log(LogLevel::Warning, "Embedded profile is not RGB; treating the image as sRGB");
        source.reset();
    }
    if (!source) {
        source = srgbProfile();
    }
    const ProfilePtr working = workingProfile();
    const TransformPtr transform = makeTransform(source.get(), working.get());
    if (!transform) {
        return Error{ErrorCode::ColorManagement, "The image's colour profile could not be used."};
    }
    transformInPlace(transform.get(), result.image);
    return result;
}

Status save(const Image& image, const std::filesystem::path& path, const SaveOptions& options,
            const Metadata* metadata)
{
    ensureVips();
    if (image.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nothing to save: the image is empty."};
    }
    const std::string ext = lowerExtension(path);
    const bool jpeg = ext == ".jpg" || ext == ".jpeg";
    const bool sixteen = options.bitsPerSample == 16 && (ext == ".png" || ext == ".tif" || ext == ".tiff");

    Image out = image;
    const ProfilePtr working = workingProfile();
    const ProfilePtr srgb = srgbProfile();
    const TransformPtr transform = makeTransform(working.get(), srgb.get());
    if (!transform) {
        return Error{ErrorCode::ColorManagement, "Could not prepare the sRGB conversion."};
    }
    transformInPlace(transform.get(), out);
    const bool opaque = quantize(out, sixteen ? 65535.0 : 255.0);
    try {
        vips::VImage img = vips::VImage::new_from_memory(out.data(), out.pixelCount() * 4 * sizeof(float),
                                                         out.width(), out.height(), 4, VIPS_FORMAT_FLOAT);
        if (opaque || jpeg) {
            img = img.extract_band(0, vips::VImage::option()->set("n", 3));
        }
        img = img.cast(sixteen ? VIPS_FORMAT_USHORT : VIPS_FORMAT_UCHAR);
        img = img.copy(vips::VImage::option()->set("interpretation", sixteen ? VIPS_INTERPRETATION_RGB16
                                                                             : VIPS_INTERPRETATION_sRGB));
        const auto icc = profileBytes(srgb.get());
        vips_image_set_blob_copy(img.get_image(), VIPS_META_ICC_NAME, icc.data(), icc.size());
        if (metadata != nullptr && options.keepMetadata) {
            attachMetadata(img, *metadata);
        }
        img.set(VIPS_META_ORIENTATION, 1); // pixels are already upright
        img.write_to_file(path.string().c_str(), saverOptions(ext, options));
    } catch (const vips::VError& e) {
        return Error{ErrorCode::EncodeFailed, std::string("Could not save the image: ") + e.what()};
    }
    return {};
}

Result<Rgba8Image> toSrgb8(const Image& image)
{
    if (image.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nothing to convert: the image is empty."};
    }
    const ProfilePtr working = workingProfile();
    const ProfilePtr srgb = srgbProfile();
    // Float to float, then rounded here exactly as save() does: LittleCMS's own float-to-8-bit
    // path rounds differently between versions (up to 3 levels with 2.14).
    const TransformPtr transform = makeTransform(working.get(), srgb.get());
    if (!transform) {
        return Error{ErrorCode::ColorManagement, "Could not prepare the sRGB conversion."};
    }
    Rgba8Image out;
    out.width = image.width();
    out.height = image.height();
    out.pixels.resize(image.pixelCount() * 4);
    const auto rowValues = static_cast<std::size_t>(image.width()) * 4;
    detail::parallelFor(image.height(), 0, [&](int y0, int y1) {
        std::vector<float> row(rowValues);
        for (int y = y0; y < y1; ++y) {
            cmsDoTransform(transform.get(), image.row(y), row.data(),
                           static_cast<cmsUInt32Number>(image.width()));
            std::uint8_t* dst = out.pixels.data() + static_cast<std::size_t>(y) * rowValues;
            for (std::size_t i = 0; i < rowValues; ++i) {
                const auto v = double{std::clamp(row[i], 0.0F, 1.0F)};
                dst[i] = static_cast<std::uint8_t>(std::lround(v * 255.0));
            }
        }
    });
    return out;
}

std::vector<std::string> writableExtensions()
{
    ensureVips();
    std::vector<std::string> out;
    for (const char* ext : {".jpg", ".png", ".tif", ".webp", ".avif", ".heic", ".jxl"}) {
        const std::string probe = std::string("probe") + ext;
        if (vips_foreign_find_save(probe.c_str()) != nullptr) {
            out.emplace_back(ext);
        } else {
            vips_error_clear();
        }
    }
    return out;
}

namespace {

struct LoaderScan
{
    std::vector<std::string> extensions;
};

void* collectLoader(VipsForeignClass* foreign, void* data) // NOLINT(readability-non-const-parameter)
{
    auto* scan = static_cast<LoaderScan*>(data);
    const std::string_view name = VIPS_OBJECT_CLASS(foreign)->nickname;
    // The file loaders of photo formats; "_source" and "_buffer" variants share the suffixes.
    static constexpr std::array kPhotoLoaders = {"jpegload", "pngload",  "tiffload", "webpload",
                                                 "gifload",  "heifload", "jxlload",  "dcrawload"};
    if (name == "magickload") {
        scan->extensions.emplace_back(".bmp"); // the one format PixelFx takes from ImageMagick
        return nullptr;
    }
    if (std::ranges::find(kPhotoLoaders, name) == kPhotoLoaders.end() || foreign->suffs == nullptr) {
        return nullptr;
    }
    for (const char* const* suffix = foreign->suffs; *suffix != nullptr; ++suffix) {
        std::string ext(*suffix);
        std::ranges::transform(ext, ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        scan->extensions.push_back(std::move(ext));
    }
    return nullptr;
}

} // namespace

std::vector<std::string> readableExtensions()
{
    ensureVips();
    LoaderScan scan;
    vips_foreign_map(
        "VipsForeignLoad",
        [](void* foreign, void* data, void* /*unused*/) -> void* {
            return collectLoader(static_cast<VipsForeignClass*>(foreign), data);
        },
        &scan, nullptr);
    std::ranges::sort(scan.extensions);
    const auto [first, last] = std::ranges::unique(scan.extensions);
    scan.extensions.erase(first, last);
    return scan.extensions;
}

} // namespace pfx
