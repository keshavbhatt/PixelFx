#pragma once

#include <QDate>
#include <QSize>
#include <QString>
#include <QStringList>

#include <pfx/io.h>
#include <vector>

namespace pixelfx::core {

/// What Export writes. Remembered between exports.
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
    /// A long edge never makes a smaller photo larger (Batch's "Never enlarge"; Export always).
    bool noEnlarge = true;
    /// A size preset (export-sizes.html) by id, empty for none; it decides the size.
    QString sizePreset;
    /// When the preset's shape differs from the photo: fit inside with a border, or crop to fill.
    bool fitInside = false;
    bool keepMetadata = true;
    bool keepLocation = false;
    /// A saved watermark (.pfxwatermark) to lay on each photo, empty for none.
    QString watermark;
    QString folder;
    QString nameTemplate = QStringLiteral("{name}-edit");
    /// What {preset} in the name becomes: the batch's recipe ("golden-hour"); not remembered.
    QString recipeName;
    IfExists ifExists = IfExists::AddNumber;

    bool operator==(const ExportOptions&) const = default;
};

/// A size preset: where the photo is going, and the pixels that takes.
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

/// The size a photo of `photo` is saved at under `options`' size preset or long edge.
[[nodiscard]] QSize outputSize(const ExportOptions& options, QSize photo);

/// What a file name's tokens know of a photo beyond its path: its place in the batch and what
/// its Exif says.
struct NameFacts
{
    int number = 1; ///< 1 for the first photo of the list
    int count = 1;
    QDate taken;    ///< invalid when the photo does not say
    QString camera; ///< the model, else the make; empty when the photo does not say
};

/// The day taken and the camera, from Exif.
[[nodiscard]] NameFacts nameFacts(const pfx::Metadata& metadata);

/// The tokens a name can hold, in the order the + Token menu lists them.
[[nodiscard]] QStringList nameTokens();

/// Whether each photo gets a name of its own under `nameTemplate` ({name} or {n} in it).
[[nodiscard]] bool namesDiffer(const QString& nameTemplate);

/// The file name for `source` under `options`, before the if-exists policy: tokens {name}
/// (the source's base name), {n} (its number in the batch, padded to the count's width), {date}
/// (today, yyyy-MM-dd), {taken} (the day taken, today when unknown), {camera}, {w} and {h}
/// (output size), {preset} (the recipe's name, "edit" when there is none). Characters that
/// cannot be in a file name become "-"; an empty result falls back to {name}.
[[nodiscard]] QString exportFileName(const QString& source, const ExportOptions& options,
                                     const QSize& outputSize, const QDate& date, const NameFacts& facts = {});

/// The path to write, applying the if-exists policy, or an empty string to skip. Never the
/// source itself unless the policy is Overwrite and the template makes it so.
[[nodiscard]] QString exportTarget(const QString& source, const ExportOptions& options,
                                   const QSize& outputSize, const QDate& date, const NameFacts& facts = {});

[[nodiscard]] QString ifExistsKey(ExportOptions::IfExists value);
[[nodiscard]] ExportOptions::IfExists ifExistsFromKey(const QString& key);

} // namespace pixelfx::core
