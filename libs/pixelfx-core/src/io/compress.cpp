// SPDX-License-Identifier: Apache-2.0
// Making files smaller in their own format (FEATURES M10, ADR-010). Decoding goes through
// libvips; JPEG is transcoded through libjpeg, PNG and GIF are written by our own encoders,
// TIFF and lossless WebP are encoded again by libvips.
#include "pfx/compress.h"

#include "io/compress_internal.h"
#include "pfx/quantize.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <string_view>
#include <vips/vips8>

namespace pfx {

namespace {

using Bytes = std::vector<std::uint8_t>;

enum class Kind
{
    Jpeg,
    Png,
    Gif,
    Webp,
    Tiff,
    Other,
};

bool startsWith(const Bytes& file, std::initializer_list<std::uint8_t> prefix, std::size_t at = 0)
{
    return file.size() >= at + prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), file.begin() + static_cast<std::ptrdiff_t>(at));
}

std::string lowerExtension(const std::filesystem::path& name)
{
    std::string ext = name.extension().string();
    std::ranges::transform(ext, ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

Kind kindOf(const Bytes& file, const std::filesystem::path& name)
{
    if (startsWith(file, {0xFF, 0xD8, 0xFF})) {
        return Kind::Jpeg;
    }
    if (startsWith(file, {0x89, 'P', 'N', 'G'})) {
        return Kind::Png;
    }
    if (startsWith(file, {'G', 'I', 'F', '8'})) {
        return Kind::Gif;
    }
    if (startsWith(file, {'R', 'I', 'F', 'F'}) && startsWith(file, {'W', 'E', 'B', 'P'}, 8)) {
        return Kind::Webp;
    }
    // Camera RAW files are TIFF inside: only files named as TIFF are TIFF here.
    const std::string ext = lowerExtension(name);
    if ((startsWith(file, {'I', 'I', 42, 0}) || startsWith(file, {'M', 'M', 0, 42})) &&
        (ext == ".tif" || ext == ".tiff")) {
        return Kind::Tiff;
    }
    return Kind::Other;
}

Compressed unchanged(const Bytes& file, CompressNote note)
{
    Compressed c;
    c.bytes = file;
    c.originalSize = file.size();
    c.method = CompressMethod::Unchanged;
    c.note = note;
    return c;
}

// Keeps `candidate` when it is smaller than what `best` holds.
void keepSmaller(Compressed& best, Bytes candidate, CompressMethod method)
{
    if (!candidate.empty() && candidate.size() < best.bytes.size()) {
        best.bytes = std::move(candidate);
        best.method = method;
    }
}

// The image a buffer holds; the buffer must outlive it (libvips reads it lazily).
vips::VImage fromBuffer(const Bytes& file, vips::VOption* options = nullptr)
{
    return vips::VImage::new_from_buffer(file.data(), file.size(), "", options);
}

Bytes toBuffer(const vips::VImage& image, const char* suffix, vips::VOption* options)
{
    void* data = nullptr;
    std::size_t size = 0;
    image.write_to_buffer(suffix, &data, &size, options);
    const std::unique_ptr<void, decltype(&g_free)> owned(data, &g_free);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return {bytes, bytes + size};
}

Bytes blobOf(const vips::VImage& image, const char* name)
{
    if (image.get_typeof(name) == 0) {
        return {};
    }
    std::size_t size = 0;
    const auto* data = static_cast<const std::uint8_t*>(image.get_blob(name, &size));
    return {data, data + size};
}

// Removes the location and previews from an image libvips is about to save. libvips writes
// Exif again from its exif-* fields, so those go along with the blob they were read from.
void cleanMetadata(vips::VImage& image)
{
    gchar** fields = vips_image_get_fields(image.get_image());
    for (const gchar* const* f = fields; f != nullptr && *f != nullptr; ++f) {
        if (std::string_view(*f).starts_with("exif-")) {
            vips_image_remove(image.get_image(), *f);
        }
    }
    g_strfreev(fields);
    const auto replace = [&image](const char* name, const Bytes& bytes) {
        vips_image_remove(image.get_image(), name);
        if (!bytes.empty()) {
            vips_image_set_blob_copy(image.get_image(), name, bytes.data(), bytes.size());
        }
    };
    if (image.get_typeof(VIPS_META_EXIF_NAME) != 0) {
        replace(VIPS_META_EXIF_NAME, detail::exifWithoutExtras(blobOf(image, VIPS_META_EXIF_NAME)));
    }
    if (image.get_typeof(VIPS_META_XMP_NAME) != 0) {
        replace(VIPS_META_XMP_NAME, detail::xmpWithoutExtras(blobOf(image, VIPS_META_XMP_NAME)));
    }
}

// ---- JPEG

Result<Compressed> compressJpeg(const Bytes& file, const CompressOptions& options)
{
    auto lossless = detail::jpegLossless(file, options.removeExtras);
    if (!lossless) {
        return lossless.error();
    }
    Compressed c;
    c.originalSize = file.size();
    c.bytes = std::move(lossless).value();
    c.method = CompressMethod::JpegLossless;
    if (options.jpeg == JpegCompression::Quality) {
        // Encoded again only when that is smaller than the lossless result.
        try {
            vips::VImage image = fromBuffer(file).copy();
            if (options.removeExtras) {
                cleanMetadata(image);
            }
            keepSmaller(c,
                        toBuffer(image, ".jpg",
                                 vips::VImage::option()
                                     ->set("Q", std::clamp(options.jpegQuality, 1, 100))
                                     ->set("optimize_coding", true)
                                     ->set("interlace", true)),
                        CompressMethod::JpegQuality);
        } catch (const vips::VError&) {
            vips_error_clear(); // the lossless result stands
        }
    }
    return c;
}

// ---- PNG

// The decoded samples of a PNG, 8-bit values or 16-bit ones, interleaved.
struct Samples
{
    int width = 0;
    int height = 0;
    int channels = 4;
    int depth = 8;
    std::vector<std::uint16_t> values;

    [[nodiscard]] std::size_t pixels() const
    {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }
};

std::optional<Samples> decodePng(const Bytes& file)
{
    const vips::VImage image = fromBuffer(file);
    if (image.format() != VIPS_FORMAT_UCHAR && image.format() != VIPS_FORMAT_USHORT) {
        return std::nullopt;
    }
    Samples s;
    s.width = image.width();
    s.height = image.height();
    s.channels = image.bands();
    s.depth = image.format() == VIPS_FORMAT_USHORT ? 16 : 8;
    if (s.channels < 1 || s.channels > 4) {
        return std::nullopt;
    }
    std::size_t size = 0;
    void* memory = image.write_to_memory(&size);
    const std::unique_ptr<void, decltype(&g_free)> owned(memory, &g_free);
    s.values.resize(s.pixels() * static_cast<std::size_t>(s.channels));
    if (s.depth == 16) {
        const auto* v = static_cast<const std::uint16_t*>(memory);
        std::copy_n(v, s.values.size(), s.values.begin());
    } else {
        const auto* v = static_cast<const std::uint8_t*>(memory);
        std::copy_n(v, s.values.size(), s.values.begin());
    }
    return s;
}

void keepChannels(Samples& s, std::initializer_list<std::size_t> keep)
{
    const auto channels = static_cast<std::size_t>(s.channels);
    std::vector<std::uint16_t> out;
    out.reserve(s.pixels() * keep.size());
    for (std::size_t p = 0; p < s.values.size(); p += channels) {
        for (const std::size_t c : keep) {
            out.push_back(s.values[p + c]);
        }
    }
    s.values = std::move(out);
    s.channels = static_cast<int>(keep.size());
}

// Whether `test` holds for channel `channel` of every pixel, given the pixel's first sample.
template <typename Test>
bool everyPixel(const Samples& s, Test test)
{
    const auto channels = static_cast<std::size_t>(s.channels);
    for (std::size_t p = 0; p < s.values.size(); p += channels) {
        if (!test(s.values.data() + p)) {
            return false;
        }
    }
    return true;
}

// The smallest exact form: 8 bits when 16 hold nothing more, no alpha when all is opaque, grey
// when every pixel is.
void reduce(Samples& s)
{
    if (s.depth == 16 && std::ranges::all_of(s.values, [](std::uint16_t v) { return v % 257 == 0; })) {
        for (std::uint16_t& v : s.values) {
            v = static_cast<std::uint16_t>(v / 257);
        }
        s.depth = 8;
    }
    const std::uint16_t opaque = s.depth == 16 ? 65535 : 255;
    const auto last = static_cast<std::size_t>(s.channels - 1);
    if ((s.channels == 2 || s.channels == 4) &&
        everyPixel(s, [opaque, last](const std::uint16_t* p) { return p[last] == opaque; })) {
        keepChannels(s, s.channels == 2 ? std::initializer_list<std::size_t>{0}
                                        : std::initializer_list<std::size_t>{0, 1, 2});
    }
    if (s.channels >= 3 &&
        everyPixel(s, [](const std::uint16_t* p) { return p[0] == p[1] && p[0] == p[2]; })) {
        keepChannels(s, s.channels == 3 ? std::initializer_list<std::size_t>{0}
                                        : std::initializer_list<std::size_t>{0, 3});
    }
}

detail::PngRaster trueColour(const Samples& s)
{
    detail::PngRaster r;
    r.width = s.width;
    r.height = s.height;
    r.bitDepth = s.depth;
    static constexpr std::array<int, 5> kTypes = {0, 0, 4, 2, 6}; // by channel count
    r.colourType = kTypes.at(static_cast<std::size_t>(s.channels));
    r.samples.reserve(s.values.size() * (s.depth == 16 ? 2 : 1));
    for (const std::uint16_t v : s.values) {
        if (s.depth == 16) {
            r.samples.push_back(static_cast<std::uint8_t>(v >> 8U));
        }
        r.samples.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    }
    return r;
}

Rgba8Image rgba8(const Samples& s)
{
    Rgba8Image out{s.width, s.height, std::vector<std::uint8_t>(s.pixels() * 4)};
    const auto channels = static_cast<std::size_t>(s.channels);
    const auto at = [&s](std::size_t i) {
        const std::uint16_t v = s.values[i];
        return static_cast<std::uint8_t>(s.depth == 16 ? (v + 128U) / 257U : v);
    };
    for (std::size_t p = 0; p < s.pixels(); ++p) {
        const std::size_t in = p * channels;
        std::uint8_t* px = out.pixels.data() + (p * 4);
        const bool grey = s.channels <= 2;
        px[0] = at(in);
        px[1] = grey ? at(in) : at(in + 1);
        px[2] = grey ? at(in) : at(in + 2);
        px[3] = (s.channels == 2 || s.channels == 4) ? at(in + channels - 1) : 255;
    }
    return out;
}

// An iTXt XMP chunk with the location and previews taken out, written uncompressed.
std::optional<detail::PngChunk> cleanXmpChunk(const detail::PngChunk& chunk)
{
    const Bytes& d = chunk.data;
    const auto keywordEnd = std::ranges::find(d, std::uint8_t{0});
    if (keywordEnd == d.end() || std::string(d.begin(), keywordEnd) != "XML:com.adobe.xmp") {
        return chunk;
    }
    auto at = static_cast<std::size_t>(keywordEnd - d.begin()) + 1;
    if (at + 2 > d.size()) {
        return chunk;
    }
    const bool compressed = d[at] != 0;
    at += 2;
    // The language tag and the translated keyword, each ending in a zero.
    for (int field = 0; field < 2; ++field) {
        const auto end = std::find(d.begin() + static_cast<std::ptrdiff_t>(at), d.end(), std::uint8_t{0});
        if (end == d.end()) {
            return chunk;
        }
        at = static_cast<std::size_t>(end - d.begin()) + 1;
    }
    std::optional<Bytes> text = compressed ? detail::inflateAll(d.data() + at, d.size() - at)
                                           : Bytes(d.begin() + static_cast<std::ptrdiff_t>(at), d.end());
    if (!text) {
        return chunk;
    }
    const Bytes clean = detail::xmpWithoutExtras(*text);
    detail::PngChunk out{"iTXt", Bytes(d.begin(), keywordEnd + 1)};
    out.data.insert(out.data.end(), {0, 0, 0, 0}); // uncompressed, no language, no translation
    out.data.insert(out.data.end(), clean.begin(), clean.end());
    return out;
}

// The original's chunks to carry over, split by where they go, or the note to leave the file.
struct Ancillary
{
    std::vector<detail::PngChunk> early;
    std::vector<detail::PngChunk> late;
    CompressNote problem = CompressNote::None;
};

Ancillary ancillaryChunks(const std::vector<detail::PngChunk>& chunks, bool removeExtras)
{
    static const std::set<std::string> kEarly = {"gAMA", "cHRM", "sRGB", "iCCP", "cICP", "mDCV", "cLLI"};
    static const std::set<std::string> kRemade = {"IHDR", "PLTE", "tRNS", "IDAT",
                                                  "IEND", "bKGD", "hIST", "sBIT"};
    static const std::set<std::string> kKnown = {"pHYs", "sPLT", "tEXt", "zTXt", "iTXt", "tIME", "eXIf"};
    Ancillary a;
    for (const detail::PngChunk& c : chunks) {
        if (c.type == "acTL") {
            a.problem = CompressNote::Animated;
            return a;
        }
        if (kRemade.contains(c.type)) {
            continue;
        }
        if (std::isupper(static_cast<unsigned char>(c.type[0])) != 0) {
            a.problem = CompressNote::NotSupported; // a critical chunk this does not know
            return a;
        }
        if (kEarly.contains(c.type)) {
            a.early.push_back(c);
            continue;
        }
        // Unknown chunks that depend on the image data may not be copied when it changes.
        if (!kKnown.contains(c.type) && std::isupper(static_cast<unsigned char>(c.type[3])) != 0) {
            continue;
        }
        if (removeExtras && c.type == "eXIf") {
            detail::PngChunk clean{c.type, detail::exifWithoutExtras(c.data)};
            if (!clean.data.empty()) {
                a.late.push_back(std::move(clean));
            }
            continue;
        }
        if (removeExtras && c.type == "iTXt") {
            if (auto clean = cleanXmpChunk(c)) {
                a.late.push_back(std::move(*clean));
            }
            continue;
        }
        a.late.push_back(c);
    }
    return a;
}

Result<Compressed> compressPng(const Bytes& file, const CompressOptions& options)
{
    const auto chunks = detail::readPngChunks(file);
    if (!chunks) {
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
    const Ancillary ancillary = ancillaryChunks(*chunks, options.removeExtras);
    if (ancillary.problem != CompressNote::None) {
        return unchanged(file, ancillary.problem);
    }
    std::optional<Samples> samples = decodePng(file);
    if (!samples) {
        return unchanged(file, CompressNote::NotSupported);
    }
    reduce(*samples);
    constexpr std::size_t kThoroughPixels = 4'000'000;
    const bool thorough = samples->pixels() <= kThoroughPixels;
    const auto encode = [&](const detail::PngRaster& r) {
        return detail::writePng(r, ancillary.early, ancillary.late, thorough);
    };
    Compressed c;
    c.originalSize = file.size();
    c.bytes = encode(trueColour(*samples));
    c.method = CompressMethod::PngLossless;
    // Up to 256 colours fit a palette exactly; more need FewerColours, within the quality floor.
    const Rgba8Image image = rgba8(*samples);
    const std::optional<IndexedImage> exact =
        samples->depth == 8 ? exactPalette(image) : std::optional<IndexedImage>{};
    if (exact) {
        keepSmaller(c, encode(detail::paletteRaster(*exact)), CompressMethod::PngLossless);
    } else if (options.png == PngCompression::FewerColours) {
        const Quantized q = quantize(image);
        if (q.quality >= options.pngQuality) {
            keepSmaller(c, encode(detail::paletteRaster(q.image)), CompressMethod::PngFewerColours);
            c.quality = c.method == CompressMethod::PngFewerColours ? q.quality : 100;
        } else {
            c.note = CompressNote::BelowQuality;
        }
    }
    return c;
}

// ---- GIF

// The loop count of the NETSCAPE2.0 extension, -1 when there is none (the animation plays once).
int loopOf(const Bytes& file)
{
    constexpr std::string_view kApp = "NETSCAPE2.0";
    const auto found = std::ranges::search(file, kApp);
    const auto it = found.begin();
    const auto at = static_cast<std::size_t>(it - file.begin()) + kApp.size();
    if (it == file.end() || at + 4 > file.size() || file[at] != 3 || file[at + 1] != 1) {
        return -1;
    }
    return static_cast<int>(unsigned{file[at + 2]} | (unsigned{file[at + 3]} << 8U));
}

Result<Compressed> compressGif(const Bytes& file)
{
    vips::VImage image = fromBuffer(file, vips::VImage::option()->set("n", -1));
    const int pageHeight =
        image.get_typeof("page-height") != 0 ? image.get_int("page-height") : image.height();
    if (image.bands() < 3) {
        image = image.colourspace(VIPS_INTERPRETATION_sRGB);
    }
    if (image.bands() == 3) {
        image = image.bandjoin_const({255});
    }
    image = image.cast(VIPS_FORMAT_UCHAR);
    std::size_t size = 0;
    void* memory = image.write_to_memory(&size);
    const std::unique_ptr<void, decltype(&g_free)> owned(memory, &g_free);
    const auto* bytes = static_cast<const std::uint8_t*>(memory);

    detail::GifAnimation a;
    a.width = image.width();
    a.height = pageHeight;
    const int pages = std::max(1, image.height() / std::max(1, pageHeight));
    const std::size_t frameBytes = static_cast<std::size_t>(a.width) * static_cast<std::size_t>(a.height) * 4;
    for (int i = 0; i < pages; ++i) {
        const std::uint8_t* start = bytes + (frameBytes * static_cast<std::size_t>(i));
        a.frames.push_back({a.width, a.height, Bytes(start, start + frameBytes)});
    }
    if (image.get_typeof("delay") != 0) {
        a.delays = image.get_array_int("delay");
    }
    a.loop = loopOf(file);
    auto written = detail::writeGif(a);
    if (!written) {
        return unchanged(file, CompressNote::TooManyColours);
    }
    Compressed c = unchanged(file, CompressNote::None);
    keepSmaller(c, std::move(*written), CompressMethod::GifLossless);
    return c;
}

// ---- WebP

Result<Compressed> compressWebp(const Bytes& file, const CompressOptions& options)
{
    const auto kind = detail::webpKind(file);
    if (!kind) {
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
    Compressed c = unchanged(file, CompressNote::None);
    if (options.removeExtras) {
        if (auto cleaned = detail::webpWithoutExtras(file)) {
            keepSmaller(c, std::move(*cleaned), CompressMethod::WebpMetadata);
        }
    }
    if (!kind->lossless || kind->animated) {
        if (c.method == CompressMethod::Unchanged) {
            c.note = CompressNote::LossyWebp;
        }
        return c;
    }
    vips::VImage image = fromBuffer(file).copy();
    if (options.removeExtras) {
        cleanMetadata(image);
    }
    const auto save = [&image](bool exact) {
        auto* o = vips::VImage::option()->set("lossless", true)->set("effort", 6);
        if (exact) {
            o->set("exact", true); // the colour under transparent pixels stays too
        }
        return toBuffer(image, ".webp", o);
    };
    try {
        keepSmaller(c, save(true), CompressMethod::WebpLossless);
    } catch (const vips::VError&) {
        vips_error_clear();
    }
    return c;
}

// ---- TIFF

Result<Compressed> compressTiff(const Bytes& file, const CompressOptions& options)
{
    vips::VImage image;
    try {
        image = fromBuffer(file, vips::VImage::option()->set("n", -1)).copy();
    } catch (const vips::VError&) {
        vips_error_clear();
        return unchanged(file, CompressNote::NotSupported); // pages of different sizes
    }
    if (options.removeExtras) {
        cleanMetadata(image);
    }
    const bool floating = image.format() == VIPS_FORMAT_FLOAT || image.format() == VIPS_FORMAT_DOUBLE;
    const auto save = [&image, floating](bool level) {
        auto* o = vips::VImage::option()
                      ->set("compression", VIPS_FOREIGN_TIFF_COMPRESSION_DEFLATE)
                      ->set("predictor", floating ? VIPS_FOREIGN_TIFF_PREDICTOR_FLOAT
                                                  : VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL);
        if (level) {
            o->set("level", 9);
        }
        return toBuffer(image, ".tif", o);
    };
    Compressed c = unchanged(file, CompressNote::None);
    try {
        keepSmaller(c, save(true), CompressMethod::TiffLossless);
    } catch (const vips::VError&) {
        vips_error_clear();
        keepSmaller(c, save(false), CompressMethod::TiffLossless);
    }
    return c;
}

Result<Bytes> readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return Error{ErrorCode::NotFound, "The file " + path.string() + " could not be opened."};
    }
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// ---- Regions

Result<Rgba8Image> regionOf(vips::VImage image, const Region& region)
{
    image = image.autorot();
    const int x = std::clamp(region.x, 0, std::max(0, image.width() - 1));
    const int y = std::clamp(region.y, 0, std::max(0, image.height() - 1));
    const int w = std::clamp(region.width, 1, image.width() - x);
    const int h = std::clamp(region.height, 1, image.height() - y);
    image = image.crop(x, y, w, h);
    if (image.get_typeof(VIPS_META_ICC_NAME) != 0) {
        image = image.icc_transform("srgb", vips::VImage::option()->set("embedded", true)->set("depth", 8));
    } else {
        image = image.colourspace(VIPS_INTERPRETATION_sRGB);
    }
    image = image.cast(VIPS_FORMAT_UCHAR);
    if (image.bands() > 4) {
        image = image.extract_band(0, vips::VImage::option()->set("n", 4));
    }
    if (image.bands() == 3) {
        image = image.bandjoin_const({255});
    }
    std::size_t size = 0;
    void* memory = image.write_to_memory(&size);
    const std::unique_ptr<void, decltype(&g_free)> owned(memory, &g_free);
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    return Rgba8Image{w, h, Bytes(bytes, bytes + size)};
}

// The window of `cw` by `ch` cells with the most edge energy on a small grey copy.
std::pair<int, int> busiest(const Bytes& grey, int width, int height, int cw, int ch)
{
    // Edge energy, summed in an integral image so each window costs four lookups.
    const auto w = static_cast<std::size_t>(width);
    std::vector<double> sum((w + 1) * static_cast<std::size_t>(height + 1), 0.0);
    for (int y = 0; y < height; ++y) {
        double row = 0.0;
        for (int x = 0; x < width; ++x) {
            const std::size_t at = (static_cast<std::size_t>(y) * w) + static_cast<std::size_t>(x);
            const int right = x + 1 < width ? grey[at + 1] : grey[at];
            const int below = y + 1 < height ? grey[at + w] : grey[at];
            row += std::abs(right - grey[at]) + std::abs(below - grey[at]);
            sum[(static_cast<std::size_t>(y + 1) * (w + 1)) + static_cast<std::size_t>(x + 1)] =
                sum[(static_cast<std::size_t>(y) * (w + 1)) + static_cast<std::size_t>(x + 1)] + row;
        }
    }
    const auto area = [&](int x, int y) {
        const auto at = [&](int ax, int ay) {
            return sum[(static_cast<std::size_t>(ay) * (w + 1)) + static_cast<std::size_t>(ax)];
        };
        return at(x + cw, y + ch) - at(x, y + ch) - at(x + cw, y) + at(x, y);
    };
    std::pair<int, int> best = {(width - cw) / 2, (height - ch) / 2};
    double bestEnergy = -1.0;
    const int step = std::max(1, std::min(cw, ch) / 4);
    for (int y = 0; y + ch <= height; y += step) {
        for (int x = 0; x + cw <= width; x += step) {
            if (const double e = area(x, y); e > bestEnergy) {
                bestEnergy = e;
                best = {x, y};
            }
        }
    }
    return best;
}

} // namespace

Result<Compressed> compress(const std::vector<std::uint8_t>& file, const std::filesystem::path& name,
                            const CompressOptions& options)
{
    detail::startVips();
    try {
        switch (kindOf(file, name)) {
        case Kind::Jpeg:
            return compressJpeg(file, options);
        case Kind::Png:
            return compressPng(file, options);
        case Kind::Gif:
            return compressGif(file);
        case Kind::Webp:
            return compressWebp(file, options);
        case Kind::Tiff:
            return compressTiff(file, options);
        case Kind::Other:
            break;
        }
    } catch (const vips::VError&) {
        vips_error_clear();
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    } catch (const std::bad_alloc&) {
        return Error{ErrorCode::EncodeFailed, "There is not enough memory to compress this file."};
    }
    return unchanged(file, CompressNote::NotSupported);
}

Result<Compressed> compress(const std::filesystem::path& path, const CompressOptions& options)
{
    auto file = readFile(path);
    if (!file) {
        return file.error();
    }
    return compress(file.value(), path, options);
}

Result<Region> detailRegion(const std::filesystem::path& path, int width, int height)
{
    detail::startVips();
    try {
        const vips::VImage full = vips::VImage::new_from_file(path.string().c_str());
        const bool turned =
            full.get_typeof(VIPS_META_ORIENTATION) != 0 && full.get_int(VIPS_META_ORIENTATION) >= 5;
        const int fullWidth = turned ? full.height() : full.width();
        const int fullHeight = turned ? full.width() : full.height();
        Region r{0, 0, std::min(width, fullWidth), std::min(height, fullHeight)};
        if (r.width == fullWidth && r.height == fullHeight) {
            return r;
        }
        constexpr int kSmall = 256;
        const vips::VImage small = vips::VImage::thumbnail(path.string().c_str(), kSmall)
                                       .colourspace(VIPS_INTERPRETATION_sRGB)
                                       .extract_band(1) // green carries most of the detail
                                       .cast(VIPS_FORMAT_UCHAR);
        std::size_t size = 0;
        void* memory = small.write_to_memory(&size);
        const std::unique_ptr<void, decltype(&g_free)> owned(memory, &g_free);
        const auto* bytes = static_cast<const std::uint8_t*>(memory);
        const double scale = static_cast<double>(small.width()) / fullWidth;
        const int cw = std::clamp(static_cast<int>(std::lround(r.width * scale)), 1, small.width());
        const int ch = std::clamp(static_cast<int>(std::lround(r.height * scale)), 1, small.height());
        const auto [x, y] = busiest(Bytes(bytes, bytes + size), small.width(), small.height(), cw, ch);
        r.x = std::clamp(static_cast<int>(std::lround(x / scale)), 0, fullWidth - r.width);
        r.y = std::clamp(static_cast<int>(std::lround(y / scale)), 0, fullHeight - r.height);
        return r;
    } catch (const vips::VError&) {
        vips_error_clear();
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
}

Result<Rgba8Image> loadRegion(const std::filesystem::path& path, const Region& region)
{
    detail::startVips();
    try {
        return regionOf(vips::VImage::new_from_file(path.string().c_str(),
                                                    vips::VImage::option()->set("revalidate", true)),
                        region);
    } catch (const vips::VError&) {
        vips_error_clear();
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
}

Result<Rgba8Image> loadRegion(const std::vector<std::uint8_t>& file, const Region& region)
{
    detail::startVips();
    try {
        return regionOf(fromBuffer(file), region);
    } catch (const vips::VError&) {
        vips_error_clear();
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
}

} // namespace pfx
