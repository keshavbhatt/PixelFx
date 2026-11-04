// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/pipeline.h"

#include <doctest/doctest.h>

namespace {

pfx::Image grade(const pfx::Image& img, std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

pfx::Image grey(double encoded)
{
    const auto v = static_cast<float>(pfx::color::perceptualDecode(encoded));
    return fixtures::flat(v, v, v, 2, 2);
}

} // namespace

TEST_CASE("grading: shadows take the shadow colour, highlights the highlight colour")
{
    // Teal shadows (hue 180), orange highlights (hue 30).
    const std::initializer_list<std::pair<const char*, double>> look = {{"grading.shadows.hue", 180},
                                                                        {"grading.shadows.amount", 60},
                                                                        {"grading.highlights.hue", 30},
                                                                        {"grading.highlights.amount", 60}};
    const pfx::Image dark = grade(grey(0.2), look);
    CHECK(dark.pixel(0, 0)[2] > dark.pixel(0, 0)[0]); // bluer than red
    const pfx::Image light = grade(grey(0.8), look);
    CHECK(light.pixel(0, 0)[0] > light.pixel(0, 0)[2]); // redder than blue
}

TEST_CASE("grading: lightness stays, black and white stay neutral")
{
    const pfx::Image mid = grey(0.5);
    const pfx::Image toned = grade(mid, {{"grading.shadows.hue", 220}, {"grading.shadows.amount", 80}});
    CHECK(fixtures::meanLuma(toned) == doctest::Approx(fixtures::meanLuma(mid)).epsilon(0.03));
    const pfx::Image black =
        grade(grey(0.0), {{"grading.shadows.hue", 220}, {"grading.shadows.amount", 100}});
    CHECK(black.pixel(0, 0)[0] == doctest::Approx(black.pixel(0, 0)[2]));
}

TEST_CASE("grading: balance moves the split")
{
    const std::initializer_list<std::pair<const char*, double>> base = {{"grading.highlights.hue", 30},
                                                                        {"grading.highlights.amount", 80}};
    const pfx::Image even = grade(grey(0.45), base);
    const pfx::Image toHighlights =
        grade(grey(0.45),
              {{"grading.highlights.hue", 30}, {"grading.highlights.amount", 80}, {"grading.balance", 100}});
    const auto warmth = [](const pfx::Image& img) {
        return double{img.pixel(0, 0)[0]} - double{img.pixel(0, 0)[2]};
    };
    CHECK(warmth(toHighlights) > warmth(even));
}
