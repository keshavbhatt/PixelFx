#pragma once

#include <QString>

#include <pfx/settings.h>

// Settings told in a few words, for presets without a written description ("Warm, faded
// blacks, fine grain") and for summaries elsewhere.
namespace pixelfx::core {

/// A file size as PixelFx shows it: SI units ("118.5 kB", "1.8 MB"), one decimal below 10 MB
/// and whole numbers above ("108 MB").
[[nodiscard]] QString sizeText(qint64 bytes);

/// The everyday name of a hue in degrees: "red", "orange", "teal" ...
[[nodiscard]] QString hueName(double degrees);

/// What `settings` do to a photo, at most four traits, the strongest first; empty when they
/// change nothing that can be told in words.
[[nodiscard]] QString describeSettings(const pfx::Settings& settings);

} // namespace pixelfx::core
