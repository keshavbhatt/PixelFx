// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"

#include "pfx/color.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

namespace fixtures {
namespace {

// A stable hash noise in [-1, 1], identical on every platform.
double noise(int x, int y)
{
    auto h = static_cast<std::uint32_t>(x) * 374761393U + static_cast<std::uint32_t>(y) * 668265263U;
    h = (h ^ (h >> 13U)) * 1274126177U;
    h ^= h >> 16U;
    return static_cast<double>(h & 0xFFFFU) / 32767.5 - 1.0;
}

void put(pfx::Image& img, int x, int y, double r, double g, double b)
{
    float* p = img.pixel(x, y);
    p[0] = static_cast<float>(std::max(0.0, r));
    p[1] = static_cast<float>(std::max(0.0, g));
    p[2] = static_cast<float>(std::max(0.0, b));
    p[3] = 1.0F;
}

} // namespace

pfx::Image scene(int width, int height)
{
    pfx::Image img(width, height);
    const double horizon = height * 0.55;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double u = static_cast<double>(x) / width;
            const double v = static_cast<double>(y) / height;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            if (y < horizon) {
                r = 0.10 + 0.55 * v;
                g = 0.15 + 0.35 * v;
                b = 0.45 - 0.20 * v;
                const double d = std::hypot(u - 0.7, v - 0.35);
                const double sun = std::exp(-d * d * 300.0) * 3.0;
                r += sun;
                g += sun * 0.9;
                b += sun * 0.6;
            } else {
                r = 0.05 + 0.04 * u;
                g = 0.12 + 0.10 * (1.0 - v);
                b = 0.04;
            }
            // Coloured patches with hard edges.
            if (v > 0.70 && v < 0.90) {
                const int patch = static_cast<int>(u * 6.0);
                constexpr double kPatches[6][3] = {{0.40, 0.05, 0.04}, {0.05, 0.30, 0.06},
                                                   {0.04, 0.06, 0.40}, {0.45, 0.35, 0.05},
                                                   {0.30, 0.18, 0.12}, {0.70, 0.70, 0.70}};
                r = kPatches[patch][0];
                g = kPatches[patch][1];
                b = kPatches[patch][2];
            }
            const double n = 0.015 * noise(x, y);
            put(img, x, y, r + n, g + n, b + n);
        }
    }
    return img;
}

pfx::Image ramp(int width, int height)
{
    pfx::Image img(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double v = pfx::color::perceptualDecode(static_cast<double>(x) / (width - 1));
            put(img, x, y, v, v, v);
        }
    }
    return img;
}

pfx::Image flat(float r, float g, float b, int width, int height)
{
    pfx::Image img(width, height);
    img.fill(r, g, b, 1.0F);
    return img;
}

Diff compare(const pfx::Image& a, const pfx::Image& b)
{
    Diff d;
    if (a.width() != b.width() || a.height() != b.height()) {
        d.mean = d.max = 1e9;
        return d;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < a.pixelCount() * 4; ++i) {
        if (i % 4 == 3) {
            continue;
        }
        const double e = std::abs(pfx::color::srgbEncode(std::clamp(a.data()[i], 0.0F, 1.0F)) -
                                  pfx::color::srgbEncode(std::clamp(b.data()[i], 0.0F, 1.0F)));
        sum += e;
        d.max = std::max(d.max, e);
    }
    d.mean = sum / static_cast<double>(a.pixelCount() * 3);
    return d;
}

double meanLuma(const pfx::Image& image)
{
    double sum = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const float* p = image.pixel(x, y);
            sum += pfx::color::luminance(p[0], p[1], p[2]);
        }
    }
    return sum / static_cast<double>(image.pixelCount());
}

double lumaStdDev(const pfx::Image& image)
{
    const double mean = meanLuma(image);
    double sum = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const float* p = image.pixel(x, y);
            const double d = pfx::color::luminance(p[0], p[1], p[2]) - mean;
            sum += d * d;
        }
    }
    return std::sqrt(sum / static_cast<double>(image.pixelCount()));
}

TempDir::TempDir()
{
    std::random_device rd;
    m_path = std::filesystem::temp_directory_path() / ("pfx-test-" + std::to_string(rd()));
    std::filesystem::create_directories(m_path);
}

TempDir::~TempDir()
{
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
}

} // namespace fixtures
