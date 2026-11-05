// SPDX-License-Identifier: Apache-2.0
// Creative operations for looks (FEATURES E9), checked by their effect.
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/pipeline.h"

#include <doctest/doctest.h>

namespace {

pfx::Image run(const pfx::Image& image, std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto out = pfx::render(image, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

double luma(const pfx::Image& image, int x, int y)
{
    const float* p = image.pixel(x, y);
    return pfx::color::luminance(p[0], p[1], p[2]);
}

} // namespace

TEST_CASE("sepia and colorize tint the photo and keep its light")
{
    const pfx::Image grey = fixtures::flat(0.2F, 0.2F, 0.2F);
    const pfx::Image sepia = run(grey, {{"creative.sepia.amount", 100}});
    const float* s = sepia.pixel(1, 1);
    CHECK(s[0] > s[1]);
    CHECK(s[1] > s[2]);
    CHECK(luma(sepia, 1, 1) == doctest::Approx(0.2).epsilon(0.01));
    const pfx::Image half = run(grey, {{"creative.sepia.amount", 50}});
    CHECK(half.pixel(1, 1)[0] > 0.2F);
    CHECK(half.pixel(1, 1)[0] < s[0]);

    const pfx::Image blue = run(grey, {{"creative.colorize.amount", 100}, {"creative.colorize.hue", 220}});
    CHECK(blue.pixel(1, 1)[2] > blue.pixel(1, 1)[0]);
    CHECK(luma(blue, 1, 1) == doctest::Approx(0.2).epsilon(0.01));
    const pfx::Image none =
        run(grey, {{"creative.colorize.amount", 100}, {"creative.colorize.saturation", 0}});
    CHECK(none.pixel(1, 1)[0] == doctest::Approx(none.pixel(1, 1)[2]).epsilon(1e-4));
}

TEST_CASE("threshold makes black and white; invert turns black to white and keeps mid grey")
{
    const pfx::Image ramp = fixtures::ramp(256, 4);
    const pfx::Image bw = run(ramp, {{"creative.threshold.amount", 100}, {"creative.threshold.level", 50}});
    CHECK(bw.pixel(10, 1)[1] == doctest::Approx(0.0));
    CHECK(bw.pixel(250, 1)[1] == doctest::Approx(1.0));

    const pfx::Image black = run(fixtures::flat(0, 0, 0), {{"creative.invert.amount", 100}});
    CHECK(black.pixel(1, 1)[1] == doctest::Approx(1.0).epsilon(1e-3));
    const double middle = pfx::color::perceptualDecode(0.5);
    const auto m = static_cast<float>(middle);
    const pfx::Image grey = run(fixtures::flat(m, m, m), {{"creative.invert.amount", 100}});
    CHECK(grey.pixel(1, 1)[1] == doctest::Approx(middle).epsilon(1e-3));
}

TEST_CASE("blur smooths, pixelate makes cells of one colour")
{
    const pfx::Image scene = fixtures::scene();
    CHECK(fixtures::lumaStdDev(run(scene, {{"creative.blur.radius", 0.008}})) < fixtures::lumaStdDev(scene));
    // Cells of about 2 pixels (0.008 of a 256 pixel long edge), from the top left corner.
    const pfx::Image cells = run(scene, {{"creative.pixelate.size", 0.008}});
    CHECK(cells.pixel(0, 0)[0] == cells.pixel(1, 1)[0]);
    CHECK(cells.pixel(0, 0)[0] != scene.pixel(0, 0)[0]);
    CHECK(fixtures::meanLuma(cells) == doctest::Approx(fixtures::meanLuma(scene)).epsilon(0.01));
}

TEST_CASE("emboss turns flat areas mid grey; edges turn them black and outline the rest")
{
    const pfx::Image flat = fixtures::flat(0.6F, 0.3F, 0.1F, 64, 64);
    const pfx::Image relief = run(flat, {{"creative.emboss.amount", 100}});
    CHECK(relief.pixel(30, 30)[1] == doctest::Approx(pfx::color::perceptualDecode(0.5)).epsilon(1e-3));
    CHECK(run(flat, {{"creative.edges.amount", 100}}).pixel(30, 30)[1] == doctest::Approx(0.0).epsilon(1e-3));
    pfx::Image split = fixtures::flat(0.05F, 0.05F, 0.05F, 64, 64);
    for (int y = 0; y < 64; ++y) {
        for (int x = 32; x < 64; ++x) {
            float* p = split.pixel(x, y);
            p[0] = p[1] = p[2] = 0.8F;
        }
    }
    const pfx::Image outline = run(split, {{"creative.edges.amount", 100}});
    CHECK(outline.pixel(32, 30)[1] > 0.3F);
    CHECK(outline.pixel(10, 30)[1] < 0.01F);
}
