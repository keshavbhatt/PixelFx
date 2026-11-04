// SPDX-License-Identifier: Apache-2.0
#include "pfx/xmp_import.h"

#include <algorithm>
#include <doctest/doctest.h>

namespace {

// Shaped like a Lightroom Classic preset (attributes for numbers, elements for lists).
const char* const kPreset = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about="" xmlns:crs="http://ns.adobe.com/camera-raw-settings/1.0/"
   crs:PresetType="Normal" crs:Version="15.0" crs:ProcessVersion="11.0"
   crs:Exposure2012="+0.35" crs:Contrast2012="+18" crs:Highlights2012="-35" crs:Shadows2012="+28"
   crs:Whites2012="0" crs:Blacks2012="-10" crs:IncrementalTemperature="+14" crs:IncrementalTint="-15"
   crs:Vibrance="+22" crs:Saturation="0" crs:Clarity2012="+15" crs:Dehaze="+5" crs:Texture="+20"
   crs:HueAdjustmentYellow="-8" crs:SaturationAdjustmentGreen="-30"
   crs:SplitToningShadowHue="200" crs:SplitToningShadowSaturation="20" crs:SplitToningBalance="+10"
   crs:PostCropVignetteAmount="-22" crs:GrainAmount="0" crs:LensProfileEnable="1" crs:SharpenRadius="+1.0">
   <crs:Name><rdf:Alt><rdf:li xml:lang="x-default">Clean Travel</rdf:li></rdf:Alt></crs:Name>
   <crs:ToneCurvePV2012><rdf:Seq><rdf:li>0, 10</rdf:li><rdf:li>128, 120</rdf:li><rdf:li>255, 250</rdf:li></rdf:Seq></crs:ToneCurvePV2012>
   <crs:ToneCurvePV2012Red><rdf:Seq><rdf:li>0, 0</rdf:li><rdf:li>255, 255</rdf:li></rdf:Seq></crs:ToneCurvePV2012Red>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>)";

} // namespace

TEST_CASE("xmp import: the settings PixelFx has are mapped")
{
    const auto r = pfx::importXmpPreset(kPreset);
    REQUIRE(r.ok());
    const pfx::Settings& s = r.value().document.settings;
    CHECK(r.value().document.name == "Clean Travel");
    CHECK(s.number("light.exposure", 0) == doctest::Approx(0.35));
    CHECK(s.number("light.highlights", 0) == -35.0);
    CHECK(s.contains("light.whites")); // zero, but part of the preset: the Light unit is included
    CHECK(s.number("color.wb.temperature", 0) == 14.0);
    CHECK(s.number("color.wb.tint", 0) == doctest::Approx(-10.0));
    CHECK(s.number("mixer.hue.yellow", 0) == -8.0);
    CHECK(s.number("mixer.saturation.green", 0) == -30.0);
    CHECK(s.number("grading.shadows.hue", 0) == 200.0);
    CHECK(s.number("effects.vignette.amount", 0) == -22.0);
    CHECK(s.number("detail.sharpen.radius", 0) == doctest::Approx(1.0 / 6000));
    const pfx::Curve rgb = s.curve("curves.rgb");
    REQUIRE(rgb.size() == 3);
    CHECK(rgb[0].y == doctest::Approx(10.0 / 255));
    CHECK_FALSE(s.contains("curves.r")); // a straight curve is no curve
}

TEST_CASE("xmp import: what has no equivalent is reported, neutral values are not")
{
    const auto r = pfx::importXmpPreset(kPreset);
    REQUIRE(r.ok());
    const auto& skipped = r.value().skipped;
    CHECK(std::ranges::find(skipped, "Texture") != skipped.end());
    CHECK(std::ranges::find(skipped, "LensProfileEnable") != skipped.end());
    CHECK(std::ranges::find(skipped, "Version") == skipped.end());
    CHECK(std::ranges::find(skipped, "GrainAmount") == skipped.end()); // mapped
}

TEST_CASE("xmp import: absolute white balance becomes a shift, other files are refused")
{
    const auto r = pfx::importXmpPreset(R"(<x crs:Temperature="9200" crs:Tint="+30"/>)");
    REQUIRE(r.ok());
    CHECK(r.value().document.settings.number("color.wb.temperature", 0) ==
          doctest::Approx(50.0).epsilon(0.02));
    CHECK(r.value().document.settings.number("color.wb.tint", 0) == doctest::Approx(20.0));
    CHECK_FALSE(pfx::importXmpPreset("<x:xmpmeta/>").ok());
}
