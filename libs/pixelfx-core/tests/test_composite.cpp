// SPDX-License-Identifier: Apache-2.0
#include "pfx/color.h"
#include "pfx/composite.h"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

TEST_CASE("an overlay blends in linear light, scaled by its opacity, clipped to the image")
{
    pfx::Image image(4, 2);
    image.fill(0.0F, 0.0F, 0.0F);
    // A 2 by 2 overlay of white: one opaque pixel, one at half alpha (premultiplied), two empty.
    const std::vector<std::uint8_t> pixels = {255, 255, 255, 255, 128, 128, 128, 128, //
                                              0,   0,   0,   0,   0,   0,   0,   0};
    pfx::blendOverlay(image, {pixels.data(), 2, 2, 8, 3, 0}); // placed at x 3: half of it is off
    CHECK(image.pixel(3, 0)[0] == doctest::Approx(1.0F).epsilon(1e-4));
    CHECK(image.pixel(2, 0)[0] == 0.0F); // left of the overlay: untouched
    CHECK(image.pixel(3, 1)[0] == 0.0F); // under an empty pixel: untouched

    pfx::Image grey(1, 1);
    grey.fill(0.0F, 0.0F, 0.0F);
    pfx::blendOverlay(grey, {pixels.data() + 4, 1, 1, 8, 0, 0}, 0.5F);  // white at alpha 0.5, half opacity
    CHECK(grey.pixel(0, 0)[1] == doctest::Approx(0.25F).epsilon(1e-3)); // linear: a quarter of white
    pfx::blendOverlay(grey, {pixels.data(), 1, 1, 8, 0, 0}, 0.0F);      // opacity 0: nothing
    CHECK(grey.pixel(0, 0)[1] == doctest::Approx(0.25F).epsilon(1e-3));
}

TEST_CASE("a pure sRGB colour lands on its Rec.2020 value")
{
    pfx::Image image(1, 1);
    image.fill(1.0F, 1.0F, 1.0F);
    const std::array<std::uint8_t, 4> red = {255, 0, 0, 255};
    pfx::blendOverlay(image, {red.data(), 1, 1, 4, 0, 0});
    for (std::size_t c = 0; c < 3; ++c) {
        CHECK(image.pixel(0, 0)[c] == doctest::Approx(pfx::color::kSrgbToRec2020.at(c)[0]).epsilon(1e-4));
    }
}

TEST_CASE("mean luminance of a rectangle is clipped to the image")
{
    pfx::Image image(4, 4);
    image.fill(0.0F, 0.0F, 0.0F);
    for (int y = 0; y < 4; ++y) {
        for (int x = 2; x < 4; ++x) {
            float* p = image.pixel(x, y);
            p[0] = p[1] = p[2] = 1.0F;
        }
    }
    CHECK(pfx::meanLuminance(image, 0, 0, 4, 4) == doctest::Approx(0.5));
    CHECK(pfx::meanLuminance(image, 2, 0, 10, 10) == doctest::Approx(1.0)); // the right half
    CHECK(pfx::meanLuminance(image, 10, 10, 2, 2) == 0.0);
}
