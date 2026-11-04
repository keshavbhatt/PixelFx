// SPDX-License-Identifier: Apache-2.0
#include "pfx/pipeline.h"

#include "core/log_internal.h"
#include "ops/ops.h"
#include "pfx/registry.h"

#include <array>
#include <string>

namespace pfx {
namespace {

// Implementations in pipeline order; every id must be in the registry (checked by a test).
constexpr std::array<detail::OpImpl, 14> kImpls = {{
    {"geometry", detail::geometryActive, detail::geometryApply},
    {"denoise", detail::denoiseActive, detail::denoiseApply},
    {"white_balance", detail::whiteBalanceActive, detail::whiteBalanceApply},
    {"exposure", detail::exposureActive, detail::exposureApply},
    {"tone", detail::toneActive, detail::toneApply},
    {"clarity", detail::clarityActive, detail::clarityApply},
    {"curves", detail::curvesActive, detail::curvesApply},
    {"mixer", detail::mixerActive, detail::mixerApply},
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
    if (input.empty()) {
        return Error{ErrorCode::InvalidArgument, "Nothing to render: the image is empty."};
    }
    Settings checked = settings;
    for (const std::string& line : registry().normalize(checked)) {
        detail::log(LogLevel::Debug, line);
    }
    Image image = input;
    const detail::OpContext ctx{options.threads};
    for (const OpSpec& spec : registry().ops()) {
        if (options.cancel != nullptr && options.cancel->load()) {
            return Error{ErrorCode::InvalidArgument, "Rendering was cancelled."};
        }
        for (const detail::OpImpl& impl : kImpls) {
            if (impl.id == spec.id && impl.active(checked)) {
                impl.apply(image, checked, ctx);
            }
        }
    }
    return image;
}

} // namespace pfx

namespace pfx::detail {

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
