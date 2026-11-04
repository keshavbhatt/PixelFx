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
