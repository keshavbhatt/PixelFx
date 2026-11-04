// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"
#include "pfx/settings.h"

#include <atomic>

namespace pfx {

struct RenderOptions
{
    int threads = 0;                           // 0: one per hardware thread
    const std::atomic<bool>* cancel = nullptr; // checked between operations
};

// Renders `settings` onto `input` (working space) in the fixed pipeline order. Spatial
// parameters are relative to the image's long edge, so a preview at any size matches the
// full-size export (ADR-004). Identity settings cost nothing.
[[nodiscard]] Result<Image> render(const Image& input, const Settings& settings,
                                   const RenderOptions& options = {});
// The same, working in `input`'s own memory: for a caller done with the source (an export),
// it saves a full-size copy.
[[nodiscard]] Result<Image> render(Image&& input, const Settings& settings,
                                   const RenderOptions& options = {});

} // namespace pfx
