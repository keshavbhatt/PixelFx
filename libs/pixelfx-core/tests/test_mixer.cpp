// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <doctest/doctest.h>

namespace {

// One pixel given in the perceptual encoding (what the mixer's HSV works on).
pfx::Image swatch(double r, double g, double b)
{
    return fixtures::flat(static_cast<float>(pfx::color::perceptualDecode(r)),
                          static_cast<float>(pfx::color::perceptualDecode(g)),
                          static_cast<float>(pfx::color::perceptualDecode(b)), 2, 2);
}

std::array<double, 3> encoded(const pfx::Image& img)
{
    const float* p = img.pixel(0, 0);
    return {pfx::color::perceptualEncode(double{p[0]}), pfx::color::perceptualEncode(double{p[1]}),
            pfx::color::perceptualEncode(double{p[2]})};
}

double hueOf(const std::array<double, 3>& e)
{
    const double v = std::max({e[0], e[1], e[2]});
    const double c = v - std::min({e[0], e[1], e[2]});
    if (v == e[0]) {
        return 60.0 * std::fmod(((e[1] - e[2]) / c) + 6.0, 6.0);
    }
    if (v == e[1]) {
        return 60.0 * (((e[2] - e[0]) / c) + 2.0);
    }
    return 60.0 * (((e[0] - e[1]) / c) + 4.0);
}

double satOf(const std::array<double, 3>& e)
{
    const double v = std::max({e[0], e[1], e[2]});
    return (v - std::min({e[0], e[1], e[2]})) / v;
}

pfx::Image mix(const pfx::Image& img, const char* key, double value)
{
    pfx::Settings s;
    s.set(key, value);
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

} // namespace

TEST_CASE("mixer: grey never moves")
{
    const pfx::Image grey = swatch(0.5, 0.5, 0.5);
    for (const char* key : {"mixer.hue.red", "mixer.saturation.yellow", "mixer.luminance.blue"}) {
        CHECK(mix(grey, key, 100) == grey);
    }
}

TEST_CASE("mixer: a band's hue shift moves its colours and not others")
{
    const pfx::Image yellow = swatch(0.8, 0.8, 0.1); // hue 60
    const double shifted = hueOf(encoded(mix(yellow, "mixer.hue.yellow", -100)));
    CHECK(shifted == doctest::Approx(30.0).epsilon(0.02)); // toward orange
    const pfx::Image blue = swatch(0.1, 0.1, 0.8);         // hue 240
    CHECK(hueOf(encoded(mix(blue, "mixer.hue.yellow", -100))) == doctest::Approx(240.0).epsilon(0.001));
}

TEST_CASE("mixer: saturation and luminance per band")
{
    const pfx::Image green = swatch(0.2, 0.7, 0.2);
    CHECK(satOf(encoded(mix(green, "mixer.saturation.green", -100))) < 0.01);
    const pfx::Image blue = swatch(0.1, 0.2, 0.6);
    CHECK(encoded(mix(blue, "mixer.luminance.blue", 100))[2] > encoded(blue)[2] * 1.3);
    const pfx::Image red = swatch(0.7, 0.1, 0.1);
    CHECK(encoded(mix(red, "mixer.luminance.blue", 100))[0] == doctest::Approx(encoded(red)[0]));
}

TEST_CASE("mixer: hues between two bands take some of each")
{
    const pfx::Image between = swatch(0.8, 0.6, 0.1); // hue about 46, between orange and yellow
    const double h = hueOf(encoded(between));
    const double shifted = hueOf(encoded(mix(between, "mixer.hue.yellow", 100)));
    CHECK(shifted > h);
    CHECK(shifted < h + 30.0);
}
