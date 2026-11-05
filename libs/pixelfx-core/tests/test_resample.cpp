// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/resample.h"

#include <doctest/doctest.h>

TEST_CASE("fitting to a long edge keeps the shape and the average brightness, and never enlarges")
{
    const pfx::Image scene = fixtures::scene(400, 300);
    const auto small = pfx::fitLongEdge(scene, 200);
    REQUIRE(small.ok());
    CHECK(small.value().width() == 200);
    CHECK(small.value().height() == 150);
    CHECK(fixtures::meanLuma(small.value()) == doctest::Approx(fixtures::meanLuma(scene)).epsilon(0.02));
    const auto same = pfx::fitLongEdge(scene, 1000);
    REQUIRE(same.ok());
    CHECK(same.value() == scene);
    CHECK_FALSE(pfx::fitLongEdge(scene, 0).ok());
}

TEST_CASE("size presets: fill keeps the middle, fit adds a border, both exact")
{
    pfx::Image wide = fixtures::flat(0.5F, 0.5F, 0.5F, 300, 200); // 3:2
    for (int x = 0; x < 300; ++x) {
        for (int y = 0; y < 200; ++y) {
            wide.pixel(x, y)[0] = x < 30 ? 1.0F : 0.5F; // a red band on the left edge
        }
    }
    const auto filled = pfx::fillSize(wide, 100, 125); // Instagram portrait shape, 4:5
    REQUIRE(filled.ok());
    CHECK(filled.value().width() == 100);
    CHECK(filled.value().height() == 125);
    CHECK(filled.value().pixel(0, 60)[0] ==
          doctest::Approx(0.5).epsilon(0.05)); // the edge band was cropped away
    // The frame dragged to the left edge keeps the band.
    const auto left = pfx::fillSize(wide, 100, 125, 0.0F, 0.5F);
    REQUIRE(left.ok());
    CHECK(left.value().pixel(2, 60)[0] == doctest::Approx(1.0).epsilon(0.05));
    const auto fitted = pfx::fitInside(wide, 100, 125, 1.0F);
    REQUIRE(fitted.ok());
    CHECK(fitted.value().width() == 100);
    CHECK(fitted.value().height() == 125);
    CHECK(fitted.value().pixel(50, 2)[1] == doctest::Approx(1.0)); // white border above
    CHECK(fitted.value().pixel(50, 62)[1] == doctest::Approx(0.5).epsilon(0.05));
    CHECK_FALSE(pfx::fillSize(wide, 0, 10).ok());
}

TEST_CASE("output sharpening lifts an edge, Print more than Screen, flat areas unchanged")
{
    pfx::Image edge = fixtures::flat(0.1F, 0.1F, 0.1F, 40, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 20; x < 40; ++x) {
            float* p = edge.pixel(x, y);
            p[0] = p[1] = p[2] = 0.4F;
        }
    }
    const auto sharpened = [&edge](pfx::OutputSharpen kind) {
        pfx::Image copy = edge;
        pfx::sharpenForOutput(copy, kind);
        return copy;
    };
    const pfx::Image none = sharpened(pfx::OutputSharpen::None);
    const pfx::Image screen = sharpened(pfx::OutputSharpen::Screen);
    const pfx::Image print = sharpened(pfx::OutputSharpen::Print);
    CHECK(none == edge);
    // The bright side of the edge gets brighter, the dark side darker.
    CHECK(screen.pixel(20, 4)[1] > 0.4F);
    CHECK(screen.pixel(19, 4)[1] < 0.1F);
    CHECK(print.pixel(20, 4)[1] > screen.pixel(20, 4)[1]);
    // Far from the edge nothing moves.
    CHECK(screen.pixel(5, 4)[1] == doctest::Approx(0.1).epsilon(1e-3));
    CHECK(print.pixel(35, 4)[1] == doctest::Approx(0.4).epsilon(1e-3));
}
