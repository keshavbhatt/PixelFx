#pragma once

// Every settings key, once (CODING_STANDARDS section 7). Format: section/camelCase.
namespace pixelfx::core::keys {

inline constexpr auto kWindowGeometry = "window/geometry";
inline constexpr auto kWindowState = "window/state";
inline constexpr auto kLastOpenFolder = "files/lastOpenFolder";
inline constexpr auto kRecentFiles = "files/recent";
inline constexpr auto kExportExtension = "export/extension";
inline constexpr auto kExportQuality = "export/quality";
inline constexpr auto kExportBits = "export/bitsPerSample";
inline constexpr auto kExportLongEdge = "export/longEdge";
inline constexpr auto kExportKeepMetadata = "export/keepMetadata";
inline constexpr auto kExportKeepLocation = "export/keepLocation";
inline constexpr auto kExportFolder = "export/folder";
inline constexpr auto kExportNameTemplate = "export/nameTemplate";
inline constexpr auto kExportIfExists = "export/ifExists";
inline constexpr auto kLegacyMigrated = "migration/v1Done";
inline constexpr auto kTheme = "appearance/theme";
inline constexpr auto kCanvasBackground = "appearance/canvasBackground";
inline constexpr auto kRememberEdits = "editing/rememberEdits";
inline constexpr auto kOpenAsLeft = "editing/openAsLeft";
inline constexpr auto kClippingWarnings = "display/clippingWarnings";

// PixelFx 1 (org.keshavnrj.ubuntu/pixelFx.conf): the only key worth carrying over.
inline constexpr auto kLegacyLastFolder = "last_image_dir_path";

} // namespace pixelfx::core::keys
