// SPDX-License-Identifier: Apache-2.0
// Lens profiles (ADR-013), on a lens database written here: one camera and one lens with a
// strong vignetting and a barrel distortion, so the tests do not depend on the system's.
#include "fixtures.h"
#include "pfx/io.h"
#include "pfx/lens.h"
#include "pfx/pipeline.h"

#include <cstdlib>
#include <doctest/doctest.h>
#include <fstream>

namespace {

constexpr const char* kDatabase = R"(<lensdatabase version="1">
    <mount><name>Test mount</name></mount>
    <camera><maker>Testmaker</maker><model>Model 7</model><mount>Test mount</mount>
        <cropfactor>1.0</cropfactor></camera>
    <lens><maker>Testmaker</maker><model>Test 24mm f/2.8</model><mount>Test mount</mount>
        <cropfactor>1.0</cropfactor><focal value="24"/><aperture min="2.8" max="22"/>
        <calibration>
            <distortion model="ptlens" focal="24" a="0" b="-0.08" c="0"/>
            <vignetting model="pa" focal="24" aperture="2.8" distance="1000" k1="-0.5" k2="0" k3="0"/>
        </calibration>
    </lens>
</lensdatabase>
)";

const pfx::LensFacts kFacts{"Testmaker", "Model 7", "Test 24mm f/2.8", 24.0, 2.8};

// The first column of `row` brighter than the middle of 0.1 and 0.5.
int edgeAt(const pfx::Image& image, int row)
{
    for (int x = 0; x < image.width(); ++x) {
        if (image.pixel(x, row)[1] > 0.3F) {
            return x;
        }
    }
    return -1;
}

// Dark on the left fifth, light on the rest: a straight vertical edge.
pfx::Image edgePicture(int width, int height)
{
    pfx::Image image = fixtures::flat(0.5F, 0.5F, 0.5F, width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width / 5; ++x) {
            float* p = image.pixel(x, y);
            p[0] = p[1] = p[2] = 0.1F;
        }
    }
    return image;
}

} // namespace

TEST_CASE("lens profiles: the photo's lens is found, its vignetting and distortion undone")
{
    const fixtures::TempDir dir;
    std::ofstream(dir.path() / "test.xml") << kDatabase;
    pfx::setLensDatabaseFolder(dir.path());
    if (!pfx::lensProfilesAvailable()) {
        MESSAGE("built without lensfun: lens profiles are off");
        CHECK(pfx::lensProfileName(kFacts).empty());
        pfx::setLensDatabaseFolder({});
        return;
    }
    CHECK(pfx::lensProfileName(kFacts) == "Test 24mm f/2.8");
    CHECK(pfx::lensProfileName({"Testmaker", "Model 7", "Other 85mm f/1.4", 85.0, 1.4}).empty());
    CHECK(pfx::lensProfileName({"Nobody", "Nothing", "", 0.0, 0.0}).empty());

    pfx::RenderOptions withLens;
    withLens.lens = &kFacts;
    const pfx::Image grey = fixtures::flat(0.2F, 0.2F, 0.2F, 300, 200);
    pfx::Settings vignetting;
    vignetting.set("detail.lensProfile.enable", true);
    vignetting.set("detail.lensProfile.distortion", 0.0);
    const auto brighter = pfx::render(grey, vignetting, withLens);
    REQUIRE(brighter.ok());
    CHECK(brighter.value().pixel(150, 100)[1] == doctest::Approx(0.2).epsilon(0.02));
    CHECK(brighter.value().pixel(1, 1)[1] > 0.3F);
    // Half the amount, part of the way; without the photo's lens, nothing.
    vignetting.set("detail.lensProfile.vignetting", 50.0);
    const auto half = pfx::render(grey, vignetting, withLens);
    REQUIRE(half.ok());
    CHECK(half.value().pixel(1, 1)[1] > 0.21F);
    CHECK(half.value().pixel(1, 1)[1] < brighter.value().pixel(1, 1)[1]);
    CHECK(pfx::render(grey, vignetting).value() == grey);

    // Barrel distortion bows a straight edge; the correction moves it, more at the top than in
    // the middle, and a half-size preview the same way.
    pfx::Settings distortion;
    distortion.set("detail.lensProfile.enable", true);
    distortion.set("detail.lensProfile.vignetting", 0.0);
    const auto full = pfx::render(edgePicture(600, 400), distortion, withLens);
    const auto small = pfx::render(edgePicture(300, 200), distortion, withLens);
    REQUIRE(full.ok());
    REQUIRE(small.ok());
    const int top = edgeAt(full.value(), 0);
    const int middle = edgeAt(full.value(), 200);
    CHECK(middle != 120);
    CHECK(std::abs(top - middle) >= 3);
    CHECK(std::abs((edgeAt(small.value(), 0) * 2) - top) <= 2);
    CHECK(std::abs((edgeAt(small.value(), 100) * 2) - middle) <= 2);
    pfx::setLensDatabaseFolder({});
}

TEST_CASE("lens facts come from Exif")
{
    pfx::ExifSummary exif;
    exif.make = "Testmaker";
    exif.model = "Model 7";
    exif.lens = "Test 24mm f/2.8";
    exif.focalLength = 24.0;
    exif.fNumber = 4.0;
    const pfx::LensFacts facts = pfx::lensFacts(exif);
    CHECK(facts == pfx::LensFacts{"Testmaker", "Model 7", "Test 24mm f/2.8", 24.0, 4.0});
}

TEST_CASE("golden image: the lens profile's output is unchanged")
{
    const fixtures::TempDir dir;
    std::ofstream(dir.path() / "test.xml") << kDatabase;
    pfx::setLensDatabaseFolder(dir.path());
    if (!pfx::lensProfilesAvailable()) {
        pfx::setLensDatabaseFolder({});
        return;
    }
    pfx::Settings on;
    on.set("detail.lensProfile.enable", true);
    pfx::RenderOptions withLens;
    withLens.lens = &kFacts;
    const auto rendered = pfx::render(fixtures::scene(), on, withLens);
    REQUIRE(rendered.ok());
    const std::filesystem::path golden = std::filesystem::path(PFX_GOLDEN_DIR) / "lens_profile.png";
    pfx::SaveOptions sixteen;
    sixteen.bitsPerSample = 16;
    if (std::getenv("PFX_GOLDEN_UPDATE") != nullptr) {
        REQUIRE(pfx::save(rendered.value(), golden, sixteen).ok());
    }
    const auto expected = pfx::load(golden);
    REQUIRE_MESSAGE(expected.ok(), "missing golden image; run with PFX_GOLDEN_UPDATE=1 once and review it");
    // Through a 16-bit PNG, as the golden file went.
    REQUIRE(pfx::save(rendered.value(), dir.path() / "actual.png", sixteen).ok());
    const auto actual = pfx::load(dir.path() / "actual.png");
    REQUIRE(actual.ok());
    CHECK(fixtures::compare(actual.value().image, expected.value().image).max < 2e-3);
    pfx::setLensDatabaseFolder({});
}
