// SPDX-License-Identifier: Apache-2.0
#include "pfx/resample.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vips/vips8>

namespace pfx {

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
