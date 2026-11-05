// SPDX-License-Identifier: Apache-2.0
#include "pfx/resample.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vips/vips8>

namespace pfx {

namespace {

void ensureVips()
{
    static std::once_flag once;
    std::call_once(once, [] { VIPS_INIT("pixelfx"); });
}

vips::VImage wrap(const Image& image)
{
    return vips::VImage::new_from_memory(const_cast<float*>(image.data()), // NOLINT: read only
                                         image.pixelCount() * 4 * sizeof(float), image.width(),
                                         image.height(), 4, VIPS_FORMAT_FLOAT);
}

Image unwrap(const vips::VImage& out)
{
    Image result(out.width(), out.height());
    std::size_t size = 0;
    void* memory = out.write_to_memory(&size);
    std::copy_n(static_cast<const float*>(memory), std::min(size / sizeof(float), result.pixelCount() * 4),
                result.data());
    g_free(memory);
    return result;
}

// Lanczos 3 to `nw` by `nh` (the two scales can differ by under a pixel's worth).
vips::VImage resizedTo(const vips::VImage& in, int nw, int nh)
{
    const double h = static_cast<double>(nw) / in.width();
    const double v = static_cast<double>(nh) / in.height();
    return in.resize(h, vips::VImage::option()->set("vscale", v)->set("kernel", VIPS_KERNEL_LANCZOS3));
}

} // namespace

Result<Image> fillSize(const Image& image, int width, int height, float anchorX, float anchorY)
{
    if (image.empty() || width <= 0 || height <= 0) {
        return Error{ErrorCode::InvalidArgument, "Nothing to resize."};
    }
    ensureVips();
    const double scale =
        std::max(static_cast<double>(width) / image.width(), static_cast<double>(height) / image.height());
    const int nw = std::max(width, static_cast<int>(std::lround(image.width() * scale)));
    const int nh = std::max(height, static_cast<int>(std::lround(image.height() * scale)));
    try {
        const vips::VImage out = resizedTo(wrap(image), nw, nh);
        const auto offset = [](int spare, float anchor) {
            return static_cast<int>(std::lround(spare * std::clamp(static_cast<double>(anchor), 0.0, 1.0)));
        };
        return unwrap(out.extract_area(offset(out.width() - width, anchorX),
                                       offset(out.height() - height, anchorY), width, height));
    } catch (const vips::VError& e) {
        return Error{ErrorCode::InvalidArgument, std::string("Could not resize the image: ") + e.what()};
    }
}

Result<Image> fitInside(const Image& image, int width, int height, float border)
{
    if (image.empty() || width <= 0 || height <= 0) {
        return Error{ErrorCode::InvalidArgument, "Nothing to resize."};
    }
    ensureVips();
    const double scale =
        std::min(static_cast<double>(width) / image.width(), static_cast<double>(height) / image.height());
    const int nw = std::clamp(static_cast<int>(std::lround(image.width() * scale)), 1, width);
    const int nh = std::clamp(static_cast<int>(std::lround(image.height() * scale)), 1, height);
    try {
        const vips::VImage scaled = resizedTo(wrap(image), nw, nh);
        const auto grey = double{border};
        const std::vector<double> background = {grey, grey, grey, 1.0};
        return unwrap(scaled.embed(
            (width - nw) / 2, (height - nh) / 2, width, height,
            vips::VImage::option()->set("extend", VIPS_EXTEND_BACKGROUND)->set("background", background)));
    } catch (const vips::VError& e) {
        return Error{ErrorCode::InvalidArgument, std::string("Could not resize the image: ") + e.what()};
    }
}

Result<Image> fitLongEdge(const Image& image, int longEdge)
{
    if (image.empty() || longEdge <= 0) {
        return Error{ErrorCode::InvalidArgument, "Nothing to resize."};
    }
    if (image.longEdge() <= longEdge) {
        return image;
    }
    static std::once_flag once;
    std::call_once(once, [] { VIPS_INIT("pixelfx"); });
    const double scale = static_cast<double>(longEdge) / image.longEdge();
    try {
        const vips::VImage in = vips::VImage::new_from_memory(
            const_cast<float*>(image.data()), // NOLINT: read only
            image.pixelCount() * 4 * sizeof(float), image.width(), image.height(), 4, VIPS_FORMAT_FLOAT);
        const vips::VImage out =
            in.resize(scale, vips::VImage::option()->set("kernel", VIPS_KERNEL_LANCZOS3));
        Image result(out.width(), out.height());
        std::size_t size = 0;
        void* memory = out.write_to_memory(&size);
        const std::size_t count = std::min(size / sizeof(float), result.pixelCount() * 4);
        std::copy_n(static_cast<const float*>(memory), count, result.data());
        g_free(memory);
        return result;
    } catch (const vips::VError& e) {
        return Error{ErrorCode::InvalidArgument, std::string("Could not resize the image: ") + e.what()};
    }
}

} // namespace pfx
