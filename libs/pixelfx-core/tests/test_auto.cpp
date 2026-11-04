// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/auto.h"
#include "pfx/pipeline.h"
#include "pfx/registry.h"

#include <array>
#include <doctest/doctest.h>

namespace {

pfx::Image scaled(const pfx::Image& image, float gain, float r = 1.0F, float g = 1.0F, float b = 1.0F)
{
    pfx::Image out = image;
    for (std::size_t i = 0; i < out.pixelCount(); ++i) {
        float* p = out.data() + i * 4;
        p[0] *= gain * r;
        p[1] *= gain * g;
        p[2] *= gain * b;
    }
    return out;
}

} // namespace

TEST_CASE("auto brightens a dark photo and darkens a bright one")
{
    const pfx::Image scene = fixtures::scene();
    CHECK(pfx::autoAdjust(scaled(scene, 0.15F)).number("light.exposure", 0) > 0.5);
    CHECK(pfx::autoAdjust(scaled(scene, 4.0F)).number("light.exposure", 0) < -0.5);
}

TEST_CASE("auto warms a photo with a blue cast, and the result is less blue")
{
    const pfx::Image blue = scaled(fixtures::scene(), 1.0F, 0.75F, 1.0F, 1.35F);
    const pfx::Settings s = pfx::autoAdjust(blue);
    CHECK(s.number("color.wb.temperature", 0) > 5);
    const pfx::Image fixed = pfx::render(blue, s).value();
    const auto ratio = [](const pfx::Image& img) {
        double r = 0;
        double b = 0;
        for (std::size_t i = 0; i < img.pixelCount(); ++i) {
            r += double{img.data()[i * 4]};
            b += double{img.data()[i * 4 + 2]};
        }
        return b / r;
    };
    CHECK(ratio(fixed) < ratio(blue));
}

TEST_CASE("auto stays inside the registry's ranges and leaves a neutral grey ramp alone")
{
    for (const float gain : {0.05F, 0.5F, 1.0F, 8.0F}) {
        pfx::Settings s = pfx::autoAdjust(scaled(fixtures::scene(), gain));
        CHECK(pfx::registry().normalize(s).empty()); // nothing out of range or unknown
    }
    const pfx::Settings ramp = pfx::autoAdjust(fixtures::ramp());
    CHECK_FALSE(ramp.contains("color.wb.temperature"));
    CHECK_FALSE(ramp.contains("color.wb.tint"));
    CHECK(std::abs(ramp.number("light.exposure", 0)) < 0.3);
    CHECK(pfx::autoAdjust(pfx::Image()).isEmpty());
}

TEST_CASE("white balance picker: the picked colour comes out grey")
{
    const std::array<std::array<float, 3>, 4> casts = {
        {{0.50F, 0.40F, 0.30F}, {0.30F, 0.40F, 0.55F}, {0.40F, 0.50F, 0.40F}, {0.45F, 0.38F, 0.45F}}};
    for (const auto& c : casts) {
        const pfx::Settings wb = pfx::neutralWhiteBalance(c[0], c[1], c[2]);
        CAPTURE(c[0]);
        CAPTURE(c[2]);
        const auto out = pfx::render(fixtures::flat(c[0], c[1], c[2], 2, 2), wb);
        REQUIRE(out.ok());
        const float* p = out.value().pixel(0, 0);
        const double mean = (double{p[0]} + double{p[1]} + double{p[2]}) / 3.0;
        for (int ch = 0; ch < 3; ++ch) {
            CHECK(double{p[ch]} == doctest::Approx(mean).epsilon(0.03));
        }
    }
    CHECK(pfx::neutralWhiteBalance(0.3, 0.3, 0.3).isEmpty()); // already grey: nothing to change
    CHECK(pfx::neutralWhiteBalance(0, 0, 0).isEmpty());
}
