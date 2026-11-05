// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/look.h"
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

// The version this library writes, per kind (DOCS/presets.md). Looks are at 1. Presets are at
// 2: version 2 added the Lightroom-compatible settings (B&W, full colour grading, Texture,
// Calibration, the profile, the secondary detail and effects settings, the parametric curve
// and the lens corrections). Batch recipes are at 3: version 2 added the user's looks
// embedded, size presets and the watermark, version 3 the same settings as presets. Recipes
// (sidecars) are at 3: version 2 added healed spots, version 3 the same settings. Older files
// of every kind read as they always did.
inline constexpr int kDocumentVersion = 1;
inline constexpr int kPresetVersion = 2;
inline constexpr int kBatchVersion = 3;
inline constexpr int kRecipeVersion = 3;
[[nodiscard]] int currentVersion(DocumentKind kind);

// A step of a batch recipe's stack (DOCS/presets.md section 4): settings applied onto each
// photo. A look on its own is a step whose settings are only `look.id` and `look.strength`.
struct BatchStep
{
    std::string name;
    Settings settings;
    bool operator==(const BatchStep&) const = default;
};

// A look of the user's that a batch stack refers to by id, carried inside the batch file so it
// runs on another machine. Built-in looks stay references.
struct EmbeddedLook
{
    std::string id;
    std::string name;
    Look look;
    bool operator==(const EmbeddedLook&) const = default;
};

struct BatchBody
{
    std::vector<BatchStep> stack;
    std::vector<EmbeddedLook> looks; // version 2
    bool autoFirst = false;
    bool keepExistingEdits = true;
    int longEdge = 0;            // 0: original size
    bool noEnlarge = true;       // a long edge never makes a smaller photo larger
    std::string sizePreset;      // version 2: a named size of the app's (an id), wins over longEdge
    bool fitInside = false;      // with sizePreset: fit inside with a border instead of cropping
    std::string watermark;       // version 2: the watermark as JSON text (the app's format), or empty
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

// The JSON envelope shared by every PixelFx file (DOCS/presets.md section 1): recipe, preset,
// look and batch bodies.
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
    Look look;       // look documents only
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
