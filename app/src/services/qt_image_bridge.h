#pragma once

#include <QImage>

#include <pfx/io.h>

// The one place where pixels cross between libpixelfx and Qt (CODING_STANDARDS section 5).
namespace pixelfx::services {

/// An sRGB QImage (RGBA8888, tagged sRGB) holding a copy of `image`.
[[nodiscard]] QImage toQImage(const pfx::Rgba8Image& image);
/// A rendered photo (working space) as an sRGB QImage for the screen; null when it failed.
[[nodiscard]] QImage toDisplayImage(const pfx::Result<pfx::Image>& rendered);
[[nodiscard]] QImage toDisplayImage(const pfx::Image& image);
/// An sRGB QImage (a preview, the clipboard) in the working space; an error when it is null.
[[nodiscard]] pfx::Result<pfx::Image> fromDisplayImage(const QImage& image);

} // namespace pixelfx::services
