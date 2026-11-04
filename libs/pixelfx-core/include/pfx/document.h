// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/result.h"
#include "pfx/settings.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pfx {

enum class DocumentKind
{
    Recipe,
    Preset,
    Look,
    Batch,
};

inline constexpr int kDocumentVersion = 1;

// The JSON envelope shared by every PixelFx file (DOCS/presets.md section 1). This version
// reads and writes recipe and preset bodies; look and batch bodies arrive with M4 and M5.
struct Document
{
    DocumentKind kind = DocumentKind::Preset;
    int version = kDocumentVersion;
    std::string id;
    std::string name;
    std::string group;
    std::string created;
    std::string modified;
    std::string generator;
    std::string workingSpace = "rec2020-linear";
    Settings settings;
    // Keys this version does not know, kept as JSON text and written back unchanged.
    std::string unknownTopLevel;
    std::string unknownBody;
};

struct ParseReport
{
    std::vector<std::string> warnings;
};

[[nodiscard]] Result<Document> parseDocument(std::string_view text, ParseReport* report = nullptr);
[[nodiscard]] std::string serializeDocument(const Document& document);

[[nodiscard]] Result<Document> loadDocument(const std::filesystem::path& path, ParseReport* report = nullptr);
[[nodiscard]] Status saveDocument(const Document& document, const std::filesystem::path& path);

[[nodiscard]] std::string_view kindName(DocumentKind kind);

} // namespace pfx
