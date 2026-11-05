// SPDX-License-Identifier: Apache-2.0
// Linear DNG and the previews inside camera RAW files (ADR-012).
#pragma once

#include "pfx/io.h"
#include "pfx/result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace pfx::detail {

/// Whether the file is a linear (demosaiced) DNG, which PixelFx decodes itself.
[[nodiscard]] bool isLinearDng(const std::vector<std::uint8_t>& file);
[[nodiscard]] Result<LoadedImage> loadLinearDng(const std::vector<std::uint8_t>& file,
                                                const LoadOptions& options);
/// The largest JPEG inside a TIFF-based RAW, with its size and the RAW's orientation.
[[nodiscard]] std::optional<std::vector<std::uint8_t>>
embeddedPreview(const std::vector<std::uint8_t>& file, int& width, int& height, std::uint32_t& orientation);

} // namespace pfx::detail
