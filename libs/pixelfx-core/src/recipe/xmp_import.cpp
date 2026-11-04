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

// crs: settings that are about the preset itself, not the photo: never reported.
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
                                               "Look"};
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

// crs: name -> PixelFx key and a scale (value * scale).
const std::map<std::string, std::pair<std::string, double>>& direct()
{
    static const std::map<std::string, std::pair<std::string, double>> map = [] {
        std::map<std::string, std::pair<std::string, double>> m = {
            {"Exposure2012", {"light.exposure", 1.0}},
            {"Contrast2012", {"light.contrast", 1.0}},
            {"Highlights2012", {"light.highlights", 1.0}},
            {"Shadows2012", {"light.shadows", 1.0}},
            {"Whites2012", {"light.whites", 1.0}},
            {"Blacks2012", {"light.blacks", 1.0}},
            {"Vibrance", {"color.vibrance", 1.0}},
            {"Saturation", {"color.saturation", 1.0}},
            {"IncrementalTemperature", {"color.wb.temperature", 1.0}},
            {"IncrementalTint", {"color.wb.tint", 100.0 / 150.0}},
            {"Clarity2012", {"detail.clarity", 1.0}},
            {"Dehaze", {"detail.dehaze", 1.0}},
            {"Sharpness", {"detail.sharpen.amount", 1.0}},
            {"SharpenEdgeMasking", {"detail.sharpen.masking", 1.0}},
            {"LuminanceSmoothing", {"detail.denoise.luminance", 1.0}},
            {"ColorNoiseReduction", {"detail.denoise.color", 1.0}},
            {"PostCropVignetteAmount", {"effects.vignette.amount", 1.0}},
            {"PostCropVignetteMidpoint", {"effects.vignette.midpoint", 1.0}},
            {"PostCropVignetteRoundness", {"effects.vignette.roundness", 1.0}},
            {"GrainAmount", {"effects.grain.amount", 1.0}},
            {"GrainSize", {"effects.grain.size", 1.0}},
            {"SplitToningShadowHue", {"grading.shadows.hue", 1.0}},
            {"SplitToningShadowSaturation", {"grading.shadows.amount", 1.0}},
            {"SplitToningHighlightHue", {"grading.highlights.hue", 1.0}},
            {"SplitToningHighlightSaturation", {"grading.highlights.amount", 1.0}},
            {"SplitToningBalance", {"grading.balance", 1.0}},
            {"ColorGradeShadowHue", {"grading.shadows.hue", 1.0}},
            {"ColorGradeShadowSat", {"grading.shadows.amount", 1.0}},
            {"ColorGradeHighlightHue", {"grading.highlights.hue", 1.0}},
            {"ColorGradeHighlightSat", {"grading.highlights.amount", 1.0}},
        };
        for (const auto& [lr, ours] : kBands) {
            m[std::string("HueAdjustment") + lr] = {std::string("mixer.hue.") + ours, 1.0};
            m[std::string("SaturationAdjustment") + lr] = {std::string("mixer.saturation.") + ours, 1.0};
            m[std::string("LuminanceAdjustment") + lr] = {std::string("mixer.luminance.") + ours, 1.0};
        }
        return m;
    }();
    return map;
}

} // namespace

namespace {

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
    const auto v = number(value);
    if (key == "Temperature") {
        // Absolute Kelvin (raw presets): as a shift from daylight, the way the slider reads.
        if (v && *v > 1000) {
            s.set("color.wb.temperature", std::clamp(100.0 * std::log2(*v / 6504.0), -100.0, 100.0));
        }
    } else if (key == "Tint") {
        if (v) {
            s.set("color.wb.tint", std::clamp(*v * 100.0 / 150.0, -100.0, 100.0));
        }
    } else if (key == "SharpenRadius") {
        // Pixels on the camera's photo; PixelFx sizes are shares of the long edge (a 6000 px one).
        if (v) {
            s.set("detail.sharpen.radius", std::clamp(*v / 6000.0, 0.0001, 0.003));
        }
    } else if (!bookkeeping().contains(key) && !neutral(value)) {
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
