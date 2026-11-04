// SPDX-License-Identifier: Apache-2.0
#include "image/filters.h"

#include "core/parallel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

namespace {

// The window minimum along each row: blocks of the window's length, a running minimum forward
// and backward within each block, and each window is the smaller of two of them.
void minRows(const Plane& in, Plane& out, int radius, int threads)
{
    const int width = in.width;
    const int window = (2 * radius) + 1;
    const int padded = width + (2 * radius);
    parallelFor(in.height, threads, [&](int y0, int y1) {
        std::vector<float> forward(static_cast<std::size_t>(padded));
        std::vector<float> backward(static_cast<std::size_t>(padded));
        const auto value = [&](int y, int i) {
            const int x = i - radius;
            return (x < 0 || x >= width) ? std::numeric_limits<float>::max() : in.at(x, y);
        };
        for (int y = y0; y < y1; ++y) {
            for (int i = 0; i < padded; ++i) {
                const float v = value(y, i);
                forward[static_cast<std::size_t>(i)] =
                    (i % window == 0) ? v : std::min(forward[static_cast<std::size_t>(i - 1)], v);
            }
            for (int i = padded - 1; i >= 0; --i) {
                const float v = value(y, i);
                const bool blockEnd = (i % window == window - 1) || i == padded - 1;
                backward[static_cast<std::size_t>(i)] =
                    blockEnd ? v : std::min(backward[static_cast<std::size_t>(i) + 1], v);
            }
            for (int x = 0; x < width; ++x) {
                out.at(x, y) =
                    std::min(backward[static_cast<std::size_t>(x)],
                             forward[static_cast<std::size_t>(x) + (2 * static_cast<std::size_t>(radius))]);
            }
        }
    });
}

Plane transposed(const Plane& in, int threads)
{
    constexpr int kBlock = 64;
    Plane out(in.height, in.width);
    parallelFor((in.height + kBlock - 1) / kBlock, threads, [&](int b0, int b1) {
        for (int by = b0 * kBlock; by < std::min(in.height, b1 * kBlock); by += kBlock) {
            for (int bx = 0; bx < in.width; bx += kBlock) {
                for (int y = by; y < std::min(in.height, by + kBlock); ++y) {
                    for (int x = bx; x < std::min(in.width, bx + kBlock); ++x) {
                        out.at(y, x) = in.at(x, y);
                    }
                }
            }
        }
    });
    return out;
}

Plane multiplied(const Plane& a, const Plane& b)
{
    Plane out(a.width, a.height);
    for (std::size_t i = 0; i < a.values.size(); ++i) {
        out.values[i] = a.values[i] * b.values[i];
    }
    return out;
}

} // namespace

Plane minFilter(const Plane& source, int radius, int threads)
{
    if (radius <= 0 || source.values.empty()) {
        return source;
    }
    Plane rows(source.width, source.height);
    minRows(source, rows, radius, threads);
    const Plane flipped = transposed(rows, threads);
    Plane columns(flipped.width, flipped.height);
    minRows(flipped, columns, radius, threads);
    return transposed(columns, threads);
}

GuidedCoefficients guidedCoefficients(const Plane& guide, const Plane& input, double sigma, double epsilon,
                                      int threads)
{
    const Plane meanI = gaussianBlur(guide, sigma, threads);
    const Plane meanP = gaussianBlur(input, sigma, threads);
    const Plane corrI = gaussianBlur(multiplied(guide, guide), sigma, threads);
    const Plane corrIP = gaussianBlur(multiplied(guide, input), sigma, threads);
    Plane a(guide.width, guide.height);
    Plane b(guide.width, guide.height);
    for (std::size_t i = 0; i < a.values.size(); ++i) {
        const double variance = double{corrI.values[i]} - (double{meanI.values[i]} * double{meanI.values[i]});
        const double covariance =
            double{corrIP.values[i]} - (double{meanI.values[i]} * double{meanP.values[i]});
        const double slope = covariance / (variance + epsilon);
        a.values[i] = static_cast<float>(slope);
        b.values[i] = static_cast<float>(double{meanP.values[i]} - (slope * double{meanI.values[i]}));
    }
    return {gaussianBlur(a, sigma, threads), gaussianBlur(b, sigma, threads)};
}

Plane guidedFilter(const Plane& guide, const Plane& input, double sigma, double epsilon, int threads)
{
    const GuidedCoefficients k = guidedCoefficients(guide, input, sigma, epsilon, threads);
    Plane out(guide.width, guide.height);
    for (std::size_t i = 0; i < out.values.size(); ++i) {
        out.values[i] = (k.slope.values[i] * guide.values[i]) + k.offset.values[i];
    }
    return out;
}

float sampleBilinear(const Plane& plane, double x, double y)
{
    x = std::clamp(x, 0.0, static_cast<double>(plane.width - 1));
    y = std::clamp(y, 0.0, static_cast<double>(plane.height - 1));
    const auto x0 = static_cast<int>(x);
    const auto y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, plane.width - 1);
    const int y1 = std::min(y0 + 1, plane.height - 1);
    const auto fx = static_cast<float>(x - x0);
    const auto fy = static_cast<float>(y - y0);
    const float top = plane.at(x0, y0) + (fx * (plane.at(x1, y0) - plane.at(x0, y0)));
    const float bottom = plane.at(x0, y1) + (fx * (plane.at(x1, y1) - plane.at(x0, y1)));
    return top + (fy * (bottom - top));
}

} // namespace pfx::detail
