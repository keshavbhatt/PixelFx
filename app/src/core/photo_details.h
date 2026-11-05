#pragma once

#include <QString>

namespace pfx {
struct ExifSummary;
}

// What the camera wrote, said the way photographers write it (FEATURES O7).
namespace pixelfx::core {

/// "ISO 100 · f/8 · 1/250 s · 24 mm", leaving out what the photo does not say; empty when it
/// says nothing.
[[nodiscard]] QString exposureLine(const pfx::ExifSummary& exif);
/// "Canon EOS R5", with the make said once; empty when unknown.
[[nodiscard]] QString cameraName(const pfx::ExifSummary& exif);

} // namespace pixelfx::core
