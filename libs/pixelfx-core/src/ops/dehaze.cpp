// SPDX-License-Identifier: Apache-2.0
// Dehaze (FEATURES E6), after He, Sun and Tang's dark channel prior: in a clear photo almost
// every small area has some channel near black; haze lifts that floor toward the haze colour.
// The haze colour comes from the haziest bright areas, the haze amount from a minimum filter,
// smoothed along the photo's edges by a guided filter so no halos form. Negative amounts add
// haze. The haze map is worked out on a copy about 1000 pixels across and applied at full size
// through the guided filter's coefficients: a preview and the export get the same map.
#include "image/filters.h"
#include "ops/ops.h"
#include "ops/value_table.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pfx::detail {

namespace {

constexpr double kWindow = 0.008;   // minimum-filter radius, share of the long edge
constexpr double kSmoothing = 0.02; // guided filter, share of the long edge
constexpr double kFloor = 0.1;      // never divide by less transmission than this
constexpr double kBrightShare = 0.001;
constexpr int kWorkEdge = 1024; // long edge of the copy the haze map is worked out on

// The mean of each `factor` by `factor` block (smaller blocks at the right and bottom edges).
Image boxDownscale(const Image& image, int factor, int threads)
{
    const int w = (image.width() + factor - 1) / factor;
    const int h = (image.height() + factor - 1) / factor;
    Image out(w, h);
    parallelFor(h, threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = 0; x < w; ++x) {
                std::array<double, 4> sum{};
                int n = 0;
                for (int sy = y * factor; sy < std::min(image.height(), (y + 1) * factor); ++sy) {
                    for (int sx = x * factor; sx < std::min(image.width(), (x + 1) * factor); ++sx) {
                        const float* p = image.pixel(sx, sy);
                        for (std::size_t c = 0; c < 4; ++c) {
                            sum.at(c) += double{p[c]};
                        }
                        ++n;
                    }
                }
                float* q = out.pixel(x, y);
                for (std::size_t c = 0; c < 4; ++c) {
                    q[c] = static_cast<float>(sum.at(c) / n);
                }
            }
        }
    });
    return out;
}

struct Encoded
{
    std::array<Plane, 3> channels;
    Plane dark; // the smallest channel of each pixel
    Plane luma; // the guide
};

Encoded encode(const Image& image, int threads)
{
    static const ValueTable table([](double v) { return color::perceptualEncode(v); });
    Encoded e;
    for (Plane& c : e.channels) {
        c = Plane(image.width(), image.height());
    }
    e.dark = Plane(image.width(), image.height());
    e.luma = Plane(image.width(), image.height());
    parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                float smallest = std::numeric_limits<float>::max();
                for (std::size_t c = 0; c < 3; ++c) {
                    const auto v = static_cast<float>(table(std::max(0.0, double{p[c]})));
                    e.channels.at(c).at(x, y) = v;
                    smallest = std::min(smallest, v);
                }
                e.dark.at(x, y) = smallest;
                e.luma.at(x, y) =
                    static_cast<float>(table(std::max(0.0, color::luminance(p[0], p[1], p[2]))));
            }
        }
    });
    return e;
}

// The haze colour: the mean colour of the pixels whose filtered dark channel is brightest.
std::array<double, 3> hazeColour(const Encoded& e, const Plane& darkFiltered)
{
    std::vector<float> sorted = darkFiltered.values;
    const auto count =
        std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<double>(sorted.size()) * kBrightShare));
    std::nth_element(sorted.begin(), sorted.end() - static_cast<std::ptrdiff_t>(count), sorted.end());
    const float threshold = *(sorted.end() - static_cast<std::ptrdiff_t>(count));
    std::array<double, 3> sum{};
    std::size_t n = 0;
    for (std::size_t i = 0; i < darkFiltered.values.size(); ++i) {
        if (darkFiltered.values[i] >= threshold) {
            for (std::size_t c = 0; c < 3; ++c) {
                sum.at(c) += double{e.channels.at(c).values[i]};
            }
            ++n;
        }
    }
    for (double& s : sum) {
        s = std::clamp(s / static_cast<double>(n), 0.2, 1.0);
    }
    return sum;
}

} // namespace

bool dehazeActive(const Settings& s)
{
    return s.number("detail.dehaze", 0.0) != 0.0;
}

void dehazeApply(Image& image, const Settings& s, const OpContext& ctx)
{
    const double amount = s.number("detail.dehaze", 0.0) / 100.0;
    const int factor = std::max(1, (image.longEdge() + kWorkEdge - 1) / kWorkEdge);
    const Image small = factor > 1 ? boxDownscale(image, factor, ctx.threads) : image;
    const auto radius = std::max(1, static_cast<int>(std::lround(kWindow * small.longEdge())));
    const double sigma = std::max(1.0, kSmoothing * small.longEdge());
    const Encoded e = encode(small, ctx.threads);
    const std::array<double, 3> haze = hazeColour(e, minFilter(e.dark, radius, ctx.threads));

    // Haze per pixel (1 - transmission): the dark channel relative to the haze colour.
    Plane relative(small.width(), small.height());
    for (std::size_t i = 0; i < relative.values.size(); ++i) {
        double smallest = 1e9;
        for (std::size_t c = 0; c < 3; ++c) {
            smallest = std::min(smallest, double{e.channels.at(c).values[i]} / haze.at(c));
        }
        relative.values[i] = static_cast<float>(smallest);
    }
    const GuidedCoefficients k =
        guidedCoefficients(e.luma, minFilter(relative, radius, ctx.threads), sigma, 1e-3, ctx.threads);

    static const ValueTable encodeValue([](double v) { return color::perceptualEncode(v); });
    static const ValueTable decodeValue([](double v) { return color::perceptualDecode(v); });
    const double scale = 1.0 / factor;
    parallelFor(image.height(), ctx.threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            const double sy = ((y + 0.5) * scale) - 0.5;
            for (int x = 0; x < image.width(); ++x, p += 4) {
                const double sx = ((x + 0.5) * scale) - 0.5;
                const double luma = encodeValue(std::max(0.0, color::luminance(p[0], p[1], p[2])));
                const double h = std::clamp((double{sampleBilinear(k.slope, sx, sy)} * luma) +
                                                double{sampleBilinear(k.offset, sx, sy)},
                                            0.0, 1.0);
                for (std::size_t c = 0; c < 3; ++c) {
                    const double v = encodeValue(std::max(0.0, double{p[c]}));
                    const double a = haze.at(c);
                    double out = v;
                    if (amount > 0.0) {
                        const double t = std::max(kFloor, 1.0 - (0.95 * amount * h));
                        out = ((v - a) / t) + a;
                    } else {
                        // Adding haze: more where the scene already reads as far away.
                        out = v + ((a - v) * (-amount) * (0.3 + (0.5 * h)));
                    }
                    p[c] = static_cast<float>(decodeValue(std::max(0.0, out)));
                }
            }
        }
    });
}

} // namespace pfx::detail
