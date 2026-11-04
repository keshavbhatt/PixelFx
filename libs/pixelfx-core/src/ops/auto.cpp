// SPDX-License-Identifier: Apache-2.0
#include "pfx/auto.h"

#include "pfx/color.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pfx {

namespace {

constexpr int kBins = 1024;
constexpr double kMiddleGrey = 0.4633; // 0.18 in the perceptual encoding

struct Stats
{
    std::array<double, kBins> histogram{};
    double count = 0.0;
    double midR = 0.0;
    double midG = 0.0;
    double midB = 0.0;
    double midCount = 0.0;
    double chroma = 0.0;
};

Stats gather(const Image& image)
{
    Stats s;
    // About 250 000 samples are plenty for percentiles; keep it fast on large images.
    const int step =
        std::max(1, static_cast<int>(std::sqrt(static_cast<double>(image.pixelCount()) / 250000.0)));
    for (int y = 0; y < image.height(); y += step) {
        for (int x = 0; x < image.width(); x += step) {
            const float* p = image.pixel(x, y);
            if (p[3] <= 0.0F) {
                continue;
            }
            const auto r = double{std::max(0.0F, p[0])};
            const auto g = double{std::max(0.0F, p[1])};
            const auto b = double{std::max(0.0F, p[2])};
            const double pe = color::perceptualEncode(color::luminance(r, g, b));
            const auto bin = static_cast<std::size_t>(std::clamp(pe, 0.0, 1.0) * (kBins - 1));
            s.histogram[bin] += 1.0;
            s.count += 1.0;
            const double hi = std::max({r, g, b});
            s.chroma += hi > 1e-6 ? (hi - std::min({r, g, b})) / hi : 0.0;
            if (pe > 0.2 && pe < 0.85) { // grey world on the midtones only
                s.midR += r;
                s.midG += g;
                s.midB += b;
                s.midCount += 1.0;
            }
        }
    }
    return s;
}

double percentile(const Stats& s, double fraction)
{
    const double target = fraction * s.count;
    double seen = 0.0;
    for (std::size_t i = 0; i < s.histogram.size(); ++i) {
        seen += s.histogram[i];
        if (seen >= target) {
            return static_cast<double>(i) / (kBins - 1);
        }
    }
    return 1.0;
}

void put(Settings& settings, const char* key, double value, double precision)
{
    const double rounded = std::round(value / precision) * precision;
    if (std::abs(rounded) >= precision / 2.0) {
        settings.set(key, rounded);
    }
}

// Shifts a perceptual value by an exposure change in stops.
double expose(double perceptual, double ev)
{
    return color::perceptualEncode(color::perceptualDecode(perceptual) * std::exp2(ev));
}

} // namespace

Settings autoAdjust(const Image& image)
{
    Settings out;
    if (image.empty()) {
        return out;
    }
    const Stats s = gather(image);
    if (s.count < 1.0) {
        return out;
    }
    const double median = std::max(percentile(s, 0.5), 0.02);
    const double ev = std::clamp(
        0.7 * std::log2(color::perceptualDecode(kMiddleGrey) / color::perceptualDecode(median)), -2.0, 2.0);
    put(out, "light.exposure", std::abs(ev) < 0.1 ? 0.0 : ev, 0.01);
    const double appliedEv = out.number("light.exposure", 0.0);

    const double low = expose(percentile(s, 0.005), appliedEv);
    const double high = expose(percentile(s, 0.995), appliedEv);
    put(out, "light.blacks", -std::clamp((low - 0.03) / 0.10 * 40.0, 0.0, 40.0), 1.0);
    put(out, "light.whites", std::clamp((0.96 - high) / 0.25 * 40.0, 0.0, 40.0), 1.0);
    const double spread = expose(percentile(s, 0.9), appliedEv) - expose(percentile(s, 0.1), appliedEv);
    put(out, "light.contrast", std::clamp((0.55 - spread) / 0.3 * 25.0, 0.0, 25.0), 1.0);

    if (s.midCount > 0.0) {
        const double r = s.midR / s.midCount;
        const double g = s.midG / s.midCount;
        const double b = s.midB / s.midCount;
        if (r > 1e-6 && b > 1e-6 && g > 1e-6) {
            const double warmth = std::log2(b / r);              // positive: too blue, warm it
            const double green = std::log2(g / ((r + b) / 2.0)); // positive: too green, add magenta
            if (std::abs(warmth) > 0.15) {
                put(out, "color.wb.temperature", std::clamp(25.0 * warmth, -30.0, 30.0), 1.0);
            }
            if (std::abs(green) > 0.12) {
                put(out, "color.wb.tint", std::clamp(30.0 * green, -20.0, 20.0), 1.0);
            }
        }
    }
    const double meanChroma = s.chroma / s.count;
    if (meanChroma < 0.25) {
        put(out, "color.vibrance", std::clamp((0.25 - meanChroma) / 0.25 * 25.0, 0.0, 25.0), 1.0);
    }
    return out;
}

} // namespace pfx
