// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <pfx/settings.h>

// What PixelFx Pro adds, decided from the photo, the batch and the output alone: no widgets and
// no account. The window asks these before it saves anything; the answer for Free is a prompt.
namespace pixelfx::core::licensing {

/// Batch and paste settings handle this many photos at a time without Pro.
inline constexpr int kFreePhotos = 10;

/// What in a photo or a batch needs Pro to be saved.
enum class ProItem
{
    Raw,             ///< a camera RAW photo
    LightroomPreset, ///< settings from an imported Lightroom or Camera Raw preset
    Lut,             ///< a look built on an imported LUT
    Watermark,
};

/// One use of a Pro item: what it is and which one ("DSC_0412.NEF", "Clean Travel").
struct ProUse
{
    ProItem item = ProItem::Raw;
    QString name;
    bool operator==(const ProUse&) const = default;
};

/// The settings key an imported Lightroom preset leaves on a photo: the preset's name.
inline constexpr auto kLightroomPresetKey = "source.lightroomPreset";

/// The Lightroom preset `settings` came from, or empty.
[[nodiscard]] QString lightroomPreset(const pfx::Settings& settings);
/// `settings` without the mark of a Lightroom preset.
[[nodiscard]] pfx::Settings withoutLightroomPreset(const pfx::Settings& settings);
/// Whether the user's look `id` has a LUT step (a LUT import, or a look made with one).
[[nodiscard]] bool lookUsesLut(const QString& id);

/// What saving a photo with `settings` uses of Pro: the RAW (when `rawFile` is not empty, its
/// file name), the Lightroom preset, a LUT look, the watermark at `watermarkPath`.
[[nodiscard]] QList<ProUse> photoUses(const pfx::Settings& settings, const QString& rawFile,
                                      const QString& watermarkPath);

/// What a batch uses of Pro: each step's settings, the photos that are RAW (by extension; the
/// first one named), the watermark. `edits` are the remembered edits the batch keeps.
[[nodiscard]] QList<ProUse> batchUses(const std::vector<pfx::Settings>& steps, const QStringList& files,
                                      const std::vector<pfx::Settings>& edits, const QString& watermarkPath);

/// "Camera RAW photo", "Lightroom preset" ...: what the item is called in the Export box.
[[nodiscard]] QString itemLabel(ProItem item);

/// Where a buyer came from, sent to the checkout as `source` so the sales figures can say which
/// prompt sells.
enum class Source
{
    Account,  ///< the PixelFx Pro sheet
    Batch,    ///< Batch above the free count, or with Pro items
    Paste,    ///< paste settings to more photos than the free count
    Export,   ///< Export or Copy to clipboard with Pro items
    Share,    ///< share a look or preset as a file
    Bake,     ///< bake a look to .cube
    TrialEnd, ///< the note when the trial ends
};
[[nodiscard]] QString sourceName(Source source);

} // namespace pixelfx::core::licensing
