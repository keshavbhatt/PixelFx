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

QImage toDisplayImage(const pfx::Result<pfx::Image>& rendered)
{
    return rendered ? toDisplayImage(rendered.value()) : QImage();
}

QImage toDisplayImage(const pfx::Image& image)
{
    const auto pixels = pfx::toSrgb8(image);
    return pixels ? toQImage(pixels.value()) : QImage();
}

pfx::Result<pfx::Image> fromDisplayImage(const QImage& image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    pfx::Rgba8Image pixels;
    pixels.width = rgba.width();
    pixels.height = rgba.height();
    const auto rowBytes = static_cast<std::size_t>(rgba.width()) * 4;
    pixels.pixels.resize(rowBytes * static_cast<std::size_t>(rgba.height()));
    for (int y = 0; y < rgba.height(); ++y) {
        std::memcpy(pixels.pixels.data() + static_cast<std::size_t>(y) * rowBytes, rgba.constScanLine(y),
                    rowBytes);
    }
    return pfx::fromSrgb8(pixels);
}

} // namespace pixelfx::services
