#pragma once

#include <array>
#include <cstdint>

class QImage;

namespace pixelfx::core {

/// 256-bin counts per channel of an 8-bit display image (FEATURES E12). Pure.
struct Histogram
{
    std::array<std::array<std::uint32_t, 256>, 3> rgb{};
    std::uint64_t pixels = 0;

    /// Share of pixels at 0 or 255 in any channel: clipped shadows and highlights.
    double clippedShadows = 0.0;
    double clippedHighlights = 0.0;

    [[nodiscard]] bool isEmpty() const { return pixels == 0; }
    /// The tallest bin, ignoring the two end bins so clipping does not flatten the rest.
    [[nodiscard]] std::uint32_t peak() const;
};

/// Counts `image` (any format; converted to RGBA8888 if needed). Fully transparent pixels are
/// skipped.
[[nodiscard]] Histogram histogramOf(const QImage& source);

} // namespace pixelfx::core
