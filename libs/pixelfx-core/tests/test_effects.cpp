// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/pipeline.h"

#include <cmath>
#include <doctest/doctest.h>

namespace {

pfx::Image render(const pfx::Image& img, std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

double green(const pfx::Image& img, int x, int y)
{
    return double{img.pixel(x, y)[1]};
}

} // namespace

TEST_CASE("vignette: darkens or lightens the corners, leaves the centre")
{
    const pfx::Image grey = fixtures::flat(0.2F, 0.2F, 0.2F, 300, 200);
    const pfx::Image dark = render(grey, {{"effects.vignette.amount", -60}});
    CHECK(green(dark, 150, 100) == doctest::Approx(0.2).epsilon(0.001));
    CHECK(green(dark, 0, 0) < 0.15);
    CHECK(green(dark, 0, 0) < green(dark, 0, 100)); // corners darker than edge middles
    const pfx::Image light = render(grey, {{"effects.vignette.amount", 60}});
    CHECK(green(light, 0, 0) > 0.4);
    // A higher midpoint keeps more of the frame untouched.
    const pfx::Image late =
        render(grey, {{"effects.vignette.amount", -60}, {"effects.vignette.midpoint", 100}});
    CHECK(green(late, 30, 100) > green(dark, 30, 100));
}

TEST_CASE("vignette: roundness bends the shape toward a circle")
{
    const pfx::Image grey = fixtures::flat(0.2F, 0.2F, 0.2F, 400, 100);
    const pfx::Image oval = render(grey, {{"effects.vignette.amount", -80}});
    const pfx::Image round =
        render(grey, {{"effects.vignette.amount", -80}, {"effects.vignette.roundness", 100}});
    // A wide frame: a circle reaches into the sides sooner than the frame-shaped ellipse.
    CHECK(green(round, 30, 50) < green(oval, 30, 50));
}

TEST_CASE("grain: keeps the brightness, adds texture, the same every time")
{
    const pfx::Image grey = fixtures::flat(0.18F, 0.18F, 0.18F, 400, 300);
    const pfx::Image grainy = render(grey, {{"effects.grain.amount", 60}, {"effects.grain.size", 60}});
    CHECK(fixtures::meanLuma(grainy) == doctest::Approx(fixtures::meanLuma(grey)).epsilon(0.03));
    CHECK(fixtures::lumaStdDev(grainy) > 0.01);
    CHECK(render(grey, {{"effects.grain.amount", 60}, {"effects.grain.size", 60}}) == grainy);
    // Bigger grains vary less from one pixel to the next, for the texture they add (on a strip
    // wide enough for both sizes to be over a pixel).
    const pfx::Image strip = fixtures::flat(0.18F, 0.18F, 0.18F, 1600, 40);
    const pfx::Image fine = render(strip, {{"effects.grain.amount", 60}, {"effects.grain.size", 0}});
    const pfx::Image coarse = render(strip, {{"effects.grain.amount", 60}, {"effects.grain.size", 100}});
    double fineStep = 0;
    double coarseStep = 0;
    for (int x = 1; x < 1600; ++x) {
        fineStep += std::abs(green(fine, x, 20) - green(fine, x - 1, 20));
        coarseStep += std::abs(green(coarse, x, 20) - green(coarse, x - 1, 20));
    }
    CHECK(coarseStep / fixtures::lumaStdDev(coarse) < fineStep / fixtures::lumaStdDev(fine));
}

TEST_CASE("grain: a half-size render has about the same strength once grains are a pixel or more")
{
    const pfx::Image grey = fixtures::flat(0.18F, 0.18F, 0.18F, 1600, 1200);
    const pfx::Image half = fixtures::flat(0.18F, 0.18F, 0.18F, 800, 600);
    const double full =
        fixtures::lumaStdDev(render(grey, {{"effects.grain.amount", 50}, {"effects.grain.size", 50}}));
    const double small =
        fixtures::lumaStdDev(render(half, {{"effects.grain.amount", 50}, {"effects.grain.size", 50}}));
    CHECK(small == doctest::Approx(full).epsilon(0.25));
}

TEST_CASE("fade: lifts the blacks, softens the whites, keeps grey neutral")
{
    const pfx::Image black = render(fixtures::flat(0, 0, 0), {{"effects.fade", 100}});
    CHECK(green(black, 0, 0) > 0.01);
    const pfx::Image white = render(fixtures::flat(1, 1, 1), {{"effects.fade", 100}});
    CHECK(green(white, 0, 0) < 0.95);
    const pfx::Image grey = render(fixtures::flat(0.3F, 0.3F, 0.3F), {{"effects.fade", 50}});
    CHECK(double{grey.pixel(0, 0)[0]} == doctest::Approx(double{grey.pixel(0, 0)[2]}));
}
