// SPDX-License-Identifier: Apache-2.0
// Creative operations, from PixelFx 1, as steps for looks (FEATURES E9): sepia, colorize,
// threshold, invert, blur, pixelate, emboss and edges. Each has an amount (0 none, 100 full) or a
// size; sizes are shares of the long edge, so a preview and the export agree.
#include "image/filters.h"
#include "ops/ops.h"
#include "pfx/color.h"

#include <algorithm>
#include <cmath>

namespace pfx::detail {

namespace {

constexpr double kTiny = 1e-6;

// Runs `body(pixel)` on every pixel, split over threads.
template <typename Body>
void forEachPixel(Image& image, int threads, const Body& body)
{
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                body(p);
            }
        }
    });
}

// Moves the pixel `amount` of the way to `target` (linear light).
void blendTo(float* p, const color::Vec3& target, double amount)
{
    for (std::size_t c = 0; c < 3; ++c) {
        p[c] = static_cast<float>(double{p[c]} + (amount * (target.at(c) - double{p[c]})));
    }
}

// A colour of sRGB `encoded` in the working space, scaled to luminance 1.
color::Vec3 unitColour(const color::Vec3& encoded)
{
    color::Vec3 linear{};
    for (std::size_t c = 0; c < 3; ++c) {
        linear.at(c) = color::srgbDecode(encoded.at(c));
    }
    const color::Vec3 working = color::transform(color::kSrgbToRec2020, linear);
    const double y = std::max(kTiny, color::luminance(working[0], working[1], working[2]));
    return {working[0] / y, working[1] / y, working[2] / y};
}

double lumaOf(const float* p)
{
    return std::max(0.0, color::luminance(p[0], p[1], p[2]));
}

// The perceptual luminance of every pixel (0 black, 1 white).
Plane perceptualPlane(const Image& image, int threads)
{
    Plane out(image.width(), image.height());
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                out.at(x, y) = static_cast<float>(color::perceptualEncode(lumaOf(p)));
            }
        }
    });
    return out;
}

// Each pixel moved `amount` of the way to the neutral grey `grey(x, y)` gives (perceptual).
template <typename Grey>
void blendToGrey(Image& image, double amount, int threads, const Grey& grey)
{
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double v = color::perceptualDecode(std::clamp(grey(x, y), 0.0, 1.0));
                blendTo(p, {v, v, v}, amount);
            }
        }
    });
}

double amountOf(const Settings& s, const char* key)
{
    return std::clamp(s.number(key, 0.0), 0.0, 100.0) / 100.0;
}

} // namespace

bool sepiaActive(const Settings& s)
{
    return amountOf(s, "creative.sepia.amount") > 0.0;
}

// The photo's light in an old print's brown, darker browns in the shadows.
void sepiaApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = amountOf(s, "creative.sepia.amount");
    const color::Vec3 tone = unitColour({0.635, 0.541, 0.396});
    forEachPixel(image, ctx.threads, [&](float* p) {
        const double y = lumaOf(p);
        blendTo(p, {tone[0] * y, tone[1] * y, tone[2] * y}, amount);
    });
}

bool colorizeActive(const Settings& s)
{
    return amountOf(s, "creative.colorize.amount") > 0.0;
}

// Every pixel takes one hue (degrees, as on a colour wheel) at `saturation`, keeping its light.
void colorizeApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = amountOf(s, "creative.colorize.amount");
    const double hue = std::fmod(std::fmod(s.number("creative.colorize.hue", 30.0), 360.0) + 360.0, 360.0);
    const double saturation = std::clamp(s.number("creative.colorize.saturation", 50.0), 0.0, 100.0) / 100.0;
    // The hue at full saturation and brightness (HSV), mixed with white by the saturation.
    color::Vec3 pure{};
    for (std::size_t c = 0; c < 3; ++c) {
        const double k =
            std::fmod((static_cast<double>(5 - (2 * static_cast<int>(c))) + (hue / 60.0)) + 6.0, 6.0);
        pure.at(c) = 1.0 - std::clamp(std::min(k, 4.0 - k), 0.0, 1.0);
    }
    color::Vec3 encoded{};
    for (std::size_t c = 0; c < 3; ++c) {
        encoded.at(c) = 1.0 - (saturation * (1.0 - pure.at(c)));
    }
    const color::Vec3 tone = unitColour(encoded);
    forEachPixel(image, ctx.threads, [&](float* p) {
        const double y = lumaOf(p);
        blendTo(p, {tone[0] * y, tone[1] * y, tone[2] * y}, amount);
    });
}

bool thresholdActive(const Settings& s)
{
    return amountOf(s, "creative.threshold.amount") > 0.0;
}

// Black or white by the pixel's light against `level` (0 to 100, perceptual), with a narrow
// soft step so edges do not stair.
void thresholdApply(Image& image, const Settings& s, const OpContext& ctx)
{
    constexpr double kSoft = 0.01;
    const double amount = amountOf(s, "creative.threshold.amount");
    const double level = std::clamp(s.number("creative.threshold.level", 50.0), 0.0, 100.0) / 100.0;
    forEachPixel(image, ctx.threads, [&](float* p) {
        const double light = color::perceptualEncode(lumaOf(p));
        const double t = std::clamp((light - level + kSoft) / (2.0 * kSoft), 0.0, 1.0);
        const double v = t * t * (3.0 - (2.0 * t));
        blendTo(p, {v, v, v}, amount);
    });
}

bool invertActive(const Settings& s)
{
    return amountOf(s, "creative.invert.amount") > 0.0;
}

// A negative: each channel turned over in perceptual terms, so mid grey stays mid grey.
void invertApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = amountOf(s, "creative.invert.amount");
    forEachPixel(image, ctx.threads, [&](float* p) {
        color::Vec3 target{};
        for (std::size_t c = 0; c < 3; ++c) {
            const double e = color::perceptualEncode(std::clamp(double{p[c]}, 0.0, 1.0));
            target.at(c) = color::perceptualDecode(1.0 - e);
        }
        blendTo(p, target, amount);
    });
}

bool blurActive(const Settings& s)
{
    return s.number("creative.blur.radius", 0.0) > 0.0;
}

// A Gaussian blur of every channel; the radius is a share of the long edge.
void blurApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double sigma = std::clamp(s.number("creative.blur.radius", 0.0), 0.0, 0.05) * image.longEdge();
    if (sigma < 0.3) {
        return;
    }
    for (int c = 0; c < 4; ++c) {
        Plane plane(image.width(), image.height());
        for (int y = 0; y < image.height(); ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x) {
                plane.at(x, y) = p[(4 * x) + c];
            }
        }
        const Plane blurred = gaussianBlur(plane, sigma, ctx.threads);
        for (int y = 0; y < image.height(); ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x) {
                p[(4 * x) + c] = blurred.at(x, y);
            }
        }
    }
}

bool pixelateActive(const Settings& s)
{
    return s.number("creative.pixelate.size", 0.0) > 0.0;
}

// Square cells of one colour each, the average of the pixels they cover; the cell is a share of
// the long edge, laid from the top left corner.
void pixelateApply(Image& image, const Settings& s, const OpContext& /*ctx*/)
{
    const double cell = std::clamp(s.number("creative.pixelate.size", 0.0), 0.0, 0.05) * image.longEdge();
    if (cell < 1.0) {
        return;
    }
    const auto cellsX = static_cast<std::size_t>(std::ceil(image.width() / cell));
    const auto cellsY = static_cast<std::size_t>(std::ceil(image.height() / cell));
    std::vector<double> sums(cellsX * cellsY * 5, 0.0);
    const auto cellOf = [&](int x, int y) {
        const auto cx = std::min(cellsX - 1, static_cast<std::size_t>(x / cell));
        const auto cy = std::min(cellsY - 1, static_cast<std::size_t>(y / cell));
        return ((cy * cellsX) + cx) * 5;
    };
    for (int y = 0; y < image.height(); ++y) {
        const float* p = image.row(y);
        for (int x = 0; x < image.width(); ++x, p += 4) {
            const std::size_t at = cellOf(x, y);
            for (std::size_t c = 0; c < 4; ++c) {
                sums[at + c] += double{p[c]};
            }
            sums[at + 4] += 1.0;
        }
    }
    for (int y = 0; y < image.height(); ++y) {
        float* p = image.row(y);
        for (int x = 0; x < image.width(); ++x, p += 4) {
            const std::size_t at = cellOf(x, y);
            for (std::size_t c = 0; c < 4; ++c) {
                p[c] = static_cast<float>(sums[at + c] / sums[at + 4]);
            }
        }
    }
}

bool embossActive(const Settings& s)
{
    return amountOf(s, "creative.emboss.amount") > 0.0;
}

// The photo pressed into grey metal: light from the top left, the depth from the light's change
// over a short distance (a share of the long edge).
void embossApply(Image& image, const Settings& s, const OpContext& ctx)
{
    constexpr double kDistance = 0.0015;
    constexpr double kDepth = 2.0;
    const double amount = amountOf(s, "creative.emboss.amount");
    const double d = std::max(0.25, kDistance * image.longEdge());
    const Plane light = perceptualPlane(image, ctx.threads);
    blendToGrey(image, amount, ctx.threads, [&](int x, int y) {
        const auto ahead = double{sampleBilinear(light, x + d, y + d)};
        const auto behind = double{sampleBilinear(light, x - d, y - d)};
        return 0.5 + (kDepth * (ahead - behind));
    });
}

bool edgesActive(const Settings& s)
{
    return amountOf(s, "creative.edges.amount") > 0.0;
}

// The outlines of the photo, white on black: how fast the light changes (Sobel) over a short
// distance (a share of the long edge), measured on a lightly smoothed copy.
void edgesApply(Image& image, const Settings& s, const OpContext& ctx)
{
    constexpr double kDistance = 0.0015;
    constexpr double kGain = 8.0;
    const double amount = amountOf(s, "creative.edges.amount");
    const double d = std::max(0.25, kDistance * image.longEdge());
    const Plane light = gaussianBlur(perceptualPlane(image, ctx.threads), d / 2.0, ctx.threads);
    blendToGrey(image, amount, ctx.threads, [&](int x, int y) {
        const auto at = [&](double dx, double dy) {
            return double{sampleBilinear(light, x + (dx * d), y + (dy * d))};
        };
        const double gx =
            (at(1, -1) + (2 * at(1, 0)) + at(1, 1)) - (at(-1, -1) + (2 * at(-1, 0)) + at(-1, 1));
        const double gy =
            (at(-1, 1) + (2 * at(0, 1)) + at(1, 1)) - (at(-1, -1) + (2 * at(0, -1)) + at(1, -1));
        return kGain * std::hypot(gx, gy) / 8.0;
    });
}

} // namespace pfx::detail
