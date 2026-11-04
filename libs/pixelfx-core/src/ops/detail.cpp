// SPDX-License-Identifier: Apache-2.0
// Noise reduction, clarity and sharpening. Radii are fractions of the image's long edge
// (ADR-004), so the same settings look the same on a preview and on the full-size export.
#include "image/filters.h"
#include "ops/ops.h"
#include "ops/value_table.h"

#include <cmath>

namespace pfx::detail {

namespace {

/// body(i) for every value of `plane`, split over threads (each value is independent).
template <typename Body>
void forEachValue(const Plane& plane, int threads, const Body& body)
{
    const auto width = static_cast<std::size_t>(plane.width);
    parallelFor(plane.height, threads, [&](int y0, int y1) {
        for (std::size_t i = static_cast<std::size_t>(y0) * width; i < static_cast<std::size_t>(y1) * width;
             ++i) {
            body(i);
        }
    });
}

const ValueTable& encodeTable()
{
    static const ValueTable table([](double v) { return color::perceptualEncode(v); });
    return table;
}

const ValueTable& decodeTable()
{
    static const ValueTable table([](double v) { return color::perceptualDecode(v); });
    return table;
}

Plane perceptualLuma(const Image& image, int threads)
{
    const ValueTable& encode = encodeTable();
    Plane out(image.width(), image.height());
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                out.at(x, y) = static_cast<float>(encode(std::max(0.0, color::luminance(p[0], p[1], p[2]))));
            }
        }
    });
    return out;
}

// Replaces each pixel's luminance with the decoded value of `encoded`, keeping its colour.
void setPerceptualLuma(Image& image, const Plane& encoded, int threads)
{
    constexpr double kTiny = 1e-5;
    const ValueTable& decode = decodeTable();
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double luma = std::max(0.0, color::luminance(p[0], p[1], p[2]));
                const double target = decode(std::max(0.0, double{encoded.at(x, y)}));
                if (luma > kTiny) {
                    const double ratio = target / luma;
                    for (int c = 0; c < 3; ++c) {
                        p[c] = static_cast<float>(double{p[c]} * ratio);
                    }
                } else {
                    for (int c = 0; c < 3; ++c) {
                        p[c] = static_cast<float>(double{p[c]} + (target - luma));
                    }
                }
            }
        }
    });
}

double pixels(double fraction, const Image& image, double minimum)
{
    return std::max(minimum, fraction * image.longEdge());
}

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace

bool denoiseActive(const Settings& s)
{
    return s.number("detail.denoise.luminance", 0.0) > 0.0 || s.number("detail.denoise.color", 0.0) > 0.0;
}

namespace {

// Colour noise: blur each channel's difference from luminance, keep luminance as is.
void denoiseColor(Image& image, double colorAmount, const OpContext& ctx)
{
    constexpr double kColorRadius = 0.004;
    const double sigma = pixels(kColorRadius, image, 1.0);
    Plane diff[3] = {Plane(image.width(), image.height()), Plane(image.width(), image.height()),
                     Plane(image.width(), image.height())};
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const auto luma = static_cast<float>(color::luminance(p[0], p[1], p[2]));
                for (int c = 0; c < 3; ++c) {
                    diff[c].at(x, y) = p[c] - luma;
                }
            }
        }
    });
    for (Plane& d : diff) {
        const Plane blurred = gaussianBlur(d, sigma, ctx.threads);
        forEachValue(d, ctx.threads, [&](std::size_t i) {
            d.values[i] = static_cast<float>(double{d.values[i]} +
                                             colorAmount * double{blurred.values[i] - d.values[i]});
        });
    }
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const auto luma = static_cast<float>(color::luminance(p[0], p[1], p[2]));
                for (int c = 0; c < 3; ++c) {
                    p[c] = luma + diff[c].at(x, y);
                }
            }
        }
    });
}

// Luminance noise: pull towards a small blur where the difference looks like noise, not
// where it looks like an edge.
void denoiseLuma(Image& image, double lumaAmount, const OpContext& ctx)
{
    constexpr double kLumaRadius = 0.0012;
    constexpr double kEdge = 0.04;
    Plane p = perceptualLuma(image, ctx.threads);
    const Plane blurred = gaussianBlur(p, pixels(kLumaRadius, image, 0.6), ctx.threads);
    forEachValue(p, ctx.threads, [&](std::size_t i) {
        const auto d = double{blurred.values[i] - p.values[i]};
        const double keep = std::exp(-(d * d) / (kEdge * kEdge));
        p.values[i] = static_cast<float>(double{p.values[i]} + lumaAmount * keep * d);
    });
    setPerceptualLuma(image, p, ctx.threads);
}

} // namespace

void denoiseApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double lumaAmount = s.number("detail.denoise.luminance", 0.0) / 100.0;
    const double colorAmount = s.number("detail.denoise.color", 0.0) / 100.0;
    if (colorAmount > 0.0) {
        denoiseColor(image, colorAmount, ctx);
    }
    if (lumaAmount > 0.0) {
        denoiseLuma(image, lumaAmount, ctx);
    }
}

bool clarityActive(const Settings& s)
{
    return s.number("detail.clarity", 0.0) != 0.0;
}

// Clarity: local contrast at a large radius, strongest in the midtones so whites and blacks
// do not clip.
void clarityApply(Image& image, const Settings& s, const OpContext& ctx)
{
    constexpr double kRadius = 0.015;
    constexpr double kStrength = 0.8;
    const double amount = kStrength * s.number("detail.clarity", 0.0) / 100.0;
    Plane p = perceptualLuma(image, ctx.threads);
    const Plane blurred = gaussianBlur(p, pixels(kRadius, image, 1.0), ctx.threads);
    forEachValue(p, ctx.threads, [&](std::size_t i) {
        const auto v = double{p.values[i]};
        const double centred = (2.0 * std::clamp(v, 0.0, 1.0)) - 1.0;
        const double mid = 1.0 - (centred * centred);
        p.values[i] = static_cast<float>(std::max(0.0, v + amount * (v - double{blurred.values[i]}) * mid));
    });
    setPerceptualLuma(image, p, ctx.threads);
}

bool sharpenActive(const Settings& s)
{
    return s.number("detail.sharpen.amount", 0.0) > 0.0;
}

// Unsharp mask on perceptual luminance (no colour fringes); masking limits it to edges.
void sharpenApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = s.number("detail.sharpen.amount", 0.0) / 100.0;
    const double radius = s.number("detail.sharpen.radius", 0.0004);
    const double threshold = s.number("detail.sharpen.masking", 0.0) / 100.0 * 0.05;
    Plane p = perceptualLuma(image, ctx.threads);
    const Plane blurred = gaussianBlur(p, pixels(radius, image, 0.3), ctx.threads);
    forEachValue(p, ctx.threads, [&](std::size_t i) {
        const auto d = double{p.values[i] - blurred.values[i]};
        const double mask = threshold > 0.0 ? smoothstep(0.0, threshold, std::abs(d)) : 1.0;
        p.values[i] = static_cast<float>(std::max(0.0, double{p.values[i]} + amount * d * mask));
    });
    setPerceptualLuma(image, p, ctx.threads);
}

} // namespace pfx::detail
