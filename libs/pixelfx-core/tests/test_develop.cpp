// SPDX-License-Identifier: Apache-2.0
// The develop settings added for Lightroom compatibility (FEATURES P8): each does what its
// name says, and its default is no change.
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/document.h"
#include "pfx/pipeline.h"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <string>

namespace {

pfx::Image rendered(const pfx::Image& img, const pfx::Settings& s)
{
    auto out = pfx::render(img, s);
    REQUIRE(out.ok());
    return std::move(out).value();
}

pfx::Settings with(std::initializer_list<std::pair<const char*, double>> values)
{
    pfx::Settings s;
    for (const auto& [key, value] : values) {
        s.set(key, value);
    }
    return s;
}

pfx::Image grey(double encoded, int size = 4)
{
    const auto v = static_cast<float>(pfx::color::perceptualDecode(encoded));
    return fixtures::flat(v, v, v, size, size);
}

double lightness(const pfx::Image& img, int x = 0, int y = 0)
{
    const float* p = img.pixel(x, y);
    return pfx::color::perceptualEncode(pfx::color::luminance(p[0], p[1], p[2]));
}

pfx::Settings blackAndWhite(std::initializer_list<std::pair<const char*, double>> mix)
{
    pfx::Settings s = with(mix);
    s.set("bw.enabled", true);
    return s;
}

} // namespace

TEST_CASE("black and white: grey output, the mix brightens or darkens a colour, greys keep their tone")
{
    const pfx::Image blue = fixtures::flat(0.05F, 0.1F, 0.6F);
    const pfx::Image plain = rendered(blue, blackAndWhite({}));
    CHECK(plain.pixel(0, 0)[0] == plain.pixel(0, 0)[1]);
    CHECK(plain.pixel(0, 0)[1] == plain.pixel(0, 0)[2]);
    CHECK(lightness(plain) == doctest::Approx(lightness(blue)).epsilon(0.01));
    CHECK(lightness(rendered(blue, blackAndWhite({{"bw.mix.blue", -80}}))) < lightness(plain) - 0.05);
    CHECK(lightness(rendered(blue, blackAndWhite({{"bw.mix.blue", 80}}))) > lightness(plain) + 0.05);
    const pfx::Image mid = grey(0.5);
    CHECK(lightness(rendered(mid, blackAndWhite({{"bw.mix.blue", -80}}))) ==
          doctest::Approx(0.5).epsilon(0.01));
}

TEST_CASE("colour grading: midtones tint the middle, global tints everything, luminance lightens")
{
    const pfx::Settings warmMid = with({{"grading.midtones.hue", 30}, {"grading.midtones.amount", 80}});
    const auto warmth = [](const pfx::Image& img) { return double{img.pixel(0, 0)[0] - img.pixel(0, 0)[2]}; };
    CHECK(warmth(rendered(grey(0.45), warmMid)) > 0.02);
    CHECK(std::abs(warmth(rendered(grey(0.08), warmMid))) < 0.005); // deep shadows barely
    const pfx::Settings coolAll = with({{"grading.global.hue", 220}, {"grading.global.amount", 60}});
    CHECK(warmth(rendered(grey(0.3), coolAll)) < -0.01);
    CHECK(warmth(rendered(grey(0.7), coolAll)) < -0.01);
    CHECK(lightness(rendered(grey(0.3), with({{"grading.shadows.luminance", 60}}))) > 0.33);
    // Blending narrows or widens how far the shadow colour reaches into the midtones.
    const auto shadowTint = [&](double blending) {
        const pfx::Image out = rendered(grey(0.62), with({{"grading.shadows.hue", 220},
                                                          {"grading.shadows.amount", 80},
                                                          {"grading.blending", blending}}));
        return -warmth(out);
    };
    CHECK(shadowTint(100) > shadowTint(0));
}

TEST_CASE("texture: more medium detail, or less; flat areas stay flat")
{
    const pfx::Image scene = fixtures::scene();
    const double base = fixtures::lumaStdDev(scene);
    CHECK(fixtures::lumaStdDev(rendered(scene, with({{"detail.texture", 80}}))) > base);
    CHECK(fixtures::lumaStdDev(rendered(scene, with({{"detail.texture", -80}}))) < base);
    const pfx::Image flat = grey(0.5, 32);
    CHECK(fixtures::compare(rendered(flat, with({{"detail.texture", 80}})), flat).max < 1e-4);
}

TEST_CASE("calibration: primaries move, white and grey stay neutral; shadow tint")
{
    const pfx::Image red = fixtures::flat(0.7F, 0.05F, 0.05F);
    const pfx::Image toOrange = rendered(red, with({{"calibration.red.hue", 80}}));
    CHECK(toOrange.pixel(0, 0)[1] > red.pixel(0, 0)[1]);
    const pfx::Image mid = grey(0.5);
    const pfx::Image moved = rendered(mid, with({{"calibration.red.hue", 80},
                                                 {"calibration.green.saturation", -60},
                                                 {"calibration.blue.hue", -50}}));
    CHECK(moved.pixel(0, 0)[0] == doctest::Approx(mid.pixel(0, 0)[0]).epsilon(1e-4));
    CHECK(moved.pixel(0, 0)[2] == doctest::Approx(mid.pixel(0, 0)[2]).epsilon(1e-4));
    const pfx::Image magenta = rendered(grey(0.15), with({{"calibration.shadows.tint", 80}}));
    CHECK(magenta.pixel(0, 0)[1] < magenta.pixel(0, 0)[0]);
    const pfx::Image bright = rendered(grey(0.9), with({{"calibration.shadows.tint", 80}}));
    CHECK(bright.pixel(0, 0)[1] == doctest::Approx(bright.pixel(0, 0)[0]).epsilon(1e-3));
}

TEST_CASE("vivid profile: purer colour and more contrast, greys stay grey")
{
    pfx::Settings vivid;
    vivid.set("color.profile", std::string("vivid"));
    const pfx::Image teal = fixtures::flat(0.1F, 0.35F, 0.4F);
    const pfx::Image out = rendered(teal, vivid);
    CHECK(out.pixel(0, 0)[2] - out.pixel(0, 0)[0] > teal.pixel(0, 0)[2] - teal.pixel(0, 0)[0]);
    CHECK(lightness(rendered(grey(0.25), vivid)) < 0.25);
    CHECK(lightness(rendered(grey(0.75), vivid)) > 0.75);
    const pfx::Image g = rendered(grey(0.6), vivid);
    CHECK(g.pixel(0, 0)[0] == doctest::Approx(g.pixel(0, 0)[2]));
}

TEST_CASE("parametric curve: each region moves its own tones, the curve keeps rising")
{
    const pfx::Image ramp = fixtures::ramp(256, 1);
    const pfx::Image lifted = rendered(ramp, with({{"curves.parametric.shadows", 100}}));
    CHECK(lifted.pixel(32, 0)[1] > ramp.pixel(32, 0)[1]);
    CHECK(lifted.pixel(250, 0)[1] == doctest::Approx(ramp.pixel(250, 0)[1]).epsilon(0.01));
    const pfx::Image wild = rendered(ramp, with({{"curves.parametric.shadows", 100},
                                                 {"curves.parametric.darks", -100},
                                                 {"curves.parametric.lights", 100},
                                                 {"curves.parametric.highlights", -100},
                                                 {"curves.parametric.split.shadows", 10},
                                                 {"curves.parametric.split.midtones", 15}}));
    for (int x = 1; x < 256; ++x) {
        CHECK(wild.pixel(x, 0)[1] >= wild.pixel(x - 1, 0)[1]);
    }
}

TEST_CASE("vignette: feather, paint style and highlight protection")
{
    const pfx::Image dark = grey(0.4, 120);
    const auto corner = [](const pfx::Image& img) { return lightness(img, 2, 2); };
    const auto edgeMid = [](const pfx::Image& img) { return lightness(img, 2, 60); };
    const pfx::Image hard =
        rendered(dark, with({{"effects.vignette.amount", -60}, {"effects.vignette.feather", 0}}));
    const pfx::Image soft =
        rendered(dark, with({{"effects.vignette.amount", -60}, {"effects.vignette.feather", 100}}));
    CHECK(corner(hard) != doctest::Approx(corner(soft)));
    CHECK(edgeMid(soft) < 0.4);
    const pfx::Image bright = grey(0.95, 120);
    const pfx::Image plain = rendered(bright, with({{"effects.vignette.amount", -60}}));
    const pfx::Image kept =
        rendered(bright, with({{"effects.vignette.amount", -60}, {"effects.vignette.highlights", 100}}));
    CHECK(corner(kept) > corner(plain));
    pfx::Settings paint = with({{"effects.vignette.amount", 60}});
    paint.set("effects.vignette.style", std::string("paint"));
    CHECK(corner(rendered(dark, paint)) > 0.45); // mixed toward white
}

TEST_CASE("grain roughness: changes the grain, the same settings give the same grain")
{
    const pfx::Image mid = grey(0.5, 64);
    const pfx::Settings even = with({{"effects.grain.amount", 60}});
    const pfx::Settings rough = with({{"effects.grain.amount", 60}, {"effects.grain.roughness", 100}});
    CHECK(fixtures::compare(rendered(mid, even), rendered(mid, rough)).mean > 1e-3);
    CHECK(fixtures::compare(rendered(mid, rough), rendered(mid, rough)).max == 0.0);
}

TEST_CASE("sharpen detail: low detail softens halos at a hard edge")
{
    pfx::Image edge = grey(0.2, 32);
    for (int y = 0; y < 32; ++y) {
        for (int x = 16; x < 32; ++x) {
            float* p = edge.pixel(x, y);
            p[0] = p[1] = p[2] = 0.8F;
        }
    }
    const auto overshoot = [&](double detail) {
        const pfx::Image out = rendered(edge, with({{"detail.sharpen.amount", 150},
                                                    {"detail.sharpen.radius", 0.03},
                                                    {"detail.sharpen.detail", detail}}));
        return double{out.pixel(16, 16)[1] - edge.pixel(16, 16)[1]};
    };
    CHECK(overshoot(0) < overshoot(25));
    CHECK(overshoot(100) > overshoot(25));
}

TEST_CASE("chromatic aberration: a red image larger than the green one is fitted back")
{
    // Rings, with red drawn 0.25 percent larger than green and blue.
    pfx::Image img(400, 300);
    const double cx = 199.5;
    const double cy = 149.5;
    const auto ring = [](double r) { return 0.5 + (0.45 * std::sin(r / 6.0)); };
    for (int y = 0; y < 300; ++y) {
        for (int x = 0; x < 400; ++x) {
            const double r = std::hypot(x - cx, y - cy);
            float* p = img.pixel(x, y);
            p[0] = static_cast<float>(ring(r / 1.0025));
            p[1] = p[2] = static_cast<float>(ring(r));
            p[3] = 1.0F;
        }
    }
    pfx::Settings s;
    s.set("detail.ca.auto", true);
    const pfx::Image fixed = rendered(img, s);
    const auto misfit = [](const pfx::Image& im) {
        double sum = 0;
        for (int y = 0; y < 300; ++y) {
            for (int x = 0; x < 400; ++x) {
                sum += std::abs(double{im.pixel(x, y)[0] - im.pixel(x, y)[1]});
            }
        }
        return sum;
    };
    CHECK(misfit(fixed) < 0.5 * misfit(img));
}

TEST_CASE("defringe: purple next to a hard edge turns grey, purple away from edges stays")
{
    pfx::Image img(60, 40);
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 60; ++x) {
            float* p = img.pixel(x, y);
            const bool white = x >= 30 && x < 34;
            const bool fringe = x == 29 || x == 34 || (x > 50 && y > 30);
            p[0] = white ? 1.0F : (fringe ? 0.35F : 0.02F);
            p[1] = white ? 1.0F : (fringe ? 0.05F : 0.02F);
            p[2] = white ? 1.0F : (fringe ? 0.5F : 0.02F);
            p[3] = 1.0F;
        }
    }
    const pfx::Image out = rendered(img, with({{"detail.defringe.purple.amount", 20}}));
    const auto chroma = [](const pfx::Image& im, int x, int y) {
        const float* p = im.pixel(x, y);
        return double{std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]})};
    };
    CHECK(chroma(out, 29, 20) < 0.5 * chroma(img, 29, 20));
    CHECK(chroma(out, 56, 36) == doctest::Approx(chroma(img, 56, 36)).epsilon(0.05));
}

TEST_CASE("presets version 2 and recipes version 3: older files read unchanged, new settings round-trip")
{
    const auto old = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"preset","version":1,"body":{"settings":{"light":{"exposure":0.5}}}})");
    REQUIRE(old.ok());
    CHECK(old.value().version == 2);
    CHECK(old.value().settings.number("light.exposure", 0) == 0.5);
    const auto recipe = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"recipe","version":2,"body":{"settings":{"color":{"vibrance":10}}}})");
    REQUIRE(recipe.ok());
    CHECK(recipe.value().version == 3);
    CHECK(pfx::parseDocument(R"({"format":"pixelfx","kind":"preset","version":3})").error().code ==
          pfx::ErrorCode::NewerVersion);

    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Preset;
    doc.name = "Film";
    doc.settings.set("bw.enabled", true);
    doc.settings.set("bw.mix.blue", -30.0);
    doc.settings.set("grading.midtones.hue", 40.0);
    doc.settings.set("grading.blending", 70.0);
    doc.settings.set("calibration.red.hue", 12.0);
    doc.settings.set("color.profile", std::string("vivid"));
    doc.settings.set("effects.vignette.style", std::string("paint"));
    doc.settings.set("curves.parametric.lights", 20.0);
    doc.settings.set("detail.ca.auto", true);
    const std::string text = pfx::serializeDocument(doc);
    CHECK(text.find("\"version\": 2") != std::string::npos);
    const auto back = pfx::parseDocument(text);
    REQUIRE(back.ok());
    CHECK(back.value().settings == doc.settings);
}
