// SPDX-License-Identifier: Apache-2.0
// Decoding and encoding through libvips; colour conversion through LittleCMS2 in float
// (ADR-003). The working space is linear Rec.2020; files without a profile are sRGB.
#include "pfx/io.h"

#include "core/log_internal.h"
#include "core/parallel.h"
#include "io/compress_internal.h"
#include "ops/value_table.h"
#include "pfx/color.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <lcms2.h>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vips/vips8>

namespace pfx {
namespace {

// The largest image load() accepts, as Lightroom does; a damaged or hostile header cannot make
// it reserve more than about 8 GB.
constexpr double kMaxMegapixels = 512.0;

using ProfilePtr = std::unique_ptr<void, decltype(&cmsCloseProfile)>;

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

// Converts RGBA float rows between two RGB profiles, in place. When both profiles are
// matrix-shapers with no lookup tables (sRGB, Display P3, Adobe RGB, the working space: nearly
// every photo), that is three tone curves, a 3 by 3 matrix and three more curves, read from the
// profiles themselves and tabulated, several times faster than LittleCMS's float path. Other
// profiles go through LittleCMS.
class RowConverter
{
public:
    static std::optional<RowConverter> create(cmsHPROFILE from, cmsHPROFILE to)
    {
        RowConverter c;
        const auto fromColorants = colorants(from);
        const auto toColorants = colorants(to);
        if (matrixShaper(from, cmsSigAToB0Tag) && matrixShaper(to, cmsSigBToA0Tag) && fromColorants &&
            toColorants && std::abs(determinant(*toColorants)) > 1e-9) {
            c.m_matrix = color::multiply(color::inverse(*toColorants), *fromColorants);
            for (std::size_t i = 0; i < 3; ++i) {
                c.m_decode.at(i) = curveTable(from, kTrcTags.at(i), false);
                c.m_encode.at(i) = curveTable(to, kTrcTags.at(i), true);
            }
            return c;
        }
        c.m_lcms = makeTransform(from, to);
        if (!c.m_lcms) {
            return std::nullopt;
        }
        return c;
    }

    void operator()(float* row, int width) const
    {
        if (m_lcms) {
            cmsDoTransform(m_lcms.get(), row, row, static_cast<cmsUInt32Number>(width));
            return;
        }
        for (int x = 0; x < width; ++x, row += 4) {
            color::Vec3 v{};
            for (std::size_t c = 0; c < 3; ++c) {
                v.at(c) = m_decode.at(c) ? (*m_decode.at(c))(double{row[c]}) : double{row[c]};
            }
            v = color::transform(m_matrix, v);
            for (std::size_t c = 0; c < 3; ++c) {
                row[c] = static_cast<float>(m_encode.at(c) ? (*m_encode.at(c))(v.at(c)) : v.at(c));
            }
        }
    }

private:
    static constexpr std::array<cmsTagSignature, 3> kTrcTags = {cmsSigRedTRCTag, cmsSigGreenTRCTag,
                                                                cmsSigBlueTRCTag};

    static bool matrixShaper(cmsHPROFILE profile, cmsTagSignature lut)
    {
        return cmsIsMatrixShaper(profile) != 0 && cmsIsTag(profile, lut) == 0 &&
               cmsGetColorSpace(profile) == cmsSigRgbData;
    }

    // RGB to the profile connection space (XYZ, D50): the colorant tags are its columns. None
    // when a tag cannot be read (a damaged profile goes to LittleCMS, which reports it).
    static std::optional<color::Mat3> colorants(cmsHPROFILE profile)
    {
        const std::array<cmsTagSignature, 3> tags = {cmsSigRedColorantTag, cmsSigGreenColorantTag,
                                                     cmsSigBlueColorantTag};
        color::Mat3 m{};
        for (std::size_t c = 0; c < 3; ++c) {
            const auto* xyz = static_cast<const cmsCIEXYZ*>(cmsReadTag(profile, tags.at(c)));
            if (xyz == nullptr || !std::isfinite(xyz->X) || !std::isfinite(xyz->Y) ||
                !std::isfinite(xyz->Z)) {
                return std::nullopt;
            }
            m[0][c] = xyz->X;
            m[1][c] = xyz->Y;
            m[2][c] = xyz->Z;
        }
        return m;
    }

    static double determinant(const color::Mat3& m)
    {
        return (m[0][0] * ((m[1][1] * m[2][2]) - (m[1][2] * m[2][1]))) -
               (m[0][1] * ((m[1][0] * m[2][2]) - (m[1][2] * m[2][0]))) +
               (m[0][2] * ((m[1][0] * m[2][1]) - (m[1][1] * m[2][0])));
    }

    // The profile's tone curve (decoding) or its inverse (encoding) as a table; none when it is
    // linear. Negative values pass through the curve's start, as the clamp after does anyway.
    static std::shared_ptr<detail::ValueTable> curveTable(cmsHPROFILE profile, cmsTagSignature tag,
                                                          bool inverse)
    {
        const auto* curve = static_cast<const cmsToneCurve*>(cmsReadTag(profile, tag));
        if (curve == nullptr || cmsIsToneCurveLinear(curve) != 0) {
            return nullptr;
        }
        const std::shared_ptr<cmsToneCurve> owned(
            inverse ? cmsReverseToneCurve(curve) : cmsDupToneCurve(curve), &cmsFreeToneCurve);
        if (!owned) {
            return nullptr; // treated as linear rather than crash on a curve that cannot be inverted
        }
        return std::make_shared<detail::ValueTable>([owned](double v) {
            return double{cmsEvalToneCurveFloat(owned.get(), static_cast<cmsFloat32Number>(v))};
        });
    }

    color::Mat3 m_matrix{};
    std::array<std::shared_ptr<detail::ValueTable>, 3> m_decode;
    std::array<std::shared_ptr<detail::ValueTable>, 3> m_encode;
    std::shared_ptr<void> m_lcms;

    static std::shared_ptr<void> makeTransform(cmsHPROFILE from, cmsHPROFILE to)
    {
        cmsHTRANSFORM t = cmsCreateTransform(from, TYPE_RGBA_FLT, to, TYPE_RGBA_FLT, INTENT_PERCEPTUAL,
                                             cmsFLAGS_NOCACHE | cmsFLAGS_COPY_ALPHA);
        return t == nullptr ? nullptr : std::shared_ptr<void>(t, &cmsDeleteTransform);
    }
};

void convertInPlace(const RowConverter& convert, Image& image)
{
    detail::parallelFor(image.height(), 0, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            convert(image.row(y), image.width());
        }
    });
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
// libvips explains a failed read for developers ("VipsForeignLoad: "/path" is not a known
// file format"); people get the reason in plain words, without the path they already see.
std::string readFailure(const std::string& detail)
{
    const auto has = [&detail](const char* text) { return detail.find(text) != std::string::npos; };
    if (has("not a known file format") || has("unsupported image format") || has("not a valid")) {
        return "This file is not a photo PixelFx can read.";
    }
    if (has("Premature end") || has("truncated") || has("end of file") || has("short read") ||
        has("out of order read") || has("Corrupt") || has("corrupt")) {
        return "The file is damaged or incomplete.";
    }
    if (has("unable to unpack") || has("Unsupported file format or not RAW")) {
        return "This RAW file is compressed in a way PixelFx cannot read yet.";
    }
    if (has("Permission denied")) {
        return "PixelFx is not allowed to read this file.";
    }
    const std::string firstLine = detail.substr(0, detail.find('\n'));
    return "The photo could not be read (" + firstLine + ").";
}

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
        return Error{ErrorCode::DecodeFailed, readFailure(e.what())};
    } catch (const std::bad_alloc&) {
        return Error{ErrorCode::DecodeFailed, "There is not enough memory to open this image."};
    }
}

Result<Metadata> readMetadata(const std::filesystem::path& path)
{
    ensureVips();
    try {
        const vips::VImage in = openFile(path);
        return Metadata{blob(in, VIPS_META_EXIF_NAME), blob(in, VIPS_META_XMP_NAME),
                        blob(in, VIPS_META_IPTC_NAME)};
    } catch (const vips::VError& e) {
        return Error{ErrorCode::DecodeFailed, readFailure(e.what())};
    }
}

namespace {

// Converts decoded pixels from their embedded profile (sRGB when there is none or it is not an
// RGB profile) to the working space, in place.
Status toWorkingSpace(Image& image, const std::vector<std::uint8_t>& icc)
{
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
    const auto convert = RowConverter::create(source.get(), working.get());
    if (!convert) {
        return Error{ErrorCode::ColorManagement, "The image's colour profile could not be used."};
    }
    convertInPlace(*convert, image);
    return {};
}

} // namespace

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
        if (scale != 1.0) {
            in = in.linear(1.0 / scale, 0.0).cast(VIPS_FORMAT_FLOAT);
        }
        result.info.width = in.width();
        result.info.height = in.height();
        const double megapixels = static_cast<double>(in.width()) * static_cast<double>(in.height()) / 1e6;
        if (megapixels > kMaxMegapixels) {
            return Error{ErrorCode::DecodeFailed,
                         "The image is too large to edit: " + std::to_string(static_cast<long>(megapixels)) +
                             " megapixels, and the limit is " +
                             std::to_string(static_cast<int>(kMaxMegapixels)) + "."};
        }
        result.image = Image(in.width(), in.height());
        // libvips decodes and scales straight into the image's own buffer, in parallel.
        const vips::VImage target =
            vips::VImage::new_from_memory(result.image.data(), result.image.pixelCount() * 4 * sizeof(float),
                                          in.width(), in.height(), 4, VIPS_FORMAT_FLOAT);
        if (vips_image_write(in.get_image(), target.get_image()) != 0) {
            throw vips::VError();
        }
    } catch (const vips::VError& e) {
        return Error{ErrorCode::DecodeFailed, readFailure(e.what())};
    }

    if (const Status converted = toWorkingSpace(result.image, icc); !converted) {
        return converted.error();
    }
    return result;
}

namespace {

// Whether any pixel is less than fully opaque.
bool hasTranslucency(const Image& image)
{
    std::atomic<bool> translucent = false;
    detail::parallelFor(image.height(), 0, [&](int y0, int y1) {
        for (int y = y0; y < y1 && !translucent; ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x) {
                if (p[(x * 4) + 3] < 1.0F) {
                    translucent = true;
                    break;
                }
            }
        }
    });
    return translucent;
}

// The image as sRGB samples of `bands` bands at 8 or 16 bits, ready for libvips: each row is
// converted from a copy, clamped and rounded, in parallel, with no full-size float copy.
std::vector<std::uint8_t> encodeRows(const Image& image, const RowConverter& convert, int bands, bool sixteen)
{
    const double range = sixteen ? 65535.0 : 255.0;
    const auto width = static_cast<std::size_t>(image.width());
    const auto stride = static_cast<std::size_t>(bands);
    std::vector<std::uint8_t> bytes(image.pixelCount() * stride * (sixteen ? 2 : 1));
    auto* wide =
        reinterpret_cast<std::uint16_t*>(bytes.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    detail::parallelFor(image.height(), 0, [&](int y0, int y1) {
        std::vector<float> row(width * 4);
        for (int y = y0; y < y1; ++y) {
            std::copy_n(image.row(y), row.size(), row.begin());
            convert(row.data(), image.width());
            const std::size_t base = static_cast<std::size_t>(y) * width * stride;
            for (std::size_t x = 0; x < width; ++x) {
                for (std::size_t c = 0; c < stride; ++c) {
                    const auto level = std::lround(double{std::clamp(row[(x * 4) + c], 0.0F, 1.0F)} * range);
                    const std::size_t at = base + (x * stride) + c;
                    if (sixteen) {
                        wide[at] = static_cast<std::uint16_t>(level);
                    } else {
                        bytes[at] = static_cast<std::uint8_t>(level);
                    }
                }
            }
        }
    });
    return bytes;
}

} // namespace

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

    const ProfilePtr working = workingProfile();
    const ProfilePtr srgb = srgbProfile();
    const auto convert = RowConverter::create(working.get(), srgb.get());
    if (!convert) {
        return Error{ErrorCode::ColorManagement, "Could not prepare the sRGB conversion."};
    }
    // Alpha is kept only when some pixel is not fully opaque (and never in a JPEG).
    const int bands = (!jpeg && hasTranslucency(image)) ? 4 : 3;
    std::vector<std::uint8_t> bytes = encodeRows(image, *convert, bands, sixteen);
    try {
        vips::VImage img =
            vips::VImage::new_from_memory(bytes.data(), bytes.size(), image.width(), image.height(), bands,
                                          sixteen ? VIPS_FORMAT_USHORT : VIPS_FORMAT_UCHAR);
        img = img.copy(vips::VImage::option()->set("interpretation", sixteen ? VIPS_INTERPRETATION_RGB16
                                                                             : VIPS_INTERPRETATION_sRGB));
        const auto icc = profileBytes(srgb.get());
        vips_image_set_blob_copy(img.get_image(), VIPS_META_ICC_NAME, icc.data(), icc.size());
        if (metadata != nullptr && options.keepMetadata) {
            attachMetadata(img, options.keepLocation ? *metadata : withoutLocation(*metadata));
        }
        img.set(VIPS_META_ORIENTATION, 1); // pixels are already upright
        if (options.dpi > 0) {
            const double perMillimetre = options.dpi / 25.4; // libvips keeps resolution per millimetre
            img = img.copy(vips::VImage::option()->set("xres", perMillimetre)->set("yres", perMillimetre));
        }
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
    const auto convert = RowConverter::create(working.get(), srgb.get());
    if (!convert) {
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
            std::copy_n(image.row(y), rowValues, row.begin());
            (*convert)(row.data(), image.width());
            std::uint8_t* dst = out.pixels.data() + static_cast<std::size_t>(y) * rowValues;
            for (std::size_t i = 0; i < rowValues; ++i) {
                const auto v = double{std::clamp(row[i], 0.0F, 1.0F)};
                dst[i] = static_cast<std::uint8_t>(std::lround(v * 255.0));
            }
        }
    });
    return out;
}

namespace detail {

void startVips()
{
    ensureVips();
}

} // namespace detail

std::vector<std::string> writableExtensions()
{
    // A saver can exist without an encoder behind it (libheif without x265 or an AV1 encoder):
    // each format is tried once on a tiny image, and only those that really write are offered.
    static const std::vector<std::string> extensions = [] {
        ensureVips();
        std::vector<std::string> out;
        const vips::VImage probe =
            vips::VImage::black(8, 8, vips::VImage::option()->set("bands", 3)).cast(VIPS_FORMAT_UCHAR);
        for (const char* ext : {".jpg", ".png", ".tif", ".webp", ".avif", ".heic", ".jxl"}) {
            try {
                void* buffer = nullptr;
                std::size_t size = 0;
                probe.write_to_buffer(ext, &buffer, &size);
                g_free(buffer);
                out.emplace_back(ext);
            } catch (const vips::VError&) {
                vips_error_clear();
            }
        }
        return out;
    }();
    return extensions;
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

std::vector<EngineVersion> engineVersions()
{
    ensureVips();
    // Little CMS encodes 2.16 as 2160.
    const int cms = cmsGetEncodedCMMversion();
    return {{"libvips", vips_version_string()},
            {"Little CMS", std::to_string(cms / 1000) + "." + std::to_string((cms % 1000) / 10)}};
}

} // namespace pfx
