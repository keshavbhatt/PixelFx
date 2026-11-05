// SPDX-License-Identifier: Apache-2.0
// Palette quantisation (ADR-010). Colours are compared premultiplied, so a colour under little
// alpha counts for little, in 8-bit sRGB values scaled to 0..1.
#include "pfx/quantize.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

namespace pfx {

namespace {

using Colour = std::array<std::uint8_t, 4>;

struct Vec4
{
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 0.0;

    [[nodiscard]] double at(int axis) const
    {
        switch (axis) {
        case 0:
            return r;
        case 1:
            return g;
        case 2:
            return b;
        default:
            return a;
        }
    }
};

Vec4 operator+(const Vec4& x, const Vec4& y)
{
    return {x.r + y.r, x.g + y.g, x.b + y.b, x.a + y.a};
}

Vec4 operator-(const Vec4& x, const Vec4& y)
{
    return {x.r - y.r, x.g - y.g, x.b - y.b, x.a - y.a};
}

Vec4 operator*(const Vec4& x, double k)
{
    return {x.r * k, x.g * k, x.b * k, x.a * k};
}

double distance(const Vec4& x, const Vec4& y)
{
    const Vec4 d = x - y;
    return (d.r * d.r) + (d.g * d.g) + (d.b * d.b) + (d.a * d.a);
}

std::uint32_t packed(const std::uint8_t* p)
{
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8U) | (std::uint32_t{p[2]} << 16U) |
           (std::uint32_t{p[3]} << 24U);
}

Vec4 premultiplied(const std::uint8_t* p)
{
    const double a = p[3] / 255.0;
    return {p[0] / 255.0 * a, p[1] / 255.0 * a, p[2] / 255.0 * a, a};
}

Vec4 premultiplied(const Colour& c)
{
    return premultiplied(c.data());
}

std::uint8_t level(double v)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

Colour straight(const Vec4& v)
{
    const std::uint8_t a = level(v.a);
    if (a == 0) {
        return {0, 0, 0, 0};
    }
    const double alpha = a / 255.0;
    return {level(v.r / alpha), level(v.g / alpha), level(v.b / alpha), a};
}

// A colour bin and the pixels in it.
struct Entry
{
    Vec4 mean;
    double weight = 0.0;
};

struct Box
{
    std::size_t begin = 0;
    std::size_t end = 0;
    double error = 0.0; // weighted squared distance to the box's mean
    int axis = 0;       // the channel that varies most
    Vec4 mean;
};

Box measure(const std::vector<Entry>& entries, std::size_t begin, std::size_t end)
{
    Box box;
    box.begin = begin;
    box.end = end;
    double weight = 0.0;
    Vec4 sum;
    for (std::size_t i = begin; i < end; ++i) {
        sum = sum + (entries[i].mean * entries[i].weight);
        weight += entries[i].weight;
    }
    box.mean = weight > 0.0 ? sum * (1.0 / weight) : sum;
    std::array<double, 4> spread{};
    for (std::size_t i = begin; i < end; ++i) {
        for (int c = 0; c < 4; ++c) {
            const double d = entries[i].mean.at(c) - box.mean.at(c);
            spread.at(static_cast<std::size_t>(c)) += d * d * entries[i].weight;
        }
    }
    box.error = spread[0] + spread[1] + spread[2] + spread[3];
    box.axis = static_cast<int>(std::ranges::max_element(spread) - spread.begin());
    return box;
}

// Median cut: the box with the largest error is split at its weighted median, until there are
// `slots` boxes or nothing is left to split.
std::vector<Vec4> medianCut(std::vector<Entry>& entries, int slots)
{
    std::vector<Box> boxes = {measure(entries, 0, entries.size())};
    while (std::cmp_less(boxes.size(), slots)) {
        auto worst = boxes.end();
        for (auto it = boxes.begin(); it != boxes.end(); ++it) {
            if (it->end - it->begin > 1 && it->error > 0.0 &&
                (worst == boxes.end() || it->error > worst->error)) {
                worst = it;
            }
        }
        if (worst == boxes.end()) {
            break;
        }
        const Box box = *worst;
        const auto first = entries.begin() + static_cast<std::ptrdiff_t>(box.begin);
        const auto last = entries.begin() + static_cast<std::ptrdiff_t>(box.end);
        std::sort(first, last, [axis = box.axis](const Entry& x, const Entry& y) {
            return x.mean.at(axis) < y.mean.at(axis);
        });
        double total = 0.0;
        for (std::size_t i = box.begin; i < box.end; ++i) {
            total += entries[i].weight;
        }
        std::size_t split = box.begin + 1;
        double running = entries[box.begin].weight;
        while (split < box.end - 1 && running + entries[split].weight <= total / 2.0) {
            running += entries[split].weight;
            ++split;
        }
        *worst = measure(entries, box.begin, split);
        boxes.push_back(measure(entries, split, box.end));
    }
    std::vector<Vec4> palette;
    palette.reserve(boxes.size());
    for (const Box& box : boxes) {
        palette.push_back(box.mean);
    }
    return palette;
}

std::size_t nearest(const std::vector<Vec4>& palette, const Vec4& colour)
{
    std::size_t best = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < palette.size(); ++i) {
        const double d = distance(palette[i], colour);
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

// K-means over the bins.
void refine(const std::vector<Entry>& entries, std::vector<Vec4>& palette)
{
    const int iterations = entries.size() > 200000 ? 3 : 6;
    for (int pass = 0; pass < iterations; ++pass) {
        std::vector<Vec4> sums(palette.size());
        std::vector<double> weights(palette.size(), 0.0);
        for (const Entry& e : entries) {
            const std::size_t i = nearest(palette, e.mean);
            sums[i] = sums[i] + (e.mean * e.weight);
            weights[i] += e.weight;
        }
        for (std::size_t i = 0; i < palette.size(); ++i) {
            if (weights[i] > 0.0) {
                palette[i] = sums[i] * (1.0 / weights[i]);
            }
        }
    }
}

std::size_t top(std::uint8_t value, unsigned drop)
{
    return static_cast<std::size_t>(value) >> drop;
}

// Bins of 5 bits per colour channel and 4 of alpha (fully opaque has a bin of its own).
std::size_t binOf(const std::uint8_t* p)
{
    const std::size_t alpha = p[3] == 255 ? 15U : std::min<std::size_t>(14U, top(p[3], 4));
    return (top(p[0], 3) << 14U) | (top(p[1], 3) << 9U) | (top(p[2], 3) << 4U) | alpha;
}

// A colour with the error carried to it, damped, kept a valid premultiplied colour.
Vec4 withError(const Vec4& colour, const Vec4& error)
{
    constexpr double kDamping = 0.875;
    Vec4 v = colour + (error * kDamping);
    v.a = std::clamp(v.a, 0.0, 1.0);
    v.r = std::clamp(v.r, 0.0, v.a);
    v.g = std::clamp(v.g, 0.0, v.a);
    v.b = std::clamp(v.b, 0.0, v.a);
    return v;
}

// Floyd-Steinberg: the error goes ahead, behind-below, below and ahead-below, in the direction
// of travel; rows carry one slot of margin on each side.
void diffuse(Vec4 error, std::size_t x, bool forward, std::vector<Vec4>& current, std::vector<Vec4>& next)
{
    constexpr double kLimit = 0.15;
    error = {std::clamp(error.r, -kLimit, kLimit), std::clamp(error.g, -kLimit, kLimit),
             std::clamp(error.b, -kLimit, kLimit), std::clamp(error.a, -kLimit, kLimit)};
    const std::size_t ahead = forward ? x + 2 : x;
    const std::size_t behind = forward ? x : x + 2;
    current[ahead] = current[ahead] + (error * (7.0 / 16.0));
    next[behind] = next[behind] + (error * (3.0 / 16.0));
    next[x + 1] = next[x + 1] + (error * (5.0 / 16.0));
    next[ahead] = next[ahead] + (error * (1.0 / 16.0));
}

std::vector<Vec4> premultipliedAll(const std::vector<Colour>& colours)
{
    std::vector<Vec4> out;
    out.reserve(colours.size());
    for (const Colour& c : colours) {
        out.push_back(premultiplied(c));
    }
    return out;
}

// The palette entry nearest a colour, remembered by colour at 6 bits per channel and 4 of alpha.
// `clear` (the entry of fully transparent pixels) is never chosen for a visible one.
class Nearest
{
public:
    Nearest(const std::vector<Colour>& colours, std::optional<std::size_t> clear)
        : m_palette(premultipliedAll(colours))
        , m_search(m_palette)
        , m_cache(std::size_t{1} << 22U, kUnknown)
    {
        if (clear) {
            m_search[*clear] = Vec4{9.0, 9.0, 9.0, 9.0};
        }
    }

    std::size_t operator()(const Vec4& v)
    {
        const Colour c = straight(v);
        const std::size_t key =
            (top(c[0], 2) << 16U) | (top(c[1], 2) << 10U) | (top(c[2], 2) << 4U) | top(c[3], 4);
        if (m_cache[key] == kUnknown) {
            m_cache[key] = static_cast<std::uint16_t>(nearest(m_search, v));
        }
        return std::size_t{m_cache[key]};
    }

    [[nodiscard]] const Vec4& colour(std::size_t index) const { return m_palette[index]; }

private:
    static constexpr std::uint16_t kUnknown = 0xFFFF;
    std::vector<Vec4> m_palette;
    std::vector<Vec4> m_search;
    std::vector<std::uint16_t> m_cache;
};

// The mean squared error per channel of every visible pixel against its nearest entry: the
// palette's own error, before dithering spreads it.
double paletteError(const Rgba8Image& image, Nearest& lookup)
{
    double total = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
        const std::uint8_t* p = image.pixels.data() + i;
        if (p[3] == 0) {
            continue;
        }
        const Vec4 v = premultiplied(p);
        total += distance(v, lookup.colour(lookup(v)));
        ++count;
    }
    return count > 0 ? total / (4.0 * static_cast<double>(count)) : 0.0;
}

// Maps every pixel to the palette, diffusing the error (serpentine Floyd-Steinberg, slightly
// damped and clamped so flat areas stay calm). `clear` is the entry of fully transparent pixels.
void remap(const Rgba8Image& image, Nearest& lookup, std::optional<std::size_t> clear, bool dither,
           std::vector<std::uint8_t>& indexes)
{
    const auto width = static_cast<std::size_t>(image.width);
    std::vector<Vec4> current(width + 2);
    std::vector<Vec4> next(width + 2);
    for (int y = 0; y < image.height; ++y) {
        const bool forward = (y % 2) == 0;
        std::ranges::fill(next, Vec4{});
        for (std::size_t step = 0; step < width; ++step) {
            const std::size_t x = forward ? step : width - 1 - step;
            const std::size_t at = (static_cast<std::size_t>(y) * width) + x;
            const std::uint8_t* p = image.pixels.data() + (at * 4);
            if (p[3] == 0 && clear) {
                indexes[at] = static_cast<std::uint8_t>(*clear);
                continue;
            }
            Vec4 target = premultiplied(p);
            if (dither) {
                target = withError(target, current[x + 1]);
            }
            const std::size_t chosen = lookup(target);
            indexes[at] = static_cast<std::uint8_t>(chosen);
            if (!dither) {
                continue;
            }
            diffuse(target - lookup.colour(chosen), x, forward, current, next);
        }
        std::swap(current, next);
    }
}

} // namespace

double errorOfQuality(int quality)
{
    // pngquant's scale, so a quality floor means what its users expect.
    if (quality <= 0) {
        return 1.0;
    }
    if (quality >= 100) {
        return 0.0;
    }
    const double q = quality;
    const double fudge = std::max(0.0, (0.016 / (0.001 + q)) - 0.001);
    return fudge + (2.5 / std::pow(210.0 + q, 1.2) * (100.1 - q) / 100.0);
}

int qualityOfError(double meanSquaredError)
{
    for (int q = 100; q > 0; --q) {
        if (meanSquaredError <= errorOfQuality(q) + 0.000001) {
            return q;
        }
    }
    return 0;
}

std::optional<IndexedImage> exactPalette(const Rgba8Image& image, int maxColours)
{
    IndexedImage out{image.width, image.height, {}, std::vector<std::uint8_t>(image.pixels.size() / 4)};
    std::unordered_map<std::uint32_t, std::uint8_t> index;
    for (std::size_t i = 0; i < out.indexes.size(); ++i) {
        const std::uint8_t* p = image.pixels.data() + (i * 4);
        const auto [it, added] = index.try_emplace(packed(p), static_cast<std::uint8_t>(index.size()));
        if (added) {
            if (std::cmp_greater(index.size(), maxColours)) {
                return std::nullopt;
            }
            out.palette.push_back({p[0], p[1], p[2], p[3]});
        }
        out.indexes[i] = it->second;
    }
    return out;
}

Quantized quantize(const Rgba8Image& image, const QuantizeOptions& options)
{
    const int maxColours = std::clamp(options.maxColours, 2, 256);
    if (auto exact = exactPalette(image, maxColours)) {
        return {std::move(*exact), 100};
    }
    // Fully transparent pixels need no colour: they share one entry.
    struct Bin
    {
        Vec4 sum;
        double weight = 0.0;
    };
    std::vector<Bin> bins(std::size_t{1} << 19U);
    bool anyClear = false;
    for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
        const std::uint8_t* p = image.pixels.data() + i;
        if (p[3] == 0) {
            anyClear = true;
            continue;
        }
        Bin& bin = bins[binOf(p)];
        bin.sum = bin.sum + premultiplied(p);
        bin.weight += 1.0;
    }
    std::vector<Entry> entries;
    for (const Bin& bin : bins) {
        if (bin.weight > 0.0) {
            entries.push_back({bin.sum * (1.0 / bin.weight), bin.weight});
        }
    }
    bins = {};
    const int slots = maxColours - (anyClear ? 1 : 0);
    std::vector<Vec4> palette = medianCut(entries, slots);
    refine(entries, palette);

    std::vector<Colour> colours;
    colours.reserve(palette.size() + 1);
    for (const Vec4& v : palette) {
        colours.push_back(straight(v));
    }
    std::optional<std::size_t> clear;
    if (anyClear) {
        clear = colours.size();
        colours.push_back({0, 0, 0, 0});
    }
    Quantized out;
    out.image.width = image.width;
    out.image.height = image.height;
    out.image.palette = colours;
    out.image.indexes.resize(image.pixels.size() / 4);
    Nearest lookup(colours, clear);
    out.quality = qualityOfError(paletteError(image, lookup));
    remap(image, lookup, clear, options.dither, out.image.indexes);
    return out;
}

} // namespace pfx
