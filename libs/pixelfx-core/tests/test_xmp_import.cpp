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
   crs:PostCropVignetteAmount="-22" crs:GrainAmount="0" crs:LensProfileEnable="1" crs:LensProfileVignettingScale="60"
   crs:LensProfileName="Canon EF 24-70mm f/2.8L II USM" crs:SharpenRadius="+1.0">
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
    CHECK(s.boolean("detail.lensProfile.enable", false));
    CHECK(s.number("detail.lensProfile.vignetting", 0) == 60.0);
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
    CHECK(std::ranges::find(skipped, "Texture") == skipped.end());           // mapped since P8
    CHECK(std::ranges::find(skipped, "LensProfileEnable") == skipped.end()); // mapped (ADR-013)
    CHECK(std::ranges::find(skipped, "LensProfileName") == skipped.end());
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

namespace {

// A black-and-white film preset as Lightroom Classic 13 writes one: the Monochrome treatment and
// profile, the B&W mix, full colour grading and the settings behind each panel's triangle.
const char* const kFullPreset = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about="" xmlns:crs="http://ns.adobe.com/camera-raw-settings/1.0/"
   crs:Treatment="Monochrome" crs:ConvertToGrayscale="True" crs:GrayMixerBlue="-40" crs:GrayMixerOrange="+25"
   crs:ColorGradeMidtoneHue="40" crs:ColorGradeMidtoneSat="12" crs:ColorGradeGlobalHue="210"
   crs:ColorGradeGlobalSat="5" crs:ColorGradeShadowLum="-10" crs:ColorGradeBlending="70"
   crs:ColorGradeBalance="-20" crs:Texture="+18" crs:GrainAmount="30" crs:GrainRoughness="65"
   crs:PostCropVignetteAmount="-15" crs:PostCropVignetteFeather="70" crs:PostCropVignetteStyle="2"
   crs:PostCropVignetteHighlightContrast="20" crs:SharpenDetail="40" crs:LuminanceNoiseReductionDetail="60"
   crs:ColorNoiseReductionSmoothness="70" crs:ShadowTint="+6" crs:RedHue="-10" crs:BlueSaturation="+15"
   crs:ParametricShadows="-12" crs:ParametricShadowSplit="20" crs:AutoLateralCA="1"
   crs:DefringePurpleAmount="0" crs:DefringePurpleHue="30" crs:LensProfileSetup="LensDefaults">
   <crs:Look><rdf:Description crs:Name="Adobe Monochrome" crs:Amount="1.000000"/></crs:Look>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>)";

} // namespace

TEST_CASE("xmp import: black and white, full colour grading and the secondary settings are mapped")
{
    const auto r = pfx::importXmpPreset(kFullPreset);
    REQUIRE(r.ok());
    const pfx::Settings& s = r.value().document.settings;
    CHECK(r.value().skipped.empty());
    CHECK(s.boolean("bw.enabled", false));
    CHECK(s.number("bw.mix.blue", 0) == -40.0);
    CHECK(s.number("grading.midtones.amount", 0) == 12.0);
    CHECK(s.number("grading.global.hue", 0) == 210.0);
    CHECK(s.number("grading.shadows.luminance", 0) == -10.0);
    CHECK(s.number("grading.blending", 0) == 70.0);
    CHECK(s.number("grading.balance", 0) == -20.0);
    CHECK(s.number("detail.texture", 0) == 18.0);
    CHECK(s.number("effects.grain.roughness", 0) == 65.0);
    CHECK(s.number("effects.vignette.feather", 0) == 70.0);
    CHECK(s.text("effects.vignette.style", "") == "color");
    CHECK(s.number("effects.vignette.highlights", 0) == 20.0);
    CHECK(s.number("detail.sharpen.detail", 0) == 40.0);
    CHECK(s.number("detail.denoise.luminanceDetail", 0) == 60.0);
    CHECK(s.number("detail.denoise.colorSmoothness", 0) == 70.0);
    CHECK(s.number("calibration.shadows.tint", 0) == 6.0);
    CHECK(s.number("calibration.red.hue", 0) == -10.0);
    CHECK(s.number("calibration.blue.saturation", 0) == 15.0);
    CHECK(s.number("curves.parametric.shadows", 0) == -12.0);
    CHECK(s.number("curves.parametric.split.shadows", 0) == 20.0);
    CHECK(s.boolean("detail.ca.auto", false));
}

TEST_CASE("xmp import: profiles map to black and white or vivid; unknown profiles are reported")
{
    const auto vivid = pfx::importXmpPreset(
        R"(<x crs:Exposure2012="0.1"><crs:Look><rdf:Description crs:Name="Adobe Vivid"/></crs:Look></x>)");
    REQUIRE(vivid.ok());
    CHECK(vivid.value().document.settings.text("color.profile", "") == "vivid");
    const auto colour = pfx::importXmpPreset(
        R"(<x crs:Treatment="Color"><crs:Look><rdf:Description crs:Name="Adobe Color"/></crs:Look></x>)");
    REQUIRE(colour.ok());
    CHECK(colour.value().skipped.empty());
    CHECK(colour.value().document.settings.contains("bw.enabled")); // colour: black and white off
    CHECK_FALSE(colour.value().document.settings.boolean("bw.enabled", true));
    const auto creative = pfx::importXmpPreset(
        R"(<x crs:Exposure2012="0.1"><crs:Look><rdf:Description crs:Name="Artistic 03"/></crs:Look></x>)");
    REQUIRE(creative.ok());
    CHECK(creative.value().skipped == std::vector<std::string>{"Profile: Artistic 03"});
}
