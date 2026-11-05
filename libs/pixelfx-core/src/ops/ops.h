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
bool chromaticAberrationActive(const Settings& s);
void chromaticAberrationApply(Image& image, const Settings& s, const OpContext& ctx);
bool defringeActive(const Settings& s);
void defringeApply(Image& image, const Settings& s, const OpContext& ctx);
bool healActive(const Settings& s);
void healApply(Image& image, const Settings& s, const OpContext& ctx);
bool geometryActive(const Settings& s);
void geometryApply(Image& image, const Settings& s, const OpContext& ctx);
bool calibrationActive(const Settings& s);
void calibrationApply(Image& image, const Settings& s, const OpContext& ctx);
bool profileActive(const Settings& s);
void profileApply(Image& image, const Settings& s, const OpContext& ctx);
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
bool textureActive(const Settings& s);
void textureApply(Image& image, const Settings& s, const OpContext& ctx);
bool sharpenActive(const Settings& s);
void sharpenApply(Image& image, const Settings& s, const OpContext& ctx);
bool mixerActive(const Settings& s);
void mixerApply(Image& image, const Settings& s, const OpContext& ctx);
bool blackAndWhiteActive(const Settings& s);
void blackAndWhiteApply(Image& image, const Settings& s, const OpContext& ctx);
bool gradingActive(const Settings& s);
void gradingApply(Image& image, const Settings& s, const OpContext& ctx);
bool dehazeActive(const Settings& s);
void dehazeApply(Image& image, const Settings& s, const OpContext& ctx);
bool vignetteActive(const Settings& s);
void vignetteApply(Image& image, const Settings& s, const OpContext& ctx);
bool grainActive(const Settings& s);
void grainApply(Image& image, const Settings& s, const OpContext& ctx);
bool fadeActive(const Settings& s);
void fadeApply(Image& image, const Settings& s, const OpContext& ctx);

// Runs one operation by its registry id on the image with `settings` (a look step), when they
// are not its identity. False for an id with no implementation.
bool applyOperation(std::string_view id, Image& image, const Settings& settings, const OpContext& ctx);

// Ids of every implemented operation, in pipeline order (checked against the registry by tests).
[[nodiscard]] std::vector<std::string_view> implementedOpIds();

// Shared helper: scales every pixel's RGB by newY / Y, where newY = mapping(Y) on the Rec.2020
// luminance. Keeps hue and saturation while changing lightness. Near-black pixels get the
// difference added instead, so lifted blacks stay neutral.
template <typename Mapping>
void applyLuminanceMapping(Image& image, const OpContext& ctx, const Mapping& mapping);

} // namespace pfx::detail

#include "ops/ops_inl.h"
