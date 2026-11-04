// SPDX-License-Identifier: Apache-2.0
#include "pfx/color.h"

#include <doctest/doctest.h>

using namespace pfx::color;

TEST_CASE("sRGB transfer round trips, including headroom above white")
{
    for (const double v : {0.0, 0.001, 0.0031308, 0.18, 0.5, 1.0, 2.5}) {
        CHECK(srgbDecode(srgbEncode(v)) == doctest::Approx(v).epsilon(1e-9));
    }
    CHECK(srgbEncode(0.18) == doctest::Approx(0.4613561).epsilon(1e-6));
}

TEST_CASE("perceptual encoding round trips and puts middle grey near 0.46")
{
    for (const double v : {0.0, 0.01, 0.18, 1.0, 4.0}) {
        CHECK(perceptualDecode(perceptualEncode(v)) == doctest::Approx(v).epsilon(1e-9));
    }
    CHECK(perceptualEncode(0.18) == doctest::Approx(0.4587).epsilon(1e-3));
}

TEST_CASE("matrices invert and white maps to white")
{
    const Mat3 id = multiply(kSrgbToRec2020, kRec2020ToSrgb);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            CHECK(id[i][j] == doctest::Approx(i == j ? 1.0 : 0.0).epsilon(1e-9));
        }
    }
    const Vec3 white = transform(kSrgbToRec2020, {1.0, 1.0, 1.0});
    for (const double c : white) {
        CHECK(c == doctest::Approx(1.0).epsilon(1e-6));
    }
    CHECK(luminance(1.0, 1.0, 1.0) == doctest::Approx(1.0));
}

TEST_CASE("Planckian locus is close to D65 at 6504 K and Bradford of a white onto itself is identity")
{
    const auto xy = planckianXy(6504.0);
    CHECK(xy[0] == doctest::Approx(0.3135).epsilon(2e-3));
    CHECK(xy[1] == doctest::Approx(0.3237).epsilon(2e-3));
    const Mat3 m = bradford({0.3127, 0.3290}, {0.3127, 0.3290});
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            CHECK(m[i][j] == doctest::Approx(i == j ? 1.0 : 0.0).epsilon(1e-9));
        }
    }
}
