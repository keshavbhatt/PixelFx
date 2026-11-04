#include "services/qt_image_bridge.h"

#include <QColorSpace>

#include <cstring>

namespace pixelfx::services {

QImage toQImage(const pfx::Rgba8Image& image)
{
    if (image.width <= 0 || image.height <= 0) {
        return {};
    }
    QImage out(image.width, image.height, QImage::Format_RGBA8888);
    const auto rowBytes = static_cast<std::size_t>(image.width) * 4;
    for (int y = 0; y < image.height; ++y) {
        std::memcpy(out.scanLine(y), image.pixels.data() + static_cast<std::size_t>(y) * rowBytes, rowBytes);
    }
    out.setColorSpace(QColorSpace::SRgb);
    return out;
}

} // namespace pixelfx::services
