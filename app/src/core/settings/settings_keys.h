#pragma once

// Every settings key, once (CODING_STANDARDS section 7). Format: section/camelCase.
namespace pixelfx::core::keys {

inline constexpr auto kWindowGeometry = "window/geometry";
inline constexpr auto kWindowState = "window/state";
inline constexpr auto kLastOpenFolder = "files/lastOpenFolder";
inline constexpr auto kRecentFiles = "files/recent";
inline constexpr auto kLegacyMigrated = "migration/v1Done";

// PixelFx 1 (org.keshavnrj.ubuntu/pixelFx.conf): the only key worth carrying over.
inline constexpr auto kLegacyLastFolder = "last_image_dir_path";

} // namespace pixelfx::core::keys
