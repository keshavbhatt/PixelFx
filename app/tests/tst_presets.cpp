#include "core/presets.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/document.h>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

class TestPresets : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
        QVERIFY(library.rename(path, u"Golden"_s));
        pfx::Settings changed;
        changed.set("light.exposure", -0.2);
        QVERIFY(library.update(path, changed));
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
        const auto [path, error] = library.importFile(file);
        QVERIFY2(!path.isEmpty(), qPrintable(error));
        const auto entries = library.entries();
        QCOMPARE(entries.front().group, u"Imported"_s);
        QVERIFY(!library.importFile(dir.filePath(u"missing.pfxpreset"_s)).second.isEmpty());
        QVERIFY(library.exportTo(entries.front(), dir.filePath(u"out.pfxpreset"_s)));
        QVERIFY(QFile::exists(dir.filePath(u"out.pfxpreset"_s)));
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
