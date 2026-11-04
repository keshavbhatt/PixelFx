#pragma once

#include <QDate>
#include <QSize>
#include <QString>

#include <vector>

namespace pixelfx::core {

/// What Export writes (mock export.html; FEATURES X1, X2, X5, X6). Remembered between exports.
struct ExportOptions
{
    enum class IfExists
    {
        AddNumber,
        Overwrite,
        Skip,
    };
    QString extension = QStringLiteral("jpg"); ///< without the dot
    int quality = 90;
    int bitsPerSample = 8;
    int longEdge = 0; ///< 0: original size (when no size preset is chosen)
    /// A size preset (export-sizes.html) by id, empty for none; it decides the size.
    QString sizePreset;
    /// When the preset's shape differs from the photo: fit inside with a border, or crop to fill.
    bool fitInside = false;
    bool keepMetadata = true;
    bool keepLocation = false; ///< off by default for privacy (mock export.html)
    QString folder;
    QString nameTemplate = QStringLiteral("{name}-edit");
    IfExists ifExists = IfExists::AddNumber;

    bool operator==(const ExportOptions&) const = default;
};

/// A size preset: where the photo is going, and the pixels that takes (mock export-sizes.html).
struct SizePreset
{
    const char* id;
    const char* group; ///< Social, Screen, Print at 300 dpi (translatable, context "SizePresets")
    const char* name;
    int width = 0; ///< exact size, or 0 with longEdge
    int height = 0;
    int longEdge = 0;
    bool followsPhoto = false; ///< prints turn with the photo (portrait or landscape)
};

[[nodiscard]] const std::vector<SizePreset>& sizePresets();
[[nodiscard]] const SizePreset* sizePreset(const QString& id);
/// The exact output size of `preset` for a photo of `photo` (prints follow its orientation);
/// invalid for long-edge presets.
[[nodiscard]] QSize exactSize(const SizePreset& preset, const QSize& photo);

/// The file name for `source` under `options`, before the if-exists policy: tokens {name}
/// (the source's base name), {date} (yyyy-MM-dd), {w} and {h} (output size). Characters that
/// cannot be in a file name become "-"; an empty result falls back to {name}.
[[nodiscard]] QString exportFileName(const QString& source, const ExportOptions& options,
                                     const QSize& outputSize, const QDate& date);

/// The path to write, applying the if-exists policy, or an empty string to skip. Never the
/// source itself unless the policy is Overwrite and the template makes it so.
[[nodiscard]] QString exportTarget(const QString& source, const ExportOptions& options,
                                   const QSize& outputSize, const QDate& date);

[[nodiscard]] QString ifExistsKey(ExportOptions::IfExists value);
[[nodiscard]] ExportOptions::IfExists ifExistsFromKey(const QString& key);

} // namespace pixelfx::core
