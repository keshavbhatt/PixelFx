// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/document.h"

#include <doctest/doctest.h>
#include <string>

namespace {

// The example of DOCS/presets.md sections 1 and 2 (look, grading, effects and geometry are
// groups of later milestones and must survive as unknown keys).
const char* const kPresetExample = R"json({
  "format": "pixelfx", "kind": "preset", "version": 1,
  "id": "7b0f2c7e-3c1a-4d0e-9a51-2f5a2b0c9e11", "name": "Moody landscape", "group": "My presets",
  "created": "2026-11-04T18:20:00Z", "modified": "2026-11-04T18:20:00Z",
  "generator": "PixelFx 2.0.0 (libpixelfx 1.0.0)", "workingSpace": "rec2020-linear",
  "futureField": {"anything": [1, 2, 3]},
  "body": {
    "settings": {
      "light":   { "exposure": 0.35, "contrast": 18, "highlights": -35, "shadows": 28, "whites": 6, "blacks": -10 },
      "color":   { "wb": { "mode": "relative", "temperature": 14, "tint": 0 }, "vibrance": 22, "saturation": 0 },
      "curves":  { "rgb": [[0,0],[0.28,0.2],[0.72,0.76],[1,1]], "r": null, "g": null, "b": null },
      "detail":  { "clarity": 15, "sharpen": { "amount": 30, "radius": 0.0004, "masking": 0 }, "denoise": { "luminance": 0, "color": 25 } },
      "effects": { "vignette": { "amount": -22, "midpoint": 50, "roundness": 0 } }
    },
    "history": []
  }
})json";

} // namespace

TEST_CASE("the presets.md example parses into dotted settings")
{
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(kPresetExample, &report);
    REQUIRE(doc.ok());
    const pfx::Settings& s = doc.value().settings;
    CHECK(doc.value().kind == pfx::DocumentKind::Preset);
    CHECK(doc.value().name == "Moody landscape");
    CHECK(s.number("light.exposure", 0) == doctest::Approx(0.35));
    CHECK(s.number("color.wb.temperature", 0) == doctest::Approx(14));
    CHECK(s.text("color.wb.mode", "") == "relative");
    CHECK(s.number("detail.sharpen.amount", 0) == doctest::Approx(30));
    CHECK(s.curve("curves.rgb").size() == 4);
    CHECK_FALSE(s.contains("curves.r"));          // null means not set
    CHECK(s.contains("effects.vignette.amount")); // unknown to this version, kept
    CHECK(s.groups() == std::vector<std::string>{"color", "curves", "detail", "effects", "light"});
}

TEST_CASE("a document survives a round trip, unknown keys included")
{
    const auto first = pfx::parseDocument(kPresetExample);
    REQUIRE(first.ok());
    const std::string text = pfx::serializeDocument(first.value());
    const auto second = pfx::parseDocument(text);
    REQUIRE(second.ok());
    CHECK(second.value().settings == first.value().settings);
    CHECK(second.value().name == first.value().name);
    CHECK(text.find("futureField") != std::string::npos);
    CHECK(text.find("\"history\"") != std::string::npos);
    CHECK(pfx::serializeDocument(second.value()) == text);
}

TEST_CASE("out-of-range and malformed values are clamped or dropped, and reported")
{
    const char* json = R"({"format":"pixelfx","kind":"preset","version":1,"body":{"settings":{
        "light":{"exposure":9,"contrast":"high"},
        "curves":{"rgb":[[0.5,0.5],[0.2,0.9]]}}}})";
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(json, &report);
    REQUIRE(doc.ok());
    CHECK(doc.value().settings.number("light.exposure", 0) == doctest::Approx(5.0));
    CHECK_FALSE(doc.value().settings.contains("light.contrast"));
    CHECK_FALSE(doc.value().settings.contains("curves.rgb"));
    CHECK(report.warnings.size() == 3);
}

TEST_CASE("documents from a newer PixelFx, other formats and broken JSON are refused with a reason")
{
    const auto newer = pfx::parseDocument(R"({"format":"pixelfx","kind":"preset","version":99})");
    REQUIRE_FALSE(newer.ok());
    CHECK(newer.error().code == pfx::ErrorCode::NewerVersion);
    CHECK_FALSE(pfx::parseDocument(R"({"format":"other","kind":"preset","version":1})").ok());
    CHECK_FALSE(pfx::parseDocument("{not json").ok());
    CHECK_FALSE(pfx::parseDocument(R"({"format":"pixelfx","kind":"teapot","version":1})").ok());
    const auto look = pfx::parseDocument(R"({"format":"pixelfx","kind":"look","version":2})");
    REQUIRE_FALSE(look.ok());
    CHECK(look.error().code == pfx::ErrorCode::NewerVersion);
    // Batch recipes are at version 8: 8 reads, 9 is newer.
    CHECK(pfx::parseDocument(R"({"format":"pixelfx","kind":"batch","version":8})").ok());
    const auto batch = pfx::parseDocument(R"({"format":"pixelfx","kind":"batch","version":9})");
    REQUIRE_FALSE(batch.ok());
    CHECK(batch.error().code == pfx::ErrorCode::NewerVersion);
}

TEST_CASE("presets keep only the ticked groups and overlay onto a photo's settings")
{
    pfx::Settings photo;
    photo.set("light.exposure", 1.0);
    photo.set("color.vibrance", 10.0);
    pfx::Settings edit;
    edit.set("light.exposure", -0.5);
    edit.set("light.contrast", 20.0);
    edit.set("color.vibrance", 50.0);
    const pfx::Settings preset = edit.onlyGroups({"light"});
    CHECK_FALSE(preset.contains("color.vibrance"));
    const pfx::Settings applied = photo.overlaidWith(preset);
    CHECK(applied.number("light.exposure", 0) == doctest::Approx(-0.5));
    CHECK(applied.number("light.contrast", 0) == doctest::Approx(20));
    CHECK(applied.number("color.vibrance", 0) == doctest::Approx(10));
}

TEST_CASE("documents save to and load from files")
{
    const fixtures::TempDir dir;
    pfx::Document doc;
    doc.name = "Test";
    doc.settings.set("light.exposure", 0.25);
    const auto path = dir.path() / "test.pfxpreset";
    REQUIRE(pfx::saveDocument(doc, path).ok());
    const auto loaded = pfx::loadDocument(path);
    REQUIRE(loaded.ok());
    CHECK(loaded.value().settings == doc.settings);
    CHECK(pfx::loadDocument(dir.path() / "missing.pfx").error().code == pfx::ErrorCode::NotFound);
}

TEST_CASE("a recipe keeps the photo it belongs to")
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Recipe;
    doc.sourceName = "sunset-lake.jpg";
    doc.sourceSize = 8812341;
    doc.sourceFingerprint = "sha1:9f1c0a5be2d47a10";
    doc.settings.set("light.exposure", 0.5);
    const auto again = pfx::parseDocument(pfx::serializeDocument(doc));
    REQUIRE(again.ok());
    CHECK(again.value().kind == pfx::DocumentKind::Recipe);
    CHECK(again.value().sourceName == "sunset-lake.jpg");
    CHECK(again.value().sourceSize == 8812341);
    CHECK(again.value().sourceFingerprint == "sha1:9f1c0a5be2d47a10");
    CHECK(again.value().settings == doc.settings);
}

TEST_CASE("names that would not read back are dropped with a reason")
{
    const std::string text = R"({"format": "pixelfx", "kind": "preset", "version": 1, "name": "x",
        "body": {"settings": {".color": 40, "light": {"": 3, "a.b": 2, "exposure": 0.5},
                              "detail": {"clarity": 10}, "detail.clarity": {"x": 1}}}})";
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(text, &report);
    REQUIRE(doc.ok());
    const pfx::Settings& s = doc.value().settings;
    CHECK(s.keys() == std::vector<std::string>{"detail.clarity", "light.exposure"});
    CHECK(report.warnings.size() >= 3);
    const auto again = pfx::parseDocument(pfx::serializeDocument(doc.value()));
    REQUIRE(again.ok());
    CHECK(again.value().settings == s);
}

TEST_CASE("batch recipes: the stack and the output round-trip")
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    doc.name = "Web delivery";
    pfx::BatchStep warm;
    warm.name = "Warm";
    warm.settings.set("color.wb.temperature", 15.0);
    pfx::BatchStep crisp;
    crisp.name = "Crisp";
    crisp.settings.set("detail.clarity", 20.0);
    doc.batch.stack = {warm, crisp};
    doc.batch.autoFirst = true;
    doc.batch.longEdge = 2048;
    doc.batch.format = "webp";
    doc.batch.quality = 80;
    doc.batch.keepLocation = false;
    doc.batch.folder = "/tmp/out";
    doc.batch.name = "{name}-web";
    doc.batch.ifExists = "skip";
    const auto again = pfx::parseDocument(pfx::serializeDocument(doc));
    REQUIRE(again.ok());
    CHECK(again.value().kind == pfx::DocumentKind::Batch);
    CHECK(again.value().batch == doc.batch);
    CHECK(again.value().settings.isEmpty());
}

TEST_CASE("batch recipes: a version 1 file reads unchanged and is written as version 8")
{
    const std::string text = R"({"format": "pixelfx", "kind": "batch", "version": 1, "name": "x",
        "body": {"stack": [{"preset": {"embedded": {"name": "P", "settings": {"light": {"exposure": 0.5}}}}}],
                 "autoFirst": true, "resize": {"mode": "longEdge", "value": 1600, "noEnlarge": true},
                 "output": {"format": "png", "quality": 70, "folder": "/out", "name": "{name}", "ifExists": "skip"}}})";
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(text, &report);
    REQUIRE(doc.ok());
    CHECK(report.warnings.empty());
    CHECK(doc.value().version == 8);
    const pfx::BatchBody& b = doc.value().batch;
    REQUIRE(b.stack.size() == 1);
    CHECK(b.stack.front().name == "P");
    CHECK(b.stack.front().settings.number("light.exposure", 0) == 0.5);
    CHECK(b.autoFirst);
    CHECK(b.longEdge == 1600);
    CHECK(b.sizePreset.empty());
    CHECK(b.looks.empty());
    CHECK(b.watermark.empty());
    CHECK(b.format == "png");
    CHECK(b.ifExists == "skip");
    const std::string written = pfx::serializeDocument(doc.value());
    CHECK(written.find("\"version\": 8") != std::string::npos);
    const auto again = pfx::parseDocument(written);
    REQUIRE(again.ok());
    CHECK(again.value().batch == b);
}

namespace {

pfx::Look sampleLook()
{
    pfx::Look look;
    look.category = "film";
    look.defaultStrength = 80;
    pfx::LookStep step;
    step.op = "light";
    step.params.set("light.contrast", 20.0);
    look.steps.push_back(step);
    pfx::Lut lut;
    lut.size = 2;
    lut.table = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0,
                 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0.5F}; // exact in half floats
    look.resources.emplace("tone", lut);
    pfx::LookStep lutStep;
    lutStep.op = "lut";
    lutStep.params.set("resource", std::string("tone"));
    lutStep.params.set("mix", 50.0);
    look.steps.push_back(lutStep);
    return look;
}

} // namespace

TEST_CASE("batch recipes version 2: looks, embedded user looks, a size preset and the watermark round-trip")
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    pfx::BatchStep golden{"Golden hour", {}};
    golden.settings.set("look.id", std::string("builtin:golden-hour"));
    golden.settings.set("look.strength", 72.0);
    pfx::BatchStep mine{"Dusk walk", {}};
    mine.settings.set("look.id", std::string("1f0c2b9e-mine"));
    mine.settings.set("look.strength", 60.0);
    pfx::BatchStep preset{"Product white", {}};
    preset.settings.set("light.exposure", 0.3);
    preset.settings.set("look.id", std::string("1f0c2b9e-mine"));
    preset.settings.set("look.strength", 40.0);
    doc.batch.stack = {golden, mine, preset};
    doc.batch.looks = {{"1f0c2b9e-mine", "Dusk walk", sampleLook()}};
    doc.batch.sizePreset = "instagram-portrait";
    doc.batch.fitInside = true;
    doc.batch.watermark = R"({"name":"Studio","body":{"type":"text","text":"(c) {year}"}})";
    const std::string text = pfx::serializeDocument(doc);
    // A look on its own is a look step; a preset keeps its look by id.
    CHECK(text.find("\"look\": {") != std::string::npos);
    const auto again = pfx::parseDocument(text);
    REQUIRE(again.ok());
    CHECK(again.value().batch == doc.batch);
}

TEST_CASE("batch recipes: a look embedded in a step, as presets.md section 2 allows, is kept once")
{
    const std::string text = R"({"format": "pixelfx", "kind": "batch", "version": 2,
        "body": {"stack": [
            {"look": {"embedded": {"id": "abc", "name": "Mine", "defaultStrength": 90,
                                   "steps": [{"op": "light", "params": {"contrast": 10}}]}, "strength": 50}},
            {"preset": {"embedded": {"name": "P", "settings": {"light": {"exposure": 0.5},
                "look": {"embedded": {"id": "abc", "name": "Mine"}, "strength": 30}}}}},
            {"look": {"embedded": {"name": "no id"}, "strength": 50}},
            {"look": {"id": "builtin:noir"}}]}})";
    pfx::ParseReport report;
    const auto doc = pfx::parseDocument(text, &report);
    REQUIRE(doc.ok());
    const pfx::BatchBody& b = doc.value().batch;
    REQUIRE(b.looks.size() == 1);
    CHECK(b.looks.front().id == "abc");
    CHECK(b.looks.front().look.defaultStrength == 90);
    REQUIRE(b.stack.size() == 3); // the look with no id is left out, with a warning
    CHECK(b.stack[0].settings.text("look.id", "") == "abc");
    CHECK(b.stack[0].settings.number("look.strength", 0) == 50);
    CHECK(b.stack[1].settings.number("light.exposure", 0) == 0.5);
    CHECK(b.stack[1].settings.text("look.id", "") == "abc");
    CHECK(b.stack[1].settings.number("look.strength", 0) == 30);
    CHECK_FALSE(b.stack[1].settings.contains("look.embedded.id"));
    CHECK(b.stack[2].settings.number("look.strength", 0) == 100);
    CHECK_FALSE(report.warnings.empty());
}

TEST_CASE("recipes keep their snapshots")
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Recipe;
    doc.settings.set("light.exposure", 0.3);
    pfx::BatchStep warmer;
    warmer.name = "Warmer";
    warmer.settings.set("color.wb.temperature", 20.0);
    doc.snapshots = {warmer};
    const auto again = pfx::parseDocument(pfx::serializeDocument(doc));
    REQUIRE(again.ok());
    REQUIRE(again.value().snapshots.size() == 1);
    CHECK(again.value().snapshots.front() == warmer);
    CHECK(again.value().settings.number("light.exposure", 0) == 0.3);
}

TEST_CASE("batch recipes version 6: the colour space and output sharpening round-trip")
{
    // Version 5 wrote "srgb" whatever was chosen, and had no sharpening: both read as defaults.
    const auto old = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"batch","version":5,"body":{"output":{"colorSpace":"srgb"}}})");
    REQUIRE(old.ok());
    CHECK(old.value().batch.colorSpace == "srgb");
    CHECK(old.value().batch.sharpen == "none");
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    doc.batch.colorSpace = "display-p3";
    doc.batch.sharpen = "print";
    const auto back = pfx::parseDocument(pfx::serializeDocument(doc));
    REQUIRE(back.ok());
    CHECK(back.value().batch.colorSpace == "display-p3");
    CHECK(back.value().batch.sharpen == "print");
    // A value this version does not know keeps the default.
    const auto odd = pfx::parseDocument(
        R"({"format":"pixelfx","kind":"batch","version":6,"body":{"output":{"colorSpace":"cmyk","sharpen":"max"}}})");
    REQUIRE(odd.ok());
    CHECK(odd.value().batch.colorSpace == "srgb");
    CHECK(odd.value().batch.sharpen == "none");
}
