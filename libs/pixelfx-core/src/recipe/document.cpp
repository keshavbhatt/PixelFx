// SPDX-License-Identifier: Apache-2.0
#include "pfx/document.h"

#include "pfx/registry.h"

#include <algorithm>
#include <array>
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
        } else if (child.is_null()) {
            continue;
        } else if (child.is_number()) {
            out.set(key, child.get<double>());
        } else if (child.is_boolean()) {
            out.set(key, child.get<bool>());
        } else if (child.is_string()) {
            out.set(key, child.get<std::string>());
        } else if (auto curve = curveFromJson(child)) {
            out.set(key, std::move(*curve));
        } else if (report != nullptr) {
            report->warnings.push_back("Ignored " + key + ": unreadable value");
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
    if (doc.kind == DocumentKind::Look || doc.kind == DocumentKind::Batch) {
        return Error{ErrorCode::Unsupported,
                     "Looks and batch recipes are not supported by this version yet."};
    }
    const auto versionIt = json.find("version");
    if (versionIt == json.end() || !versionIt->is_number_integer()) {
        return parseError("The document has no version.");
    }
    if (versionIt->get<int>() > kDocumentVersion) {
        return Error{ErrorCode::NewerVersion,
                     "This file was made by a newer PixelFx. Update PixelFx to open it."};
    }
    // Version 1 is the first published format. When version 2 exists, a migration from 1 to 2
    // goes here, one step per version, each with a test.
    doc.version = kDocumentVersion;
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
    json["version"] = kDocumentVersion;
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
    body["settings"] = unflatten(document.settings);
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
