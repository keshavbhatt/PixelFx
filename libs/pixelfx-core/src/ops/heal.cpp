// SPDX-License-Identifier: Apache-2.0
// Spot heal (FEATURES E20, DOCS/presets.md group "heal"): each spot or brush stroke is covered
// with pixels from its source, shifted by the source offset, with the difference in tone
// between the two surroundings fitted as a plane and added, then feathered in.
#include "pfx/heal.h"

#include "core/parallel.h"
#include "ops/ops.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace pfx {

namespace {

constexpr double kRing = 1.5; // the surroundings: from the radius out to 1.5 radii

struct Point
{
    double x = 0;
    double y = 0;
};

// A spot in pixels: its points (one for a spot, the stroke for a brush) and its radius.
struct Shape
{
    std::vector<Point> points;
    double radius = 1;
    Point offset; // to the source
};

Shape shapeOf(const Image& image, const Spot& spot)
{
    const double w = image.width();
    const double h = image.height();
    Shape s;
    s.radius = std::max(1.0, spot.radius * image.longEdge());
    if (spot.stroke.size() >= 2) {
        // Points closer than a quarter radius add nothing but time.
        for (const CurvePoint& p : spot.stroke) {
            const Point q{p.x * w, p.y * h};
            if (s.points.empty() ||
                std::hypot(q.x - s.points.back().x, q.y - s.points.back().y) >= s.radius / 4.0) {
                s.points.push_back(q);
            }
        }
        const CurvePoint& last = spot.stroke.back();
        s.points.push_back({last.x * w, last.y * h});
    } else {
        s.points.push_back({spot.x * w, spot.y * h});
    }
    s.offset = {(spot.sourceX - spot.x) * w, (spot.sourceY - spot.y) * h};
    return s;
}

double segmentDistance(Point p, Point a, Point b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = (dx * dx) + (dy * dy);
    double t = len2 > 0 ? (((p.x - a.x) * dx) + ((p.y - a.y) * dy)) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + (t * dx)), p.y - (a.y + (t * dy)));
}

double shapeDistance(const Shape& s, Point p)
{
    if (s.points.size() == 1) {
        return std::hypot(p.x - s.points[0].x, p.y - s.points[0].y);
    }
    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < s.points.size(); ++i) {
        best = std::min(best, segmentDistance(p, s.points[i - 1], s.points[i]));
    }
    return best;
}

struct Box
{
    int x0 = 0;
    int y0 = 0;
    int x1 = 0; // exclusive
    int y1 = 0;
    [[nodiscard]] int width() const { return x1 - x0; }
    [[nodiscard]] int height() const { return y1 - y0; }
    [[nodiscard]] bool empty() const { return x1 <= x0 || y1 <= y0; }
};

Box boundsOf(const Shape& s, double margin, const Image& image)
{
    double x0 = std::numeric_limits<double>::max();
    double y0 = x0;
    double x1 = -x0;
    double y1 = -x0;
    for (const Point& p : s.points) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    return {std::max(0, static_cast<int>(std::floor(x0 - margin))),
            std::max(0, static_cast<int>(std::floor(y0 - margin))),
            std::min(image.width(), static_cast<int>(std::ceil(x1 + margin)) + 1),
            std::min(image.height(), static_cast<int>(std::ceil(y1 + margin)) + 1)};
}

// The distance of every pixel of `box` to the shape, segment by segment (a long stroke would be
// slow pixel by pixel).
std::vector<float> distances(const Shape& s, const Box& box, double reach)
{
    std::vector<float> d(static_cast<std::size_t>(box.width()) * static_cast<std::size_t>(box.height()),
                         std::numeric_limits<float>::max());
    const auto visit = [&](Point a, Point b) {
        const int x0 = std::max(box.x0, static_cast<int>(std::floor(std::min(a.x, b.x) - reach)));
        const int x1 = std::min(box.x1, static_cast<int>(std::ceil(std::max(a.x, b.x) + reach)) + 1);
        const int y0 = std::max(box.y0, static_cast<int>(std::floor(std::min(a.y, b.y) - reach)));
        const int y1 = std::min(box.y1, static_cast<int>(std::ceil(std::max(a.y, b.y) + reach)) + 1);
        for (int y = y0; y < y1; ++y) {
            float* row =
                d.data() + (static_cast<std::size_t>(y - box.y0) * static_cast<std::size_t>(box.width()));
            for (int x = x0; x < x1; ++x) {
                const auto dist = static_cast<float>(segmentDistance({x + 0.5, y + 0.5}, a, b));
                float& cell = row[x - box.x0];
                cell = std::min(cell, dist);
            }
        }
    };
    if (s.points.size() == 1) {
        visit(s.points[0], s.points[0]);
    }
    for (std::size_t i = 1; i < s.points.size(); ++i) {
        visit(s.points[i - 1], s.points[i]);
    }
    return d;
}

// A copy of part of the image, read with bilinear filtering; outside it the edge repeats.
class Patch
{
public:
    Patch(const Image& image, Box box)
        : m_box(box)
        , m_pixels(static_cast<std::size_t>(box.width()) * static_cast<std::size_t>(box.height()) * 4)
    {
        for (int y = box.y0; y < box.y1; ++y) {
            std::copy_n(image.pixel(box.x0, y), static_cast<std::size_t>(box.width()) * 4,
                        m_pixels.data() + (static_cast<std::size_t>(y - box.y0) *
                                           static_cast<std::size_t>(box.width()) * 4));
        }
    }

    [[nodiscard]] std::array<float, 3> at(double x, double y) const
    {
        const double fx = std::clamp(x - 0.5, static_cast<double>(m_box.x0), m_box.x1 - 1.0);
        const double fy = std::clamp(y - 0.5, static_cast<double>(m_box.y0), m_box.y1 - 1.0);
        const int ix = std::min(static_cast<int>(fx), m_box.x1 - 2 < m_box.x0 ? m_box.x0 : m_box.x1 - 2);
        const int iy = std::min(static_cast<int>(fy), m_box.y1 - 2 < m_box.y0 ? m_box.y0 : m_box.y1 - 2);
        const auto tx = static_cast<float>(std::clamp(fx - ix, 0.0, 1.0));
        const auto ty = static_cast<float>(std::clamp(fy - iy, 0.0, 1.0));
        const int jx = std::min(ix + 1, m_box.x1 - 1);
        const int jy = std::min(iy + 1, m_box.y1 - 1);
        std::array<float, 3> out{};
        for (std::size_t c = 0; c < 3; ++c) {
            const float a = value(ix, iy, c) + ((value(jx, iy, c) - value(ix, iy, c)) * tx);
            const float b = value(ix, jy, c) + ((value(jx, jy, c) - value(ix, jy, c)) * tx);
            out.at(c) = a + ((b - a) * ty);
        }
        return out;
    }

private:
    [[nodiscard]] float value(int x, int y, std::size_t c) const
    {
        return m_pixels[((static_cast<std::size_t>(y - m_box.y0) * static_cast<std::size_t>(m_box.width())) +
                         static_cast<std::size_t>(x - m_box.x0)) *
                            4 +
                        c];
    }

    Box m_box;
    std::vector<float> m_pixels;
};

// delta(u, v) = a + b u + c v per channel, fitted to the surroundings' differences.
struct Plane
{
    std::array<std::array<double, 3>, 3> coefficients{}; // [channel][a, b, c]
    double cx = 0;
    double cy = 0;
    double scale = 1;

    [[nodiscard]] float at(std::size_t channel, double x, double y) const
    {
        const auto& k = coefficients.at(channel);
        const double u = (x - cx) / scale;
        const double v = (y - cy) / scale;
        return static_cast<float>(k[0] + (k[1] * u) + (k[2] * v));
    }
};

// Solves the 3 by 3 normal equations; falls back to the mean when they are degenerate.
std::array<double, 3> solve(const std::array<std::array<double, 3>, 3>& m, const std::array<double, 3>& r,
                            double count)
{
    const auto det = [](const std::array<std::array<double, 3>, 3>& a) {
        return (a[0][0] * ((a[1][1] * a[2][2]) - (a[1][2] * a[2][1]))) -
               (a[0][1] * ((a[1][0] * a[2][2]) - (a[1][2] * a[2][0]))) +
               (a[0][2] * ((a[1][0] * a[2][1]) - (a[1][1] * a[2][0])));
    };
    const double d = det(m);
    if (std::abs(d) < 1e-9 * std::max(1.0, count * count * count)) {
        return {count > 0 ? r[0] / count : 0.0, 0.0, 0.0};
    }
    std::array<double, 3> x{};
    for (std::size_t i = 0; i < 3; ++i) {
        auto mi = m;
        for (std::size_t row = 0; row < 3; ++row) {
            mi.at(row).at(i) = r.at(row);
        }
        x.at(i) = det(mi) / d;
    }
    return x;
}

Plane fitPlane(const Shape& s, const Box& box, const std::vector<float>& dist, const Patch& target,
               const Patch& source)
{
    Plane plane;
    plane.cx = (box.x0 + box.x1) / 2.0;
    plane.cy = (box.y0 + box.y1) / 2.0;
    plane.scale = std::max(1.0, std::max(box.width(), box.height()) / 2.0);
    std::array<std::array<double, 3>, 3> m{};
    std::array<std::array<double, 3>, 3> r{}; // [channel][row]
    double count = 0;
    // Enough samples for a fit, not every pixel of a big stroke's surroundings.
    const std::size_t total = static_cast<std::size_t>(box.width()) * static_cast<std::size_t>(box.height());
    const int stride = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(total) / 40000.0)));
    for (int y = box.y0; y < box.y1; y += stride) {
        for (int x = box.x0; x < box.x1; x += stride) {
            const auto d = static_cast<double>(
                dist[(static_cast<std::size_t>(y - box.y0) * static_cast<std::size_t>(box.width())) +
                     static_cast<std::size_t>(x - box.x0)]);
            if (d <= s.radius || d > s.radius * kRing) {
                continue;
            }
            const double px = x + 0.5;
            const double py = y + 0.5;
            const std::array<float, 3> t = target.at(px, py);
            const std::array<float, 3> src = source.at(px + s.offset.x, py + s.offset.y);
            const std::array<double, 3> basis{1.0, (px - plane.cx) / plane.scale,
                                              (py - plane.cy) / plane.scale};
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) {
                    m.at(i).at(j) += basis.at(i) * basis.at(j);
                }
                for (std::size_t c = 0; c < 3; ++c) {
                    r.at(c).at(i) +=
                        basis.at(i) * (static_cast<double>(t.at(c)) - static_cast<double>(src.at(c)));
                }
            }
            count += 1;
        }
    }
    for (std::size_t c = 0; c < 3; ++c) {
        plane.coefficients.at(c) = solve(m, r.at(c), count);
    }
    return plane;
}

float featherMask(double d, double radius, double feather)
{
    const double inner = radius * (1.0 - (std::clamp(feather, 0.0, 100.0) / 100.0));
    if (d <= inner) {
        return 1.0F;
    }
    if (d >= radius) {
        return 0.0F;
    }
    const double t = (radius - d) / std::max(1e-6, radius - inner);
    return static_cast<float>(t * t * (3.0 - (2.0 * t)));
}

Box shifted(const Box& box, Point offset, const Image& image)
{
    const int dx = static_cast<int>(std::floor(offset.x));
    const int dy = static_cast<int>(std::floor(offset.y));
    return {std::clamp(box.x0 + dx - 1, 0, image.width()), std::clamp(box.y0 + dy - 1, 0, image.height()),
            std::clamp(box.x1 + dx + 2, 0, image.width()), std::clamp(box.y1 + dy + 2, 0, image.height())};
}

} // namespace

void healSpot(Image& image, const Spot& spot, int threads)
{
    if (image.empty() || spot.opacity <= 0.0) {
        return;
    }
    const Shape shape = shapeOf(image, spot);
    const Box box = boundsOf(shape, (shape.radius * kRing) + 2.0, image);
    const Box sourceBox = shifted(box, shape.offset, image);
    if (box.empty() || sourceBox.empty()) {
        return;
    }
    const std::vector<float> dist = distances(shape, box, (shape.radius * kRing) + 2.0);
    const Patch target(image, box);
    const Patch source(image, sourceBox); // copied first: the source may overlap the spot
    const Plane plane = fitPlane(shape, box, dist, target, source);
    const auto opacity = static_cast<float>(spot.opacity / 100.0);
    detail::parallelFor(box.height(), threads, [&](int begin, int end) {
        for (int y = box.y0 + begin; y < box.y0 + end; ++y) {
            float* row = image.pixel(0, y);
            for (int x = box.x0; x < box.x1; ++x) {
                const auto d = static_cast<double>(
                    dist[(static_cast<std::size_t>(y - box.y0) * static_cast<std::size_t>(box.width())) +
                         static_cast<std::size_t>(x - box.x0)]);
                const float mask = featherMask(d, shape.radius, spot.feather) * opacity;
                if (mask <= 0.0F) {
                    continue;
                }
                const double px = x + 0.5;
                const double py = y + 0.5;
                const std::array<float, 3> src = source.at(px + shape.offset.x, py + shape.offset.y);
                float* p = row + (static_cast<std::size_t>(x) * 4);
                for (std::size_t c = 0; c < 3; ++c) {
                    const float healed = std::max(0.0F, src.at(c) + plane.at(c, px, py));
                    p[c] += (healed - p[c]) * mask;
                }
            }
        }
    });
}

namespace {

// A value to compare places by, roughly perceptual: the square root of linear light, averaged
// over a small cross so noise does not decide.
std::array<double, 3> look(const Image& image, double x, double y, double spread)
{
    std::array<double, 3> sum{};
    const std::array<Point, 5> taps{{{0, 0}, {spread, 0}, {-spread, 0}, {0, spread}, {0, -spread}}};
    for (const Point& t : taps) {
        const int ix = std::clamp(static_cast<int>(x + t.x), 0, image.width() - 1);
        const int iy = std::clamp(static_cast<int>(y + t.y), 0, image.height() - 1);
        const float* p = image.pixel(ix, iy);
        for (std::size_t c = 0; c < 3; ++c) {
            sum.at(c) += std::sqrt(static_cast<double>(std::max(0.0F, p[c])));
        }
    }
    for (double& v : sum) {
        v /= static_cast<double>(taps.size());
    }
    return sum;
}

double difference(const std::array<double, 3>& a, const std::array<double, 3>& b)
{
    double d = 0;
    for (std::size_t c = 0; c < 3; ++c) {
        d += (a.at(c) - b.at(c)) * (a.at(c) - b.at(c));
    }
    return d;
}

struct Samples
{
    std::vector<Point> ring;   // around the spot, outside it
    std::vector<Point> inside; // on the spot
};

Samples samplesOf(const Shape& s)
{
    Samples out;
    const std::size_t step = std::max<std::size_t>(1, s.points.size() / 24);
    for (std::size_t i = 0; i < s.points.size(); i += step) {
        const Point c = s.points[i];
        out.inside.push_back(c);
        for (int k = 0; k < 12; ++k) {
            const double a = k * std::numbers::pi / 6.0;
            out.inside.push_back(
                {c.x + (0.5 * s.radius * std::cos(a)), c.y + (0.5 * s.radius * std::sin(a))});
            for (const double at : {1.2, 1.45}) {
                const Point q{c.x + (at * s.radius * std::cos(a)), c.y + (at * s.radius * std::sin(a))};
                if (shapeDistance(s, q) > 1.1 * s.radius) {
                    out.ring.push_back(q);
                }
            }
        }
    }
    return out;
}

bool inside(const Image& image, Point p, double margin)
{
    return p.x >= margin && p.y >= margin && p.x <= image.width() - margin && p.y <= image.height() - margin;
}

} // namespace

namespace {

// What a candidate source is compared by: the spot's surroundings and their mean.
struct Reference
{
    Samples samples;
    std::vector<std::array<double, 3>> ring;
    std::array<double, 3> ringMean{};
    double spread = 1;
};

Reference referenceOf(const Image& image, const Shape& shape)
{
    Reference r;
    r.samples = samplesOf(shape);
    r.spread = std::max(1.0, shape.radius / 6.0);
    for (const Point& p : r.samples.ring) {
        r.ring.push_back(look(image, p.x, p.y, r.spread));
        for (std::size_t c = 0; c < 3; ++c) {
            r.ringMean.at(c) += r.ring.back().at(c) / static_cast<double>(r.samples.ring.size());
        }
    }
    return r;
}

// A candidate's place must be inside the photo and clear of the spot itself.
bool fits(const Image& image, const Shape& shape, const Reference& r, Point offset)
{
    const auto clear = [&](const Point& p) {
        return inside(image, {p.x + offset.x, p.y + offset.y}, r.spread + 1);
    };
    return std::ranges::all_of(r.samples.ring, clear) &&
           std::ranges::all_of(r.samples.inside, [&](const Point& p) {
               return clear(p) && shapeDistance(shape, {p.x + offset.x, p.y + offset.y}) > 1.2 * shape.radius;
           });
}

// Lower is better: surroundings alike, and an inside like the spot's surroundings.
double score(const Image& image, const Reference& r, Point offset)
{
    double ring = 0;
    for (std::size_t i = 0; i < r.samples.ring.size(); ++i) {
        const Point& p = r.samples.ring[i];
        ring += difference(r.ring[i], look(image, p.x + offset.x, p.y + offset.y, r.spread));
    }
    double interior = 0;
    for (const Point& p : r.samples.inside) {
        interior += difference(r.ringMean, look(image, p.x + offset.x, p.y + offset.y, r.spread));
    }
    return (ring / static_cast<double>(r.samples.ring.size())) +
           (interior / static_cast<double>(r.samples.inside.size()));
}

} // namespace

Spot findHealSource(const Image& image, Spot spot)
{
    if (image.empty()) {
        return spot;
    }
    const Shape shape = shapeOf(image, spot);
    const Reference reference = referenceOf(image, shape);
    if (reference.samples.ring.empty()) {
        return spot;
    }
    // How far the shape reaches from its first point: a stroke must move clear of itself.
    double reach = 0;
    for (const Point& p : shape.points) {
        reach = std::max(reach, std::hypot(p.x - shape.points[0].x, p.y - shape.points[0].y));
    }
    double best = std::numeric_limits<double>::max();
    Point bestOffset{2.2 * shape.radius, 0};
    bool found = false;
    for (const double k : {2.2, 3.0, 4.0, 5.5}) {
        const double distance = (k * shape.radius) + (reach * 0.25);
        for (int a = 0; a < 24; ++a) {
            const double angle = a * std::numbers::pi / 12.0;
            const Point offset{distance * std::cos(angle), distance * std::sin(angle)};
            if (!fits(image, shape, reference, offset)) {
                continue;
            }
            const double s = score(image, reference, offset) * (1.0 + (0.05 * k)); // near ones first
            if (s < best) {
                best = s;
                bestOffset = offset;
                found = true;
            }
        }
    }
    if (!found) {
        // Nowhere fits (a spot as big as the photo): beside it, kept inside the photo.
        bestOffset.x = shape.points[0].x + bestOffset.x < image.width() ? bestOffset.x : -bestOffset.x;
    }
    spot.sourceX = std::clamp(spot.x + (bestOffset.x / image.width()), 0.0, 1.0);
    spot.sourceY = std::clamp(spot.y + (bestOffset.y / image.height()), 0.0, 1.0);
    return spot;
}

} // namespace pfx

namespace pfx::detail {

bool healActive(const Settings& s)
{
    return !s.spots("heal.spots").empty();
}

void healApply(Image& image, const Settings& s, const OpContext& ctx)
{
    for (const Spot& spot : s.spots("heal.spots")) {
        healSpot(image, spot, ctx.threads);
    }
}

} // namespace pfx::detail
