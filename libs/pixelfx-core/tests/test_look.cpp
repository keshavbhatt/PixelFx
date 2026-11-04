// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/document.h"
#include "pfx/look.h"
#include "pfx/pipeline.h"
#include "recipe/encoding.h"

#include <cmath>
#include <doctest/doctest.h>

namespace {

pfx::Look brighter()
{
    pfx::Look look;
    pfx::LookStep step;
    step.op = "exposure";
    step.params.set("light.exposure", 1.0); // one stop: twice the light
    look.steps.push_back(step);
    return look;
}

} // namespace

TEST_CASE("a look runs its steps and blends by strength in linear light")
{
    pfx::Image image(4, 4);
    image.fill(0.2F, 0.2F, 0.2F);
    pfx::Image full = image;
    pfx::applyLook(full, brighter(), 1.0F);
    CHECK(full.pixel(1, 1)[0] == doctest::Approx(0.4F).epsilon(1e-3));
    pfx::Image half = image;
    pfx::applyLook(half, brighter(), 0.5F);
    CHECK(half.pixel(1, 1)[0] == doctest::Approx(0.3F).epsilon(1e-3));
    pfx::Image none = image;
    pfx::applyLook(none, brighter(), 0.0F);
    CHECK(none == image);
}

TEST_CASE("disabled, unknown, geometry and LUT steps")
{
    pfx::Image image(4, 4);
    image.fill(0.2F, 0.2F, 0.2F);
    pfx::Look look = brighter();
    look.steps.front().enabled = false;
    pfx::LookStep unknown;
    unknown.op = "time_travel";
    look.steps.push_back(unknown);
    pfx::LookStep turn;
    turn.op = "geometry";
    turn.params.set("geometry.rotate90", 1.0);
    look.steps.push_back(turn);
    pfx::LookStep missing;
    missing.op = "lut";
    missing.params.set("resource", std::string("nowhere"));
    look.steps.push_back(missing);
    pfx::Image out = image;
    pfx::applyLook(out, look, 1.0F);
    CHECK(out == image); // nothing ran, nothing turned

    // A LUT that makes everything black, at a mix of 50: half the light.
    const auto black =
        pfx::parseCube("LUT_3D_SIZE 2\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n");
    REQUIRE(black.ok());
    pfx::Look dark;
    dark.resources.emplace("black", black.value());
    pfx::LookStep lut;
    lut.op = "lut";
    lut.params.set("resource", std::string("black"));
    lut.params.set("mix", 50.0);
    dark.steps.push_back(lut);
    pfx::applyLook(out, dark, 1.0F);
    CHECK(out.pixel(0, 0)[1] == doctest::Approx(0.1F).epsilon(1e-3));
}

TEST_CASE("render runs the look after colour grading and before the effects")
{
    const pfx::Image scene = fixtures::scene(64, 48);
    const pfx::Look look = brighter();
    pfx::RenderOptions options;
    options.look = &look;
    options.lookStrength = 0.75F;
    const auto rendered = pfx::render(scene, {}, options);
    REQUIRE(rendered.ok());
    pfx::Image direct = scene;
    pfx::applyLook(direct, look, 0.75F);
    CHECK(rendered.value() == direct);
    // Fade comes after the look: fading then brightening would lift the blacks twice as far.
    pfx::Settings fade;
    fade.set("effects.fade", 50.0);
    const auto withFade = pfx::render(scene, fade, options);
    const auto fadeOnly = pfx::render(direct, fade);
    REQUIRE(withFade.ok());
    REQUIRE(fadeOnly.ok());
    CHECK(withFade.value() == fadeOnly.value());
}

TEST_CASE("a look document reads back the same, LUT included")
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Look;
    doc.name = "Golden hour";
    doc.look.category = "color";
    doc.look.defaultStrength = 80;
    doc.look.steps.push_back(brighter().steps.front());
    pfx::LookStep curve;
    curve.op = "curves";
    curve.params.set("curves.rgb", pfx::Curve{{0.0, 0.04}, {0.5, 0.48}, {1.0, 0.97}});
    curve.enabled = false;
    doc.look.steps.push_back(curve);
    pfx::LookStep lut;
    lut.op = "lut";
    lut.params.set("resource", std::string("warm"));
    lut.params.set("mix", 60.0);
    doc.look.steps.push_back(lut);
    auto warm = pfx::parseCube(
        "TITLE \"warm\"\nLUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 0.9\n1 0 0.9\n0 1 0.9\n1 "
        "1 0.9\n");
    REQUIRE(warm.ok());
    doc.look.resources.emplace("warm", warm.value());

    const std::string text = pfx::serializeDocument(doc);
    CHECK(text.find("\"exposure\": 1") != std::string::npos); // params under the op's group
    pfx::ParseReport report;
    const auto back = pfx::parseDocument(text, &report);
    REQUIRE(back.ok());
    CHECK(report.warnings.empty());
    CHECK(back.value().look.category == "color");
    CHECK(back.value().look.defaultStrength == 80);
    CHECK(back.value().look.steps == doc.look.steps);
    const pfx::Lut& lutBack = back.value().look.resources.at("warm");
    CHECK(lutBack.size == 2);
    CHECK(lutBack.title == "warm");
    CHECK(lutBack.table[(7 * 3) + 2] == doctest::Approx(0.9F).epsilon(1e-3)); // half precision
}

TEST_CASE("an unknown step is kept but reported, a damaged LUT left out")
{
    const std::string text = R"({"format":"pixelfx","kind":"look","version":1,"body":{
        "steps":[{"op":"time_travel","v":3,"enabled":true,"params":{"speed":88}},
                 {"op":"exposure","params":{"exposure":9}}],
        "resources":{"bad":{"type":"lut3d","size":2,"domain":"srgb-gamma","encoding":"f16-base64","data":"AAAA"},
                     "log":{"type":"lut3d","size":2,"domain":"acescct","encoding":"f16-base64","data":""}}}})";
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(text, &report);
    REQUIRE(doc.ok());
    REQUIRE(doc.value().look.steps.size() == 2);
    CHECK(doc.value().look.steps[0].op == "time_travel");
    CHECK(doc.value().look.steps[0].params.number("speed", 0) == 88);
    CHECK(doc.value().look.steps[1].params.number("light.exposure", 0) == 5); // clamped to the range
    CHECK(doc.value().look.resources.empty());
    CHECK(report.warnings.size() >= 4);
    // Written back, the unknown step keeps its params for the newer version that knows it.
    CHECK(pfx::serializeDocument(doc.value()).find("\"speed\": 88") != std::string::npos);
}

TEST_CASE("base64 and half floats")
{
    using pfx::detail::base64Decode;
    using pfx::detail::base64Encode;
    CHECK(base64Encode({'M', 'a', 'n'}) == "TWFu");
    CHECK(base64Encode({'M', 'a'}) == "TWE=");
    CHECK(base64Encode({'M'}) == "TWE="[0] + std::string("Q=="));
    CHECK(base64Decode("TWE=").value() == std::vector<std::uint8_t>{'M', 'a'});
    CHECK(base64Decode("TQ").value() == std::vector<std::uint8_t>{'M'});
    CHECK_FALSE(base64Decode("T").has_value());
    CHECK_FALSE(base64Decode("T*==").has_value());
    CHECK(pfx::detail::toHalf(1.0F) == 0x3C00);
    CHECK(pfx::detail::toHalf(-2.0F) == 0xC000);
    CHECK(pfx::detail::toHalf(65520.0F) == 0x7C00); // rounds past the largest half
    CHECK(std::isnan(pfx::detail::fromHalf(pfx::detail::toHalf(std::nanf("")))));
    for (const float v : {0.0F, 0.5F, 0.333F, 1e-5F, 6e-8F, 1000.25F}) {
        // Half floats keep about three digits, and subnormals (below 6e-5) a step of 6e-8.
        CHECK(pfx::detail::fromHalf(pfx::detail::toHalf(v)) == doctest::Approx(v).epsilon(1e-3).scale(1e-4));
    }
}

TEST_CASE("baking keeps the colour-only steps, names the rest, and reads back as a .cube")
{
    pfx::Look look = brighter(); // exposure: pointwise
    pfx::LookStep grain;
    grain.op = "grain";
    grain.params.set("effects.grain.amount", 30.0);
    look.steps.push_back(grain);
    const pfx::BakedLook baked = pfx::bakeLook(look, 17);
    CHECK(baked.leftOut == std::vector<std::string>{"grain"});
    CHECK(baked.lut.size == 17);
    // Applying the baked table matches the look on a colour inside sRGB, below white.
    pfx::Image a(1, 1);
    a.fill(0.1F, 0.08F, 0.05F);
    pfx::Image b = a;
    pfx::applyLook(a, brighter(), 1.0F);
    pfx::applyLut(b, baked.lut);
    for (int c = 0; c < 3; ++c) {
        CHECK(b.pixel(0, 0)[c] == doctest::Approx(a.pixel(0, 0)[c]).epsilon(0.02));
    }
    const auto back = pfx::parseCube(pfx::cubeText(baked.lut));
    REQUIRE(back.ok());
    CHECK(back.value().size == 17);
    CHECK(back.value().table[100] == doctest::Approx(baked.lut.table[100]).epsilon(1e-5));
}

TEST_CASE("a look's LUT with an empty or endless domain, or non-numbers, is left out")
{
    const std::string zeros =
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"; // 48 bytes of 0
    for (const std::string& domain : {std::string(R"("domainMin":[0,0,0],"domainMax":[0,0,0],)"),
                                      std::string(R"("domainMin":[0,0,0],"domainMax":[1e300,1,1],)")}) {
        const std::string text =
            R"({"format":"pixelfx","kind":"look","version":1,"body":{"resources":{"x":{"type":"lut3d","size":2,)" +
            domain + R"("domain":"srgb-gamma","encoding":"f16-base64","data":")" + zeros + R"("}}}})";
        pfx::ParseReport report;
        const auto doc = pfx::parseDocument(text, &report);
        REQUIRE(doc.ok());
        CHECK(doc.value().look.resources.empty());
    }
    // Half-float infinity (0x7C00) in the table.
    std::vector<std::uint8_t> bytes(48, 0);
    bytes[1] = 0x7C;
    const std::string text =
        R"({"format":"pixelfx","kind":"look","version":1,"body":{"resources":{"x":{"type":"lut3d","size":2,"domain":"srgb-gamma","encoding":"f16-base64","data":")" +
        pfx::detail::base64Encode(bytes) + R"("}}}})";
    const auto doc = pfx::parseDocument(text);
    REQUIRE(doc.ok());
    CHECK(doc.value().look.resources.empty());
}
