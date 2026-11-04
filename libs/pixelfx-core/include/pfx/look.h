// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/lut.h"
#include "pfx/settings.h"

#include <map>
#include <string>
#include <vector>

namespace pfx {

// One step of a look (DOCS/presets.md section 3): a registry operation with its own settings
// (full keys, "light.exposure"), or "lut" with `resource` (a name in the look's resources) and
// `mix` (0 to 100). Disabled steps are kept, not run.
struct LookStep
{
    std::string op;
    int version = 1;
    bool enabled = true;
    Settings params;
    bool operator==(const LookStep&) const = default;
};

// A look, the user's "filter": an ordered stack of steps with one strength (FEATURES L1 to L6).
struct Look
{
    std::string category;
    int defaultStrength = 100; // 0 to 100
    std::vector<LookStep> steps;
    std::map<std::string, Lut> resources; // LUTs, embedded so a look is one file
};

// Runs the look's enabled steps on the image, then blends the result with the input by
// `strength` (0 to 1) in linear light. Geometry is never a look step; an unknown operation or a
// missing LUT is skipped.
void applyLook(Image& image, const Look& look, float strength, int threads = 0);

} // namespace pfx
