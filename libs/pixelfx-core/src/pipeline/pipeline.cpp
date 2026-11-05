// SPDX-License-Identifier: Apache-2.0
#include "pfx/pipeline.h"

#include "core/log_internal.h"
#include "ops/ops.h"
#include "pfx/registry.h"

#include <array>
#include <string>

namespace pfx {
namespace {

constexpr int kLookStage = 100; // between grading (90) and sharpen (110)

// Implementations in pipeline order; every id must be in the registry (checked by a test).
constexpr std::array<detail::OpImpl, 23> kImpls = {{
    {"lens_profile", detail::lensProfileActive, detail::lensProfileApply},
    {"chromatic_aberration", detail::chromaticAberrationActive, detail::chromaticAberrationApply},
    {"defringe", detail::defringeActive, detail::defringeApply},
    {"heal", detail::healActive, detail::healApply},
    {"geometry", detail::geometryActive, detail::geometryApply},
    {"denoise", detail::denoiseActive, detail::denoiseApply},
    {"white_balance", detail::whiteBalanceActive, detail::whiteBalanceApply},
    {"calibration", detail::calibrationActive, detail::calibrationApply},
    {"profile", detail::profileActive, detail::profileApply},
    {"exposure", detail::exposureActive, detail::exposureApply},
    {"dehaze", detail::dehazeActive, detail::dehazeApply},
    {"tone", detail::toneActive, detail::toneApply},
    {"clarity", detail::clarityActive, detail::clarityApply},
    {"texture", detail::textureActive, detail::textureApply},
    {"curves", detail::curvesActive, detail::curvesApply},
    {"mixer", detail::mixerActive, detail::mixerApply},
    {"bw", detail::blackAndWhiteActive, detail::blackAndWhiteApply},
    {"vibrance_saturation", detail::vibranceSaturationActive, detail::vibranceSaturationApply},
    {"grading", detail::gradingActive, detail::gradingApply},
    {"sharpen", detail::sharpenActive, detail::sharpenApply},
    {"vignette", detail::vignetteActive, detail::vignetteApply},
    {"grain", detail::grainActive, detail::grainApply},
    {"fade", detail::fadeActive, detail::fadeApply},
}};

} // namespace

Result<Image> render(const Image& input, const Settings& settings, const RenderOptions& options)
{
    return render(Image(input), settings, options);
}

Result<Image> render(Image&& input, const Settings& settings, const RenderOptions& options)
{
    if (input.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nothing to render: the image is empty."};
    }
    Settings checked = settings;
    for (const std::string& line : registry().normalize(checked)) {
        detail::log(LogLevel::Debug, line);
    }
    Image image = std::move(input);
    const detail::OpContext ctx{options.threads, options.lens};
    bool lookDone = options.look == nullptr;
    for (const OpSpec& spec : registry().ops()) {
        if (options.cancel != nullptr && options.cancel->load()) {
            return Error{ErrorCode::InvalidArgument, "Rendering was cancelled."};
        }
        // The look after colour grading, before sharpening and the effects (presets.md section 5).
        if (!lookDone && spec.stage > kLookStage) {
            applyLook(image, *options.look, options.lookStrength, options.threads);
            lookDone = true;
        }
        for (const detail::OpImpl& impl : kImpls) {
            if (impl.id == spec.id && impl.active(checked)) {
                impl.apply(image, checked, ctx);
            }
        }
    }
    if (!lookDone) {
        applyLook(image, *options.look, options.lookStrength, options.threads);
    }
    return image;
}

} // namespace pfx

namespace pfx::detail {

bool applyOperation(std::string_view id, Image& image, const Settings& settings, const OpContext& ctx)
{
    for (const OpImpl& impl : kImpls) {
        if (impl.id == id) {
            if (impl.active(settings)) {
                impl.apply(image, settings, ctx);
            }
            return true;
        }
    }
    return false;
}

std::vector<std::string_view> implementedOpIds()
{
    std::vector<std::string_view> ids;
    ids.reserve(kImpls.size());
    for (const OpImpl& impl : kImpls) {
        ids.push_back(impl.id);
    }
    return ids;
}

} // namespace pfx::detail
