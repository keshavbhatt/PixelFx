// SPDX-License-Identifier: Apache-2.0
#include "pfx/image.h"

#include <algorithm>

namespace pfx {

namespace {
constexpr std::size_t kChannels = 4;
} // namespace

Image::Image(int width, int height)
    : m_width(std::max(0, width))
    , m_height(std::max(0, height))
    , m_pixels(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * kChannels, 0.0F)
{}

std::size_t Image::pixelCount() const
{
    return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
}

float* Image::row(int y)
{
    return m_pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) * kChannels;
}

const float* Image::row(int y) const
{
    return m_pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) * kChannels;
}

float* Image::pixel(int x, int y)
{
    return row(y) + static_cast<std::size_t>(x) * kChannels;
}

const float* Image::pixel(int x, int y) const
{
    return row(y) + static_cast<std::size_t>(x) * kChannels;
}

void Image::fill(float r, float g, float b, float a)
{
    for (std::size_t i = 0; i < m_pixels.size(); i += kChannels) {
        m_pixels[i] = r;
        m_pixels[i + 1] = g;
        m_pixels[i + 2] = b;
        m_pixels[i + 3] = a;
    }
}

Image downscale(const Image& image, int factor)
{
    if (factor <= 1) {
        return image;
    }
    const int width = image.width() / factor;
    const int height = image.height() / factor;
    Image out(width, height);
    const auto area = static_cast<float>(factor * factor);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum[kChannels] = {};
            for (int dy = 0; dy < factor; ++dy) {
                for (int dx = 0; dx < factor; ++dx) {
                    const float* p = image.pixel(x * factor + dx, y * factor + dy);
                    for (std::size_t c = 0; c < kChannels; ++c) {
                        sum[c] += p[c];
                    }
                }
            }
            float* q = out.pixel(x, y);
            for (std::size_t c = 0; c < kChannels; ++c) {
                q[c] = sum[c] / area;
            }
        }
    }
    return out;
}

} // namespace pfx
