#include "core/histogram.h"

#include <QImage>

#include <algorithm>

namespace pixelfx::core {

std::uint32_t Histogram::peak() const
{
    std::uint32_t top = 1;
    for (const auto& channel : rgb) {
        top = std::max(top, *std::max_element(channel.begin() + 1, channel.end() - 1));
    }
    return top;
}

Histogram histogramOf(const QImage& source)
{
    Histogram h;
    if (source.isNull()) {
        return h;
    }
    const QImage image =
        source.format() == QImage::Format_RGBA8888 ? source : source.convertToFormat(QImage::Format_RGBA8888);
    std::uint64_t dark = 0;
    std::uint64_t bright = 0;
    for (int y = 0; y < image.height(); ++y) {
        const uchar* p = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x, p += 4) {
            if (p[3] == 0) {
                continue;
            }
            ++h.rgb[0][p[0]];
            ++h.rgb[1][p[1]];
            ++h.rgb[2][p[2]];
            ++h.pixels;
            dark += (p[0] == 0 || p[1] == 0 || p[2] == 0) ? 1U : 0U;
            bright += (p[0] == 255 || p[1] == 255 || p[2] == 255) ? 1U : 0U;
        }
    }
    if (h.pixels > 0) {
        h.clippedShadows = static_cast<double>(dark) / static_cast<double>(h.pixels);
        h.clippedHighlights = static_cast<double>(bright) / static_cast<double>(h.pixels);
    }
    return h;
}

} // namespace pixelfx::core
