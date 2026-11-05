// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/io.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <fstream>
#include <lcms2.h>
#include <vips/vips8>

TEST_CASE("JPEG, PNG, TIFF and WebP save and load back close to the original")
{
    const fixtures::TempDir dir;
    const pfx::Image scene = fixtures::scene();
    for (const char* ext : {".png", ".tif", ".jpg", ".webp"}) {
        CAPTURE(ext);
        const auto path = dir.path() / (std::string("scene") + ext);
        pfx::SaveOptions options;
        options.quality = 95;
        REQUIRE(pfx::save(scene, path, options).ok());
        const auto loaded = pfx::load(path);
        REQUIRE(loaded.ok());
        CHECK(loaded.value().image.width() == scene.width());
        CHECK(loaded.value().info.hasProfile);
        const fixtures::Diff diff = fixtures::compare(loaded.value().image, scene);
        CAPTURE(diff.mean);
        const bool lossy = std::string(ext) == ".jpg" || std::string(ext) == ".webp";
        CHECK(diff.mean < (lossy ? 0.02 : 0.003));
    }
}

TEST_CASE("16-bit PNG keeps more precision than 8-bit")
{
    const fixtures::TempDir dir;
    const pfx::Image ramp = fixtures::ramp(1024, 4);
    pfx::SaveOptions eight;
    pfx::SaveOptions sixteen;
    sixteen.bitsPerSample = 16;
    REQUIRE(pfx::save(ramp, dir.path() / "8.png", eight).ok());
    REQUIRE(pfx::save(ramp, dir.path() / "16.png", sixteen).ok());
    const auto a = pfx::load(dir.path() / "8.png");
    const auto b = pfx::load(dir.path() / "16.png");
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(b.value().info.bitsPerSample == 16);
    CHECK(fixtures::compare(b.value().image, ramp).max < fixtures::compare(a.value().image, ramp).max);
}

TEST_CASE("an untagged sRGB red lands on the Rec.2020 coordinates of sRGB red")
{
    const fixtures::TempDir dir;
    const auto path = dir.path() / "red.png";
    vips::VImage red = vips::VImage::black(4, 4).bandjoin_const({0, 0}).new_from_image({255, 0, 0});
    red = red.cast(VIPS_FORMAT_UCHAR)
              .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    red.write_to_file(path.string().c_str());
    const auto loaded = pfx::load(path);
    REQUIRE(loaded.ok());
    CHECK_FALSE(loaded.value().info.hasProfile);
    const float* p = loaded.value().image.pixel(1, 1);
    CHECK(p[0] == doctest::Approx(pfx::color::kSrgbToRec2020[0][0]).epsilon(0.01));
    CHECK(p[1] == doctest::Approx(pfx::color::kSrgbToRec2020[1][0]).epsilon(0.05));
    CHECK(p[2] == doctest::Approx(pfx::color::kSrgbToRec2020[2][0]).epsilon(0.05));
}

TEST_CASE("a photo tagged Adobe RGB loads as LittleCMS converts it, and saves back")
{
    // Adobe RGB (1998): its primaries, D65, gamma 2.2; and the working space for the expectation.
    const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
    const cmsCIExyYTRIPLE adobe = {{0.64, 0.33, 1.0}, {0.21, 0.71, 1.0}, {0.15, 0.06, 1.0}};
    const cmsCIExyYTRIPLE rec2020 = {{0.708, 0.292, 1.0}, {0.170, 0.797, 1.0}, {0.131, 0.046, 1.0}};
    cmsToneCurve* gamma = cmsBuildGamma(nullptr, 563.0 / 256.0);
    cmsToneCurve* linear = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve* adobeCurves[3] = {gamma, gamma, gamma};
    cmsToneCurve* linearCurves[3] = {linear, linear, linear};
    cmsHPROFILE adobeProfile = cmsCreateRGBProfile(&d65, &adobe, adobeCurves);
    cmsHPROFILE working = cmsCreateRGBProfile(&d65, &rec2020, linearCurves);
    cmsUInt32Number size = 0;
    cmsSaveProfileToMem(adobeProfile, nullptr, &size);
    std::vector<unsigned char> icc(size);
    cmsSaveProfileToMem(adobeProfile, icc.data(), &size);

    const std::array<std::array<int, 3>, 4> colours = {
        {{200, 100, 50}, {20, 180, 90}, {128, 128, 128}, {3, 2, 250}}};
    const fixtures::TempDir dir;
    const auto path = dir.path() / "adobe.png";
    std::vector<unsigned char> bytes;
    for (const auto& c : colours) {
        bytes.insert(bytes.end(), {static_cast<unsigned char>(c[0]), static_cast<unsigned char>(c[1]),
                                   static_cast<unsigned char>(c[2])});
    }
    vips::VImage img = vips::VImage::new_from_memory(bytes.data(), bytes.size(), 4, 1, 3, VIPS_FORMAT_UCHAR);
    img = img.copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    vips_image_set_blob_copy(img.get_image(), VIPS_META_ICC_NAME, icc.data(), icc.size());
    img.write_to_file(path.string().c_str());

    cmsHTRANSFORM expect = cmsCreateTransform(adobeProfile, TYPE_RGB_DBL, working, TYPE_RGB_DBL,
                                              INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    const auto loaded = pfx::load(path);
    REQUIRE(loaded.ok());
    CHECK(loaded.value().info.hasProfile);
    for (int x = 0; x < 4; ++x) {
        const auto& c = colours.at(static_cast<std::size_t>(x));
        const std::array<double, 3> in = {c[0] / 255.0, c[1] / 255.0, c[2] / 255.0};
        std::array<double, 3> out{};
        cmsDoTransform(expect, in.data(), out.data(), 1);
        const float* p = loaded.value().image.pixel(x, 0);
        for (int ch = 0; ch < 3; ++ch) {
            CAPTURE(x);
            CAPTURE(ch);
            CHECK(double{p[ch]} == doctest::Approx(out.at(static_cast<std::size_t>(ch))).epsilon(2e-3));
        }
    }
    cmsDeleteTransform(expect);
    cmsCloseProfile(adobeProfile);
    cmsCloseProfile(working);
    cmsFreeToneCurve(gamma);
    cmsFreeToneCurve(linear);
}

TEST_CASE("the orientation tag is applied on load and reset on save")
{
    const fixtures::TempDir dir;
    const auto path = dir.path() / "rotated.jpg";
    vips::VImage img = vips::VImage::black(40, 20).bandjoin_const({0, 0}).cast(VIPS_FORMAT_UCHAR);
    img = img.copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    img.set(VIPS_META_ORIENTATION, 6); // rotate 90 degrees to display
    img.write_to_file(path.string().c_str());
    const auto loaded = pfx::load(path);
    REQUIRE(loaded.ok());
    CHECK(loaded.value().image.width() == 20);
    CHECK(loaded.value().image.height() == 40);
    const auto out = dir.path() / "upright.jpg";
    REQUIRE(pfx::save(loaded.value().image, out, {}, &loaded.value().metadata).ok());
    const auto again = pfx::load(out);
    REQUIRE(again.ok());
    CHECK(again.value().image.width() == 20);
}

TEST_CASE("alpha survives a PNG round trip; a missing file and a non-image fail with reasons")
{
    const fixtures::TempDir dir;
    pfx::Image img = fixtures::flat(0.2F, 0.4F, 0.6F);
    img.pixel(0, 0)[3] = 0.25F;
    REQUIRE(pfx::save(img, dir.path() / "alpha.png").ok());
    const auto loaded = pfx::load(dir.path() / "alpha.png");
    REQUIRE(loaded.ok());
    CHECK(loaded.value().info.hasAlpha);
    CHECK(loaded.value().image.pixel(0, 0)[3] == doctest::Approx(0.25F).epsilon(0.01));
    CHECK(pfx::load(dir.path() / "nope.jpg").error().code == pfx::ErrorCode::NotFound);
    const auto text = dir.path() / "text.jpg";
    {
        std::ofstream(text) << "not an image";
    }
    CHECK(pfx::load(text).error().code == pfx::ErrorCode::DecodeFailed);
}

TEST_CASE("load can downscale for previews")
{
    const fixtures::TempDir dir;
    REQUIRE(pfx::save(fixtures::scene(400, 300), dir.path() / "big.png").ok());
    const auto small = pfx::load(dir.path() / "big.png", {100});
    REQUIRE(small.ok());
    CHECK(small.value().image.width() == 100);
    CHECK(small.value().image.height() == 75);
}

TEST_CASE("a file overwritten on disk is read again, not served from a cache")
{
    const fixtures::TempDir dir;
    const auto path = dir.path() / "same.png";
    REQUIRE(pfx::save(fixtures::flat(0.1F, 0.1F, 0.1F), path).ok());
    REQUIRE(pfx::load(path).ok());
    REQUIRE(pfx::save(fixtures::flat(0.8F, 0.8F, 0.8F), path).ok());
    const auto second = pfx::load(path);
    REQUIRE(second.ok());
    CHECK(second.value().image.pixel(0, 0)[1] == doctest::Approx(0.8F).epsilon(0.02));
}

TEST_CASE("greyscale images, with and without alpha, load as neutral RGBA")
{
    const fixtures::TempDir dir;
    vips::VImage grey = vips::VImage::black(6, 4).new_from_image(128).cast(VIPS_FORMAT_UCHAR);
    grey = grey.copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_B_W));
    grey.write_to_file((dir.path() / "grey.png").string().c_str());
    vips::VImage::bandjoin({grey, grey.new_from_image(64).cast(VIPS_FORMAT_UCHAR)})
        .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_B_W))
        .write_to_file((dir.path() / "grey-alpha.png").string().c_str());
    for (const char* name : {"grey.png", "grey-alpha.png"}) {
        CAPTURE(name);
        const auto loaded = pfx::load(dir.path() / name);
        REQUIRE(loaded.ok());
        const float* p = loaded.value().image.pixel(2, 2);
        CHECK(p[0] == doctest::Approx(p[1]).epsilon(1e-3));
        CHECK(p[1] == doctest::Approx(p[2]).epsilon(1e-3));
        CHECK(pfx::color::srgbEncode(p[1]) == doctest::Approx(128.0 / 255.0).epsilon(0.01));
        CHECK(p[3] == doctest::Approx(std::string(name) == "grey.png" ? 1.0F : 64.0F / 255.0F).epsilon(0.01));
    }
}

TEST_CASE("display conversion gives 8-bit sRGB matching what save writes")
{
    const fixtures::TempDir dir;
    const pfx::Image scene = fixtures::scene(64, 48);
    const auto display = pfx::toSrgb8(scene);
    REQUIRE(display.ok());
    CHECK(display.value().pixels.size() == static_cast<std::size_t>(64 * 48 * 4));
    REQUIRE(pfx::save(scene, dir.path() / "ref.png").ok());
    vips::VImage ref = vips::VImage::new_from_file((dir.path() / "ref.png").string().c_str());
    std::size_t size = 0;
    void* mem = ref.write_to_memory(&size);
    const auto* bytes = static_cast<const std::uint8_t*>(mem);
    int worst = 0;
    for (int i = 0; i < 64 * 48; ++i) {
        for (int c = 0; c < 3; ++c) {
            const int a =
                display.value().pixels[static_cast<std::size_t>(i) * 4 + static_cast<std::size_t>(c)];
            const int b = bytes[static_cast<std::size_t>(i) * 3 + static_cast<std::size_t>(c)];
            worst = std::max(worst, std::abs(a - b));
        }
    }
    g_free(mem);
    CHECK(worst <= 1);
    CHECK_FALSE(pfx::toSrgb8(pfx::Image()).ok());
}

TEST_CASE("the common formats are always writable")
{
    const auto exts = pfx::writableExtensions();
    for (const char* ext : {".jpg", ".png", ".tif", ".webp"}) {
        CHECK(std::find(exts.begin(), exts.end(), ext) != exts.end());
    }
}

TEST_CASE("readable extensions: the common photo formats, and no documents")
{
    const auto exts = pfx::readableExtensions();
    for (const char* ext : {".jpg", ".jpeg", ".png", ".tif", ".tiff", ".webp"}) {
        CAPTURE(ext);
        CHECK(std::find(exts.begin(), exts.end(), ext) != exts.end());
    }
    for (const char* ext : {".pdf", ".svg", ".csv", ".v", ".mat"}) {
        CAPTURE(ext);
        CHECK(std::find(exts.begin(), exts.end(), ext) == exts.end());
    }
    CHECK(std::is_sorted(exts.begin(), exts.end()));
    CHECK(std::adjacent_find(exts.begin(), exts.end()) == exts.end());
}

TEST_CASE("a header claiming a huge image is refused, not allocated")
{
    const fixtures::TempDir dir;
    const auto path = dir.path() / "huge.pgm";
    {
        std::ofstream out(path, std::ios::binary);
        out << "P5\n30000 30000\n255\n" << std::string(64, '\x80'); // 900 MP claimed, 64 bytes given
    }
    const auto loaded = pfx::load(path);
    REQUIRE_FALSE(loaded.ok());
    // Where libvips reads PPM (not every build does), the header gets as far as the size check.
    if (vips_foreign_find_load(path.string().c_str()) != nullptr) {
        CAPTURE(loaded.error().message);
        CHECK(loaded.error().message.find("too large") != std::string::npos);
    } else {
        vips_error_clear();
    }
}

namespace {

// A little-endian Exif block: IFD0 with Make and the GPS pointer, then IFD1 at 200, and a GPS
// directory with latitude "N" and a rational latitude stored out of line.
std::vector<std::uint8_t> exifWithGps()
{
    std::vector<std::uint8_t> b = {'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 42, 0, 8, 0, 0, 0};
    const auto put16 = [&b](std::size_t at, unsigned v) {
        b.resize(std::max(b.size(), 6 + at + 2));
        b[6 + at] = static_cast<std::uint8_t>(v & 0xFFU);
        b[6 + at + 1] = static_cast<std::uint8_t>(v >> 8U);
    };
    const auto put32 = [&put16](std::size_t at, unsigned v) {
        put16(at, v & 0xFFFFU);
        put16(at + 2, v >> 16U);
    };
    put16(8, 2); // IFD0: two entries
    put16(10, 0x010F);
    put16(12, 2);
    put32(14, 4);
    put32(18, 0x00434241); // Make "ABC"
    put16(22, 0x8825);
    put16(24, 4);
    put32(26, 1);
    put32(30, 100); // GPS IFD at 100
    put32(34, 200); // next IFD: IFD1 at 200
    put16(100, 2);  // GPS: two entries
    put16(102, 0x0001);
    put16(104, 2);
    put32(106, 2);
    put32(110, 'N'); // GPSLatitudeRef "N"
    put16(114, 0x0002);
    put16(116, 5);
    put32(118, 3);
    put32(122, 150); // GPSLatitude at 150
    for (unsigned i = 0; i < 6; ++i) {
        put32(150 + (4 * i), 51 + i); // the coordinates
    }
    put16(200, 0); // IFD1: empty
    put32(202, 0);
    return b;
}

} // namespace

TEST_CASE("location is removed from Exif: unlinked, zeroed, the rest kept")
{
    const std::vector<std::uint8_t> before = exifWithGps();
    const std::vector<std::uint8_t> after = pfx::exifWithoutLocation(before);
    REQUIRE(after.size() == before.size());
    CHECK(after[6 + 8] == 1);     // one entry left in IFD0
    CHECK(after[6 + 10] == 0x0F); // Make kept, first
    CHECK(after[6 + 22] == 200);  // the next-IFD pointer moved up a slot
    CHECK(after[6 + 102] == 0);   // GPS entries zeroed
    for (std::size_t i = 150; i < 174; ++i) {
        CHECK(after[6 + i] == 0); // the coordinates are gone from the bytes
    }
    // Not Exif, or damaged: returned as is.
    const std::vector<std::uint8_t> junk = {1, 2, 3};
    CHECK(pfx::exifWithoutLocation(junk) == junk);
    std::vector<std::uint8_t> cut = before;
    cut.resize(40);
    CHECK(pfx::exifWithoutLocation(cut).size() == 40);
}

TEST_CASE("the camera, the date and the exposure are read from Exif")
{
    // Big-endian this time: IFD0 with Make, Model and the Exif pointer; the Exif IFD with
    // DateTimeOriginal, ISO, FNumber and ExposureTime.
    std::vector<std::uint8_t> b = {'E', 'x', 'i', 'f', 0, 0, 'M', 'M', 0, 42, 0, 0, 0, 8};
    const auto put16 = [&b](std::size_t at, unsigned v) {
        b.resize(std::max(b.size(), 6 + at + 2));
        b[6 + at] = static_cast<std::uint8_t>(v >> 8U);
        b[6 + at + 1] = static_cast<std::uint8_t>(v & 0xFFU);
    };
    const auto put32 = [&put16](std::size_t at, unsigned v) {
        put16(at, v >> 16U);
        put16(at + 2, v & 0xFFFFU);
    };
    const auto text = [&b](std::size_t at, const std::string& s) {
        b.resize(std::max(b.size(), 6 + at + s.size() + 1));
        std::copy(s.begin(), s.end(), b.begin() + static_cast<std::ptrdiff_t>(6 + at));
    };
    const auto entry = [&](std::size_t at, unsigned tag, unsigned type, unsigned count, unsigned value) {
        put16(at, tag);
        put16(at + 2, type);
        put32(at + 4, count);
        put32(at + 8, value);
    };
    put16(8, 3);
    entry(10, 0x010F, 2, 6, 100); // Make "Nikon "
    entry(22, 0x0110, 2, 4, 0);   // Model "Z6" inline
    text(30, "Z6");
    entry(34, 0x8769, 4, 1, 120); // Exif IFD
    put32(46, 0);
    text(100, "Nikon ");
    put16(120, 4);
    entry(122, 0x9003, 2, 20, 200); // DateTimeOriginal
    entry(134, 0x8827, 3, 1, 0);    // ISO 400, a short in the first half of the field
    put16(142, 400);
    entry(146, 0x829D, 5, 1, 240); // f/2.8
    entry(158, 0x829A, 5, 1, 248); // 1/250 s
    put32(170, 0);
    text(200, "2026:11:05 07:30:00");
    put32(240, 28);
    put32(244, 10);
    put32(248, 1);
    put32(252, 250);
    const pfx::ExifSummary e = pfx::readExif(b);
    CHECK(e.make == "Nikon");
    CHECK(e.model == "Z6");
    CHECK(e.dateTaken == "2026:11:05 07:30:00");
    CHECK(e.iso == 400);
    CHECK(e.fNumber == doctest::Approx(2.8));
    CHECK(e.exposureTime == doctest::Approx(0.004));
    CHECK(e.lens.empty());
    // Damaged or not Exif: an empty summary, no crash.
    CHECK(pfx::readExif({1, 2, 3}).make.empty());
    std::vector<std::uint8_t> cut = b;
    cut.resize(60);
    CHECK(pfx::readExif(cut).dateTaken.empty());
    CHECK(pfx::readExif(exifWithGps()).make == "ABC");
}

TEST_CASE("location is removed from XMP, the rest kept")
{
    const std::string xmp =
        R"(<rdf:Description exif:GPSLatitude="51,30N" dc:creator="Me" exif:GPSLongitude='0,7W'>)"
        R"(<exif:GPSAltitude>12/1</exif:GPSAltitude><dc:rights>Mine</dc:rights></rdf:Description>)";
    pfx::Metadata m;
    m.xmp.assign(xmp.begin(), xmp.end());
    const pfx::Metadata out = pfx::withoutLocation(m);
    const std::string text(out.xmp.begin(), out.xmp.end());
    CHECK(text.find("GPS") == std::string::npos);
    CHECK(text.find("dc:creator=\"Me\"") != std::string::npos);
    CHECK(text.find("<dc:rights>Mine</dc:rights>") != std::string::npos);
}

TEST_CASE("a JPEG saved without location has no GPS block; with it, keeps it")
{
    const fixtures::TempDir dir;
    pfx::Metadata m;
    m.exif = exifWithGps();
    const pfx::Image img = fixtures::flat(0.4F, 0.4F, 0.4F, 16, 16);
    const auto hasGps = [](const std::vector<std::uint8_t>& exif) {
        for (std::size_t i = 0; i + 1 < exif.size(); ++i) {
            if ((exif[i] == 0x25 && exif[i + 1] == 0x88) || (exif[i] == 0x88 && exif[i + 1] == 0x25)) {
                return true;
            }
        }
        return false;
    };
    pfx::SaveOptions keep;
    REQUIRE(pfx::save(img, dir.path() / "keep.jpg", keep, &m).ok());
    pfx::SaveOptions drop;
    drop.keepLocation = false;
    REQUIRE(pfx::save(img, dir.path() / "drop.jpg", drop, &m).ok());
    const auto kept = pfx::load(dir.path() / "keep.jpg");
    const auto dropped = pfx::load(dir.path() / "drop.jpg");
    REQUIRE(kept.ok());
    REQUIRE(dropped.ok());
    CHECK(hasGps(kept.value().metadata.exif));
    CHECK_FALSE(hasGps(dropped.value().metadata.exif));
}

TEST_CASE("a file that is not a photo fails in plain words, without its path")
{
    const fixtures::TempDir dir;
    const auto path = dir.path() / "notes.jpg";
    {
        std::ofstream out(path, std::ios::binary);
        out << "these are words, not pixels";
    }
    const auto loaded = pfx::load(path);
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.error().message == "This file is not a photo PixelFx can read.");
    CHECK(loaded.error().message.find(path.string()) == std::string::npos);
}

TEST_CASE("engine versions: libvips and Little CMS, as dotted numbers")
{
    const auto engines = pfx::engineVersions();
    REQUIRE(engines.size() == 2);
    CHECK(engines[0].name == "libvips");
    CHECK(engines[1].name == "Little CMS");
    for (const auto& e : engines) {
        CAPTURE(e.name);
        CHECK(e.version.size() >= 3);
        CHECK(e.version.find('.') != std::string::npos);
        CHECK(e.version.find_first_not_of("0123456789.") == std::string::npos);
    }
}

namespace {

std::string profileDescription(const std::vector<std::uint8_t>& icc)
{
    cmsHPROFILE p = cmsOpenProfileFromMem(icc.data(), static_cast<cmsUInt32Number>(icc.size()));
    REQUIRE(p != nullptr);
    std::array<char, 256> text{};
    cmsGetProfileInfoASCII(p, cmsInfoDescription, "en", "US", text.data(), text.size());
    cmsCloseProfile(p);
    return text.data();
}

} // namespace

TEST_CASE("Display P3 and Adobe RGB are embedded and keep colours sRGB clips")
{
    const fixtures::TempDir dir;
    // Linear Rec.2020: a green past sRGB's reach, inside Adobe RGB's and Display P3's.
    const pfx::Image green = fixtures::flat(0.1F, 0.5F, 0.1F, 8, 8);
    const auto roundTrip = [&](pfx::OutputSpace space, const std::string& name) {
        pfx::SaveOptions options;
        options.space = space;
        const auto path = dir.path() / (name + ".png");
        REQUIRE(pfx::save(green, path, options).ok());
        auto loaded = pfx::load(path);
        REQUIRE(loaded.ok());
        return std::move(loaded).value();
    };
    const pfx::LoadedImage srgb = roundTrip(pfx::OutputSpace::Srgb, "srgb");
    const pfx::LoadedImage p3 = roundTrip(pfx::OutputSpace::DisplayP3, "p3");
    const pfx::LoadedImage adobe = roundTrip(pfx::OutputSpace::AdobeRgb, "adobe");
    CHECK(profileDescription(p3.profile) == "Display P3");
    CHECK(profileDescription(adobe.profile) == "Adobe RGB (1998) compatible");
    CHECK(fixtures::compare(adobe.image, green).max < 0.01);
    CHECK(fixtures::compare(p3.image, green).max < fixtures::compare(srgb.image, green).max);
    CHECK(fixtures::compare(adobe.image, green).max < fixtures::compare(srgb.image, green).max);

    // Same as original: the photo's profile goes back as it was stored; none gives sRGB.
    pfx::SaveOptions original;
    original.space = pfx::OutputSpace::Original;
    original.originalProfile = adobe.profile;
    REQUIRE(pfx::save(adobe.image, dir.path() / "again.png", original).ok());
    const auto again = pfx::load(dir.path() / "again.png");
    REQUIRE(again.ok());
    CHECK(again.value().profile == adobe.profile);
    original.originalProfile.clear();
    REQUIRE(pfx::save(adobe.image, dir.path() / "none.png", original).ok());
    const auto none = pfx::load(dir.path() / "none.png");
    REQUIRE(none.ok());
    CHECK(profileDescription(none.value().profile) ==
          profileDescription(srgb.profile)); // bytes carry the time
}

TEST_CASE("encode gives the bytes save writes")
{
    const fixtures::TempDir dir;
    const pfx::Image scene = fixtures::scene();
    for (const char* ext : {".jpg", ".png"}) {
        CAPTURE(ext);
        const auto path = dir.path() / (std::string("scene") + ext);
        REQUIRE(pfx::save(scene, path).ok());
        const auto bytes = pfx::encode(scene, ext);
        REQUIRE(bytes.ok());
        CHECK(bytes.value().size() == std::filesystem::file_size(path));
    }
    CHECK_FALSE(pfx::encode(pfx::Image(), ".jpg").ok());
}

TEST_CASE("8-bit sRGB comes back from the working space as it went")
{
    const pfx::Image scene = fixtures::scene();
    const auto eight = pfx::toSrgb8(scene);
    REQUIRE(eight.ok());
    const auto back = pfx::fromSrgb8(eight.value());
    REQUIRE(back.ok());
    const auto again = pfx::toSrgb8(back.value());
    REQUIRE(again.ok());
    CHECK(again.value().pixels == eight.value().pixels);
    CHECK_FALSE(pfx::fromSrgb8(pfx::Rgba8Image{}).ok());
}
