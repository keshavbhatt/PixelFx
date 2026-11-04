// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/result.h"
#include "pfx/settings.h"

#include <cstdint>
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

// A batch recipe's body (DOCS/presets.md section 4): presets applied in order onto each photo,
// then how the photos are written. Looks in the stack arrive with looks.
struct BatchStep
{
    std::string name;
    Settings settings;
    bool operator==(const BatchStep&) const = default;
};

struct BatchBody
{
    std::vector<BatchStep> stack;
    bool autoFirst = false;
    bool keepExistingEdits = true;
    int longEdge = 0;            // 0: original size; never enlarges
    std::string format = "jpeg"; // jpeg, png, tiff, webp, avif, heic, jxl
    int quality = 90;
    bool keepCamera = true;
    bool keepLocation = false;
    std::string folder;
    bool beside = false; // a folder next to each original instead of `folder`
    std::string name = "{name}-edit";
    std::string ifExists = "number"; // number, overwrite, skip
    bool operator==(const BatchBody&) const = default;
};

// The JSON envelope shared by every PixelFx file (DOCS/presets.md section 1). This version
// reads and writes recipe, preset and batch bodies; look bodies arrive with M4.
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
    // Recipe documents (sidecars): the photo they belong to (DOCS/presets.md section 4). The
    // fingerprint re-attaches a sidecar to a photo that was moved or renamed.
    std::string sourceName;
    std::uint64_t sourceSize = 0;
    std::string sourceFingerprint;
    BatchBody batch; // batch documents only
    // Recipe documents: named states of the edit the user kept (History, Save snapshot).
    std::vector<BatchStep> snapshots;
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
