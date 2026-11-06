#pragma once

#include <QDateTime>
#include <QString>

class QSize;

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
/// "1/250 s · f/8 · ISO 100": the exposure as a camera's display shows it, without the focal
/// length; empty when the photo says nothing.
[[nodiscard]] QString exposureSettings(const pfx::ExifSummary& exif);
/// "24 mm"; empty when unknown.
[[nodiscard]] QString focalLength(const pfx::ExifSummary& exif);
/// When the photo was taken, from Exif's "YYYY:MM:DD HH:MM:SS"; invalid when it does not say.
[[nodiscard]] QDateTime takenAt(const pfx::ExifSummary& exif);
/// "3 October 2026, 18:42", in the app's language; empty for an invalid time.
[[nodiscard]] QString takenText(const QDateTime& taken);
/// "JPEG", "PNG", "HEIC", "RAW (RAF)": the kind of file, from its loader ("jpegload") and its
/// file name.
[[nodiscard]] QString formatName(const QString& loader, const QString& path, bool raw);
/// "6000 x 4000 (24 MP)": megapixels to one decimal below 10.
[[nodiscard]] QString sizeWithMegapixels(const QSize& size);
/// "46.37612° N, 13.83645° E".
[[nodiscard]] QString coordinatesText(double latitude, double longitude);
/// "46.37612, 13.83645", as map sites read coordinates.
[[nodiscard]] QString coordinatesForCopy(double latitude, double longitude);

} // namespace pixelfx::core
