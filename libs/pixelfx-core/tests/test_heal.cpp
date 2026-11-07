// SPDX-License-Identifier: Apache-2.0
#include "pfx/document.h"
#include "pfx/heal.h"
#include "pfx/pipeline.h"
#include "pfx/registry.h"

#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <utility>

namespace {

// A horizontal ramp from 0.2 to 0.6, with a dark disc of `r` pixels at (cx, cy).
pfx::Image rampWithBlemish(int w, int h, int cx, int cy, int r)
{
    pfx::Image img(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float v = 0.2F + (0.4F * static_cast<float>(x) / static_cast<float>(w - 1));
            const bool blemish = ((x - cx) * (x - cx)) + ((y - cy) * (y - cy)) <= r * r;
            float* p = img.pixel(x, y);
            p[0] = blemish ? 0.02F : v;
            p[1] = blemish ? 0.02F : v * 0.9F;
            p[2] = blemish ? 0.02F : v * 0.8F;
            p[3] = 1.0F;
        }
    }
    return img;
}

// The largest difference from the clean ramp inside a disc.
float worstInside(const pfx::Image& img, int cx, int cy, int r)
{
    float worst = 0;
    const int w = img.width();
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (((x - cx) * (x - cx)) + ((y - cy) * (y - cy)) > r * r) {
                continue;
            }
            const float v = 0.2F + (0.4F * static_cast<float>(x) / static_cast<float>(w - 1));
            worst = std::max(worst, std::abs(img.pixel(x, y)[0] - v));
        }
    }
    return worst;
}

pfx::Spot spotAt(const pfx::Image& img, double x, double y, double radiusPx)
{
    pfx::Spot s;
    s.x = x / img.width();
    s.y = y / img.height();
    s.sourceX = s.x;
    s.sourceY = s.y;
    s.radius = radiusPx / img.longEdge();
    s.feather = 30;
    return s;
}

} // namespace

TEST_CASE("heal: a blemish on a gradient is covered and the tone follows the gradient")
{
    pfx::Image img = rampWithBlemish(400, 200, 200, 100, 10);
    REQUIRE(worstInside(img, 200, 100, 9) > 0.3F);
    pfx::Spot s = spotAt(img, 200, 100, 16);
    // The source 60 px to the right is brighter on the ramp; the fitted plane brings it down.
    s.sourceX = 260.0 / 400.0;
    pfx::healSpot(img, s);
    CHECK(worstInside(img, 200, 100, 9) < 0.01F);
    // Outside the spot nothing changes.
    CHECK(img.pixel(150, 100)[0] == doctest::Approx(0.2F + (0.4F * 150.0F / 399.0F)));
}

TEST_CASE("heal: opacity 0 changes nothing, and a spot with no room is left alone")
{
    pfx::Image img = rampWithBlemish(100, 100, 50, 50, 5);
    const pfx::Image before = img;
    pfx::Spot s = spotAt(img, 50, 50, 8);
    s.sourceX = 0.8;
    s.opacity = 0;
    pfx::healSpot(img, s);
    CHECK(img == before);
}

TEST_CASE("heal: the source picked avoids the image's edge and another blemish")
{
    // A second blemish right of the first, where the nearest candidates are.
    pfx::Image img = rampWithBlemish(300, 300, 150, 150, 8);
    for (int y = 130; y <= 170; ++y) {
        for (int x = 175; x <= 215; ++x) {
            float* p = img.pixel(x, y);
            p[0] = p[1] = p[2] = 0.9F;
        }
    }
    const pfx::Spot found = pfx::findHealSource(img, spotAt(img, 150, 150, 12));
    const double sx = found.sourceX * 300;
    const double sy = found.sourceY * 300;
    CHECK(std::hypot(sx - 150, sy - 150) > 24);                  // clear of the spot
    CHECK_FALSE((sx > 160 && sx < 230 && sy > 120 && sy < 180)); // not the bright patch
    // Healed from there, the spot matches its surroundings.
    pfx::healSpot(img, found);
    CHECK(worstInside(img, 150, 150, 6) < 0.05F);

    // In a corner, the source stays inside the photo.
    const pfx::Spot corner = pfx::findHealSource(img, spotAt(img, 10, 10, 8));
    CHECK(corner.sourceX * 300 > 8);
    CHECK(corner.sourceY * 300 > 8);
}

TEST_CASE("heal: a brush stroke covers a line along its length")
{
    pfx::Image img(300, 200);
    img.fill(0.4F, 0.4F, 0.4F);
    for (int x = 50; x <= 250; ++x) {
        for (int y = 99; y <= 101; ++y) {
            float* p = img.pixel(x, y);
            p[0] = p[1] = p[2] = 0.0F;
        }
    }
    pfx::Spot s = spotAt(img, 50, 100, 6);
    s.stroke = {{50.0 / 300, 0.5}, {150.0 / 300, 0.5}, {250.0 / 300, 0.5}};
    const pfx::Spot found = pfx::findHealSource(img, s);
    CHECK(std::abs(found.sourceY - 0.5) * 200 > 6); // moved off the line, not along it
    pfx::healSpot(img, found);
    for (int x = 60; x <= 240; x += 20) {
        CAPTURE(x);
        CHECK(img.pixel(x, 100)[0] == doctest::Approx(0.4F).epsilon(0.02));
    }
}

TEST_CASE("heal: render heals first, on the upright photo, so a crop never moves a spot")
{
    const pfx::Image img = rampWithBlemish(400, 200, 300, 100, 8);
    pfx::Spot s = spotAt(img, 300, 100, 14);
    s.sourceX = 340.0 / 400.0;
    pfx::Settings settings;
    settings.set("heal.spots", pfx::Spots{s});
    // Crop to the right half: the blemish is at (100, 100) of the output.
    settings.set("geometry.crop.x", 0.5);
    settings.set("geometry.crop.w", 0.5);
    const auto out = pfx::render(img, settings);
    REQUIRE(out.ok());
    REQUIRE(out.value().width() == 200);
    const float expected = 0.2F + (0.4F * 300.0F / 399.0F);
    CHECK(out.value().pixel(100, 100)[0] == doctest::Approx(expected).epsilon(0.03));
}

TEST_CASE("heal: spots read and write in a recipe (version 2 and later), and bad ones are fixed or dropped")
{
    pfx::Spot a;
    a.x = 0.25;
    a.y = 0.5;
    a.sourceX = 0.3;
    a.sourceY = 0.55;
    a.radius = 0.02;
    a.feather = 40;
    a.opacity = 90;
    pfx::Spot b = a;
    b.stroke = {{0.25, 0.5}, {0.4, 0.6}};
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Recipe;
    doc.settings.set("heal.spots", pfx::Spots{a, b});
    doc.settings.set("light.exposure", 0.5);
    const std::string text = pfx::serializeDocument(doc);
    CHECK(text.find("\"version\": 6") != std::string::npos);
    CHECK(text.find("\"source\"") != std::string::npos);
    const auto again = pfx::parseDocument(text);
    REQUIRE(again.ok());
    CHECK(again.value().settings.spots("heal.spots") == pfx::Spots{a, b});

    // A version 1 recipe reads as before; version 7 is newer.
    CHECK(pfx::parseDocument(R"({"format":"pixelfx","kind":"recipe","version":1,"body":{"settings":{}}})")
              .ok());
    CHECK_FALSE(pfx::parseDocument(R"({"format":"pixelfx","kind":"recipe","version":7})").ok());

    // Out of range values are clamped; a broken list is dropped with a warning.
    pfx::ParseReport report;
    const auto odd = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"recipe","version":2,"body":{"settings":{"heal":{"spots":[
            {"x": 1.5, "y": -1, "radius": 9, "feather": 300, "stroke": [[0.1, 0.1]]}]}}}})",
        &report);
    REQUIRE(odd.ok());
    const pfx::Spots spots = odd.value().settings.spots("heal.spots");
    REQUIRE(spots.size() == 1);
    CHECK(spots[0].x == 1.0);
    CHECK(spots[0].y == 0.0);
    CHECK(spots[0].radius == 0.25);
    CHECK(spots[0].feather == 100.0);
    CHECK(spots[0].stroke.empty()); // one point is a spot
    CHECK_FALSE(report.warnings.empty());
    const auto broken = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"recipe","version":2,"body":{"settings":{"heal":{"spots":[1, 2]}}}})");
    REQUIRE(broken.ok());
    CHECK_FALSE(broken.value().settings.contains("heal.spots"));
}

TEST_CASE("heal: odd spots on small and thin images never crash and leave no NaN")
{
    std::uint32_t seed = 12345;
    const auto next = [&seed] {
        seed = (seed * 1664525U) + 1013904223U;
        return static_cast<double>(seed >> 8) / static_cast<double>(1U << 24);
    };
    for (const auto& [w, h] : {std::pair{1, 1}, std::pair{2, 9}, std::pair{40, 3}, std::pair{64, 64}}) {
        for (int round = 0; round < 50; ++round) {
            pfx::Image img(w, h);
            img.fill(0.5F, 0.4F, 0.3F);
            pfx::Spot s;
            s.x = next();
            s.y = next();
            s.sourceX = next();
            s.sourceY = next();
            s.radius = 0.001 + (next() * 0.249);
            s.feather = next() * 100;
            s.opacity = next() * 100;
            if (round % 3 == 0) {
                s.stroke = {{s.x, s.y}, {next(), next()}, {next(), next()}};
            }
            const pfx::Spot found = pfx::findHealSource(img, s);
            CHECK(found.sourceX >= 0.0);
            CHECK(found.sourceX <= 1.0);
            pfx::healSpot(img, found);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    REQUIRE(std::isfinite(img.pixel(x, y)[0]));
                }
            }
        }
    }
}
