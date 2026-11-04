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
                    fraction("geometry.crop.h", 1.0)}});
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

void addGrading(std::vector<OpSpec>& ops)
{
    ops.push_back(
        {"grading",
         "grading",
         1,
         90,
         true,
         {number("grading.shadows.hue", 0, 360, 0, "degrees"), number("grading.shadows.amount", 0, 100),
          number("grading.highlights.hue", 0, 360, 0, "degrees"), number("grading.highlights.amount", 0, 100),
          number("grading.balance", -100, 100)}});
}

void addEffects(std::vector<OpSpec>& ops)
{
    ops.push_back(
        {"vignette",
         "effects",
         1,
         120,
         false,
         {number("effects.vignette.amount", -100, 100), number("effects.vignette.midpoint", 0, 100, 50),
          number("effects.vignette.roundness", -100, 100)}});
    ops.push_back({"grain",
                   "effects",
                   1,
                   130,
                   false,
                   {number("effects.grain.amount", 0, 100), number("effects.grain.size", 0, 100, 25)}});
    ops.push_back({"fade", "effects", 1, 140, true, {number("effects.fade", 0, 100)}});
}

// Stages follow DOCS/presets.md section 5; gaps leave room for the operations of later
// milestones (geometry 10, dehaze 45, mixer 80, grading 90, look 100, vignette 120 ...).
std::vector<OpSpec> builtInOps()
{
    std::vector<OpSpec> ops;
    addGeometry(ops);
    ops.push_back({"denoise",
                   "detail",
                   1,
                   20,
                   false,
                   {number("detail.denoise.luminance", 0, 100), number("detail.denoise.color", 0, 100)}});
    ops.push_back({"white_balance",
                   "color",
                   1,
                   30,
                   true,
                   {choice("color.wb.mode", {"relative"}), number("color.wb.temperature", -100, 100),
                    number("color.wb.tint", -100, 100)}});
    ops.push_back({"exposure", "light", 1, 40, true, {number("light.exposure", -5, 5, 0, "EV")}});
    ops.push_back({"tone",
                   "light",
                   1,
                   50,
                   true,
                   {number("light.contrast", -100, 100), number("light.highlights", -100, 100),
                    number("light.shadows", -100, 100), number("light.whites", -100, 100),
                    number("light.blacks", -100, 100)}});
    ops.push_back({"clarity", "detail", 1, 60, false, {number("detail.clarity", -100, 100)}});
    ops.push_back({"curves",
                   "curves",
                   1,
                   70,
                   true,
                   {curve("curves.rgb"), curve("curves.r"), curve("curves.g"), curve("curves.b")}});
    ops.push_back({"vibrance_saturation",
                   "color",
                   1,
                   85,
                   true,
                   {number("color.vibrance", -100, 100), number("color.saturation", -100, 100)}});
    ops.push_back(
        {"sharpen",
         "detail",
         1,
         110,
         false,
         {number("detail.sharpen.amount", 0, 150), relative("detail.sharpen.radius", 0.0001, 0.003, 0.0004),
          number("detail.sharpen.masking", 0, 100)}});
    ops.push_back({"dehaze", "detail", 1, 45, false, {number("detail.dehaze", -100, 100)}});
    addMixer(ops);
    addGrading(ops);
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
