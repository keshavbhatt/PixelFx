// SPDX-License-Identifier: Apache-2.0
#include "image/filters.h"

#include "core/parallel.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pfx::detail {

Plane::Plane(int w, int h)
    : width(w)
    , height(h)
    , values(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0F)
{}

float& Plane::at(int x, int y)
{
    return values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(x)];
}

float Plane::at(int x, int y) const
{
    return values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(x)];
}

namespace {

constexpr double kExactSigmaLimit = 4.0;

std::vector<float> gaussianKernel(double sigma)
{
    const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
    std::vector<float> kernel((static_cast<std::size_t>(radius) * 2) + 1);
    double sum = 0.0;
    for (std::size_t j = 0; j < kernel.size(); ++j) {
        const int i = static_cast<int>(j) - radius;
        const double w = std::exp(-(i * i) / (2.0 * sigma * sigma));
        kernel[j] = static_cast<float>(w);
        sum += w;
    }
    for (float& w : kernel) {
        w = static_cast<float>(double{w} / sum);
    }
    return kernel;
}

// One 1D pass along rows (horizontal) or columns, with a kernel or a box of half-width r.
// Edges clamp. Every pass walks memory along rows, so it stays in cache.
void convolveRows(const Plane& in, Plane& out, const std::vector<float>& kernel, int threads)
{
    const auto radius = static_cast<int>(kernel.size() / 2);
    const int width = in.width;
    parallelFor(in.height, threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* row = &in.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)];
            float* dst = &out.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)];
            for (int x = 0; x < width; ++x) {
                float acc = 0.0F;
                if (x >= radius && x + radius < width) { // interior: no clamping
                    const float* src = row + (x - radius);
                    for (std::size_t j = 0; j < kernel.size(); ++j) {
                        acc += kernel[j] * src[j];
                    }
                } else {
                    for (std::size_t j = 0; j < kernel.size(); ++j) {
                        acc += kernel[j] * row[std::clamp(x + static_cast<int>(j) - radius, 0, width - 1)];
                    }
                }
                dst[x] = acc;
            }
        }
    });
}

void convolveColumns(const Plane& in, Plane& out, const std::vector<float>& kernel, int threads)
{
    const auto radius = static_cast<int>(kernel.size() / 2);
    const auto width = static_cast<std::size_t>(in.width);
    parallelFor(in.height, threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* dst = &out.values[static_cast<std::size_t>(y) * width];
            std::fill(dst, dst + width, 0.0F);
            for (std::size_t j = 0; j < kernel.size(); ++j) {
                const int sy = std::clamp(y + static_cast<int>(j) - radius, 0, in.height - 1);
                const float* src = &in.values[static_cast<std::size_t>(sy) * width];
                const float w = kernel[j];
                for (std::size_t x = 0; x < width; ++x) {
                    dst[x] += w * src[x];
                }
            }
        }
    });
}

void boxRows(const Plane& in, Plane& out, int r, int threads)
{
    const auto norm = 1.0 / static_cast<double>(2 * r + 1);
    parallelFor(in.height, threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            double acc = 0.0;
            for (int k = -r; k <= r; ++k) {
                acc += double{in.at(std::clamp(k, 0, in.width - 1), y)};
            }
            for (int x = 0; x < in.width; ++x) {
                out.at(x, y) = static_cast<float>(acc * norm);
                acc += double{in.at(std::min(x + r + 1, in.width - 1), y)} -
                       double{in.at(std::max(x - r, 0), y)};
            }
        }
    });
}

// A running sum per column, carried down the rows: each thread owns a band of columns and
// reads each row's stretch of it in order.
void boxColumns(const Plane& in, Plane& out, int r, int threads)
{
    const auto norm = 1.0 / static_cast<double>(2 * r + 1);
    const auto width = static_cast<std::size_t>(in.width);
    const auto rowOf = [&in, width](int y) {
        return &in.values[static_cast<std::size_t>(std::clamp(y, 0, in.height - 1)) * width];
    };
    parallelFor(in.width, threads, [&](int x0, int x1) {
        const auto first = static_cast<std::size_t>(x0);
        const auto count = static_cast<std::size_t>(x1 - x0);
        std::vector<double> acc(count, 0.0);
        for (int k = -r; k <= r; ++k) {
            const float* src = rowOf(k) + first;
            for (std::size_t i = 0; i < count; ++i) {
                acc[i] += double{src[i]};
            }
        }
        for (int y = 0; y < in.height; ++y) {
            float* dst = &out.values[static_cast<std::size_t>(y) * width] + first;
            const float* add = rowOf(y + r + 1) + first;
            const float* drop = rowOf(y - r) + first;
            for (std::size_t i = 0; i < count; ++i) {
                dst[i] = static_cast<float>(acc[i] * norm);
                acc[i] += double{add[i]} - double{drop[i]};
            }
        }
    });
}

// Box half-widths whose three passes approximate a Gaussian of `sigma` (Kovesi).
std::array<int, 3> boxRadii(double sigma)
{
    constexpr int kPasses = 3;
    const double ideal = std::sqrt((12.0 * sigma * sigma / kPasses) + 1.0);
    int lower = static_cast<int>(std::floor(ideal));
    if (lower % 2 == 0) {
        --lower;
    }
    const int upper = lower + 2;
    const double m =
        std::round((12.0 * sigma * sigma - kPasses * lower * lower - 4.0 * kPasses * lower - 3.0 * kPasses) /
                   (-4.0 * lower - 4.0));
    std::array<int, 3> radii{};
    for (int i = 0; i < kPasses; ++i) {
        radii[static_cast<std::size_t>(i)] = ((i < m ? lower : upper) - 1) / 2;
    }
    return radii;
}

} // namespace

Plane gaussianBlur(const Plane& source, double sigma, int threads)
{
    if (sigma <= 0.0 || source.width == 0 || source.height == 0) {
        return source;
    }
    Plane tmp(source.width, source.height);
    Plane out(source.width, source.height);
    if (sigma <= kExactSigmaLimit) {
        const auto kernel = gaussianKernel(sigma);
        convolveRows(source, tmp, kernel, threads);
        convolveColumns(tmp, out, kernel, threads);
        return out;
    }
    Plane current = source;
    for (const int r : boxRadii(sigma)) {
        boxRows(current, tmp, r, threads);
        boxColumns(tmp, current, r, threads);
    }
    return current;
}

} // namespace pfx::detail
