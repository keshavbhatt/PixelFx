#include "core/reveal_dust.h"

#include <algorithm>
#include <vector>

namespace pixelfx::core {

namespace {

constexpr int kGain = 6;

} // namespace

QImage revealDust(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }
    const QImage in = image.convertToFormat(QImage::Format_RGB32);
    const int w = in.width();
    const int h = in.height();
    const int r = std::max(2, std::max(w, h) / 100);
    // Lightness, and its running sums for box averages in constant time.
    std::vector<int> light(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    std::vector<qint64> sums(static_cast<std::size_t>(w + 1) * static_cast<std::size_t>(h + 1), 0);
    const auto at = [w](int x, int y) {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(w)) + static_cast<std::size_t>(x);
    };
    const auto sumAt = [w](int x, int y) {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(w + 1)) + static_cast<std::size_t>(x);
    };
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(in.constScanLine(y));
        qint64 row = 0;
        for (int x = 0; x < w; ++x) {
            const int l = qGray(line[x]);
            light[at(x, y)] = l;
            row += l;
            sums[sumAt(x + 1, y + 1)] = sums[sumAt(x + 1, y)] + row;
        }
    }
    QImage out(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        const int y0 = std::max(0, y - r);
        const int y1 = std::min(h, y + r + 1);
        for (int x = 0; x < w; ++x) {
            const int x0 = std::max(0, x - r);
            const int x1 = std::min(w, x + r + 1);
            const qint64 total =
                sums[sumAt(x1, y1)] - sums[sumAt(x0, y1)] - sums[sumAt(x1, y0)] + sums[sumAt(x0, y0)];
            const auto mean = static_cast<int>(total / (static_cast<qint64>(x1 - x0) * (y1 - y0)));
            const int v = std::clamp(128 + ((light[at(x, y)] - mean) * kGain), 0, 255);
            line[x] = qRgb(v, v, v);
        }
    }
    return out;
}

} // namespace pixelfx::core
