// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "core/licensing/pro_features.h"

#include "core/looks.h"
#include "core/watermark.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>
#include <pfx/io.h>
#include <pfx/look.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core::licensing {

namespace {

constexpr auto kContext = "ProFeatures";

void addOnce(QList<ProUse>& uses, const ProUse& use)
{
    const bool known = std::ranges::any_of(uses, [&use](const ProUse& u) { return u.item == use.item; });
    if (!known) {
        uses << use;
    }
}

void addSettingsUses(QList<ProUse>& uses, const pfx::Settings& settings)
{
    if (const QString preset = lightroomPreset(settings); !preset.isEmpty()) {
        addOnce(uses, {ProItem::LightroomPreset, preset});
    }
    const QString look = QString::fromStdString(settings.text("look.id", ""));
    if (!look.isEmpty() && lookUsesLut(look)) {
        addOnce(uses, {ProItem::Lut, lookName(look)});
    }
}

void addWatermark(QList<ProUse>& uses, const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    const std::optional<Watermark> w = WatermarkLibrary::load(path);
    addOnce(uses, {ProItem::Watermark, w ? w->name : QFileInfo(path).completeBaseName()});
}

// The order the Export box lists them in: the photo, then what was laid on it.
void sortUses(QList<ProUse>& uses)
{
    std::ranges::stable_sort(uses, {}, [](const ProUse& u) { return static_cast<int>(u.item); });
}

} // namespace

QString lightroomPreset(const pfx::Settings& settings)
{
    return QString::fromStdString(settings.text(kLightroomPresetKey, ""));
}

pfx::Settings withoutLightroomPreset(const pfx::Settings& settings)
{
    pfx::Settings out = settings;
    out.erase(kLightroomPresetKey);
    return out;
}

bool lookUsesLut(const QString& id)
{
    const std::shared_ptr<const pfx::Look> look = findLook(id);
    return look != nullptr && std::ranges::any_of(look->steps, [](const pfx::LookStep& s) {
               return s.enabled && s.op == "lut";
           });
}

QList<ProUse> photoUses(const pfx::Settings& settings, const QString& rawFile, const QString& watermarkPath)
{
    QList<ProUse> uses;
    if (!rawFile.isEmpty()) {
        uses << ProUse{ProItem::Raw, QFileInfo(rawFile).fileName()};
    }
    addSettingsUses(uses, settings);
    addWatermark(uses, watermarkPath);
    sortUses(uses);
    return uses;
}

QList<ProUse> batchUses(const std::vector<pfx::Settings>& steps, const QStringList& files,
                        const std::vector<pfx::Settings>& edits, const QString& watermarkPath)
{
    QList<ProUse> uses;
    const auto raw =
        std::ranges::find_if(files, [](const QString& f) { return pfx::isRawFile(f.toStdString()); });
    if (raw != files.end()) {
        uses << ProUse{ProItem::Raw, QFileInfo(*raw).fileName()};
    }
    for (const pfx::Settings& s : steps) {
        addSettingsUses(uses, s);
    }
    for (const pfx::Settings& s : edits) {
        addSettingsUses(uses, s);
    }
    addWatermark(uses, watermarkPath);
    sortUses(uses);
    return uses;
}

QString itemLabel(ProItem item)
{
    switch (item) {
    case ProItem::Raw:
        return QCoreApplication::translate(kContext, "Camera RAW photo");
    case ProItem::LightroomPreset:
        return QCoreApplication::translate(kContext, "Lightroom preset");
    case ProItem::Lut:
        return QCoreApplication::translate(kContext, "LUT look");
    case ProItem::Watermark:
        return QCoreApplication::translate(kContext, "Watermark");
    }
    return {};
}

QString sourceName(Source source)
{
    switch (source) {
    case Source::Account:
        return u"account"_s;
    case Source::Batch:
        return u"batch"_s;
    case Source::Paste:
        return u"paste"_s;
    case Source::Export:
        return u"export"_s;
    case Source::Share:
        return u"share"_s;
    case Source::Bake:
        return u"bake"_s;
    case Source::TrialEnd:
        return u"trial_end"_s;
    }
    return u"app"_s;
}

} // namespace pixelfx::core::licensing
