// SPDX-License-Identifier: Apache-2.0
// pfx::compress: every lossless method gives back the same pixels, smaller or equal files, and
// the location and previews go when asked.
#include "doctest/doctest.h"
#include "fixtures.h"
#include "io/compress_internal.h"
#include "pfx/compress.h"
#include "pfx/io.h"
#include "pfx/quantize.h"

#include <algorithm>
#include <set>
#include <vips/vips8>

namespace {

using Bytes = std::vector<std::uint8_t>;

// An sRGB 8-bit RGB image libvips can encode: the fixture scene.
vips::VImage sceneImage(int width = 192, int height = 128)
{
    pfx::detail::startVips();
    const auto rgba = pfx::toSrgb8(fixtures::scene(width, height));
    REQUIRE(rgba);
    // A copy: libvips before 8.16 takes the pixels through a non-const pointer.
    pfx::Rgba8Image img = rgba.value();
    vips::VImage v = vips::VImage::new_from_memory_copy(img.pixels.data(), img.pixels.size(), img.width,
                                                        img.height, 4, VIPS_FORMAT_UCHAR);
    return v.extract_band(0, vips::VImage::option()->set("n", 3))
        .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
}

Bytes encode(const vips::VImage& image, const char* suffix, vips::VOption* options = nullptr)
{
    pfx::detail::startVips();
    void* data = nullptr;
    std::size_t size = 0;
    image.write_to_buffer(suffix, &data, &size, options);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    Bytes out(bytes, bytes + size);
    g_free(data);
    return out;
}

// Every sample of every page, as decoded.
Bytes pixels(const Bytes& file, bool allPages = false)
{
    pfx::detail::startVips();
    vips::VImage v = vips::VImage::new_from_buffer(file.data(), file.size(), "",
                                                   allPages ? vips::VImage::option()->set("n", -1) : nullptr);
    if (allPages && v.bands() == 3) {
        v = v.bandjoin_const({255}); // GIFs without transparency decode as RGB
    }
    std::size_t size = 0;
    void* data = v.write_to_memory(&size);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    Bytes out(bytes, bytes + size);
    g_free(data);
    return out;
}

bool contains(const Bytes& haystack, const Bytes& needle)
{
    return !std::ranges::search(haystack, needle).empty();
}

// A little Exif: Make "ABC", a GPS directory with the latitude, and an IFD1 thumbnail at the end.
Bytes exifWithGpsAndThumbnail()
{
    Bytes b = {'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 42, 0, 8, 0, 0, 0};
    const auto put16 = [&b](std::size_t at, unsigned v) {
        b.resize(std::max(b.size(), 6 + at + 2));
        b[6 + at] = static_cast<std::uint8_t>(v & 0xFFU);
        b[6 + at + 1] = static_cast<std::uint8_t>(v >> 8U);
    };
    const auto put32 = [&put16](std::size_t at, unsigned v) {
        put16(at, v & 0xFFFFU);
        put16(at + 2, v >> 16U);
    };
    put16(8, 2);
    put16(10, 0x010F);
    put16(12, 2);
    put32(14, 4);
    put32(18, 0x00434241); // Make "ABC"
    put16(22, 0x8825);
    put16(24, 4);
    put32(26, 1);
    put32(30, 100); // GPS at 100
    put32(34, 200); // IFD1 at 200
    put16(100, 1);
    put16(102, 0x0002);
    put16(104, 5);
    put32(106, 3);
    put32(110, 150); // GPSLatitude at 150
    for (unsigned i = 0; i < 6; ++i) {
        put32(150 + (4 * i), 0x5A5A5A00U + i); // the coordinates, easy to look for
    }
    put16(200, 1); // IFD1: the thumbnail's offset
    put16(202, 0x0201);
    put16(204, 4);
    put32(206, 1);
    put32(210, 220);
    put32(214, 0);
    for (std::size_t i = 220; i < 300; ++i) {
        b.resize(std::max(b.size(), 6 + i + 1));
        b[6 + i] = 0x77; // the thumbnail's bytes
    }
    return b;
}

// `jpeg` with APP segments inserted after SOI.
Bytes withSegments(const Bytes& jpeg, const std::vector<std::pair<int, Bytes>>& segments)
{
    Bytes out(jpeg.begin(), jpeg.begin() + 2);
    for (const auto& [code, data] : segments) {
        const std::size_t length = data.size() + 2;
        out.insert(out.end(), {0xFF, static_cast<std::uint8_t>(code), static_cast<std::uint8_t>(length >> 8U),
                               static_cast<std::uint8_t>(length & 0xFFU)});
        out.insert(out.end(), data.begin(), data.end());
    }
    out.insert(out.end(), jpeg.begin() + 2, jpeg.end());
    return out;
}

pfx::Rgba8Image frame(int width, int height, std::array<std::uint8_t, 4> fill)
{
    pfx::Rgba8Image f{width, height, Bytes(static_cast<std::size_t>(width * height) * 4)};
    for (std::size_t i = 0; i < f.pixels.size(); i += 4) {
        std::ranges::copy(fill, f.pixels.begin() + static_cast<std::ptrdiff_t>(i));
    }
    return f;
}

void paint(pfx::Rgba8Image& f, int x0, int y0, int x1, int y1, std::array<std::uint8_t, 4> c)
{
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            std::ranges::copy(c, f.pixels.begin() + (static_cast<std::ptrdiff_t>((y * f.width) + x) * 4));
        }
    }
}

} // namespace

TEST_CASE("the Exif thumbnail is unlinked and cut off; the rest stays")
{
    const Bytes before = exifWithGpsAndThumbnail();
    const Bytes after = pfx::exifWithoutThumbnail(before);
    CHECK(after.size() == 6 + 174); // ends where the GPS coordinates end
    CHECK(after[6 + 34] == 0);      // no next directory
    CHECK(after[6 + 18] == 'A');    // Make kept
    CHECK(!contains(after, Bytes{0x77, 0x77, 0x77, 0x77}));
    const Bytes clean = pfx::detail::exifWithoutExtras(before);
    CHECK(!contains(clean, Bytes{0x00, 0x5A, 0x5A, 0x5A}));
    CHECK(pfx::exifWithoutThumbnail(Bytes{1, 2, 3}) == Bytes{1, 2, 3});
}

TEST_CASE("JPEG lossless: identical pixels, no larger, location and previews removed")
{
    const Bytes plain =
        encode(sceneImage(), ".jpg", vips::VImage::option()->set("Q", 92)->set("optimize_coding", false));
    const Bytes mpf = {'M', 'P', 'F', 0, 1, 2, 3};
    const Bytes original = withSegments(plain, {{0xE1, exifWithGpsAndThumbnail()}, {0xE2, mpf}});
    const auto c = pfx::compress(original, "photo.jpg");
    REQUIRE(c);
    CHECK(c.value().method == pfx::CompressMethod::JpegLossless);
    CHECK(c.value().smaller());
    CHECK(pixels(c.value().bytes) == pixels(original));
    CHECK(!contains(c.value().bytes, Bytes{0x00, 0x5A, 0x5A, 0x5A})); // no coordinates
    CHECK(!contains(c.value().bytes, Bytes{0x77, 0x77, 0x77, 0x77})); // no thumbnail
    CHECK(!contains(c.value().bytes, Bytes{'M', 'P', 'F', 0}));
    CHECK(contains(c.value().bytes, Bytes{'A', 'B', 'C', 0})); // the camera stays

    pfx::CompressOptions keep;
    keep.removeExtras = false;
    const auto kept = pfx::compress(original, "photo.jpg", keep);
    REQUIRE(kept);
    CHECK(contains(kept.value().bytes, Bytes{0x00, 0x5A, 0x5A, 0x5A}));

    pfx::CompressOptions quality;
    quality.jpeg = pfx::JpegCompression::Quality;
    quality.jpegQuality = 60;
    const auto smaller = pfx::compress(original, "photo.jpg", quality);
    REQUIRE(smaller);
    CHECK(smaller.value().method == pfx::CompressMethod::JpegQuality);
    CHECK(smaller.value().bytes.size() < c.value().bytes.size());

    CHECK(!pfx::compress(Bytes{0xFF, 0xD8, 0xFF, 0xE0, 0, 4}, "broken.jpg"));
}

TEST_CASE("PNG lossless: identical pixels in the smallest form, colour chunks kept")
{
    const Bytes original = encode(sceneImage(), ".png", vips::VImage::option()->set("compression", 1));
    const auto c = pfx::compress(original, "scene.png");
    REQUIRE(c);
    CHECK(c.value().method == pfx::CompressMethod::PngLossless);
    CHECK(c.value().smaller());
    CHECK(pixels(c.value().bytes) == pixels(original));

    // Few colours, stored as RGBA at 16 bits: back as an 8-bit palette, the same pixels.
    vips::VImage few =
        vips::VImage::black(64, 64, vips::VImage::option()->set("bands", 3)).linear(0.0, 200.0);
    few = few.insert(vips::VImage::black(20, 20, vips::VImage::option()->set("bands", 3)).linear(0.0, 30.0),
                     10, 10)
              .bandjoin_const({255})
              .cast(VIPS_FORMAT_UCHAR)
              .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    const vips::VImage wide =
        (few.cast(VIPS_FORMAT_USHORT) * 257)
            .cast(VIPS_FORMAT_USHORT)
            .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_RGB16));
    const Bytes deep = encode(wide, ".png");
    const auto p = pfx::compress(deep, "few.png");
    REQUIRE(p);
    CHECK(p.value().smaller());
    CHECK(p.value().bytes[25] == 3); // colour type: palette
    const vips::VImage back =
        vips::VImage::new_from_buffer(p.value().bytes.data(), p.value().bytes.size(), "");
    CHECK(back.bands() == 3); // opaque: no alpha
    CHECK(pixels(p.value().bytes) ==
          pixels(encode(few.extract_band(0, vips::VImage::option()->set("n", 3)), ".png")));

    // The colour profile survives.
    const Bytes withProfile = encode(sceneImage().icc_transform("srgb"), ".png");
    const auto kept = pfx::compress(withProfile, "profile.png");
    REQUIRE(kept);
    const vips::VImage decoded =
        vips::VImage::new_from_buffer(kept.value().bytes.data(), kept.value().bytes.size(), "");
    CHECK(decoded.get_typeof(VIPS_META_ICC_NAME) != 0);
}

TEST_CASE("PNG fewer colours: a palette within the quality floor, otherwise lossless")
{
    const Bytes original = encode(sceneImage(), ".png");
    pfx::CompressOptions lossy;
    lossy.png = pfx::PngCompression::FewerColours;
    lossy.pngQuality = 0;
    const auto c = pfx::compress(original, "scene.png", lossy);
    REQUIRE(c);
    CHECK(c.value().method == pfx::CompressMethod::PngFewerColours);
    CHECK(c.value().smaller());
    CHECK(c.value().quality > 50);
    const Bytes decoded = pixels(c.value().bytes);
    std::set<std::uint32_t> colours;
    for (std::size_t i = 0; i + 2 < decoded.size(); i += 3) {
        colours.insert(std::uint32_t{decoded[i]} | (std::uint32_t{decoded[i + 1]} << 8U) |
                       (std::uint32_t{decoded[i + 2]} << 16U));
    }
    CHECK(colours.size() <= 256);

    lossy.pngQuality = 100;
    const auto strict = pfx::compress(original, "scene.png", lossy);
    REQUIRE(strict);
    CHECK(strict.value().method == pfx::CompressMethod::PngLossless);
    CHECK(strict.value().note == pfx::CompressNote::BelowQuality);
}

TEST_CASE("quantize keeps few colours exactly and scores the rest")
{
    pfx::Rgba8Image few = frame(16, 16, {10, 20, 30, 255});
    paint(few, 0, 0, 8, 8, {200, 0, 0, 128});
    const pfx::Quantized exact = pfx::quantize(few);
    CHECK(exact.quality == 100);
    CHECK(exact.image.palette.size() == 2);

    const auto rgba = pfx::toSrgb8(fixtures::scene(96, 64));
    REQUIRE(rgba);
    const pfx::Quantized q = pfx::quantize(rgba.value(), {16, true});
    CHECK(q.image.palette.size() <= 16);
    CHECK(q.quality < 100);
    CHECK(pfx::quantize(rgba.value(), {256, true}).quality > q.quality);
    CHECK(pfx::qualityOfError(pfx::errorOfQuality(80)) == 80);
}

TEST_CASE("GIF frames are stored again without loss, as changes or whole")
{
    // Frame 2 changes a square; frame 3 makes part of it transparent again, so the writer
    // must store whole frames.
    const pfx::Rgba8Image white = frame(40, 30, {255, 255, 255, 255});
    pfx::Rgba8Image square = white;
    paint(square, 5, 5, 15, 15, {255, 0, 0, 255});
    const std::vector<pfx::Rgba8Image> frames = {white, square};
    for (const bool turnsClear : {false, true}) {
        pfx::detail::GifAnimation a{40, 30, frames, {100, 200}, 0};
        if (turnsClear) {
            pfx::Rgba8Image clearCorner = square;
            paint(clearCorner, 0, 0, 4, 4, {0, 0, 0, 0});
            a.frames.push_back(clearCorner);
            a.delays.push_back(50);
        }
        const auto gif = pfx::detail::writeGif(a);
        REQUIRE(gif);
        Bytes expected;
        for (const pfx::Rgba8Image& f : a.frames) {
            for (std::size_t i = 0; i < f.pixels.size(); i += 4) {
                const bool clear = f.pixels[i + 3] == 0;
                // Fully transparent pixels decode as transparent black.
                expected.insert(expected.end(), {clear ? std::uint8_t{0} : f.pixels[i],
                                                 clear ? std::uint8_t{0} : f.pixels[i + 1],
                                                 clear ? std::uint8_t{0} : f.pixels[i + 2], f.pixels[i + 3]});
            }
        }
        CHECK(pixels(*gif, true) == expected);
        const auto again = pfx::compress(*gif, "anim.gif");
        REQUIRE(again);
        CHECK(pixels(again.value().bytes, true) == expected);
    }
}

TEST_CASE("WebP: lossless files encoded again without loss, lossy ones left alone")
{
    const Bytes lossless =
        encode(sceneImage(), ".webp", vips::VImage::option()->set("lossless", true)->set("effort", 0));
    const auto c = pfx::compress(lossless, "scene.webp");
    REQUIRE(c);
    CHECK(pixels(c.value().bytes) == pixels(lossless));
    CHECK(c.value().bytes.size() <= lossless.size());

    const Bytes lossy = encode(sceneImage(), ".webp", vips::VImage::option()->set("Q", 80));
    const auto l = pfx::compress(lossy, "scene.webp");
    REQUIRE(l);
    CHECK(l.value().method == pfx::CompressMethod::Unchanged);
    CHECK(l.value().note == pfx::CompressNote::LossyWebp);
    CHECK(l.value().bytes == lossy);
}

TEST_CASE("TIFF compressed without loss; a RAW file built on TIFF is left alone")
{
    const Bytes original = encode(
        sceneImage(), ".tif", vips::VImage::option()->set("compression", VIPS_FOREIGN_TIFF_COMPRESSION_NONE));
    const auto c = pfx::compress(original, "scan.tif");
    REQUIRE(c);
    CHECK(c.value().method == pfx::CompressMethod::TiffLossless);
    CHECK(c.value().smaller());
    CHECK(pixels(c.value().bytes) == pixels(original));

    const auto raw = pfx::compress(original, "photo.dng");
    REQUIRE(raw);
    CHECK(raw.value().method == pfx::CompressMethod::Unchanged);
    CHECK(raw.value().note == pfx::CompressNote::NotSupported);
}

TEST_CASE("the detail region is where the picture is busiest, and loads at full size")
{
    // Flat on the left, fine noise on the right.
    pfx::detail::startVips();
    vips::VImage flatHalf =
        vips::VImage::black(200, 100, vips::VImage::option()->set("bands", 3)).linear(0.0, 128.0);
    const vips::VImage noise =
        vips::VImage::gaussnoise(200, 100, vips::VImage::option()->set("sigma", 60.0)->set("seed", 7));
    const vips::VImage busyHalf = vips::VImage::bandjoin({noise, noise, noise});
    const vips::VImage image =
        flatHalf.join(busyHalf, VIPS_DIRECTION_HORIZONTAL)
            .cast(VIPS_FORMAT_UCHAR)
            .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    const fixtures::TempDir dir;
    const auto path = dir.path() / "busy.png";
    image.write_to_file(path.string().c_str());
    const auto region = pfx::detailRegion(path, 80, 40);
    REQUIRE(region);
    CHECK(region.value().x >= 190);
    CHECK(region.value().width == 80);
    const auto pixelsHere = pfx::loadRegion(path, region.value());
    REQUIRE(pixelsHere);
    CHECK(pixelsHere.value().width == 80);
    CHECK(pixelsHere.value().height == 40);
    CHECK(pixelsHere.value().pixels.size() == 80 * 40 * 4);
}
