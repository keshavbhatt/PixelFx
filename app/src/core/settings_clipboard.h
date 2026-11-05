#pragma once

#include <QString>

#include <optional>
#include <pfx/settings.h>

// Copy and paste of a photo's settings (FEATURES E15). On the clipboard they are a PixelFx
// preset document (DOCS/presets.md), so they paste between windows and into a text file as a
// preset. Turns, flips, straightening and crop belong to each photo and are neither copied nor
// replaced, as in other editors.
namespace pixelfx::core {

/// The settings as clipboard text, without their geometry, named after the photo `from` ("sunset-lake.jpg").
[[nodiscard]] QString settingsToClipboard(const pfx::Settings& settings, const QString& from = {});
/// The settings in clipboard text, when it is a PixelFx preset or recipe; without geometry.
[[nodiscard]] std::optional<pfx::Settings> settingsFromClipboard(const QString& text);
/// Where the settings in clipboard text came from: the photo they were copied from, or the
/// preset's name; empty when unknown.
[[nodiscard]] QString clipboardSource(const QString& text);
/// `current` with everything but its geometry replaced by `copied`.
[[nodiscard]] pfx::Settings pasted(const pfx::Settings& current, const pfx::Settings& copied);

} // namespace pixelfx::core
