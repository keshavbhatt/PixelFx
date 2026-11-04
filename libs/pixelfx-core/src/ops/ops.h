// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/settings.h"

#include <string_view>
#include <vector>

namespace pfx::detail {

struct OpContext
{
    int threads = 0;
};

// An operation's implementation, paired with its registry entry by id. `active` is false
// when the settings are the identity, so the pipeline skips the operation entirely.
struct OpImpl
{
    std::string_view id;
    bool (*active)(const Settings&);
    void (*apply)(Image&, const Settings&, const OpContext&);
};

// Implementations, one translation unit per group.
bool exposureActive(const Settings& s);
void exposureApply(Image& image, const Settings& s, const OpContext& ctx);
bool toneActive(const Settings& s);
void toneApply(Image& image, const Settings& s, const OpContext& ctx);
bool whiteBalanceActive(const Settings& s);
void whiteBalanceApply(Image& image, const Settings& s, const OpContext& ctx);
bool vibranceSaturationActive(const Settings& s);
void vibranceSaturationApply(Image& image, const Settings& s, const OpContext& ctx);
bool curvesActive(const Settings& s);
void curvesApply(Image& image, const Settings& s, const OpContext& ctx);
bool denoiseActive(const Settings& s);
void denoiseApply(Image& image, const Settings& s, const OpContext& ctx);
bool clarityActive(const Settings& s);
void clarityApply(Image& image, const Settings& s, const OpContext& ctx);
bool sharpenActive(const Settings& s);

// Ids of every implemented operation, in pipeline order (checked against the registry by tests).
[[nodiscard]] std::vector<std::string_view> implementedOpIds();
void sharpenApply(Image& image, const Settings& s, const OpContext& ctx);

// Shared helper: scales every pixel's RGB by newY / Y, where newY = mapping(Y) on the Rec.2020
// luminance. Keeps hue and saturation while changing lightness. Near-black pixels get the
// difference added instead, so lifted blacks stay neutral.
template <typename Mapping>
void applyLuminanceMapping(Image& image, const OpContext& ctx, const Mapping& mapping);

} // namespace pfx::detail

#include "ops/ops_inl.h"
