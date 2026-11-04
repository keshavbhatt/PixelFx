#pragma once

#include <QImage>

#include <pfx/io.h>

// The one place where pixels cross between libpixelfx and Qt (CODING_STANDARDS section 5).
namespace pixelfx::services {

/// An sRGB QImage (RGBA8888, tagged sRGB) holding a copy of `image`.
[[nodiscard]] QImage toQImage(const pfx::Rgba8Image& image);

} // namespace pixelfx::services
