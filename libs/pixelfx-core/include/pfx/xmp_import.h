// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/document.h"
#include "pfx/result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pfx {

// A Lightroom or Camera Raw preset (.xmp, the crs: settings) read as a PixelFx preset
// (DOCS/presets.md section 6). What PixelFx has an equivalent for is mapped, approximately;
// `skipped` names every other setting the preset changes (profiles, masks, lens corrections,
// Texture ...), for the user-facing report. Settings at their neutral value are not reported.
struct XmpImport
{
    Document document;
    std::vector<std::string> skipped;
};

[[nodiscard]] Result<XmpImport> importXmpPreset(std::string_view text);
[[nodiscard]] Result<XmpImport> importXmpPresetFile(const std::filesystem::path& path);

} // namespace pfx
