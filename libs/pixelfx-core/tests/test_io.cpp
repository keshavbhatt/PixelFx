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
    for (const char* ext : {".jpg", ".jpeg", ".png", ".tif", ".tiff", ".webp", ".gif"}) {
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
    CHECK(loaded.error().message.find("too large") != std::string::npos);
    // Downscaled on load it fits the limit and is read (or fails on the missing data, but sanely).
    (void)pfx::load(path, {512});
}
