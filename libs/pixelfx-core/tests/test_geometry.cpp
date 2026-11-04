// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/geometry.h"
#include "pfx/pipeline.h"

#include <cmath>
#include <doctest/doctest.h>

namespace {

// A 40 x 20 image whose red channel is x and green channel is y, so positions can be read back.
pfx::Image coordinates()
{
    pfx::Image img(40, 20);
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 40; ++x) {
            float* p = img.pixel(x, y);
            p[0] = static_cast<float>(x) / 40.0F;
            p[1] = static_cast<float>(y) / 20.0F;
            p[2] = 0.5F;
            p[3] = 1.0F;
        }
    }
    return img;
}

pfx::Image run(const pfx::Image& img, std::initializer_list<std::pair<const char*, pfx::Value>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    CHECK(pfx::outputSize(s, {img.width(), img.height()}) ==
          pfx::Size{out.value().width(), out.value().height()});
    return std::move(out).value();
}

} // namespace

TEST_CASE("a quarter turn is clockwise and swaps the sides")
{
    const pfx::Image out = run(coordinates(), {{"geometry.rotate90", 1.0}});
    CHECK(out.width() == 20);
    CHECK(out.height() == 40);
    // The original's top-left corner ends at the top-right.
    CHECK(out.pixel(19, 0)[0] == doctest::Approx(0.0F));
    CHECK(out.pixel(19, 0)[1] == doctest::Approx(0.0F));
}

TEST_CASE("flips mirror the image")
{
    const pfx::Image h = run(coordinates(), {{"geometry.flipH", true}});
    CHECK(h.pixel(0, 0)[0] == doctest::Approx(39.0F / 40.0F));
    const pfx::Image v = run(coordinates(), {{"geometry.flipV", true}});
    CHECK(v.pixel(0, 0)[1] == doctest::Approx(19.0F / 20.0F));
}

TEST_CASE("the crop is a share of the image and keeps the right part")
{
    const pfx::Image out = run(coordinates(), {{"geometry.crop.x", 0.5},
                                               {"geometry.crop.y", 0.25},
                                               {"geometry.crop.w", 0.25},
                                               {"geometry.crop.h", 0.5}});
    CHECK(out.width() == 10);
    CHECK(out.height() == 10);
    CHECK(out.pixel(0, 0)[0] == doctest::Approx(20.0F / 40.0F));
    CHECK(out.pixel(0, 0)[1] == doctest::Approx(5.0F / 20.0F));
}

TEST_CASE("straightening trims to the largest rectangle without empty corners")
{
    const pfx::Image scene = fixtures::scene(300, 200);
    const pfx::Image out = run(scene, {{"geometry.angle", 7.0}});
    const double s = pfx::straightenScale(7.0, {300, 200});
    CHECK(s < 1.0);
    CHECK(out.width() == static_cast<int>(std::floor(300 * s)) - 2);
    for (const auto& [x, y] : {std::pair{0, 0},
                               {out.width() - 1, 0},
                               {0, out.height() - 1},
                               {out.width() - 1, out.height() - 1}}) {
        CHECK(out.pixel(x, y)[3] > 0.99F); // nothing transparent in the corners
    }
    CHECK(pfx::straightenScale(0.0, {300, 200}) == 1.0);
    CHECK(pfx::straightenScale(-7.0, {300, 200}) == doctest::Approx(s));
}

TEST_CASE("everything together, and the size is known before rendering")
{
    const pfx::Image scene = fixtures::scene(320, 240);
    for (const double turns : {0.0, 1.0, 2.0, 3.0}) {
        for (const double angle : {0.0, -12.5, 30.0}) {
            run(scene, {{"geometry.rotate90", turns},
                        {"geometry.flipH", true},
                        {"geometry.angle", angle},
                        {"geometry.crop.x", 0.1},
                        {"geometry.crop.w", 0.7},
                        {"geometry.crop.h", 0.9}});
        }
    }
}
