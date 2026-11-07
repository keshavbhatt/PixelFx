// SPDX-License-Identifier: Apache-2.0
#include "pfx/registry.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

namespace pfx {
namespace {

ParamSpec number(std::string key, double lo, double hi, double def = 0.0, std::string unit = {})
{
    ParamSpec p;
    p.key = std::move(key);
    p.type = ParamType::Number;
    p.minimum = lo;
    p.maximum = hi;
    p.defaultValue = def;
    p.unit = std::move(unit);
    return p;
}

ParamSpec relative(std::string key, double lo, double hi, double def)
{
    ParamSpec p = number(std::move(key), lo, hi, def, "long edge");
    p.relative = true;
    return p;
}

ParamSpec curve(std::string key)
{
    ParamSpec p;
    p.key = std::move(key);
    p.type = ParamType::Points;
    p.minimum = 0.0;
    p.maximum = 1.0;
    return p;
}

ParamSpec boolean(std::string key)
{
    ParamSpec p;
    p.key = std::move(key);
    p.type = ParamType::Boolean;
    return p;
}

ParamSpec fraction(std::string key, double def)
{
    ParamSpec p = number(std::move(key), 0.0, 1.0, def, "fraction");
    p.relative = true;
    return p;
}

ParamSpec choice(std::string key, std::vector<std::string> choices)
{
    ParamSpec p;
    p.key = std::move(key);
    p.type = ParamType::Choice;
    p.choices = std::move(choices);
    return p;
}

void addHeal(std::vector<OpSpec>& ops)
{
    ParamSpec spots;
    spots.key = "heal.spots";
    spots.type = ParamType::SpotList;
    // First of all, on the upright photo, so a spot's position never depends on the crop.
    ops.push_back({"heal", "heal", 1, 5, false, {spots}, false});
}

void addGeometry(std::vector<OpSpec>& ops)
{
    ops.push_back({"geometry",
                   "geometry",
                   1,
                   10,
                   false,
                   {number("geometry.rotate90", 0, 3), boolean("geometry.flipH"), boolean("geometry.flipV"),
                    number("geometry.angle", -45, 45, 0, "degrees"), fraction("geometry.crop.x", 0.0),
                    fraction("geometry.crop.y", 0.0), fraction("geometry.crop.w", 1.0),
                    fraction("geometry.crop.h", 1.0), number("geometry.perspective.vertical", -100, 100),
                    number("geometry.perspective.horizontal", -100, 100),
                    fraction("geometry.perspective.corners.topLeft.x", 0.0),
                    fraction("geometry.perspective.corners.topLeft.y", 0.0),
                    fraction("geometry.perspective.corners.topRight.x", 1.0),
                    fraction("geometry.perspective.corners.topRight.y", 0.0),
                    fraction("geometry.perspective.corners.bottomRight.x", 1.0),
                    fraction("geometry.perspective.corners.bottomRight.y", 1.0),
                    fraction("geometry.perspective.corners.bottomLeft.x", 0.0),
                    fraction("geometry.perspective.corners.bottomLeft.y", 1.0),
                    choice("geometry.perspective.keep", {"whole", "inside"})},
                   false});
}

void addMixer(std::vector<OpSpec>& ops)
{
    std::vector<ParamSpec> params;
    for (const char* mode : {"hue", "saturation", "luminance"}) {
        for (const char* band : {"red", "orange", "yellow", "green", "aqua", "blue", "purple", "magenta"}) {
            params.push_back(number(std::string("mixer.") + mode + "." + band, -100, 100));
        }
    }
    ops.push_back({"mixer", "mixer", 1, 80, true, std::move(params)});
}

// Version 2 added midtones, global, a luminance per wheel and blending.
void addGrading(std::vector<OpSpec>& ops)
{
    std::vector<ParamSpec> params;
    for (const char* zone : {"shadows", "midtones", "highlights", "global"}) {
        const std::string key = std::string("grading.") + zone;
        params.push_back(number(key + ".hue", 0, 360, 0, "degrees"));
        params.push_back(number(key + ".amount", 0, 100));
        params.push_back(number(key + ".luminance", -100, 100));
    }
    params.push_back(number("grading.balance", -100, 100));
    params.push_back(number("grading.blending", 0, 100, 50));
    ops.push_back({"grading", "grading", 2, 90, true, std::move(params)});
}

// Black and white: the treatment and how bright each colour band turns (Lightroom's B&W mix).
void addBlackAndWhite(std::vector<OpSpec>& ops)
{
    std::vector<ParamSpec> params = {boolean("bw.enabled")};
    for (const char* band : {"red", "orange", "yellow", "green", "aqua", "blue", "purple", "magenta"}) {
        params.push_back(number(std::string("bw.mix.") + band, -100, 100));
    }
    ops.push_back({"bw", "bw", 1, 82, true, std::move(params)});
}

// Calibration: moves the red, green and blue primaries and tints the shadows, as a camera
// profile would; neutrals stay neutral.
void addCalibration(std::vector<OpSpec>& ops)
{
    std::vector<ParamSpec> params = {number("calibration.shadows.tint", -100, 100)};
    for (const char* primary : {"red", "green", "blue"}) {
        params.push_back(number(std::string("calibration.") + primary + ".hue", -100, 100));
        params.push_back(number(std::string("calibration.") + primary + ".saturation", -100, 100));
    }
    ops.push_back({"calibration", "calibration", 1, 32, true, std::move(params)});
}

// Lens corrections: the lens's own profile (ADR-013; amounts 100 for the profile as measured),
// then those that need none: lateral chromatic aberration found from the photo itself, and
// purple and green fringes along high-contrast edges.
void addLensCorrections(std::vector<OpSpec>& ops)
{
    ops.push_back(
        {"lens_profile",
         "detail",
         1,
         2,
         false,
         {boolean("detail.lensProfile.enable"), number("detail.lensProfile.distortion", 0, 200, 100),
          number("detail.lensProfile.vignetting", 0, 200, 100)},
         false});
    ops.push_back({"chromatic_aberration", "detail", 1, 3, false, {boolean("detail.ca.auto")}});
    ops.push_back(
        {"defringe",
         "detail",
         1,
         4,
         false,
         {number("detail.defringe.purple.amount", 0, 20), number("detail.defringe.purple.hueLow", 0, 100, 30),
          number("detail.defringe.purple.hueHigh", 0, 100, 70), number("detail.defringe.green.amount", 0, 20),
          number("detail.defringe.green.hueLow", 0, 100, 40),
          number("detail.defringe.green.hueHigh", 0, 100, 60)}});
}

// Creative operations for looks (FEATURES E9), after colour grading and before a look's own
// place: amounts 0 to 100, sizes shares of the long edge.
void addCreative(std::vector<OpSpec>& ops)
{
    ops.push_back({"blur", "creative", 1, 91, false, {relative("creative.blur.radius", 0.0, 0.01, 0.0)}});
    ops.push_back(
        {"pixelate", "creative", 1, 92, false, {relative("creative.pixelate.size", 0.0, 0.01, 0.0)}});
    ops.push_back({"emboss", "creative", 1, 93, false, {number("creative.emboss.amount", 0, 100)}});
    ops.push_back({"edges", "creative", 1, 94, false, {number("creative.edges.amount", 0, 100)}});
    ops.push_back(
        {"colorize",
         "creative",
         1,
         95,
         true,
         {number("creative.colorize.amount", 0, 100), number("creative.colorize.hue", 0, 360, 30, "degrees"),
          number("creative.colorize.saturation", 0, 100, 50)}});
    ops.push_back({"sepia", "creative", 1, 96, true, {number("creative.sepia.amount", 0, 100)}});
    ops.push_back(
        {"threshold",
         "creative",
         1,
         97,
         true,
         {number("creative.threshold.amount", 0, 100), number("creative.threshold.level", 0, 100, 50)}});
    ops.push_back({"invert", "creative", 1, 98, true, {number("creative.invert.amount", 0, 100)}});
}

void addEffects(std::vector<OpSpec>& ops)
{
    ops.push_back(
        {"vignette",
         "effects",
         2,
         120,
         false,
         {number("effects.vignette.amount", -100, 100), number("effects.vignette.midpoint", 0, 100, 50),
          number("effects.vignette.roundness", -100, 100), number("effects.vignette.feather", 0, 100, 50),
          choice("effects.vignette.style", {"highlight", "color", "paint"}),
          number("effects.vignette.highlights", 0, 100)}});
    ops.push_back({"grain",
                   "effects",
                   2,
                   130,
                   false,
                   {number("effects.grain.amount", 0, 100), number("effects.grain.size", 0, 100, 25),
                    number("effects.grain.roughness", 0, 100, 50)}});
    ops.push_back({"fade", "effects", 1, 140, true, {number("effects.fade", 0, 100)}});
}

void addLightAndColor(std::vector<OpSpec>& ops)
{
    ops.push_back({"white_balance",
                   "color",
                   1,
                   30,
                   true,
                   {choice("color.wb.mode", {"relative"}), number("color.wb.temperature", -100, 100),
                    number("color.wb.tint", -100, 100)}});
    // The base rendering a Lightroom profile stands for: standard, or vivid (more contrast and colour).
    ops.push_back(
        {"profile", "color", 1, 35, true, {choice("color.profile", {"standard", "vivid", "camera"})}});
    ops.push_back({"exposure", "light", 1, 40, true, {number("light.exposure", -5, 5, 0, "EV")}});
    ops.push_back({"tone",
                   "light",
                   1,
                   50,
                   true,
                   {number("light.contrast", -100, 100), number("light.highlights", -100, 100),
                    number("light.shadows", -100, 100), number("light.whites", -100, 100),
                    number("light.blacks", -100, 100)}});
    ops.push_back({"vibrance_saturation",
                   "color",
                   1,
                   85,
                   true,
                   {number("color.vibrance", -100, 100), number("color.saturation", -100, 100)}});
}

void addDetail(std::vector<OpSpec>& ops)
{
    ops.push_back({"denoise",
                   "detail",
                   2,
                   20,
                   false,
                   {number("detail.denoise.luminance", 0, 100), number("detail.denoise.color", 0, 100),
                    number("detail.denoise.luminanceDetail", 0, 100, 50),
                    number("detail.denoise.luminanceContrast", 0, 100),
                    number("detail.denoise.colorDetail", 0, 100, 50),
                    number("detail.denoise.colorSmoothness", 0, 100, 50)}});
    ops.push_back({"clarity", "detail", 1, 60, false, {number("detail.clarity", -100, 100)}});
    ops.push_back({"texture", "detail", 1, 62, false, {number("detail.texture", -100, 100)}});
    ops.push_back(
        {"sharpen",
         "detail",
         2,
         110,
         false,
         {number("detail.sharpen.amount", 0, 150), relative("detail.sharpen.radius", 0.0001, 0.003, 0.0004),
          number("detail.sharpen.masking", 0, 100), number("detail.sharpen.detail", 0, 100, 25)}});
    ops.push_back({"dehaze", "detail", 1, 45, false, {number("detail.dehaze", -100, 100)}});
}

void addCurves(std::vector<OpSpec>& ops)
{
    ops.push_back(
        {"curves",
         "curves",
         2,
         70,
         true,
         {curve("curves.rgb"), curve("curves.r"), curve("curves.g"), curve("curves.b"),
          number("curves.parametric.shadows", -100, 100), number("curves.parametric.darks", -100, 100),
          number("curves.parametric.lights", -100, 100), number("curves.parametric.highlights", -100, 100),
          number("curves.parametric.split.shadows", 5, 95, 25),
          number("curves.parametric.split.midtones", 5, 95, 50),
          number("curves.parametric.split.highlights", 5, 95, 75)}});
}

// Stages follow DOCS/presets.md section 5; gaps leave room for the operations of later
// milestones (lens profile 2, CA 3, defringe 4, heal 5, geometry 10, calibration 32, profile 35, dehaze 45,
// texture 62, mixer 80, B&W 82, grading 90, creative 91 to 98, look 100, vignette 120 ...).
std::vector<OpSpec> builtInOps()
{
    std::vector<OpSpec> ops;
    addHeal(ops);
    addGeometry(ops);
    addLightAndColor(ops);
    addDetail(ops);
    addCurves(ops);
    addMixer(ops);
    addBlackAndWhite(ops);
    addCalibration(ops);
    addLensCorrections(ops);
    addGrading(ops);
    addCreative(ops);
    addEffects(ops);
    std::ranges::sort(ops, [](const OpSpec& a, const OpSpec& b) { return a.stage < b.stage; });
    return ops;
}

bool validCurve(const Curve& c)
{
    if (c.size() < 2) {
        return false;
    }
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (!std::isfinite(c[i].x) || !std::isfinite(c[i].y)) {
            return false;
        }
        if (i > 0 && c[i].x <= c[i - 1].x) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> normalizeCurve(const std::string& key, Settings& settings)
{
    const auto* c = std::get_if<Curve>(settings.find(key));
    if (c == nullptr || !validCurve(*c)) {
        settings.erase(key);
        return "Ignored " + key + ": a curve needs two or more points with rising x";
    }
    Curve clamped = *c;
    for (CurvePoint& p : clamped) {
        p.x = std::clamp(p.x, 0.0, 1.0);
        p.y = std::clamp(p.y, 0.0, 1.0);
    }
    if (clamped == *c) {
        return std::nullopt;
    }
    if (!validCurve(clamped)) { // points beyond 1 can land on the same x once clamped
        settings.erase(key);
        return "Ignored " + key + ": its points fall on each other once clamped into 0 to 1";
    }
    settings.set(key, clamped);
    return "Clamped points of " + key + " into 0 to 1";
}

constexpr std::size_t kMaxSpots = 1000;
constexpr std::size_t kMaxStrokePoints = 4096;

bool finiteSpot(const Spot& s)
{
    const auto ok = [](double v) { return std::isfinite(v); };
    return ok(s.x) && ok(s.y) && ok(s.sourceX) && ok(s.sourceY) && ok(s.radius) && ok(s.feather) &&
           ok(s.opacity) &&
           std::ranges::all_of(s.stroke, [&](const CurvePoint& p) { return ok(p.x) && ok(p.y); });
}

Spot clampedSpot(Spot s)
{
    const auto unit = [](double& v) { v = std::clamp(v, 0.0, 1.0); };
    unit(s.x);
    unit(s.y);
    unit(s.sourceX);
    unit(s.sourceY);
    s.radius = std::clamp(s.radius, 0.001, 0.25);
    s.feather = std::clamp(s.feather, 0.0, 100.0);
    s.opacity = std::clamp(s.opacity, 0.0, 100.0);
    if (s.stroke.size() == 1) {
        s.stroke.clear(); // one point is a spot
    }
    if (s.stroke.size() > kMaxStrokePoints) {
        s.stroke.resize(kMaxStrokePoints);
    }
    for (CurvePoint& p : s.stroke) {
        unit(p.x);
        unit(p.y);
    }
    return s;
}

std::optional<std::string> normalizeSpots(const std::string& key, Settings& settings)
{
    const auto* spots = std::get_if<Spots>(settings.find(key));
    if (spots == nullptr) {
        settings.erase(key);
        return "Ignored " + key + ": not a list of spots";
    }
    Spots fixed;
    for (const Spot& s : *spots) {
        if (finiteSpot(s) && fixed.size() < kMaxSpots) {
            fixed.push_back(clampedSpot(s));
        }
    }
    if (fixed == *spots) {
        return std::nullopt;
    }
    if (fixed.empty()) {
        settings.erase(key);
        return "Ignored " + key + ": no usable spot";
    }
    settings.set(key, std::move(fixed));
    return "Fixed spots of " + key + " (out of range, unreadable or too many)";
}

// Checks one value against its spec; fixes or drops it and says what happened.
std::optional<std::string> normalizeOne(const ParamSpec& spec, const std::string& key, Settings& settings)
{
    const Value& value = *settings.find(key);
    switch (spec.type) {
    case ParamType::Number: {
        const auto* d = std::get_if<double>(&value);
        if (d == nullptr || !std::isfinite(*d)) {
            settings.erase(key);
            return "Ignored " + key + ": not a number";
        }
        if (*d < spec.minimum || *d > spec.maximum) {
            const double clamped = std::clamp(*d, spec.minimum, spec.maximum);
            std::ostringstream line;
            line << "Clamped " << key << " from " << *d << " to " << clamped;
            settings.set(key, clamped);
            return line.str();
        }
        return std::nullopt;
    }
    case ParamType::Boolean:
        if (!std::holds_alternative<bool>(value)) {
            settings.erase(key);
            return "Ignored " + key + ": not true or false";
        }
        return std::nullopt;
    case ParamType::Choice: {
        const auto* s = std::get_if<std::string>(&value);
        if (s == nullptr || std::ranges::find(spec.choices, *s) == spec.choices.end()) {
            settings.erase(key);
            return "Ignored " + key + ": not one of the known choices";
        }
        return std::nullopt;
    }
    case ParamType::Points:
        return normalizeCurve(key, settings);
    case ParamType::SpotList:
        return normalizeSpots(key, settings);
    }
    return std::nullopt;
}

} // namespace

Registry::Registry(std::vector<OpSpec> ops)
    : m_ops(std::move(ops))
{}

const Registry& registry()
{
    static const Registry instance(builtInOps());
    return instance;
}

Settings settingsBefore(const Settings& settings, std::string_view opId)
{
    const OpSpec* until = registry().op(opId);
    Settings out;
    if (until == nullptr) {
        return out;
    }
    for (const std::string& key : settings.keys()) {
        for (const OpSpec& o : registry().ops()) {
            const bool owns =
                std::ranges::any_of(o.params, [&key](const ParamSpec& p) { return p.key == key; });
            if (owns && o.stage < until->stage) {
                out.set(key, *settings.find(key));
            }
        }
    }
    return out;
}

const OpSpec* Registry::op(std::string_view id) const
{
    const auto it = std::ranges::find_if(m_ops, [&](const OpSpec& o) { return o.id == id; });
    return it == m_ops.end() ? nullptr : &*it;
}

const ParamSpec* Registry::param(std::string_view key) const
{
    for (const OpSpec& o : m_ops) {
        for (const ParamSpec& p : o.params) {
            if (p.key == key) {
                return &p;
            }
        }
    }
    return nullptr;
}

Value Registry::defaultValue(const ParamSpec& spec)
{
    switch (spec.type) {
    case ParamType::Number:
        return spec.defaultValue;
    case ParamType::Boolean:
        return spec.defaultValue != 0.0;
    case ParamType::Choice:
        return spec.choices.empty() ? std::string() : spec.choices.front();
    case ParamType::Points:
        return Curve{{0.0, 0.0}, {1.0, 1.0}};
    case ParamType::SpotList:
        return Spots{};
    }
    return spec.defaultValue;
}

std::vector<std::string> Registry::normalize(Settings& settings) const
{
    std::vector<std::string> report;
    for (const std::string& key : settings.keys()) {
        const ParamSpec* spec = param(key);
        if (spec == nullptr) {
            report.push_back("Unknown setting kept as is: " + key);
            continue;
        }
        if (auto line = normalizeOne(*spec, key, settings)) {
            report.push_back(std::move(*line));
        }
    }
    return report;
}

} // namespace pfx
