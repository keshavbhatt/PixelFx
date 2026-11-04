// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <vector>

namespace pfx {

// A float RGBA image in the library's working space: linear light, Rec.2020 primaries, D65
// white (ADR-004). Values may exceed 1.0 (highlights with headroom); alpha is straight (not
// premultiplied). Pixels are interleaved R, G, B, A, row by row.
class Image
{
public:
    Image() = default;
    Image(int width, int height);

    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] bool empty() const { return m_width == 0 || m_height == 0; }
    [[nodiscard]] int longEdge() const { return m_width > m_height ? m_width : m_height; }
    [[nodiscard]] std::size_t pixelCount() const;

    [[nodiscard]] float* data() { return m_pixels.data(); }
    [[nodiscard]] const float* data() const { return m_pixels.data(); }
    [[nodiscard]] float* row(int y);
    [[nodiscard]] const float* row(int y) const;
    [[nodiscard]] float* pixel(int x, int y);
    [[nodiscard]] const float* pixel(int x, int y) const;

    void fill(float r, float g, float b, float a = 1.0F);

    bool operator==(const Image& other) const = default;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<float> m_pixels;
};

// Box-filter downscale by an integer factor; used by scale tests and quick previews.
[[nodiscard]] Image downscale(const Image& image, int factor);

} // namespace pfx
