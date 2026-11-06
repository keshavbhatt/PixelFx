#pragma once

#include <QDate>
#include <QMutex>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

#include <pfx/io.h>
#include <pfx/resample.h>
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
    /// A size preset by id, empty for none; it decides the size.
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
    /// A custom size: both sides make an exact frame, one side scales the photo to it; empty
    /// for none.
    QSize customSize;
    /// Pixels per inch written into the file (prints); 0 leaves it out.
    int dpi = 0;
    /// Where Crop to fill keeps the photo: 0 the left or top, 0.5 the middle, 1 the right or
    /// bottom. Chosen per export by dragging the frame; not remembered.
    double frameX = 0.5;
    double frameY = 0.5;
    /// Sharpening for the screen or for print, once the photo has its final size.
    pfx::OutputSharpen sharpen = pfx::OutputSharpen::None;
    /// The colour space written; its profile is embedded.
    pfx::OutputSpace colorSpace = pfx::OutputSpace::Srgb;

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

/// A size the user kept under "Mine" (Custom size).
struct CustomSize
{
    QString name;
    QSize size; ///< a side of 0 follows the photo's shape
    int dpi = 0;

    bool operator==(const CustomSize&) const = default;
};
/// Whether a custom size has a side at all (one side alone follows the photo's shape).
[[nodiscard]] inline bool hasSize(const QSize& size)
{
    return size.width() > 0 || size.height() > 0;
}

/// The id of a custom size in a size list ("mine:Shop listing").
[[nodiscard]] QString customSizeId(const CustomSize& size);

[[nodiscard]] const std::vector<SizePreset>& sizePresets();
[[nodiscard]] const SizePreset* sizePreset(const QString& id);
/// The exact output size of `preset` for a photo of `photo` (prints follow its orientation);
/// invalid for long-edge presets.
[[nodiscard]] QSize exactSize(const SizePreset& preset, const QSize& photo);

/// The exact frame a photo of `photo` is saved in (a preset or custom size with both sides),
/// whose shape may differ from the photo's; invalid when the photo keeps its shape.
[[nodiscard]] QSize frameSize(const ExportOptions& options, const QSize& photo);
/// The size a photo of `photo` is saved at under `options`' size preset, custom size or long
/// edge.
[[nodiscard]] QSize outputSize(const ExportOptions& options, QSize photo);
/// The common name of a shape ("3:2", "4:5"), else the ratio to one ("1.85:1").
[[nodiscard]] QString ratioName(const QSize& size);
/// What a size list row says on its right: "1080 x 1350 · 4:5", "2048 long edge", "1800 x 1200".
[[nodiscard]] QString sizeDetail(const SizePreset& preset, const QSize& photo);
[[nodiscard]] QString sizeDetail(const CustomSize& size);

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

/// The paths one run (a batch) has chosen, shared by its workers: a name one photo took is
/// never given to another, whatever the if-exists policy says of files from before the run.
class NameClaims
{
public:
    /// Takes `path` for the run; false when another photo of the run has it.
    bool claim(const QString& path);
    [[nodiscard]] bool contains(const QString& path);

private:
    QMutex m_mutex;
    QSet<QString> m_paths;
};

/// The path to write, applying the if-exists policy, or an empty string to skip. Never the
/// source itself unless the policy is Overwrite and the template makes it so. With `claims`, a
/// path another photo of the run took gets a number, and the path chosen is claimed.
[[nodiscard]] QString exportTarget(const QString& source, const ExportOptions& options,
                                   const QSize& outputSize, const QDate& date, const NameFacts& facts = {},
                                   NameClaims* claims = nullptr);

[[nodiscard]] QString ifExistsKey(ExportOptions::IfExists value);
[[nodiscard]] ExportOptions::IfExists ifExistsFromKey(const QString& key);
/// How the options encode a file; `originalProfile` is the photo's own, for Same as original.
[[nodiscard]] pfx::SaveOptions saveOptions(const ExportOptions& options,
                                           std::vector<std::uint8_t> originalProfile = {});
/// "none", "screen", "print"; anything else reads as none.
[[nodiscard]] QString sharpenKey(pfx::OutputSharpen value);
[[nodiscard]] pfx::OutputSharpen sharpenFromKey(const QString& key);
/// "srgb", "display-p3", "adobe-rgb", "original"; anything else reads as sRGB.
[[nodiscard]] QString colorSpaceKey(pfx::OutputSpace value);
[[nodiscard]] pfx::OutputSpace colorSpaceFromKey(const QString& key);

} // namespace pixelfx::core
