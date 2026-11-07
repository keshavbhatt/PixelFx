// SPDX-License-Identifier: Apache-2.0
#include "pfx/document.h"

#include "pfx/registry.h"
#include "recipe/encoding.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>

namespace pfx {
namespace {

using Json = nlohmann::ordered_json;

constexpr std::size_t kMaxDocumentBytes = std::size_t{32} * 1024 * 1024;

constexpr std::array<std::pair<DocumentKind, std::string_view>, 4> kKinds = {{
    {DocumentKind::Recipe, "recipe"},
    {DocumentKind::Preset, "preset"},
    {DocumentKind::Look, "look"},
    {DocumentKind::Batch, "batch"},
}};

constexpr std::array<std::string_view, 9> kKnownTopLevel = {
    "format", "kind", "version", "id", "name", "group", "created", "modified", "generator"};

Error parseError(std::string message)
{
    return {ErrorCode::ParseError, std::move(message)};
}

bool isKnownTopLevel(std::string_view key)
{
    return key == "body" || key == "workingSpace" ||
           std::ranges::find(kKnownTopLevel, key) != kKnownTopLevel.end();
}

std::optional<Curve> curveFromJson(const Json& j)
{
    if (!j.is_array()) {
        return std::nullopt;
    }
    Curve curve;
    for (const Json& point : j) {
        if (!point.is_array() || point.size() != 2 || !point[0].is_number() || !point[1].is_number()) {
            return std::nullopt;
        }
        curve.push_back({point[0].get<double>(), point[1].get<double>()});
    }
    return curve;
}

// A list of spots (group "heal"): {"x", "y", "source": {"x", "y"}, "radius", "feather",
// "opacity", "stroke": [[x, y], ...]}. Missing fields take Spot's defaults.
std::optional<Spots> spotsFromJson(const Json& j)
{
    if (!j.is_array()) {
        return std::nullopt;
    }
    Spots spots;
    for (const Json& item : j) {
        if (!item.is_object()) {
            return std::nullopt;
        }
        Spot s;
        const auto num = [](const Json& o, const char* name, double fallback) {
            const auto it = o.find(name);
            return it != o.end() && it->is_number() ? it->get<double>() : fallback;
        };
        s.x = num(item, "x", s.x);
        s.y = num(item, "y", s.y);
        s.sourceX = s.x;
        s.sourceY = s.y;
        if (const auto source = item.find("source"); source != item.end() && source->is_object()) {
            s.sourceX = num(*source, "x", s.x);
            s.sourceY = num(*source, "y", s.y);
        }
        s.radius = num(item, "radius", s.radius);
        s.feather = num(item, "feather", s.feather);
        s.opacity = num(item, "opacity", s.opacity);
        if (const auto stroke = item.find("stroke"); stroke != item.end()) {
            auto points = curveFromJson(*stroke);
            if (!points) {
                return std::nullopt;
            }
            s.stroke = std::move(*points);
        }
        spots.push_back(std::move(s));
    }
    return spots;
}

// One value at `key`: a number, true or false, text, a curve, or the spots of "heal.spots".
void readValue(const std::string& key, const Json& child, Settings& out, ParseReport* report)
{
    if (child.is_number()) {
        out.set(key, child.get<double>());
    } else if (child.is_boolean()) {
        out.set(key, child.get<bool>());
    } else if (child.is_string()) {
        out.set(key, child.get<std::string>());
    } else if (key == "heal.spots") {
        if (auto spots = spotsFromJson(child)) {
            out.set(key, std::move(*spots));
        } else if (report != nullptr) {
            report->warnings.push_back("Ignored " + key + ": unreadable spots");
        }
    } else if (auto curve = curveFromJson(child)) {
        out.set(key, std::move(*curve));
    } else if (report != nullptr) {
        report->warnings.push_back("Ignored " + key + ": unreadable value");
    }
}

// Nested JSON objects become dotted keys ("detail": {"sharpen": {"amount": 30}} becomes
// "detail.sharpen.amount"). null means "not set" and is skipped.
void flatten(const Json& node, const std::string& prefix, Settings& out, ParseReport* report)
{
    for (const auto& [name, child] : node.items()) {
        if (name.empty() || name.find('.') != std::string::npos) {
            // A dotted key is built from names; an empty or dotted name would not read back.
            if (report != nullptr) {
                report->warnings.push_back("Ignored \"" + name + "\" in " +
                                           (prefix.empty() ? "settings" : prefix) +
                                           ": a name must not be empty or contain a dot");
            }
            continue;
        }
        std::string key = prefix;
        if (!key.empty()) {
            key += '.';
        }
        key += name;
        if (child.is_object()) {
            flatten(child, key, out, report);
        } else if (!child.is_null()) {
            readValue(key, child, out, report);
        }
    }
}

// A key cannot be both a value and a group ("light": 5 beside "light": {...} after merging):
// the value goes, so what is written reads back the same.
void dropValuesThatAreGroups(Settings& settings, ParseReport* report)
{
    const std::vector<std::string> keys = settings.keys(); // sorted
    for (const std::string& key : keys) {
        const std::string group = key + '.';
        const auto at = std::ranges::lower_bound(keys, group);
        if (at != keys.end() && at->starts_with(group)) {
            settings.erase(key);
            if (report != nullptr) {
                report->warnings.push_back("Ignored " + key + ": it is also the name of a group");
            }
        }
    }
}

Json valueToJson(const Value& value)
{
    return std::visit(
        [](const auto& v) -> Json {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Curve>) {
                Json points = Json::array();
                for (const CurvePoint& p : v) {
                    points.push_back(Json::array({p.x, p.y}));
                }
                return points;
            } else if constexpr (std::is_same_v<T, Spots>) {
                Json spots = Json::array();
                for (const Spot& s : v) {
                    Json spot = {{"x", s.x},
                                 {"y", s.y},
                                 {"source", {{"x", s.sourceX}, {"y", s.sourceY}}},
                                 {"radius", s.radius},
                                 {"feather", s.feather},
                                 {"opacity", s.opacity}};
                    if (!s.stroke.empty()) {
                        Json points = Json::array();
                        for (const CurvePoint& p : s.stroke) {
                            points.push_back(Json::array({p.x, p.y}));
                        }
                        spot["stroke"] = points;
                    }
                    spots.push_back(std::move(spot));
                }
                return spots;
            } else {
                return Json(v);
            }
        },
        value);
}

Json unflatten(const Settings& settings)
{
    Json root = Json::object();
    for (const std::string& key : settings.keys()) {
        Json* node = &root;
        std::size_t start = 0;
        while (true) {
            const std::size_t dot = key.find('.', start);
            const std::string part =
                key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
            if (dot == std::string::npos) {
                (*node)[part] = valueToJson(*settings.find(key));
                break;
            }
            Json& child = (*node)[part];
            if (!child.is_object()) {
                child = Json::object();
            }
            node = &child;
            start = dot + 1;
        }
    }
    return root;
}

std::string stringField(const Json& j, const char* name)
{
    const auto it = j.find(name);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// Kind, version and the descriptive fields. Returns an error for what this version cannot read.
std::optional<Error> readEnvelope(const Json& json, Document& doc)
{
    const std::string kind = stringField(json, "kind");
    // An iterator, not a pointer: MSVC's std::array iterators are class types.
    const auto kindIt = // NOLINT(readability-qualified-auto)
        std::ranges::find_if(kKinds, [&](const auto& k) { return k.second == kind; });
    if (kindIt == kKinds.end()) {
        return parseError("Unknown document kind \"" + kind + "\".");
    }
    doc.kind = kindIt->first;
    const auto versionIt = json.find("version");
    if (versionIt == json.end() || !versionIt->is_number_integer()) {
        return parseError("The document has no version.");
    }
    const int version = versionIt->get<int>();
    if (version > currentVersion(doc.kind)) {
        return Error{ErrorCode::NewerVersion,
                     "This file was made by a newer PixelFx. Update PixelFx to open it."};
    }
    // Migrations, one step per version, each with a test. Batch 1 to 2: version 2 only added
    // keys (embedded looks, a size preset, the watermark), so a version 1 body reads unchanged.
    // Recipe 1 to 2: version 2 only added the "heal" group (spots), likewise. Preset 1 to 2,
    // recipe 2 to 3 and batch 2 to 3: the new version only added settings whose defaults are
    // no change (FEATURES P8), so an older body reads unchanged and renders as before. Preset 4
    // to 5, recipe 5 to 6 and batch 6 to 7 added the four-corner perspective, whose defaults
    // are the photo's own corners: no change either.
    doc.version = currentVersion(doc.kind);
    doc.id = stringField(json, "id");
    doc.name = stringField(json, "name");
    doc.group = stringField(json, "group");
    doc.created = stringField(json, "created");
    doc.modified = stringField(json, "modified");
    doc.generator = stringField(json, "generator");
    if (const std::string space = stringField(json, "workingSpace"); !space.empty()) {
        doc.workingSpace = space;
    }
    Json unknownTop = Json::object();
    for (const auto& [name, value] : json.items()) {
        if (!isKnownTopLevel(name)) {
            unknownTop[name] = value;
        }
    }
    if (!unknownTop.empty()) {
        doc.unknownTopLevel = unknownTop.dump();
    }
    return std::nullopt;
}

bool boolField(const Json& json, const char* name, bool fallback)
{
    const auto it = json.find(name);
    return it != json.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

int intField(const Json& json, const char* name, int fallback)
{
    const auto it = json.find(name);
    return it != json.end() && it->is_number() ? static_cast<int>(it->get<double>()) : fallback;
}

bool readLookField(const std::string& name, const Json& value, Look& look, ParseReport* report);
Json lookToJson(const Look& look, Json body);

// An embedded look (`{"id", "name", ...look body...}`), kept once per id. Its id, or empty
// when it has none.
std::string readEmbeddedLook(const Json& value, BatchBody& batch, ParseReport* report)
{
    if (!value.is_object()) {
        return {};
    }
    EmbeddedLook e;
    e.id = stringField(value, "id");
    e.name = stringField(value, "name");
    if (e.id.empty() || e.id.starts_with("builtin:")) {
        if (report != nullptr) {
            report->warnings.emplace_back("Left out an embedded look with no id of its own");
        }
        return {};
    }
    for (const auto& [key, field] : value.items()) {
        readLookField(key, field, e.look, report);
    }
    if (std::ranges::none_of(batch.looks, [&](const EmbeddedLook& l) { return l.id == e.id; })) {
        batch.looks.push_back(std::move(e));
    }
    return stringField(value, "id");
}

// `{"id" | "embedded", "strength"}` as look settings; an embedded look is kept in the body.
Settings lookSettings(const Json& look, BatchBody& batch, ParseReport* report)
{
    Settings s;
    std::string id = stringField(look, "id");
    if (const auto embedded = look.find("embedded"); embedded != look.end()) {
        id = readEmbeddedLook(*embedded, batch, report);
    }
    if (!id.empty()) {
        s.set("look.id", id);
        const auto strength = look.find("strength");
        s.set("look.strength",
              strength != look.end() && strength->is_number() ? strength->get<double>() : 100.0);
    }
    return s;
}

void readStack(const Json& value, BatchBody& batch, ParseReport* report)
{
    for (const Json& step : value) {
        if (!step.is_object()) {
            continue;
        }
        const auto preset = step.find("preset");
        const auto look = step.find("look");
        if (preset != step.end() && preset->is_object() && preset->contains("embedded") &&
            (*preset)["embedded"].is_object()) {
            Json embedded = (*preset)["embedded"];
            BatchStep s;
            s.name = stringField(embedded, "name");
            if (const auto settings = embedded.find("settings");
                settings != embedded.end() && settings->is_object()) {
                // A look embedded in the preset's own settings (presets.md section 2).
                Settings lookPart;
                if (const auto l = settings->find("look");
                    l != settings->end() && l->is_object() && l->contains("embedded")) {
                    lookPart = lookSettings(*l, batch, report);
                    settings->erase("look");
                }
                flatten(*settings, {}, s.settings, report);
                dropValuesThatAreGroups(s.settings, report);
                s.settings = s.settings.overlaidWith(lookPart);
            }
            batch.stack.push_back(std::move(s));
        } else if (look != step.end() && look->is_object()) {
            BatchStep s;
            s.name = stringField(*look, "name");
            s.settings = lookSettings(*look, batch, report);
            if (!s.settings.isEmpty()) {
                batch.stack.push_back(std::move(s));
            }
        } else if (report != nullptr) {
            report->warnings.emplace_back("Skipped a step of the stack: preset references are not "
                                          "supported by this version yet");
        }
    }
}

void readOutput(const Json& value, BatchBody& batch)
{
    if (const std::string format = stringField(value, "format"); !format.empty()) {
        batch.format = format;
    }
    batch.quality = std::clamp(intField(value, "quality", batch.quality), 1, 100);
    // Version 6: before it, the colour space was always written "srgb" and there was no sharpening.
    if (const std::string space = stringField(value, "colorSpace");
        space == "srgb" || space == "display-p3" || space == "adobe-rgb" || space == "original") {
        batch.colorSpace = space;
    }
    if (const std::string sharpen = stringField(value, "sharpen");
        sharpen == "none" || sharpen == "screen" || sharpen == "print") {
        batch.sharpen = sharpen;
    }
    if (const auto metadata = value.find("metadata"); metadata != value.end() && metadata->is_object()) {
        batch.keepCamera = boolField(*metadata, "camera", batch.keepCamera);
        batch.keepLocation = boolField(*metadata, "location", batch.keepLocation);
    }
    batch.folder = stringField(value, "folder");
    batch.beside = boolField(value, "beside", false);
    if (const std::string pattern = stringField(value, "name"); !pattern.empty()) {
        batch.name = pattern;
    }
    if (const std::string policy = stringField(value, "ifExists"); !policy.empty()) {
        batch.ifExists = policy;
    }
    if (const auto watermark = value.find("watermark"); watermark != value.end() && watermark->is_object()) {
        batch.watermark = watermark->dump();
    }
}

// One field of a batch body; false when the name is not a batch field (kept as unknown).
bool readBatchField(const std::string& name, const Json& value, BatchBody& batch, ParseReport* report)
{
    if (name == "stack" && value.is_array()) {
        readStack(value, batch, report);
        return true;
    }
    if (name == "autoFirst") {
        batch.autoFirst = value.is_boolean() && value.get<bool>();
        return true;
    }
    if (name == "keepExistingEdits") {
        batch.keepExistingEdits = !value.is_boolean() || value.get<bool>();
        return true;
    }
    if (name == "resize" && value.is_object()) {
        const std::string mode = stringField(value, "mode");
        batch.longEdge = mode == "longEdge" ? std::max(0, intField(value, "value", 0)) : 0;
        batch.noEnlarge = boolField(value, "noEnlarge", true);
        batch.sizePreset = mode == "preset" ? stringField(value, "preset") : std::string();
        batch.fitInside = mode == "preset" && boolField(value, "fitInside", false);
        return true;
    }
    if (name == "looks" && value.is_array()) {
        for (const Json& look : value) {
            readEmbeddedLook(look, batch, report);
        }
        return true;
    }
    if (name == "output" && value.is_object()) {
        readOutput(value, batch);
        return true;
    }
    return false;
}

// A step whose settings are only a look is written as a look step.
bool isLookStep(const BatchStep& step)
{
    return step.settings.contains("look.id") &&
           std::ranges::all_of(step.settings.keys(),
                               [](const std::string& k) { return k.starts_with("look."); });
}

Json batchToJson(const BatchBody& batch, Json body)
{
    Json stack = Json::array();
    for (const BatchStep& step : batch.stack) {
        if (isLookStep(step)) {
            stack.push_back({{"look",
                              {{"id", step.settings.text("look.id", "")},
                               {"strength", step.settings.number("look.strength", 100.0)},
                               {"name", step.name}}}});
        } else {
            stack.push_back(
                {{"preset", {{"embedded", {{"name", step.name}, {"settings", unflatten(step.settings)}}}}}});
        }
    }
    body["stack"] = stack;
    if (!batch.looks.empty()) {
        Json looks = Json::array();
        for (const EmbeddedLook& e : batch.looks) {
            Json look = lookToJson(e.look, Json::object());
            look["id"] = e.id;
            look["name"] = e.name;
            looks.push_back(std::move(look));
        }
        body["looks"] = looks;
    }
    body["autoFirst"] = batch.autoFirst;
    body["keepExistingEdits"] = batch.keepExistingEdits;
    if (!batch.sizePreset.empty()) {
        body["resize"] =
            Json{{"mode", "preset"}, {"preset", batch.sizePreset}, {"fitInside", batch.fitInside}};
    } else {
        body["resize"] =
            batch.longEdge > 0
                ? Json{{"mode", "longEdge"}, {"value", batch.longEdge}, {"noEnlarge", batch.noEnlarge}}
                : Json{{"mode", "none"}};
    }
    body["output"] = {{"format", batch.format},
                      {"quality", batch.quality},
                      {"colorSpace", batch.colorSpace},
                      {"sharpen", batch.sharpen},
                      {"metadata", {{"camera", batch.keepCamera}, {"location", batch.keepLocation}}},
                      {"folder", batch.folder},
                      {"beside", batch.beside},
                      {"name", batch.name},
                      {"ifExists", batch.ifExists}};
    if (const Json watermark = Json::parse(batch.watermark, nullptr, false);
        !batch.watermark.empty() && watermark.is_object()) {
        body["output"]["watermark"] = watermark;
    }
    return body;
}

// DOMAIN_MIN and DOMAIN_MAX as the .cube had them; absent means 0 to 1.
void readDomain(const Json& j, Lut& lut)
{
    for (const auto& [key, target] :
         {std::pair{"domainMin", &lut.domainMin}, std::pair{"domainMax", &lut.domainMax}}) {
        const auto it = j.find(key);
        if (it == j.end() || !it->is_array() || it->size() != 3) {
            continue;
        }
        for (std::size_t c = 0; c < 3; ++c) {
            if ((*it)[c].is_number()) {
                target->at(c) = static_cast<float>((*it)[c].get<double>()); // may be inf: checked below
            }
        }
    }
}

// The table from base64 half floats, little-endian; false when the count is not the size's.
bool readTable(const std::string& data, Lut& lut)
{
    const auto n = static_cast<std::size_t>(lut.size);
    const std::size_t entries = (lut.kind == Lut::Kind::ThreeD ? n * n * n : n) * 3;
    const auto bytes = detail::base64Decode(data);
    if (!bytes || bytes->size() != entries * 2) {
        return false;
    }
    lut.table.resize(entries);
    for (std::size_t i = 0; i < entries; ++i) {
        const auto low = static_cast<std::uint32_t>((*bytes)[2 * i]);
        const auto high = static_cast<std::uint32_t>((*bytes)[(2 * i) + 1]);
        lut.table[i] = detail::fromHalf(static_cast<std::uint16_t>(low | (high << 8U)));
    }
    return true;
}

// A look's LUT resource: half floats in base64, in the sRGB encoding (presets.md section 3).
std::optional<Lut> lutFromJson(const std::string& name, const Json& j, ParseReport* report)
{
    const auto warn = [&](const std::string& why) {
        if (report != nullptr) {
            report->warnings.push_back("Left out the LUT \"" + name + "\": " + why);
        }
        return std::nullopt;
    };
    if (!j.is_object()) {
        return warn("not an object");
    }
    const std::string type = stringField(j, "type");
    if (type != "lut3d" && type != "lut1d") {
        return warn("unknown type");
    }
    if (const std::string domain = stringField(j, "domain"); domain != "srgb-gamma") {
        return warn("unsupported domain \"" + domain + "\"");
    }
    if (stringField(j, "encoding") != "f16-base64") {
        return warn("unsupported encoding");
    }
    Lut lut;
    lut.kind = type == "lut3d" ? Lut::Kind::ThreeD : Lut::Kind::OneD;
    const auto sizeIt = j.find("size");
    lut.size = sizeIt != j.end() && sizeIt->is_number_integer() ? sizeIt->get<int>() : 0;
    const int limit = lut.kind == Lut::Kind::ThreeD ? 256 : 65536;
    if (lut.size < 2 || lut.size > limit) {
        return warn("size out of range");
    }
    readDomain(j, lut);
    for (std::size_t c = 0; c < 3; ++c) {
        // The same rule as a .cube: a finite domain whose top is above its bottom.
        if (!std::isfinite(lut.domainMin.at(c)) || !std::isfinite(lut.domainMax.at(c)) ||
            !(lut.domainMax.at(c) > lut.domainMin.at(c))) {
            return warn("its domain is not a range");
        }
    }
    if (!readTable(stringField(j, "data"), lut)) {
        return warn("the data does not match its size");
    }
    if (!std::ranges::all_of(lut.table, [](float v) { return std::isfinite(v); })) {
        return warn("the data holds values that are not numbers");
    }
    lut.title = stringField(j, "name");
    return lut;
}

Json lutToJson(const Lut& lut)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(lut.table.size() * 2);
    for (const float v : lut.table) {
        const std::uint16_t half = detail::toHalf(v);
        bytes.push_back(static_cast<std::uint8_t>(half & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>(half >> 8U));
    }
    return Json{{"type", lut.kind == Lut::Kind::ThreeD ? "lut3d" : "lut1d"},
                {"size", lut.size},
                {"domain", "srgb-gamma"},
                {"domainMin", lut.domainMin},
                {"domainMax", lut.domainMax},
                {"interpolation", lut.kind == Lut::Kind::ThreeD ? "tetrahedral" : "linear"},
                {"encoding", "f16-base64"},
                {"name", lut.title},
                {"data", detail::base64Encode(bytes)}};
}

// The group a step's params are written under: the registry operation's own group; LUT steps
// and unknown operations keep their params as they are.
std::string stepGroup(const std::string& op)
{
    const OpSpec* spec = registry().op(op);
    return spec != nullptr ? spec->group : std::string();
}

void readLookSteps(const Json& value, Look& look, ParseReport* report)
{
    for (const Json& j : value) {
        if (!j.is_object()) {
            continue;
        }
        LookStep step;
        step.op = stringField(j, "op");
        step.version = intField(j, "v", 1);
        step.enabled = boolField(j, "enabled", true);
        if (const auto params = j.find("params"); params != j.end() && params->is_object()) {
            flatten(*params, stepGroup(step.op), step.params, report);
        }
        if (step.op != "lut" && registry().op(step.op) == nullptr && report != nullptr) {
            // Kept for a newer PixelFx; this one skips it when rendering.
            report->warnings.push_back("The look step \"" + step.op +
                                       "\" is not known to this version and is skipped");
        } else if (step.op != "lut") {
            for (const std::string& line : registry().normalize(step.params)) {
                if (report != nullptr) {
                    report->warnings.push_back(line);
                }
            }
        }
        look.steps.push_back(std::move(step));
    }
}

bool readLookField(const std::string& name, const Json& value, Look& look, ParseReport* report)
{
    if (name == "category" && value.is_string()) {
        look.category = value.get<std::string>();
    } else if (name == "defaultStrength" && value.is_number()) {
        look.defaultStrength = std::clamp(static_cast<int>(value.get<double>()), 0, 100);
    } else if (name == "steps" && value.is_array()) {
        readLookSteps(value, look, report);
    } else if (name == "resources" && value.is_object()) {
        for (const auto& [key, resource] : value.items()) {
            if (auto lut = lutFromJson(key, resource, report)) {
                look.resources.emplace(key, std::move(*lut));
            }
        }
    } else {
        return false;
    }
    return true;
}

Json lookToJson(const Look& look, Json body)
{
    Json steps = Json::array();
    for (const LookStep& step : look.steps) {
        Json params = unflatten(step.params);
        if (const std::string group = stepGroup(step.op); !group.empty()) {
            params = params.contains(group) ? params[group] : Json::object();
        }
        steps.push_back(
            {{"op", step.op}, {"v", step.version}, {"enabled", step.enabled}, {"params", params}});
    }
    Json resources = Json::object();
    for (const auto& [key, lut] : look.resources) {
        resources[key] = lutToJson(lut);
    }
    body["category"] = look.category;
    body["defaultStrength"] = look.defaultStrength;
    body["steps"] = steps;
    body["resources"] = resources;
    return body;
}

// A recipe's snapshots: named states of the edit.
void readSnapshots(const Json& value, Document& doc, ParseReport* report)
{
    for (const Json& snapshot : value) {
        BatchStep s;
        s.name = stringField(snapshot, "name");
        if (const auto settings = snapshot.find("settings"); settings != snapshot.end()) {
            flatten(*settings, {}, s.settings, report);
            dropValuesThatAreGroups(s.settings, report);
        }
        doc.snapshots.push_back(std::move(s));
    }
}

void readBody(const Json& json, Document& doc, ParseReport* report)
{
    const auto bodyIt = json.find("body");
    if (bodyIt == json.end() || !bodyIt->is_object()) {
        return;
    }
    Json unknownBody = Json::object();
    for (const auto& [name, value] : bodyIt->items()) {
        if (name == "settings" && value.is_object()) {
            flatten(value, {}, doc.settings, report);
            dropValuesThatAreGroups(doc.settings, report);
        } else if ((doc.kind == DocumentKind::Batch && readBatchField(name, value, doc.batch, report)) ||
                   (doc.kind == DocumentKind::Look && readLookField(name, value, doc.look, report))) {
            continue;
        } else if (doc.kind == DocumentKind::Recipe && name == "snapshots" && value.is_array()) {
            readSnapshots(value, doc, report);
        } else if (name == "source" && value.is_object() && doc.kind == DocumentKind::Recipe) {
            doc.sourceName = stringField(value, "name");
            doc.sourceFingerprint = stringField(value, "fingerprint");
            const auto size = value.find("size");
            doc.sourceSize =
                size != value.end() && size->is_number_unsigned() ? size->get<std::uint64_t>() : 0;
        } else {
            unknownBody[name] = value;
        }
    }
    if (!unknownBody.empty()) {
        doc.unknownBody = unknownBody.dump();
    }
}

} // namespace

std::string_view kindName(DocumentKind kind)
{
    for (const auto& [k, name] : kKinds) {
        if (k == kind) {
            return name;
        }
    }
    return "preset";
}

int currentVersion(DocumentKind kind)
{
    switch (kind) {
    case DocumentKind::Batch:
        return kBatchVersion;
    case DocumentKind::Recipe:
        return kRecipeVersion;
    case DocumentKind::Preset:
        return kPresetVersion;
    case DocumentKind::Look:
        break;
    }
    return kDocumentVersion;
}

Result<Document> parseDocument(std::string_view text, ParseReport* report)
{
    if (text.size() > kMaxDocumentBytes) {
        return parseError("The file is larger than 32 MB and was not read.");
    }
    const Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        return parseError("The file is not a PixelFx document (not valid JSON).");
    }
    if (stringField(json, "format") != "pixelfx") {
        return parseError("The file is not a PixelFx document.");
    }
    Document doc;
    if (auto error = readEnvelope(json, doc)) {
        return std::move(*error);
    }
    readBody(json, doc, report);
    const auto lines = registry().normalize(doc.settings);
    if (report != nullptr) {
        report->warnings.insert(report->warnings.end(), lines.begin(), lines.end());
    }
    return doc;
}

std::string serializeDocument(const Document& document)
{
    Json json = Json::object();
    json["format"] = "pixelfx";
    json["kind"] = std::string(kindName(document.kind));
    json["version"] = currentVersion(document.kind);
    const auto put = [&](const char* name, const std::string& value) {
        if (!value.empty()) {
            json[name] = value;
        }
    };
    put("id", document.id);
    put("name", document.name);
    put("group", document.group);
    put("created", document.created);
    put("modified", document.modified);
    put("generator", document.generator);
    json["workingSpace"] = document.workingSpace;
    if (!document.unknownTopLevel.empty()) {
        const Json extra = Json::parse(document.unknownTopLevel);
        for (const auto& [name, value] : extra.items()) {
            json[name] = value;
        }
    }
    Json body = Json::object();
    if (document.kind == DocumentKind::Recipe && !document.sourceFingerprint.empty()) {
        body["source"] = Json{{"name", document.sourceName},
                              {"size", document.sourceSize},
                              {"fingerprint", document.sourceFingerprint}};
    }
    if (document.kind == DocumentKind::Batch) {
        body = batchToJson(document.batch, body);
    }
    if (document.kind == DocumentKind::Look) {
        body = lookToJson(document.look, body);
    }
    if (document.kind == DocumentKind::Recipe && !document.snapshots.empty()) {
        Json snapshots = Json::array();
        for (const BatchStep& snapshot : document.snapshots) {
            snapshots.push_back({{"name", snapshot.name}, {"settings", unflatten(snapshot.settings)}});
        }
        body["snapshots"] = snapshots;
    }
    if ((document.kind != DocumentKind::Batch && document.kind != DocumentKind::Look) ||
        !document.settings.isEmpty()) {
        body["settings"] = unflatten(document.settings);
    }
    if (!document.unknownBody.empty()) {
        const Json extra = Json::parse(document.unknownBody);
        for (const auto& [name, value] : extra.items()) {
            body[name] = value;
        }
    }
    json["body"] = body;
    return json.dump(2) + "\n";
}

Result<Document> loadDocument(const std::filesystem::path& path, ParseReport* report)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return Error{ErrorCode::NotFound, "Could not open " + path.string() + "."};
    }
    if (size > kMaxDocumentBytes) {
        return parseError("The file is larger than 32 MB and was not read.");
    }
    const std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parseDocument(buffer.str(), report);
}

Status saveDocument(const Document& document, const std::filesystem::path& path)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Error{ErrorCode::Io, "Could not write " + path.string() + "."};
    }
    out << serializeDocument(document);
    if (!out) {
        return Error{ErrorCode::Io, "Could not write " + path.string() + "."};
    }
    return {};
}

} // namespace pfx
