// SPDX-License-Identifier: Apache-2.0
// What each operation does, checked by its effect rather than exact numbers (the golden tests
// in test_pipeline.cpp pin the exact output).
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/pipeline.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <doctest/doctest.h>

namespace {

pfx::Image run(const pfx::Image& input, std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    auto result = pfx::render(input, s);
    REQUIRE(result.ok());
    return std::move(result).value();
}

double luma(const pfx::Image& img, int x, int y)
{
    const float* p = img.pixel(x, y);
    return pfx::color::luminance(p[0], p[1], p[2]);
}

double chroma(const float* p)
{
    return std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]});
}

} // namespace

TEST_CASE("empty settings leave the image untouched")
{
    const pfx::Image input = fixtures::scene();
    CHECK(run(input, {}) == input);
}

TEST_CASE("exposure +1 doubles linear values")
{
    const pfx::Image out = run(fixtures::flat(0.1F, 0.2F, 0.3F), {{"light.exposure", 1.0}});
    CHECK(out.pixel(0, 0)[0] == doctest::Approx(0.2F));
    CHECK(out.pixel(0, 0)[2] == doctest::Approx(0.6F));
    CHECK(out.pixel(0, 0)[3] == doctest::Approx(1.0F)); // alpha untouched
}

TEST_CASE("contrast spreads the tones around middle grey and keeps the ends")
{
    const pfx::Image ramp = fixtures::ramp();
    const pfx::Image out = run(ramp, {{"light.contrast", 60.0}});
    CHECK(luma(out, 40, 0) < luma(ramp, 40, 0));   // a dark tone gets darker
    CHECK(luma(out, 200, 0) > luma(ramp, 200, 0)); // a bright tone gets brighter
    CHECK(luma(out, 255, 0) == doctest::Approx(luma(ramp, 255, 0)).epsilon(1e-3));
    for (int x = 1; x < ramp.width(); ++x) {
        CHECK(luma(out, x, 0) >= luma(out, x - 1, 0)); // never reverses
    }
}

TEST_CASE("shadows lift dark tones more than bright ones; highlights pull bright tones down")
{
    const pfx::Image ramp = fixtures::ramp();
    const pfx::Image lifted = run(ramp, {{"light.shadows", 80.0}});
    CHECK(luma(lifted, 64, 0) / luma(ramp, 64, 0) > luma(lifted, 220, 0) / luma(ramp, 220, 0));
    CHECK(luma(lifted, 64, 0) > luma(ramp, 64, 0));
    const pfx::Image pulled = run(ramp, {{"light.highlights", -80.0}});
    CHECK(luma(pulled, 190, 0) < luma(ramp, 190, 0));
    CHECK(luma(pulled, 30, 0) == doctest::Approx(luma(ramp, 30, 0)).epsilon(0.05));
}

TEST_CASE("blacks lift pure black to a neutral grey")
{
    const pfx::Image out = run(fixtures::flat(0, 0, 0), {{"light.blacks", 100.0}});
    const float* p = out.pixel(0, 0);
    CHECK(p[0] > 0.0F);
    CHECK(p[0] == doctest::Approx(p[1]));
    CHECK(p[1] == doctest::Approx(p[2]));
}

TEST_CASE("temperature warms and tint turns magenta; grey stays the same brightness")
{
    const pfx::Image grey = fixtures::flat(0.18F, 0.18F, 0.18F);
    const pfx::Image warmed = run(grey, {{"color.wb.temperature", 50.0}});
    CHECK(warmed.pixel(0, 0)[0] > warmed.pixel(0, 0)[2]);
    const pfx::Image tinted = run(grey, {{"color.wb.tint", 50.0}});
    const float* t = tinted.pixel(0, 0);
    CHECK(t[1] < t[0]);
    CHECK(t[1] < t[2]);
    const pfx::Image cool = run(grey, {{"color.wb.temperature", -50.0}});
    CHECK(cool.pixel(0, 0)[2] > cool.pixel(0, 0)[0]);
    CHECK(fixtures::meanLuma(cool) == doctest::Approx(0.18).epsilon(0.08));
}

TEST_CASE("saturation -100 gives grey; vibrance boosts muted colours more than vivid ones")
{
    const pfx::Image grey = run(fixtures::scene(), {{"color.saturation", -100.0}});
    for (int x = 0; x < grey.width(); x += 17) {
        CHECK(chroma(grey.pixel(x, 150)) == doctest::Approx(0.0).epsilon(1e-5));
    }
    pfx::Image two(2, 1);
    float muted[4] = {0.22F, 0.18F, 0.16F, 1.0F};
    float vivid[4] = {0.50F, 0.05F, 0.02F, 1.0F};
    std::copy(muted, muted + 4, two.pixel(0, 0));
    std::copy(vivid, vivid + 4, two.pixel(1, 0));
    const pfx::Image out = run(two, {{"color.vibrance", 60.0}});
    const double mutedGain = chroma(out.pixel(0, 0)) / chroma(two.pixel(0, 0));
    const double vividGain = chroma(out.pixel(1, 0)) / chroma(two.pixel(1, 0));
    CHECK(mutedGain > vividGain);
    CHECK(vividGain >= 1.0);
}

TEST_CASE("an identity curve does nothing; a raised midpoint brightens and stays monotonic")
{
    const pfx::Image ramp = fixtures::ramp();
    pfx::Settings identity;
    identity.set("curves.rgb", pfx::Curve{{0, 0}, {1, 1}});
    CHECK(pfx::render(ramp, identity).value() == ramp);
    pfx::Settings lift;
    lift.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.65}, {1, 1}});
    const pfx::Image out = pfx::render(ramp, lift).value();
    CHECK(luma(out, 128, 0) > luma(ramp, 128, 0));
    for (int x = 1; x < ramp.width(); ++x) {
        CHECK(luma(out, x, 0) >= luma(out, x - 1, 0));
    }
    pfx::Settings red;
    red.set("curves.r", pfx::Curve{{0, 0}, {0.5, 0.7}, {1, 1}});
    const pfx::Image reddened = pfx::render(fixtures::flat(0.18F, 0.18F, 0.18F), red).value();
    CHECK(reddened.pixel(0, 0)[0] > reddened.pixel(0, 0)[1]);
    CHECK(reddened.pixel(0, 0)[1] == doctest::Approx(0.18F));
}

TEST_CASE("clarity and sharpening add local contrast; noise reduction removes it")
{
    const pfx::Image scene = fixtures::scene();
    const double before = fixtures::lumaStdDev(scene);
    CHECK(fixtures::lumaStdDev(run(scene, {{"detail.clarity", 80.0}})) > before);
    CHECK(fixtures::lumaStdDev(run(scene, {{"detail.sharpen.amount", 120.0}})) > before);
    // Noise reduction: the fine noise of a sky patch (difference between neighbouring pixels)
    // drops, while the patch's average stays where it was.
    const auto fineNoise = [](const pfx::Image& img) {
        double sum = 0;
        int n = 0;
        for (int y = 20; y < 60; ++y) {
            for (int x = 10; x < 60; ++x) {
                sum += std::abs(luma(img, x, y) - luma(img, x + 1, y));
                ++n;
            }
        }
        return sum / n;
    };
    const pfx::Image smoothed = run(scene, {{"detail.denoise.luminance", 100.0}});
    CHECK(fineNoise(smoothed) < fineNoise(scene) * 0.5);
    CHECK(fixtures::meanLuma(smoothed) == doctest::Approx(fixtures::meanLuma(scene)).epsilon(0.01));
}

TEST_CASE("rendering can be cancelled")
{
    std::atomic<bool> cancel{true};
    pfx::Settings s;
    s.set("light.exposure", 1.0);
    const auto result = pfx::render(fixtures::scene(), s, {0, &cancel});
    CHECK_FALSE(result.ok());
}
