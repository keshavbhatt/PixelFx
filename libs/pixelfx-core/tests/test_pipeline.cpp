// SPDX-License-Identifier: Apache-2.0
// Golden images (exact output, per operation) and scale tests (a half-size render matches the
// full-size render downscaled, so previews match exports: ADR-004). Regenerate goldens only on
// purpose: PFX_GOLDEN_UPDATE=1 ctest, then review the image diff like any other change.
#include "fixtures.h"
#include "pfx/io.h"
#include "pfx/pipeline.h"

#include <cmath>
#include <cstdlib>
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {

struct Case
{
    const char* name;
    pfx::Settings settings;
    double scaleMean; // tolerance of the scale test, sRGB-encoded units
};

pfx::Settings with(std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    return s;
}

std::vector<Case> cases()
{
    pfx::Settings curve;
    curve.set("curves.rgb", pfx::Curve{{0, 0.03}, {0.3, 0.22}, {0.7, 0.8}, {1, 1}});
    curve.set("curves.b", pfx::Curve{{0, 0.06}, {1, 0.95}});
    return {
        {"exposure", with({{"light.exposure", 0.7}}), 0.004},
        {"tone",
         with({{"light.contrast", 35},
               {"light.highlights", -40},
               {"light.shadows", 45},
               {"light.whites", 10},
               {"light.blacks", -15}}),
         0.004},
        {"white_balance", with({{"color.wb.temperature", 30}, {"color.wb.tint", -15}}), 0.004},
        {"vibrance_saturation", with({{"color.vibrance", 40}, {"color.saturation", -20}}), 0.004},
        {"curves", curve, 0.004},
        {"clarity", with({{"detail.clarity", 60}}), 0.012},
        {"sharpen", with({{"detail.sharpen.amount", 80}, {"detail.sharpen.radius", 0.004}}), 0.015},
        {"denoise", with({{"detail.denoise.luminance", 70}, {"detail.denoise.color", 60}}), 0.012},
        {"geometry",
         with({{"geometry.angle", 4.5},
               {"geometry.crop.x", 0.1},
               {"geometry.crop.y", 0.05},
               {"geometry.crop.w", 0.8},
               {"geometry.crop.h", 0.85}}),
         0.02},
        {"vignette", with({{"effects.vignette.amount", -45}, {"effects.vignette.roundness", 30}}), 0.004},
        {"grain", with({{"effects.grain.amount", 40}, {"effects.grain.size", 80}}), 0.05},
        {"fade", with({{"effects.fade", 40}}), 0.004},
    };
}

// Saves and reloads through a 16-bit PNG, the same path the golden file took.
pfx::Image throughPng(const pfx::Image& image, const std::filesystem::path& path)
{
    pfx::SaveOptions options;
    options.bitsPerSample = 16;
    REQUIRE(pfx::save(image, path, options).ok());
    auto loaded = pfx::load(path);
    REQUIRE(loaded.ok());
    return std::move(loaded).value().image;
}

} // namespace

TEST_CASE("golden images: each operation's output is unchanged")
{
    const bool update = std::getenv("PFX_GOLDEN_UPDATE") != nullptr;
    const fixtures::TempDir dir;
    const pfx::Image scene = fixtures::scene();
    for (const Case& c : cases()) {
        CAPTURE(c.name);
        const auto rendered = pfx::render(scene, c.settings);
        REQUIRE(rendered.ok());
        const std::filesystem::path golden =
            std::filesystem::path(PFX_GOLDEN_DIR) / (std::string(c.name) + ".png");
        if (update) {
            pfx::SaveOptions options;
            options.bitsPerSample = 16;
            REQUIRE(pfx::save(rendered.value(), golden, options).ok());
            continue;
        }
        const auto expected = pfx::load(golden);
        REQUIRE_MESSAGE(expected.ok(),
                        "missing golden image; run with PFX_GOLDEN_UPDATE=1 once and review it");
        const pfx::Image actual = throughPng(rendered.value(), dir.path() / "actual.png");
        const fixtures::Diff diff = fixtures::compare(actual, expected.value().image);
        CHECK(diff.max < 2e-3);
    }
}

TEST_CASE("scale: a half-size render matches the full-size render downscaled")
{
    const pfx::Image full = fixtures::scene(512, 384);
    const pfx::Image half = pfx::downscale(full, 2);
    for (const Case& c : cases()) {
        CAPTURE(c.name);
        const auto big = pfx::render(full, c.settings);
        const auto small = pfx::render(half, c.settings);
        REQUIRE(big.ok());
        REQUIRE(small.ok());
        const pfx::Image halved = pfx::downscale(big.value(), 2);
        if (halved.width() != small.value().width() || halved.height() != small.value().height()) {
            // Cropping and straightening round to whole pixels at each size: the sizes agree
            // within two pixels and the picture does too, but not pixel for pixel.
            CHECK(std::abs(halved.width() - small.value().width()) <= 2);
            CHECK(std::abs(halved.height() - small.value().height()) <= 2);
            CHECK(fixtures::meanLuma(halved) ==
                  doctest::Approx(fixtures::meanLuma(small.value())).epsilon(0.02));
            continue;
        }
        const fixtures::Diff diff = fixtures::compare(halved, small.value());
        CAPTURE(diff.mean);
        CAPTURE(diff.max);
        CHECK(diff.mean < c.scaleMean);
    }
}

TEST_CASE("rendering is deterministic across thread counts")
{
    const pfx::Image scene = fixtures::scene();
    for (const Case& c : cases()) {
        CAPTURE(c.name);
        const auto one = pfx::render(scene, c.settings, {1, nullptr});
        const auto many = pfx::render(scene, c.settings, {7, nullptr});
        CHECK(one.value() == many.value());
    }
}
