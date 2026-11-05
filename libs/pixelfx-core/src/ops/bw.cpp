// SPDX-License-Identifier: Apache-2.0
// Black and white (FEATURES P8): the photo turned to grey from its luminance, with each colour
// band made lighter or darker by the B&W mix, as a coloured filter on black-and-white film
// would. A band's change follows how saturated the pixel is, so greys keep their tone.
#include "ops/hue_bands.h"
#include "ops/ops.h"

#include <array>
#include <cmath>
#include <string>

namespace pfx::detail {

namespace {

constexpr double kMixReach = 1.0; // stops of lightness, in the perceptual encoding, at +-100

std::array<double, 8> mixOf(const Settings& s)
{
    std::array<double, 8> mix{};
    for (std::size_t i = 0; i < kHueBands.size(); ++i) {
        mix.at(i) = s.number(std::string("bw.mix.") + kHueBands.at(i), 0.0) / 100.0;
    }
    return mix;
}

} // namespace

bool blackAndWhiteActive(const Settings& s)
{
    return s.boolean("bw.enabled", false);
}

void blackAndWhiteApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const std::array<double, 8> mix = mixOf(s);
    const bool mixed = std::ranges::any_of(mix, [](double m) { return m != 0.0; });
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                double grey = std::max(0.0, color::luminance(p[0], p[1], p[2]));
                if (mixed) {
                    std::array<double, 3> e{};
                    for (std::size_t c = 0; c < 3; ++c) {
                        e.at(c) = color::perceptualEncode(std::max(0.0, double{p[c]}));
                    }
                    const Hsv hsv = toHsv(e);
                    const double change = bandValue(mix, hsv.hue) * smoothstep(0.0, 0.6, hsv.saturation);
                    grey = color::perceptualDecode(color::perceptualEncode(grey) *
                                                   std::exp2(kMixReach * change));
                }
                p[0] = p[1] = p[2] = static_cast<float>(grey);
            }
        }
    });
}

} // namespace pfx::detail
