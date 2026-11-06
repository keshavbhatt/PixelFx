#include "core/describe.h"
#include "core/file_names.h"
#include "core/presets.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <pfx/curve.h>
#include <pfx/document.h>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

class TestPresets : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void namesBecomeFileNamesOnEverySystem()
    {
        using pixelfx::core::fileSafeName;
        QCOMPARE(fileSafeName(u"Warm / faded: v2?"_s, u"Preset"_s), u"Warm - faded- v2-"_s);
        QCOMPARE(fileSafeName(u"..hidden"_s, u"Preset"_s), u"hidden"_s);
        QCOMPARE(fileSafeName(u"Ends with a dot. "_s, u"Preset"_s), u"Ends with a dot"_s);
        QCOMPARE(fileSafeName(u"   "_s, u"Preset"_s), u"Preset"_s);
        QCOMPARE(fileSafeName(u"..."_s, u"Look"_s), u"Look"_s);
        // Windows keeps these for devices, with or without more after a dot.
        QCOMPARE(fileSafeName(u"con"_s, u"Preset"_s), u"con-"_s);
        QCOMPARE(fileSafeName(u"NUL.old"_s, u"Preset"_s), u"NUL-.old"_s);
        QCOMPARE(fileSafeName(u"Console"_s, u"Preset"_s), u"Console"_s);
        // Long names are cut at 80 characters, never inside one (an emoji is two UTF-16 units).
        const QString emoji = QString(79, u'a') + QString::fromUtf8("\xF0\x9F\x8C\x85");
        const QString cut = fileSafeName(emoji, u"Preset"_s);
        QCOMPARE(cut, QString(79, u'a'));
        QVERIFY(!cut.back().isHighSurrogate());
    }

    void aPresetCarriesTheLook()
    {
        pfx::Settings photo;
        photo.set("light.exposure", 0.5);
        photo.set("look.id", std::string("builtin:noir"));
        photo.set("look.strength", 64.0);
        QVERIFY(pixelfx::core::unitsIn(photo).contains(u"look"_s));
        const pfx::Settings preset = pixelfx::core::presetFrom(photo, {u"look"_s});
        QCOMPARE(preset.text("look.id", ""), std::string("builtin:noir"));
        QCOMPARE(preset.number("look.strength", 0), 64.0);
        QVERIFY(!preset.contains("light.exposure"));
        QVERIFY(pixelfx::core::presetSummary(preset).contains(u"Look"_s));
        pfx::Settings other;
        other.set("look.id", std::string("builtin:warm"));
        const pfx::Settings applied = pixelfx::core::applyPreset(other, preset);
        QCOMPARE(applied.text("look.id", ""), std::string("builtin:noir"));
        // A preset from a photo without a look takes the look off, as other units reset theirs.
        const pfx::Settings noLook = pixelfx::core::presetFrom(pfx::Settings{}, {u"look"_s});
        QVERIFY(pixelfx::core::unitsIn(noLook).contains(u"look"_s));
        QVERIFY(pixelfx::core::applyPreset(other, noLook).text("look.id", "").empty());
    }

    void unitsSplitWhiteBalanceAndPerspective()
    {
        pfx::Settings s;
        s.set("color.vibrance", 20.0);
        s.set("color.wb.temperature", 10.0);
        s.set("geometry.perspective.vertical", 5.0);
        QCOMPARE(unitsIn(s), (QStringList{u"color"_s, u"wb"_s, u"perspective"_s}));
        QCOMPARE(changedCount(s, u"color"_s), 1);
        QVERIFY(keyInUnit("geometry.crop.w", u"crop"_s));
        QVERIFY(!keyInUnit("geometry.perspective.vertical", u"crop"_s));
    }

    void aPresetHoldsWholeUnitsAndStacks()
    {
        pfx::Settings photo;
        photo.set("light.exposure", 0.5);
        photo.set("light.contrast", 20.0);
        photo.set("color.vibrance", 30.0);
        photo.set("color.wb.temperature", 15.0);
        const pfx::Settings preset = presetFrom(photo, {u"light"_s});
        QVERIFY(preset.contains("light.highlights")); // every value of the unit, defaults too
        QVERIFY(!preset.contains("color.vibrance"));
        QCOMPARE(presetSummary(preset), u"Light"_s);

        pfx::Settings other;
        other.set("light.shadows", 40.0);     // replaced: the preset sets the whole of Light
        other.set("color.saturation", -20.0); // kept: Color is not in the preset
        const pfx::Settings out = applyPreset(other, preset);
        QCOMPARE(out.number("light.exposure", 0), 0.5);
        QCOMPARE(out.number("light.contrast", 0), 20.0);
        QVERIFY(!out.contains("light.shadows"));
        QCOMPARE(out.number("color.saturation", 0), -20.0);
        QVERIFY(!out.contains("light.highlights")); // defaults are not written into the photo
    }

    void colorWithoutWhiteBalanceKeepsThePhotosOwn()
    {
        pfx::Settings photo;
        photo.set("color.vibrance", 30.0);
        photo.set("color.wb.temperature", 15.0);
        const pfx::Settings preset = presetFrom(photo, {u"color"_s});
        pfx::Settings other;
        other.set("color.wb.temperature", -40.0);
        const pfx::Settings out = applyPreset(other, preset);
        QCOMPARE(out.number("color.vibrance", 0), 30.0);
        QCOMPARE(out.number("color.wb.temperature", 0), -40.0);
    }

    void libraryRoundTrip()
    {
        QTemporaryDir dir;
        const PresetLibrary library(dir.filePath(u"presets"_s));
        pfx::Settings s;
        s.set("light.exposure", 0.3);
        const QString path = library.save(u"Warm / evening"_s, u"My presets"_s, s);
        QVERIFY(!path.isEmpty());
        QVERIFY(!path.contains(u"Warm / "_s)); // the slash is not a folder
        const QString second = library.save(u"Warm / evening"_s, u"My presets"_s, s);
        QVERIFY(second != path);
        auto entries = library.entries();
        const auto mine = std::ranges::count_if(entries, [](const PresetEntry& e) { return !e.isBuiltIn(); });
        QCOMPARE(mine, 2);
        QCOMPARE(entries.front().name, u"Warm / evening"_s);
        QVERIFY(PresetLibrary::rename(path, u"Golden"_s));
        pfx::Settings changed;
        changed.set("light.exposure", -0.2);
        QVERIFY(PresetLibrary::update(path, changed));
        entries = library.entries();
        const auto golden =
            std::ranges::find_if(entries, [](const PresetEntry& e) { return e.name == u"Golden"_s; });
        QVERIFY(golden != entries.end());
        QCOMPARE(golden->settings.number("light.exposure", 0), -0.2);
        QVERIFY(library.remove(path));
        QVERIFY(!QFile::exists(path));
        QVERIFY(!library.remove(dir.filePath(u"elsewhere.pfxpreset"_s))); // only files of the library
    }

    void importAndExport()
    {
        QTemporaryDir dir;
        const PresetLibrary library(dir.filePath(u"presets"_s));
        pfx::Document doc;
        doc.name = "Shared look";
        doc.settings.set("detail.clarity", 20.0);
        const QString file = dir.filePath(u"shared.pfxpreset"_s);
        QVERIFY(pfx::saveDocument(doc, file.toStdString()).ok());
        const auto imported = library.importFile(file);
        QVERIFY2(!imported.path.isEmpty(), qPrintable(imported.error));
        QVERIFY(!library.save(u"Mine"_s, u"My presets"_s, doc.settings).isEmpty());
        const auto entries = library.entries();
        QCOMPARE(entries.front().group, u"My presets"_s); // My presets come first, Imported after
        QCOMPARE(entries.at(1).group, u"Imported"_s);
        QVERIFY(!library.importFile(dir.filePath(u"missing.pfxpreset"_s)).error.isEmpty());
        QVERIFY(PresetLibrary::exportTo(entries.front(), dir.filePath(u"out.pfxpreset"_s)));
        QVERIFY(QFile::exists(dir.filePath(u"out.pfxpreset"_s)));
    }

    void aColourPresetTurnsBlackAndWhiteBackToColour()
    {
        pfx::Settings photo;
        photo.set("bw.enabled", true);
        photo.set("bw.mix.blue", -40.0);
        photo.set("calibration.red.hue", 10.0);
        pfx::Settings colour;
        colour.set("bw.enabled", false);
        colour.set("light.exposure", 0.3);
        const pfx::Settings out = pixelfx::core::applyPreset(photo, colour);
        QVERIFY(!out.boolean("bw.enabled", false));
        QVERIFY(!out.contains("bw.mix.blue"));                // the whole Black & white part is the preset's
        QCOMPARE(out.number("calibration.red.hue", 0), 10.0); // other parts stay
        QCOMPARE(out.number("light.exposure", 0), 0.3);
    }

    void newPartsAreOfferedAndChoicesResetToTheirDefault()
    {
        pfx::Settings s;
        s.set("bw.enabled", true);
        s.set("detail.ca.auto", true);
        s.set("detail.clarity", 10.0);
        s.set("calibration.blue.saturation", 5.0);
        s.set("effects.vignette.style", std::string("highlight"));
        const QStringList units = pixelfx::core::unitsIn(s);
        QVERIFY(units.contains(u"bw"_s));
        QVERIFY(units.contains(u"optics"_s));
        QVERIFY(units.contains(u"calibration"_s));
        QVERIFY(!pixelfx::core::keyInUnit("detail.ca.auto", u"detail"_s)); // Optics, not Detail
        QVERIFY(pixelfx::core::keyInUnit("detail.lensProfile.enable", u"optics"_s));
        QVERIFY(!pixelfx::core::keyInUnit("detail.lensProfile.distortion", u"detail"_s));
        QCOMPARE(pixelfx::core::changedCount(s, u"effects"_s), 0); // the default style
        const pfx::Settings preset = pixelfx::core::presetFrom(pfx::Settings(), {u"color"_s});
        QCOMPARE(preset.text("color.profile", ""), std::string("standard"));
    }

    void theAmountBlendsFromThePhotoToThePresetAndPast()
    {
        pfx::Settings photo;
        photo.set("light.exposure", 0.2);
        photo.set("detail.clarity", 10.0);
        pfx::Settings preset;
        preset.set("light.exposure", 0.6);
        preset.set("light.contrast", 20.0);
        preset.set("bw.enabled", true);
        preset.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.7}, {1, 1}});
        const pfx::Settings none = blendPreset(photo, preset, 0.0);
        QCOMPARE(none.number("light.exposure", 0), 0.2);
        QVERIFY(!none.contains("light.contrast"));
        QVERIFY(!none.boolean("bw.enabled", false));
        QCOMPARE(none.number("detail.clarity", 0), 10.0); // not the preset's: always the photo's
        const pfx::Settings half = blendPreset(photo, preset, 50.0);
        QCOMPARE(half.number("light.exposure", 0), 0.4);
        QCOMPARE(half.number("light.contrast", 0), 10.0);
        QVERIFY(half.boolean("bw.enabled", false)); // a switch follows from any amount
        const std::vector<double> mid = pfx::sampleCurve(half.curve("curves.rgb"), 3);
        QVERIFY(std::abs(mid[1] - 0.6) < 0.02);
        QCOMPARE(blendPreset(photo, preset, 100.0), applyPreset(photo, preset));
        const pfx::Settings twice = blendPreset(photo, preset, 200.0);
        QCOMPARE(twice.number("light.contrast", 0), 40.0);
        QVERIFY(std::abs(twice.number("light.exposure", 0) - 1.0) < 1e-9);
        pfx::Settings strong;
        strong.set("light.contrast", 80.0);
        QCOMPARE(blendPreset({}, strong, 200.0).number("light.contrast", 0), 100.0); // held in range
    }

    void settingsAreToldInWords()
    {
        pfx::Settings s;
        QCOMPARE(describeSettings(s), QString());
        s.set("color.wb.temperature", 20.0);
        s.set("effects.grain.amount", 30.0);
        s.set("light.blacks", 15.0);
        const QString text = describeSettings(s);
        QVERIFY2(text.startsWith(u"Warm"_s), qPrintable(text));
        QVERIFY(text.contains(u"faded blacks"_s));
        QVERIFY(text.contains(u"grain"_s));
        s.set("bw.enabled", true);
        QVERIFY(describeSettings(s).contains(u"lack and white"_s));
        QVERIFY(!describeSettings(s).contains(u"arm"_s)); // no colour words in black and white
        QCOMPARE(hueName(200), u"teal"_s);
        QCOMPARE(hueName(-10), u"red"_s);
    }

    void theCatalogLoadsWithUniqueNamesAndNoBrands()
    {
        const std::vector<PresetEntry> all = PresetLibrary::builtIn();
        QVERIFY2(all.size() >= 400, qPrintable(QString::number(all.size())));
        QSet<QString> names;
        QSet<QString> ids;
        // Names say what a preset looks like, never whose product or style it copies.
        static const QStringList kForbidden = {
            u"Kodak"_s,     u"Fuji"_s,     u"Ilford"_s,    u"Agfa"_s,       u"Portra"_s,     u"Ektar"_s,
            u"Velvia"_s,    u"Provia"_s,   u"Tri-X"_s,     u"Kodachrome"_s, u"Ektachrome"_s, u"CineStill"_s,
            u"Polaroid"_s,  u"Lomo"_s,     u"Holga"_s,     u"Rollei"_s,     u"Foma"_s,       u"Instagram"_s,
            u"Clarendon"_s, u"Valencia"_s, u"Nashville"_s, u"Anderson"_s,   u"Wong"_s,       u"McCurry"_s,
            u"Adams"_s,     u"LOTR"_s,     u"Canon"_s,     u"Ricoh"_s};
        for (const PresetEntry& e : all) {
            QVERIFY2(!names.contains(e.name), qPrintable(e.name));
            names.insert(e.name);
            QVERIFY2(!ids.contains(e.id), qPrintable(e.id));
            ids.insert(e.id);
            QVERIFY(e.isBuiltIn());
            QVERIFY(!e.category.isEmpty());
            QVERIFY2(e.autoEnhance || !e.settings.keys().empty(), qPrintable(e.name));
            for (const QString& word : kForbidden) {
                const QRegularExpression whole(u"\\b%1\\b"_s.arg(QRegularExpression::escape(word)),
                                               QRegularExpression::CaseInsensitiveOption);
                QVERIFY2(!whole.match(e.name).hasMatch(), qPrintable(e.name));
            }
        }
        QVERIFY(std::ranges::any_of(all, [](const PresetEntry& e) {
            return e.name == u"Portrait 400"_s && e.category == u"film-color"_s && !e.description.isEmpty();
        }));
    }

    void lightroomPresetsComeInWithAReport()
    {
        QTemporaryDir dir;
        const PresetLibrary library(dir.filePath(u"presets"_s));
        const QString file = dir.filePath(u"clean.xmp"_s);
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(
            R"(<x:xmpmeta><rdf:Description xmlns:crs="x" crs:Exposure2012="+0.5" crs:LensManualDistortionAmount="+10">)"
            R"(<crs:Name><rdf:Alt><rdf:li xml:lang="x-default">Clean</rdf:li></rdf:Alt></crs:Name>)"
            R"(</rdf:Description></x:xmpmeta>)");
        out.close();
        const auto imported = library.importFile(file);
        QVERIFY2(!imported.path.isEmpty(), qPrintable(imported.error));
        QCOMPARE(imported.skipped, QStringList{u"LensManualDistortionAmount"_s});
        const auto entries = library.entries();
        QCOMPARE(entries.front().name, u"Lightroom: Clean"_s);
        QCOMPARE(entries.front().settings.number("light.exposure", 0), 0.5);
    }

    void builtInsAreThere()
    {
        const auto built = PresetLibrary::builtIn();
        QVERIFY(built.size() >= 3);
        QVERIFY(built.front().autoEnhance);
        for (const PresetEntry& e : built) {
            QVERIFY(e.isBuiltIn());
        }
    }
};

QTEST_GUILESS_MAIN(TestPresets)
#include "tst_presets.moc"
