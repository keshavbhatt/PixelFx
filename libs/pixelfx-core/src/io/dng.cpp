// SPDX-License-Identifier: Apache-2.0
// Linear DNG (ADR-012): the demosaiced RAW that phones write (Samsung Expert RAW, Apple ProRAW),
// read here because LibRaw cannot unpack its JPEG XL compression without Adobe's DNG SDK. The
// data goes through the steps the DNG specification gives for a default rendering: black and
// white levels, the camera's colour (calibration, colour and forward matrices interpolated for
// the white balance), the as-shot white balance, the baseline exposure, and, for the camera's own
// look, the profile gain table map. Strips and tiles are decoded by libvips (jxlload for JPEG XL).
#include "io/dng.h"

#include "core/parallel.h"
#include "io/compress_internal.h"
#include "io/tiff_reader.h"
#include "pfx/color.h"
#include "pfx/resample.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vips/vips8>

namespace pfx::detail {

namespace {

// TIFF and DNG tags.
constexpr std::uint16_t kNewSubfileType = 254;
constexpr std::uint16_t kWidth = 256;
constexpr std::uint16_t kHeight = 257;
constexpr std::uint16_t kBitsPerSample = 258;
constexpr std::uint16_t kCompression = 259;
constexpr std::uint16_t kPhotometric = 262;
constexpr std::uint16_t kMake = 271;
constexpr std::uint16_t kModel = 272;
constexpr std::uint16_t kStripOffsets = 273;
constexpr std::uint16_t kOrientation = 274;
constexpr std::uint16_t kSamplesPerPixel = 277;
constexpr std::uint16_t kRowsPerStrip = 278;
constexpr std::uint16_t kStripByteCounts = 279;
constexpr std::uint16_t kSoftware = 305;
constexpr std::uint16_t kDateTime = 306;
constexpr std::uint16_t kTileWidth = 322;
constexpr std::uint16_t kTileLength = 323;
constexpr std::uint16_t kTileOffsets = 324;
constexpr std::uint16_t kTileByteCounts = 325;
constexpr std::uint16_t kJpegOffset = 513;
constexpr std::uint16_t kJpegLength = 514;
constexpr std::uint16_t kExifIfd = 34665;
constexpr std::uint16_t kMakerNote = 37500;
constexpr std::uint16_t kInteropIfd = 40965;
constexpr std::uint16_t kDngVersion = 50706;
constexpr std::uint16_t kBlackLevelRepeatDim = 50713;
constexpr std::uint16_t kBlackLevel = 50714;
constexpr std::uint16_t kWhiteLevel = 50717;
constexpr std::uint16_t kDefaultCropOrigin = 50719;
constexpr std::uint16_t kDefaultCropSize = 50720;
constexpr std::uint16_t kColorMatrix1 = 50721;
constexpr std::uint16_t kColorMatrix2 = 50722;
constexpr std::uint16_t kCameraCalibration1 = 50723;
constexpr std::uint16_t kCameraCalibration2 = 50724;
constexpr std::uint16_t kAnalogBalance = 50727;
constexpr std::uint16_t kAsShotNeutral = 50728;
constexpr std::uint16_t kAsShotWhiteXy = 50729;
constexpr std::uint16_t kBaselineExposure = 50730;
constexpr std::uint16_t kIlluminant1 = 50778;
constexpr std::uint16_t kIlluminant2 = 50779;
constexpr std::uint16_t kActiveArea = 50829;
constexpr std::uint16_t kForwardMatrix1 = 50964;
constexpr std::uint16_t kForwardMatrix2 = 50965;
constexpr std::uint16_t kBaselineExposureOffset = 51109;
constexpr std::uint16_t kProfileGainTableMap = 52525;

constexpr std::uint32_t kLinearRaw = 34892;
constexpr std::uint32_t kJpegXl = 52546;
constexpr std::uint32_t kJpeg = 7;
constexpr std::uint32_t kOldJpeg = 6;

using color::Mat3;
using color::Vec3;

const Mat3 kIdentity = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
const std::array<double, 2> kD50 = {0.3457, 0.3585};
const std::array<double, 2> kD65 = {0.3127, 0.3290};
// XYZ (D50) to linear ProPhoto RGB: the space the gain table map's weights are given for.
const Mat3 kXyzD50ToProPhoto = {
    {{1.3459433, -0.2556075, -0.0511118}, {-0.5445989, 1.5081673, 0.0205351}, {0.0, 0.0, 1.2118128}}};

// The colour temperature of an EXIF light source, for interpolating the two matrix sets.
double illuminantKelvin(std::uint32_t code)
{
    switch (code) {
    case 2:
    case 14:
        return 4150.0;
    case 3:
    case 24:
        return 3200.0;
    case 10:
    case 21:
        return 6504.0;
    case 11:
    case 22:
        return 7504.0;
    case 12:
        return 6430.0;
    case 13:
        return 5003.0;
    case 15:
        return 3450.0;
    case 17:
        return 2856.0;
    case 18:
        return 4874.0;
    case 19:
        return 6774.0;
    case 20:
        return 5503.0;
    case 23:
        return 5003.0;
    default:
        return 5500.0;
    }
}

// McCamy's approximation of the correlated colour temperature of a white.
double kelvinOfXy(const std::array<double, 2>& xy)
{
    const double n = (xy[0] - 0.3320) / (0.1858 - xy[1]);
    return (449.0 * n * n * n) + (3525.0 * n * n) + (6823.3 * n) + 5520.33;
}

std::array<double, 2> xyOf(const Vec3& xyz)
{
    const double sum = xyz[0] + xyz[1] + xyz[2];
    return sum > 0.0 ? std::array<double, 2>{xyz[0] / sum, xyz[1] / sum} : kD50;
}

Mat3 lerp(const Mat3& a, const Mat3& b, double t)
{
    Mat3 out{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            out.at(r).at(c) = (a.at(r).at(c) * t) + (b.at(r).at(c) * (1.0 - t));
        }
    }
    return out;
}

Mat3 diagonal(const Vec3& v)
{
    return {{{v[0], 0, 0}, {0, v[1], 0}, {0, 0, v[2]}}};
}

// The DNG directories that matter: the main image and the one holding the DNG tags.
struct Dng
{
    const TiffReader& tiff;
    TiffDirectory image;
    TiffDirectory tags;

    [[nodiscard]] std::optional<Mat3> matrix(std::uint16_t tag) const
    {
        const auto v = tiff.reals(tags, tag);
        if (v.size() != 9) {
            return std::nullopt;
        }
        return Mat3{{{v[0], v[1], v[2]}, {v[3], v[4], v[5]}, {v[6], v[7], v[8]}}};
    }
};

std::optional<Dng> findDng(const TiffReader& tiff)
{
    if (!tiff.valid() || tiff.directories().empty() || !tiff.directories().front().contains(kDngVersion)) {
        return std::nullopt;
    }
    for (const TiffDirectory& d : tiff.directories()) {
        if (tiff.u32(d, kNewSubfileType).value_or(0) == 0 && tiff.u32(d, kPhotometric) == kLinearRaw) {
            return Dng{tiff, d, tiff.directories().front()};
        }
    }
    return std::nullopt;
}

// One strip or tile, decoded: its samples, three per pixel, and its size.
struct Chunk
{
    std::vector<std::uint16_t> values;
    int width = 0;
    int height = 0;
};

std::optional<Chunk> decodeChunk(const TiffReader& tiff, std::size_t offset, std::size_t count, bool jpegXl,
                                 int width, int height)
{
    Chunk c{{}, width, height};
    if (!jpegXl) {
        c.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
        for (std::size_t i = 0; i < c.values.size() && (i * 2) + 1 < count; ++i) {
            c.values[i] = tiff.read16(offset + (i * 2));
        }
        return c;
    }
    try {
        const vips::VImage tile =
            vips::VImage::new_from_buffer(tiff.bytes().data() + offset, count, "").cast(VIPS_FORMAT_USHORT);
        if (tile.bands() != 3) {
            return std::nullopt;
        }
        c.width = tile.width();
        c.height = tile.height();
        std::size_t size = 0;
        void* memory = tile.write_to_memory(&size);
        const std::unique_ptr<void, decltype(&g_free)> owned(memory, &g_free);
        const auto* v = static_cast<const std::uint16_t*>(memory);
        c.values.assign(v, v + (size / 2));
        return c;
    } catch (const vips::VError&) {
        vips_error_clear();
        return std::nullopt;
    }
}

// Copies a chunk into the image's samples at x0, y0, as far as the image goes.
void place(const Chunk& c, int x0, int y0, int width, int height, std::vector<float>& out)
{
    for (int y = 0; y < c.height && y0 + y < height; ++y) {
        for (int x = 0; x < c.width && x0 + x < width; ++x) {
            const std::size_t from = ((static_cast<std::size_t>(y) * static_cast<std::size_t>(c.width)) +
                                      static_cast<std::size_t>(x)) *
                                     3;
            const std::size_t to = ((static_cast<std::size_t>(y0 + y) * static_cast<std::size_t>(width)) +
                                    static_cast<std::size_t>(x0 + x)) *
                                   3;
            for (std::size_t i = 0; i < 3; ++i) {
                out[to + i] = static_cast<float>(c.values[from + i]);
            }
        }
    }
}

// The image's samples as floats, three per pixel, from its strips or tiles.
std::optional<std::vector<float>> decodeSamples(const TiffReader& tiff, const TiffDirectory& d, int width,
                                                int height, std::string& error)
{
    const bool tiled = d.contains(kTileOffsets);
    const auto offsets = tiff.u32s(d, tiled ? kTileOffsets : kStripOffsets);
    const auto counts = tiff.u32s(d, tiled ? kTileByteCounts : kStripByteCounts);
    const int chunkWidth = tiled ? static_cast<int>(tiff.u32(d, kTileWidth).value_or(0)) : width;
    const int chunkHeight =
        static_cast<int>(tiled ? tiff.u32(d, kTileLength).value_or(0)
                               : tiff.u32(d, kRowsPerStrip).value_or(static_cast<std::uint32_t>(height)));
    const std::uint32_t compression = tiff.u32(d, kCompression).value_or(1);
    if (offsets.size() != counts.size() || offsets.empty() || chunkWidth <= 0 || chunkHeight <= 0 ||
        (compression != 1 && compression != kJpegXl) || tiff.u32(d, kSamplesPerPixel) != 3U ||
        tiff.u32(d, kBitsPerSample) != 16U) {
        error = "This DNG stores its image in a way PixelFx cannot read yet.";
        return std::nullopt;
    }
    std::vector<float> out(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3, 0.0F);
    const int across = (width + chunkWidth - 1) / chunkWidth;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const int x0 = static_cast<int>(i % static_cast<std::size_t>(across)) * chunkWidth;
        const int y0 = static_cast<int>(i / static_cast<std::size_t>(across)) * chunkHeight;
        const bool inside =
            offsets[i] <= tiff.bytes().size() && counts[i] <= tiff.bytes().size() - offsets[i];
        const auto chunk = inside ? decodeChunk(tiff, offsets[i], counts[i], compression == kJpegXl,
                                                chunkWidth, std::min(chunkHeight, height - y0))
                                  : std::nullopt;
        if (!chunk) {
            error = "The file is damaged or incomplete.";
            return std::nullopt;
        }
        place(*chunk, x0, y0, width, height, out);
    }
    return out;
}

// Camera RGB (black 0, white 1) to the working space, and the as-shot white's temperature.
struct CameraColour
{
    Mat3 toWorking = kIdentity;
    double kelvin = 5500.0;
};

// A DNG's two sets of colour matrices and calibrations, with the temperatures they are for.
struct MatrixSets
{
    Mat3 cm1 = kIdentity;
    Mat3 cm2 = kIdentity;
    Mat3 cc1 = kIdentity;
    Mat3 cc2 = kIdentity;
    std::optional<Mat3> fm1;
    std::optional<Mat3> fm2;
    Mat3 analog = kIdentity;
    double k1 = 6504.0;
    double k2 = 6504.0;

    // The weight of set 1, by inverse temperature, as the DNG specification interpolates.
    [[nodiscard]] double weight(double kelvin) const
    {
        if (k1 == k2) {
            return 1.0;
        }
        return std::clamp(((1.0 / kelvin) - (1.0 / k2)) / ((1.0 / k1) - (1.0 / k2)), 0.0, 1.0);
    }

    [[nodiscard]] Mat3 calibration(double w) const { return color::multiply(analog, lerp(cc1, cc2, w)); }
    [[nodiscard]] Mat3 xyzToCamera(double w) const
    {
        return color::multiply(calibration(w), lerp(cm1, cm2, w));
    }
};

MatrixSets matrixSets(const Dng& dng)
{
    const TiffReader& t = dng.tiff;
    MatrixSets m;
    m.cm1 = dng.matrix(kColorMatrix1).value_or(kIdentity);
    m.cm2 = dng.matrix(kColorMatrix2).value_or(m.cm1);
    m.cc1 = dng.matrix(kCameraCalibration1).value_or(kIdentity);
    m.cc2 = dng.matrix(kCameraCalibration2).value_or(m.cc1);
    m.fm1 = dng.matrix(kForwardMatrix1);
    m.fm2 = dng.matrix(kForwardMatrix2);
    if (const auto ab = t.reals(dng.tags, kAnalogBalance); ab.size() == 3) {
        m.analog = diagonal({ab[0], ab[1], ab[2]});
    }
    m.k1 = illuminantKelvin(t.u32(dng.tags, kIlluminant1).value_or(21));
    m.k2 = dng.tags.contains(kColorMatrix2) ? illuminantKelvin(t.u32(dng.tags, kIlluminant2).value_or(17))
                                            : m.k1;
    return m;
}

// The as-shot white's chromaticity: the white and the matrices depend on each other, so a few
// rounds settle both.
std::array<double, 2> asShotWhite(const Dng& dng, const MatrixSets& m, const Vec3& neutral)
{
    if (const auto xy = dng.tiff.reals(dng.tags, kAsShotWhiteXy);
        xy.size() == 2 && !dng.tags.contains(kAsShotNeutral)) {
        return {xy[0], xy[1]};
    }
    std::array<double, 2> white = kD50;
    for (int round = 0; round < 30; ++round) {
        const double w = m.weight(kelvinOfXy(white));
        const std::array<double, 2> next = xyOf(color::transform(color::inverse(m.xyzToCamera(w)), neutral));
        const bool settled = std::abs(next[0] - white[0]) < 1e-7 && std::abs(next[1] - white[1]) < 1e-7;
        white = next;
        if (settled) {
            break;
        }
    }
    return white;
}

CameraColour cameraColour(const Dng& dng)
{
    const MatrixSets m = matrixSets(dng);
    Vec3 neutral = {1.0, 1.0, 1.0};
    if (const auto n = dng.tiff.reals(dng.tags, kAsShotNeutral);
        n.size() == 3 && n[0] > 0 && n[1] > 0 && n[2] > 0) {
        neutral = {n[0], n[1], n[2]};
    }
    CameraColour out;
    out.kelvin = kelvinOfXy(asShotWhite(dng, m, neutral));
    const double w = m.weight(out.kelvin);
    const Mat3 calibration = m.calibration(w);
    if (m.fm1) {
        // Forward matrices map white-balanced camera values to XYZ (D50), white to D50 itself.
        const Mat3 toD65 = color::multiply(color::kXyzToRec2020, color::bradford(kD50, kD65));
        const Mat3 fm = lerp(*m.fm1, m.fm2.value_or(*m.fm1), w);
        const Vec3 reference = color::transform(color::inverse(calibration), neutral);
        const Mat3 balance = diagonal({1.0 / reference[0], 1.0 / reference[1], 1.0 / reference[2]});
        out.toWorking = color::multiply(
            toD65, color::multiply(fm, color::multiply(balance, color::inverse(calibration))));
        return out;
    }
    const Mat3 cameraToXyz = color::inverse(m.xyzToCamera(w));
    const Vec3 whiteXyz = color::transform(cameraToXyz, neutral);
    Mat3 matrix = color::multiply(color::kXyzToRec2020,
                                  color::multiply(color::bradford(xyOf(whiteXyz), kD65), cameraToXyz));
    const double scale = whiteXyz[1] > 0.0 ? 1.0 / whiteXyz[1] : 1.0;
    for (auto& row : matrix) {
        for (double& v : row) {
            v *= scale;
        }
    }
    out.toWorking = matrix;
    return out;
}

// The profile gain table map (DNG 1.6): a gain per place on the photo and per brightness.
struct GainTableMap
{
    std::uint32_t pointsV = 0;
    std::uint32_t pointsH = 0;
    double spacingV = 1.0;
    double spacingH = 1.0;
    double originV = 0.0;
    double originH = 0.0;
    std::uint32_t pointsN = 0;
    std::array<double, 5> weights{};
    std::vector<float> gains; // [v][h][n]
};

std::optional<GainTableMap> readGainTableMap(const Dng& dng)
{
    const auto it = dng.tags.find(kProfileGainTableMap);
    if (it == dng.tags.end()) {
        return std::nullopt;
    }
    const TiffReader& t = dng.tiff;
    const std::size_t at = it->second.offset;
    const std::size_t size = it->second.count;
    constexpr std::size_t kHeader = 64;
    if (size < kHeader) {
        return std::nullopt;
    }
    GainTableMap g;
    g.pointsV = t.read32(at);
    g.pointsH = t.read32(at + 4);
    g.spacingV = t.readDouble(at + 8);
    g.spacingH = t.readDouble(at + 16);
    g.originV = t.readDouble(at + 24);
    g.originH = t.readDouble(at + 32);
    g.pointsN = t.read32(at + 40);
    for (std::size_t i = 0; i < 5; ++i) {
        g.weights.at(i) = double{t.readFloat(at + 44 + (4 * i))};
    }
    const std::size_t count = std::size_t{g.pointsV} * g.pointsH * g.pointsN;
    if (g.pointsV == 0 || g.pointsH == 0 || g.pointsN < 2 || count > (size - kHeader) / 4 ||
        g.spacingV <= 0 || g.spacingH <= 0) {
        return std::nullopt;
    }
    g.gains.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        g.gains[i] = t.readFloat(at + kHeader + (4 * i));
    }
    return g;
}

void applyGainTableMap(Image& image, const GainTableMap& g, int threads)
{
    const Mat3 toProPhoto = color::multiply(
        kXyzD50ToProPhoto, color::multiply(color::bradford(kD65, kD50), color::kRec2020ToXyz));
    const auto gainAt = [&g](std::size_t v, std::size_t h, double n) {
        const std::size_t base = ((v * g.pointsH) + h) * g.pointsN;
        const auto i = static_cast<std::size_t>(std::floor(n));
        const std::size_t j = std::min<std::size_t>(i + 1, g.pointsN - 1);
        const double f = n - static_cast<double>(i);
        return (double{g.gains[base + i]} * (1.0 - f)) + (double{g.gains[base + j]} * f);
    };
    const auto grid = [](double rel, double origin, double spacing, std::uint32_t points, std::size_t& i0,
                         double& f) {
        const double pos = std::clamp((rel - origin) / spacing, 0.0, static_cast<double>(points - 1));
        i0 = std::min<std::size_t>(static_cast<std::size_t>(pos), points - 1);
        f = pos - static_cast<double>(i0);
    };
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            std::size_t v0 = 0;
            double fv = 0.0;
            grid((y + 0.5) / image.height(), g.originV, g.spacingV, g.pointsV, v0, fv);
            const std::size_t v1 = std::min<std::size_t>(v0 + 1, g.pointsV - 1);
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                std::size_t h0 = 0;
                double fh = 0.0;
                grid((x + 0.5) / image.width(), g.originH, g.spacingH, g.pointsH, h0, fh);
                const std::size_t h1 = std::min<std::size_t>(h0 + 1, g.pointsH - 1);
                const Vec3 pro = color::transform(toProPhoto, {double{p[0]}, double{p[1]}, double{p[2]}});
                const double lo = std::min({pro[0], pro[1], pro[2]});
                const double hi = std::max({pro[0], pro[1], pro[2]});
                const double input =
                    std::clamp((g.weights[0] * pro[0]) + (g.weights[1] * pro[1]) + (g.weights[2] * pro[2]) +
                                   (g.weights[3] * lo) + (g.weights[4] * hi),
                               0.0, 1.0);
                const double n = input * (g.pointsN - 1);
                const double gain = ((gainAt(v0, h0, n) * (1 - fh) + gainAt(v0, h1, n) * fh) * (1 - fv)) +
                                    ((gainAt(v1, h0, n) * (1 - fh) + gainAt(v1, h1, n) * fh) * fv);
                for (int c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(double{p[c]} * gain);
                }
            }
        }
    });
}

// Pixels turned and flipped as the TIFF orientation says, so the photo is upright.
Image upright(const Image& in, std::uint32_t orientation)
{
    if (orientation <= 1 || orientation > 8) {
        return in;
    }
    const bool swap = orientation >= 5;
    Image out(swap ? in.height() : in.width(), swap ? in.width() : in.height());
    const int w = out.width();
    const int h = out.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sx = x;
            int sy = y;
            switch (orientation) {
            case 2:
                sx = w - 1 - x;
                break;
            case 3:
                sx = w - 1 - x;
                sy = h - 1 - y;
                break;
            case 4:
                sy = h - 1 - y;
                break;
            case 5:
                sx = y;
                sy = x;
                break;
            case 6:
                sx = y;
                sy = w - 1 - x;
                break;
            case 7:
                sx = h - 1 - y;
                sy = w - 1 - x;
                break;
            default: // 8
                sx = h - 1 - y;
                sy = x;
                break;
            }
            std::copy_n(in.pixel(sx, sy), 4, out.pixel(x, y));
        }
    }
    return out;
}

// Writes a small TIFF structure in the source's byte order, copying entries and their values.
class ExifWriter
{
public:
    explicit ExifWriter(const TiffReader& source)
        : m_source(source)
        , m_little(source.littleEndian())
    {
        m_out = {'E', 'x', 'i', 'f', 0, 0};
        m_out.insert(m_out.end(), m_little ? std::initializer_list<std::uint8_t>{'I', 'I'}
                                           : std::initializer_list<std::uint8_t>{'M', 'M'});
        put16(42);
        put32(8);
    }

    // Writes a directory of entries (one of them, kExifIfd, a pointer filled in later) and its
    // values; returns where it starts and, through `pointer`, where the Exif pointer is.
    std::size_t directory(const std::vector<std::pair<std::uint16_t, TiffEntry>>& entries,
                          std::optional<std::size_t>& pointer)
    {
        align();
        const std::size_t start = here();
        put16(static_cast<std::uint32_t>(entries.size()));
        std::vector<std::pair<std::size_t, TiffEntry>> later;
        for (const auto& [tag, e] : entries) {
            put16(tag);
            put16(e.type);
            put32(e.count);
            const std::size_t size = TiffReader::typeSize(e.type) * e.count;
            if (tag == kExifIfd) {
                pointer = here();
                put32(0);
            } else if (size <= 4) {
                copy(e.offset, size);
                m_out.insert(m_out.end(), 4 - size, 0);
            } else {
                later.emplace_back(here(), e);
                put32(0);
            }
        }
        put32(0); // no next directory
        for (const auto& [field, e] : later) {
            align();
            set32(field, static_cast<std::uint32_t>(here()));
            copy(e.offset, TiffReader::typeSize(e.type) * e.count);
        }
        return start;
    }

    void set32(std::size_t at, std::uint32_t v)
    {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto shift = static_cast<unsigned>(m_little ? 8 * i : 8 * (3 - i));
            m_out[kBase + at + i] = static_cast<std::uint8_t>((v >> shift) & 0xFFU);
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(m_out); }

private:
    static constexpr std::size_t kBase = 6; // "Exif\0\0"

    [[nodiscard]] std::size_t here() const { return m_out.size() - kBase; }

    void align()
    {
        if (here() % 2 != 0) {
            m_out.push_back(0);
        }
    }

    void put16(std::uint32_t v)
    {
        const auto a = static_cast<std::uint8_t>(v & 0xFFU);
        const auto b = static_cast<std::uint8_t>((v >> 8U) & 0xFFU);
        m_out.insert(m_out.end(), m_little ? std::initializer_list<std::uint8_t>{a, b}
                                           : std::initializer_list<std::uint8_t>{b, a});
    }

    void put32(std::uint32_t v)
    {
        put16(m_little ? v & 0xFFFFU : v >> 16U);
        put16(m_little ? v >> 16U : v & 0xFFFFU);
    }

    void copy(std::size_t offset, std::size_t size)
    {
        const auto first = m_source.bytes().begin() + static_cast<std::ptrdiff_t>(offset);
        m_out.insert(m_out.end(), first, first + static_cast<std::ptrdiff_t>(size));
    }

    const TiffReader& m_source;
    bool m_little = true;
    std::vector<std::uint8_t> m_out;
};

// A small Exif block from the DNG's own: the camera, the dates and the Exif directory, upright,
// without the maker note (its offsets would not survive the move).
std::vector<std::uint8_t> exifOf(const TiffReader& t, const TiffDirectory& ifd0)
{
    ExifWriter writer(t);
    std::vector<std::pair<std::uint16_t, TiffEntry>> first;
    for (const std::uint16_t tag : {kMake, kModel, kSoftware, kDateTime}) {
        if (const auto it = ifd0.find(tag); it != ifd0.end()) {
            first.emplace_back(tag, it->second);
        }
    }
    const auto exifIt = ifd0.find(kExifIfd);
    const auto exif = exifIt != ifd0.end() ? t.directoryAt(t.read32(exifIt->second.offset)) : std::nullopt;
    if (exif) {
        first.emplace_back(kExifIfd, TiffEntry{4, 1, 0});
    }
    std::optional<std::size_t> pointer;
    writer.directory(first, pointer);
    if (pointer && exif) {
        std::vector<std::pair<std::uint16_t, TiffEntry>> entries;
        for (const auto& [tag, e] : *exif) {
            if (tag != kMakerNote && tag != kInteropIfd && TiffReader::typeSize(e.type) > 0) {
                entries.emplace_back(tag, e);
            }
        }
        std::optional<std::size_t> none;
        writer.set32(*pointer, static_cast<std::uint32_t>(writer.directory(entries, none)));
    }
    return writer.take();
}

// Black and white levels: the black repeats over a small pattern, per sample.
struct Levels
{
    std::size_t rows = 1;
    std::size_t cols = 1;
    std::vector<double> black;
    std::vector<double> white;

    [[nodiscard]] double scaled(double value, int x, int y, std::size_t c) const
    {
        const std::size_t pattern =
            ((static_cast<std::size_t>(y) % rows) * cols) + (static_cast<std::size_t>(x) % cols);
        const double b = black[(pattern * 3) + c];
        return std::max(0.0, (value - b) / std::max(1.0, white[c] - b));
    }
};

Levels levelsOf(const Dng& dng)
{
    Levels l;
    const auto repeat = dng.tiff.u32s(dng.tags, kBlackLevelRepeatDim);
    if (repeat.size() == 2) {
        l.rows = std::max<std::uint32_t>(1, repeat[0]);
        l.cols = std::max<std::uint32_t>(1, repeat[1]);
    }
    l.black = dng.tiff.reals(dng.tags, kBlackLevel);
    if (l.black.size() != l.rows * l.cols * 3) {
        l.black.assign(l.rows * l.cols * 3, l.black.empty() ? 0.0 : l.black.front());
    }
    l.white = dng.tiff.reals(dng.tags, kWhiteLevel);
    if (l.white.size() != 3) {
        l.white.assign(3, l.white.empty() ? 65535.0 : l.white.front());
    }
    return l;
}

// The part of the stored image that is the photo: the active area, then the default crop.
struct Area
{
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

Area areaOf(const Dng& dng, int width, int height)
{
    Area a{0, 0, width, height};
    const TiffReader& t = dng.tiff;
    if (const auto active = t.u32s(dng.tags, kActiveArea);
        active.size() == 4 && active[2] > active[0] && active[3] > active[1]) {
        a.top = static_cast<int>(active[0]);
        a.left = static_cast<int>(active[1]);
        a.height = std::min(height - a.top, static_cast<int>(active[2] - active[0]));
        a.width = std::min(width - a.left, static_cast<int>(active[3] - active[1]));
    }
    const auto origin = t.reals(dng.tags, kDefaultCropOrigin);
    const auto crop = t.reals(dng.tags, kDefaultCropSize);
    if (origin.size() == 2 && crop.size() == 2 && crop[0] >= 1 && crop[1] >= 1) {
        a.left += static_cast<int>(origin[0]);
        a.top += static_cast<int>(origin[1]);
        a.width = std::min(a.width - static_cast<int>(origin[0]), static_cast<int>(crop[0]));
        a.height = std::min(a.height - static_cast<int>(origin[1]), static_cast<int>(crop[1]));
    }
    return a;
}

// The samples of `area`, levelled, in the working space, with the baseline exposure.
Image develop(const std::vector<float>& samples, int stride, const Area& area, const Levels& levels,
              const CameraColour& colour, double exposure)
{
    Image image(area.width, area.height);
    parallelFor(area.height, 0, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < area.width; ++x, p += 4) {
                const int sx = x + area.left;
                const int sy = y + area.top;
                const std::size_t at = ((static_cast<std::size_t>(sy) * static_cast<std::size_t>(stride)) +
                                        static_cast<std::size_t>(sx)) *
                                       3;
                Vec3 v{};
                for (std::size_t c = 0; c < 3; ++c) {
                    v.at(c) = levels.scaled(double{samples[at + c]}, sx, sy, c);
                }
                const Vec3 w = color::transform(colour.toWorking, v);
                for (std::size_t c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>(std::max(0.0, w.at(c) * exposure));
                }
                p[3] = 1.0F;
            }
        }
    });
    return image;
}

} // namespace

bool isLinearDng(const std::vector<std::uint8_t>& file)
{
    const TiffReader tiff(file);
    return findDng(tiff).has_value();
}

std::optional<ImageInfo> probeLinearDng(const std::vector<std::uint8_t>& file)
{
    const TiffReader tiff(file);
    const auto dng = findDng(tiff);
    if (!dng) {
        return std::nullopt;
    }
    const Area area = areaOf(*dng, static_cast<int>(tiff.u32(dng->image, kWidth).value_or(0)),
                             static_cast<int>(tiff.u32(dng->image, kHeight).value_or(0)));
    const bool turned = tiff.u32(tiff.directories().front(), kOrientation).value_or(1) >= 5;
    ImageInfo info;
    info.width = turned ? area.height : area.width;
    info.height = turned ? area.width : area.height;
    info.format = "dng";
    info.bitsPerSample = 16;
    info.raw = true;
    return info;
}

Result<LoadedImage> loadLinearDng(const std::vector<std::uint8_t>& file, const LoadOptions& options)
{
    const TiffReader tiff(file);
    const auto dng = findDng(tiff);
    if (!dng) {
        return Error{ErrorCode::DecodeFailed, "This file is not a DNG PixelFx can read."};
    }
    const TiffDirectory& d = dng->image;
    const int width = static_cast<int>(tiff.u32(d, kWidth).value_or(0));
    const int height = static_cast<int>(tiff.u32(d, kHeight).value_or(0));
    constexpr double kMaxMegapixels = 512.0;
    if (width <= 0 || height <= 0 || static_cast<double>(width) * height / 1e6 > kMaxMegapixels) {
        return Error{ErrorCode::DecodeFailed, "The file is damaged or incomplete."};
    }
    std::string error;
    const auto samples = decodeSamples(tiff, d, width, height, error);
    if (!samples) {
        return Error{ErrorCode::DecodeFailed, error};
    }
    const CameraColour colour = cameraColour(*dng);
    // The camera's exposure for its default rendering, and the profile's change to it.
    const auto baseline = tiff.reals(dng->tags, kBaselineExposure);
    const auto offset = tiff.reals(dng->tags, kBaselineExposureOffset);
    const double exposure =
        (baseline.empty() ? 0.0 : baseline.front()) + (offset.empty() ? 0.0 : offset.front());
    Image image =
        develop(*samples, width, areaOf(*dng, width, height), levelsOf(*dng), colour, std::exp2(exposure));
    if (options.rawTone) {
        if (const auto map = readGainTableMap(*dng)) {
            applyGainTableMap(image, *map, 0);
        }
    }
    image = upright(image, tiff.u32(tiff.directories().front(), kOrientation).value_or(1));
    if (options.maxLongEdge > 0 && image.longEdge() > options.maxLongEdge) {
        auto smaller = fitLongEdge(image, options.maxLongEdge);
        if (!smaller) {
            return smaller.error();
        }
        image = std::move(smaller).value();
    }
    LoadedImage result;
    result.info.format = "dng";
    result.info.bitsPerSample = 16;
    result.info.raw = true;
    result.info.asShotKelvin = static_cast<int>(std::lround(colour.kelvin));
    result.info.width = image.width();
    result.info.height = image.height();
    result.metadata.exif = exifOf(tiff, tiff.directories().front());
    result.image = std::move(image);
    return result;
}

std::optional<std::vector<std::uint8_t>> embeddedPreview(const std::vector<std::uint8_t>& file, int& width,
                                                         int& height, std::uint32_t& orientation)
{
    const TiffReader tiff(file);
    if (!tiff.valid() || tiff.directories().empty()) {
        return std::nullopt;
    }
    orientation = tiff.u32(tiff.directories().front(), kOrientation).value_or(1);
    std::optional<std::vector<std::uint8_t>> best;
    long long bestArea = 0;
    for (const TiffDirectory& d : tiff.directories()) {
        std::size_t offset = 0;
        std::size_t length = 0;
        const std::uint32_t compression = tiff.u32(d, kCompression).value_or(0);
        if (compression == kJpeg && tiff.u32s(d, kStripOffsets).size() == 1) {
            offset = tiff.u32(d, kStripOffsets).value_or(0);
            length = tiff.u32(d, kStripByteCounts).value_or(0);
        } else if (d.contains(kJpegOffset)) {
            offset = tiff.u32(d, kJpegOffset).value_or(0);
            length = tiff.u32(d, kJpegLength).value_or(0);
        } else {
            continue;
        }
        if (compression != kJpeg && compression != kOldJpeg && !d.contains(kJpegOffset)) {
            continue;
        }
        const long long area =
            static_cast<long long>(tiff.u32(d, kWidth).value_or(0)) * tiff.u32(d, kHeight).value_or(0);
        if (length < 4 || offset > file.size() || length > file.size() - offset || file[offset] != 0xFF ||
            file[offset + 1] != 0xD8 || (best && area <= bestArea)) {
            continue;
        }
        best = std::vector<std::uint8_t>(file.begin() + static_cast<std::ptrdiff_t>(offset),
                                         file.begin() + static_cast<std::ptrdiff_t>(offset + length));
        bestArea = area;
        width = static_cast<int>(tiff.u32(d, kWidth).value_or(0));
        height = static_cast<int>(tiff.u32(d, kHeight).value_or(0));
    }
    return best;
}

} // namespace pfx::detail
