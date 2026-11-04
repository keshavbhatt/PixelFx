// SPDX-License-Identifier: Apache-2.0
#include "pfx/composite.h"

#include "pfx/color.h"

#include <algorithm>
#include <array>

namespace pfx {

namespace {

// sRGB byte to linear light, once for all 256 values.
const std::array<float, 256>& decodeTable()
{
    static const std::array<float, 256> table = [] {
        std::array<float, 256> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            t.at(i) = static_cast<float>(color::srgbDecode(static_cast<double>(i) / 255.0));
        }
        return t;
    }();
    return table;
}

} // namespace

void blendOverlay(Image& image, const OverlayView& overlay, float opacity)
{
    opacity = std::clamp(opacity, 0.0F, 1.0F);
    if (overlay.pixels == nullptr || opacity <= 0.0F) {
        return;
    }
    const int x0 = std::max(0, overlay.x);
    const int y0 = std::max(0, overlay.y);
    const int x1 = std::min(image.width(), overlay.x + overlay.width);
    const int y1 = std::min(image.height(), overlay.y + overlay.height);
    const auto& decode = decodeTable();
    const color::Mat3& m = color::kSrgbToRec2020;
    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* src =
            overlay.pixels + (static_cast<std::ptrdiff_t>(y - overlay.y) * overlay.stride);
        float* dst = image.row(y);
        for (int x = x0; x < x1; ++x) {
            const std::uint8_t* s = src + (static_cast<std::ptrdiff_t>(x - overlay.x) * 4);
            const auto alpha = static_cast<unsigned>(s[3]);
            if (alpha == 0) {
                continue;
            }
            // Unpremultiply in 8 bits (as drawn), decode, then move to Rec.2020.
            const auto straight = [alpha](std::uint8_t c) {
                return static_cast<std::size_t>(
                    std::min(255U, ((static_cast<unsigned>(c) * 255U) + (alpha / 2)) / alpha));
            };
            const auto r = static_cast<double>(decode.at(straight(s[0])));
            const auto g = static_cast<double>(decode.at(straight(s[1])));
            const auto b = static_cast<double>(decode.at(straight(s[2])));
            const float a = opacity * static_cast<float>(alpha) / 255.0F;
            float* d = dst + (static_cast<std::ptrdiff_t>(x) * 4);
            for (std::size_t c = 0; c < 3; ++c) {
                const auto value = static_cast<float>((m.at(c)[0] * r) + (m.at(c)[1] * g) + (m.at(c)[2] * b));
                d[c] = (d[c] * (1.0F - a)) + (value * a);
            }
            d[3] = a + (d[3] * (1.0F - a));
        }
    }
}

double meanLuminance(const Image& image, int x, int y, int width, int height)
{
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(image.width(), x + width);
    const int y1 = std::min(image.height(), y + height);
    if (x1 <= x0 || y1 <= y0) {
        return 0.0;
    }
    double sum = 0.0;
    for (int row = y0; row < y1; ++row) {
        const float* p = image.row(row) + (static_cast<std::ptrdiff_t>(x0) * 4);
        for (int col = x0; col < x1; ++col, p += 4) {
            sum += color::luminance(p[0], p[1], p[2]);
        }
    }
    return sum / (static_cast<double>(x1 - x0) * static_cast<double>(y1 - y0));
}

} // namespace pfx
