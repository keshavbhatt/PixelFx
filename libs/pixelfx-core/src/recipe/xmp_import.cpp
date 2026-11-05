// SPDX-License-Identifier: Apache-2.0
// Lightroom / Camera Raw presets: the crs: settings of an XMP packet, read with a few regular
// expressions (no XML library: the packets are small and regular, and anything unexpected is
// simply not mapped).
#include "pfx/xmp_import.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace pfx {

namespace {

std::optional<double> number(const std::string& text)
{
    std::string t = text;
    std::erase(t, '+');
    std::istringstream in(t);
    in.imbue(std::locale::classic());
    double v = 0;
    in >> v;
    if (in.fail() || !std::isfinite(v)) {
        return std::nullopt;
    }
    return v;
}

// The crs: values of the packet: attributes (crs:Exposure2012="+0.35") and simple elements
// (<crs:Exposure2012>0.35</crs:Exposure2012>).
std::map<std::string, std::string> scalars(const std::string& text)
{
    std::map<std::string, std::string> out;
    static const std::regex kAttribute(R"re(crs:([A-Za-z0-9]+)\s*=\s*"([^"]*)")re");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), kAttribute); it != std::sregex_iterator();
         ++it) {
        out[(*it)[1].str()] = (*it)[2].str();
    }
    static const std::regex kElement(R"(<crs:([A-Za-z0-9]+)>([^<]*)</crs:\1>)");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), kElement); it != std::sregex_iterator();
         ++it) {
        out[(*it)[1].str()] = (*it)[2].str();
    }
    return out;
}

// <crs:Key><rdf:Seq><rdf:li>a</rdf:li>...</rdf:Seq></crs:Key>, and the first rdf:li of an
// rdf:Alt (names).
std::map<std::string, std::vector<std::string>> lists(const std::string& text)
{
    std::map<std::string, std::vector<std::string>> out;
    static const std::regex kList(
        R"(<crs:([A-Za-z0-9]+)>\s*<rdf:(?:Seq|Alt|Bag)>([\s\S]*?)</rdf:(?:Seq|Alt|Bag)>\s*</crs:\1>)");
    static const std::regex kItem(R"(<rdf:li[^>]*>([^<]*)</rdf:li>)");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), kList); it != std::sregex_iterator();
         ++it) {
        std::vector<std::string> items;
        const std::string body = (*it)[2].str();
        for (auto li = std::sregex_iterator(body.begin(), body.end(), kItem); li != std::sregex_iterator();
             ++li) {
            items.push_back((*li)[1].str());
        }
        out[(*it)[1].str()] = std::move(items);
    }
    return out;
}

std::optional<Curve> curveFrom(const std::vector<std::string>& points)
{
    Curve c;
    for (const std::string& p : points) {
        const auto comma = p.find(',');
        if (comma == std::string::npos) {
            return std::nullopt;
        }
        const auto x = number(p.substr(0, comma));
        const auto y = number(p.substr(comma + 1));
        if (!x || !y) {
            return std::nullopt;
        }
        c.push_back({std::clamp(*x / 255.0, 0.0, 1.0), std::clamp(*y / 255.0, 0.0, 1.0)});
    }
    if (c.size() < 2 || c == Curve{{0, 0}, {1, 1}}) {
        return std::nullopt;
    }
    return c;
}

// crs: settings that are about the preset itself, not the photo: never reported. Lightroom's
// own lens profile names are among them: PixelFx finds each photo's lens itself (ADR-013).
const std::set<std::string>& bookkeeping()
{
    static const std::set<std::string> keys = {"Version",
                                               "ProcessVersion",
                                               "PresetType",
                                               "UUID",
                                               "SupportsAmount",
                                               "SupportsAmount2",
                                               "SupportsColor",
                                               "SupportsMonochrome",
                                               "SupportsHighDynamicRange",
                                               "SupportsNormalDynamicRange",
                                               "SupportsSceneReferred",
                                               "SupportsOutputReferred",
                                               "CameraModelRestriction",
                                               "Copyright",
                                               "ContactInfo",
                                               "Name",
                                               "ShortName",
                                               "SortName",
                                               "Group",
                                               "Description",
                                               "HasSettings",
                                               "AlreadyApplied",
                                               "RequiresRGBTables",
                                               "CompatibleVersion",
                                               "WhiteBalance",
                                               "ToneCurveName2012",
                                               "ToneCurveName",
                                               "CameraProfile",
                                               "CameraProfileDigest",
                                               "OverrideLookVignette",
                                               "HasCrop",
                                               "Look",
                                               "Amount",
                                               "RequiresRenditionBehavior",
                                               "ProfileName",
                                               "LensProfileSetup",
                                               "LensProfileName",
                                               "LensProfileFilename",
                                               "LensProfileDigest",
                                               "LensProfileIsEmbedded"};
    return keys;
}

bool neutral(const std::string& value)
{
    if (value.empty() || value == "False" || value == "false") {
        return true;
    }
    const auto v = number(value);
    return v && *v == 0.0;
}

constexpr std::array<std::pair<const char*, const char*>, 8> kBands = {{{"Red", "red"},
                                                                        {"Orange", "orange"},
                                                                        {"Yellow", "yellow"},
                                                                        {"Green", "green"},
                                                                        {"Aqua", "aqua"},
                                                                        {"Blue", "blue"},
                                                                        {"Purple", "purple"},
                                                                        {"Magenta", "magenta"}}};

// crs: name -> PixelFx key and a scale (value * scale), by panel group.
using KeyMap = std::map<std::string, std::pair<std::string, double>>;

void addLightAndColorKeys(KeyMap& m)
{
    m["Exposure2012"] = {"light.exposure", 1.0};
    m["Contrast2012"] = {"light.contrast", 1.0};
    m["Highlights2012"] = {"light.highlights", 1.0};
    m["Shadows2012"] = {"light.shadows", 1.0};
    m["Whites2012"] = {"light.whites", 1.0};
    m["Blacks2012"] = {"light.blacks", 1.0};
    m["Vibrance"] = {"color.vibrance", 1.0};
    m["Saturation"] = {"color.saturation", 1.0};
    m["IncrementalTemperature"] = {"color.wb.temperature", 1.0};
    m["IncrementalTint"] = {"color.wb.tint", 100.0 / 150.0};
    for (const auto& [lr, ours] : kBands) {
        m[std::string("HueAdjustment") + lr] = {std::string("mixer.hue.") + ours, 1.0};
        m[std::string("SaturationAdjustment") + lr] = {std::string("mixer.saturation.") + ours, 1.0};
        m[std::string("LuminanceAdjustment") + lr] = {std::string("mixer.luminance.") + ours, 1.0};
        m[std::string("GrayMixer") + lr] = {std::string("bw.mix.") + ours, 1.0};
    }
}

void addGradingKeys(KeyMap& m)
{
    m["SplitToningShadowHue"] = {"grading.shadows.hue", 1.0};
    m["SplitToningShadowSaturation"] = {"grading.shadows.amount", 1.0};
    m["SplitToningHighlightHue"] = {"grading.highlights.hue", 1.0};
    m["SplitToningHighlightSaturation"] = {"grading.highlights.amount", 1.0};
    m["SplitToningBalance"] = {"grading.balance", 1.0};
    m["ColorGradeShadowHue"] = {"grading.shadows.hue", 1.0};
    m["ColorGradeShadowSat"] = {"grading.shadows.amount", 1.0};
    m["ColorGradeHighlightHue"] = {"grading.highlights.hue", 1.0};
    m["ColorGradeHighlightSat"] = {"grading.highlights.amount", 1.0};
    m["ColorGradeMidtoneHue"] = {"grading.midtones.hue", 1.0};
    m["ColorGradeMidtoneSat"] = {"grading.midtones.amount", 1.0};
    m["ColorGradeGlobalHue"] = {"grading.global.hue", 1.0};
    m["ColorGradeGlobalSat"] = {"grading.global.amount", 1.0};
    m["ColorGradeShadowLum"] = {"grading.shadows.luminance", 1.0};
    m["ColorGradeMidtoneLum"] = {"grading.midtones.luminance", 1.0};
    m["ColorGradeHighlightLum"] = {"grading.highlights.luminance", 1.0};
    m["ColorGradeGlobalLum"] = {"grading.global.luminance", 1.0};
    m["ColorGradeBlending"] = {"grading.blending", 1.0};
    m["ColorGradeBalance"] = {"grading.balance", 1.0};
}

void addDetailKeys(KeyMap& m)
{
    m["Clarity2012"] = {"detail.clarity", 1.0};
    m["Dehaze"] = {"detail.dehaze", 1.0};
    m["Sharpness"] = {"detail.sharpen.amount", 1.0};
    m["SharpenEdgeMasking"] = {"detail.sharpen.masking", 1.0};
    m["LuminanceSmoothing"] = {"detail.denoise.luminance", 1.0};
    m["ColorNoiseReduction"] = {"detail.denoise.color", 1.0};
    m["Texture"] = {"detail.texture", 1.0};
    m["SharpenDetail"] = {"detail.sharpen.detail", 1.0};
    m["LuminanceNoiseReductionDetail"] = {"detail.denoise.luminanceDetail", 1.0};
    m["LuminanceNoiseReductionContrast"] = {"detail.denoise.luminanceContrast", 1.0};
    m["ColorNoiseReductionDetail"] = {"detail.denoise.colorDetail", 1.0};
    m["ColorNoiseReductionSmoothness"] = {"detail.denoise.colorSmoothness", 1.0};
    m["DefringePurpleAmount"] = {"detail.defringe.purple.amount", 1.0};
    m["DefringePurpleHueLo"] = {"detail.defringe.purple.hueLow", 1.0};
    m["DefringePurpleHueHi"] = {"detail.defringe.purple.hueHigh", 1.0};
    m["DefringeGreenAmount"] = {"detail.defringe.green.amount", 1.0};
    m["DefringeGreenHueLo"] = {"detail.defringe.green.hueLow", 1.0};
    m["DefringeGreenHueHi"] = {"detail.defringe.green.hueHigh", 1.0};
}

void addEffectsKeys(KeyMap& m)
{
    m["PostCropVignetteAmount"] = {"effects.vignette.amount", 1.0};
    m["PostCropVignetteMidpoint"] = {"effects.vignette.midpoint", 1.0};
    m["PostCropVignetteRoundness"] = {"effects.vignette.roundness", 1.0};
    m["GrainAmount"] = {"effects.grain.amount", 1.0};
    m["GrainSize"] = {"effects.grain.size", 1.0};
    m["PostCropVignetteFeather"] = {"effects.vignette.feather", 1.0};
    m["PostCropVignetteHighlightContrast"] = {"effects.vignette.highlights", 1.0};
    m["GrainRoughness"] = {"effects.grain.roughness", 1.0};
}

void addCalibrationAndCurveKeys(KeyMap& m)
{
    m["ShadowTint"] = {"calibration.shadows.tint", 1.0};
    m["RedHue"] = {"calibration.red.hue", 1.0};
    m["RedSaturation"] = {"calibration.red.saturation", 1.0};
    m["GreenHue"] = {"calibration.green.hue", 1.0};
    m["GreenSaturation"] = {"calibration.green.saturation", 1.0};
    m["BlueHue"] = {"calibration.blue.hue", 1.0};
    m["BlueSaturation"] = {"calibration.blue.saturation", 1.0};
    m["ParametricShadows"] = {"curves.parametric.shadows", 1.0};
    m["ParametricDarks"] = {"curves.parametric.darks", 1.0};
    m["ParametricLights"] = {"curves.parametric.lights", 1.0};
    m["ParametricHighlights"] = {"curves.parametric.highlights", 1.0};
    m["ParametricShadowSplit"] = {"curves.parametric.split.shadows", 1.0};
    m["ParametricMidtoneSplit"] = {"curves.parametric.split.midtones", 1.0};
    m["ParametricHighlightSplit"] = {"curves.parametric.split.highlights", 1.0};
    m["LensProfileDistortionScale"] = {"detail.lensProfile.distortion", 1.0};
    m["LensProfileVignettingScale"] = {"detail.lensProfile.vignetting", 1.0};
}

const KeyMap& direct()
{
    static const KeyMap map = [] {
        KeyMap m;
        addLightAndColorKeys(m);
        addGradingKeys(m);
        addDetailKeys(m);
        addEffectsKeys(m);
        addCalibrationAndCurveKeys(m);
        return m;
    }();
    return map;
}

} // namespace

namespace {

// The crs: values that need more than a scale. False when `key` is not one of them.
bool mapWhiteBalance(const std::string& key, const std::optional<double>& v, Settings& s)
{
    if (key == "Temperature") {
        // Absolute Kelvin (raw presets): as a shift from daylight, the way the slider reads.
        if (v && *v > 1000) {
            s.set("color.wb.temperature", std::clamp(100.0 * std::log2(*v / 6504.0), -100.0, 100.0));
        }
        return true;
    }
    if (key == "Tint") {
        if (v) {
            s.set("color.wb.tint", std::clamp(*v * 100.0 / 150.0, -100.0, 100.0));
        }
        return true;
    }
    return false;
}

bool mapSpecial(const std::string& key, const std::string& value, Settings& s)
{
    const auto v = number(value);
    if (mapWhiteBalance(key, v, s)) {
        return true;
    }
    if (key == "Treatment" || key == "ConvertToGrayscale") {
        // Colour switches black and white off, so a colour preset turns a black-and-white photo
        // back; either key saying black and white wins.
        if (value == "Black & White" || value == "Monochrome" || value == "True") {
            s.set("bw.enabled", true);
        } else if (!s.boolean("bw.enabled", false)) {
            s.set("bw.enabled", false);
        }
    } else if (key == "AutoLateralCA") {
        if (v && *v != 0.0) {
            s.set("detail.ca.auto", true);
        }
    } else if (key == "LensProfileEnable") {
        if (v && *v != 0.0) {
            s.set("detail.lensProfile.enable", true);
        }
    } else if (key == "PostCropVignetteStyle") {
        // 1 highlight priority, 2 colour priority, 3 paint overlay.
        if (v && *v == 2.0) {
            s.set("effects.vignette.style", std::string("color"));
        } else if (v && *v == 3.0) {
            s.set("effects.vignette.style", std::string("paint"));
        }
    } else if (key == "SharpenRadius") {
        // Pixels on the camera's photo; PixelFx sizes are shares of the long edge (a 6000 px one).
        if (v) {
            s.set("detail.sharpen.radius", std::clamp(*v / 6000.0, 0.0001, 0.003));
        }
    } else {
        return false;
    }
    return true;
}

// One crs: value: mapped into `s`, or named in `skipped` when it changes something PixelFx
// cannot do.
void mapScalar(const std::string& key, const std::string& value, Settings& s,
               std::vector<std::string>& skipped)
{
    if (const auto it = direct().find(key); it != direct().end()) {
        if (const auto v = number(value)) {
            s.set(it->second.first, *v * it->second.second);
        }
        return;
    }
    if (mapSpecial(key, value, s)) {
        return;
    }
    if (!bookkeeping().contains(key) && !neutral(value)) {
        skipped.push_back(key);
    }
}

void mapLists(const std::map<std::string, std::vector<std::string>>& seqs, Settings& s,
              std::vector<std::string>& skipped)
{
    const std::array<std::pair<const char*, const char*>, 4> curves = {{{"ToneCurvePV2012", "curves.rgb"},
                                                                        {"ToneCurvePV2012Red", "curves.r"},
                                                                        {"ToneCurvePV2012Green", "curves.g"},
                                                                        {"ToneCurvePV2012Blue", "curves.b"}}};
    for (const auto& [lr, ours] : curves) {
        if (const auto it = seqs.find(lr); it != seqs.end()) {
            if (auto curve = curveFrom(it->second)) {
                s.set(ours, std::move(*curve));
            }
        }
    }
    for (const auto& [key, items] : seqs) {
        const bool known =
            key == "Name" || key == "Group" || key == "Description" || key.starts_with("ToneCurve");
        if (!known && !items.empty()) {
            skipped.push_back(key);
        }
    }
}

// The camera profile (<crs:Look crs:Name="...">): Adobe's standard ones are PixelFx's own
// rendering, Monochrome is black and white, Vivid the vivid profile; others are reported.
void mapProfile(const std::string& packet, Settings& s, std::vector<std::string>& skipped)
{
    const auto look = packet.find("<crs:Look>");
    if (look == std::string::npos) {
        return;
    }
    static const std::regex kName(R"re(crs:Name\s*=\s*"([^"]*)")re");
    std::smatch match;
    const std::string block = packet.substr(look, packet.find("</crs:Look>", look) - look);
    if (!std::regex_search(block, match, kName)) {
        return;
    }
    const std::string name = match[1].str();
    static const std::set<std::string> standard = {"Adobe Color",     "Adobe Standard", "Adobe Neutral",
                                                   "Adobe Landscape", "Adobe Portrait", "Camera Standard"};
    if (name == "Adobe Monochrome" || name == "Camera Monochrome") {
        s.set("bw.enabled", true);
    } else if (name == "Adobe Vivid" || name == "Camera Vivid") {
        s.set("color.profile", std::string("vivid"));
    } else if (!standard.contains(name)) {
        skipped.push_back("Profile: " + name);
    }
}

std::string firstItem(const std::map<std::string, std::vector<std::string>>& seqs, const std::string& key)
{
    const auto it = seqs.find(key);
    return (it == seqs.end() || it->second.empty()) ? std::string() : it->second.front();
}

} // namespace

Result<XmpImport> importXmpPreset(std::string_view text)
{
    const std::string packet(text);
    if (packet.find("crs:") == std::string::npos) {
        return Error{ErrorCode::ParseError,
                     "This is not a Lightroom or Camera Raw preset: it has no crs: settings."};
    }
    const auto seqs = lists(packet);
    XmpImport out;
    out.document.kind = DocumentKind::Preset;
    out.document.name = firstItem(seqs, "Name");
    out.document.group = firstItem(seqs, "Group");
    for (const auto& [key, value] : scalars(packet)) {
        mapScalar(key, value, out.document.settings, out.skipped);
    }
    mapLists(seqs, out.document.settings, out.skipped);
    mapProfile(packet, out.document.settings, out.skipped);
    // A fringe colour's hue range does nothing while its amount is 0.
    for (const auto& [lr, ours] : {std::pair{"Purple", "purple"}, std::pair{"Green", "green"}}) {
        if (out.document.settings.number(std::string("detail.defringe.") + ours + ".amount", 0.0) == 0.0) {
            const std::string hue = std::string("Defringe") + lr + "Hue";
            std::erase_if(out.skipped, [&](const std::string& k) { return k.starts_with(hue); });
        }
    }
    std::ranges::sort(out.skipped);
    const auto [first, last] = std::ranges::unique(out.skipped);
    out.skipped.erase(first, last);
    return out;
}

Result<XmpImport> importXmpPresetFile(const std::filesystem::path& path)
{
    const std::ifstream in(path, std::ios::binary);
    if (!in) {
        return Error{ErrorCode::NotFound, "The file " + path.string() + " could not be read."};
    }
    std::ostringstream text;
    text << in.rdbuf();
    auto result = importXmpPreset(text.str());
    if (result && result.value().document.name.empty()) {
        result.value().document.name = path.stem().string();
    }
    return result;
}

} // namespace pfx
