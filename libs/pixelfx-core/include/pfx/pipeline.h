// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/lens.h"
#include "pfx/look.h"
#include "pfx/result.h"
#include "pfx/settings.h"

#include <atomic>

namespace pfx {

struct RenderOptions
{
    int threads = 0;                           // 0: one per hardware thread
    const std::atomic<bool>* cancel = nullptr; // checked between operations
    const Look* look = nullptr;                // run after colour grading, before sharpening
    float lookStrength = 1.0F;                 // 0 to 1
    const LensFacts* lens = nullptr;           // the photo's camera and lens, for its lens profile
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
