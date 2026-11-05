// End to end on real photos: every feature that changes or writes pixels, run on a folder of
// photos named by PIXELFX_E2E_PHOTOS (skipped when it is not set; no photos are kept in the
// repository). Outputs go to a temporary folder; the photos are only read.
#include "core/crop_geometry.h"
#include "core/looks.h"
#include "core/presets.h"
#include "core/settings_clipboard.h"
#include "core/sidecar_store.h"
#include "core/watermark.h"
#include "services/batch_runner.h"
#include "services/document.h"
#include "services/exporter.h"
#include "services/qt_image_bridge.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <pfx/document.h>
#include <pfx/io.h>
#include <pfx/resample.h>

using namespace Qt::StringLiterals;
using pixelfx::core::ExportOptions;
using pixelfx::services::Document;

namespace {

constexpr int kPreview = 1200;

// The mean of all channels of an sRGB picture, 0 to 255.
double meanLevel(const QImage& image)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    double sum = 0.0;
    for (int y = 0; y < rgb.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            sum += qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]);
        }
    }
    return sum / (3.0 * rgb.width() * rgb.height());
}

// The mean difference between two pictures of one size, 0 to 255.
double difference(const QImage& a, const QImage& b)
{
    const QImage x = a.convertToFormat(QImage::Format_RGB32);
    const QImage y = b.convertToFormat(QImage::Format_RGB32).scaled(x.size());
    double sum = 0.0;
    for (int row = 0; row < x.height(); ++row) {
        const auto* p = reinterpret_cast<const QRgb*>(x.constScanLine(row));
        const auto* q = reinterpret_cast<const QRgb*>(y.constScanLine(row));
        for (int col = 0; col < x.width(); ++col) {
            sum += std::abs(qRed(p[col]) - qRed(q[col])) + std::abs(qGreen(p[col]) - qGreen(q[col])) +
                   std::abs(qBlue(p[col]) - qBlue(q[col]));
        }
    }
    return sum / (3.0 * x.width() * x.height());
}

QImage shown(const pfx::Image& image, const pfx::Settings& settings)
{
    return pixelfx::services::toDisplayImage(pixelfx::core::renderPhoto(image, settings));
}

int longEdgeOf(const QString& file)
{
    const auto loaded = pfx::load(file.toStdString());
    return loaded ? std::max(loaded.value().info.width, loaded.value().info.height) : 0;
}

} // namespace

class TestEndToEnd : public QObject
{
    Q_OBJECT

private:
    QStringList m_photos;
    QTemporaryDir m_out;
    pfx::Image m_sample; // the first photo, small, for the render checks

    // Opens `path` in `doc` and waits for its preview.
    static bool openAndWait(Document& doc, const QString& path)
    {
        QSignalSpy ready(&doc, &Document::previewReady);
        QSignalSpy failed(&doc, &Document::failed);
        doc.open(path, kPreview);
        return QTest::qWaitFor([&] { return !ready.isEmpty() || !failed.isEmpty(); }, 30000) &&
               failed.isEmpty();
    }

    // Exports `source` with `options` and returns the written file, or an empty string.
    static QString exportOne(const QString& source, const pfx::Settings& settings,
                             const ExportOptions& options)
    {
        pixelfx::services::Exporter exporter;
        QSignalSpy done(&exporter, &pixelfx::services::Exporter::finished);
        QSignalSpy failed(&exporter, &pixelfx::services::Exporter::failed);
        exporter.start(source, settings, options);
        if (!QTest::qWaitFor([&] { return !done.isEmpty() || !failed.isEmpty(); }, 60000) || done.isEmpty()) {
            if (!failed.isEmpty()) {
                qWarning() << "export failed:" << failed.first().at(1).toString();
            }
            return {};
        }
        return done.first().first().toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        const QString folder = qEnvironmentVariable("PIXELFX_E2E_PHOTOS");
        if (folder.isEmpty()) {
            QSKIP("set PIXELFX_E2E_PHOTOS to a folder of photos to run the end-to-end checks");
        }
        for (const QFileInfo& f : QDir(folder).entryInfoList(QDir::Files, QDir::Name)) {
            const QString suffix = f.suffix().toLower();
            if (suffix == u"jpg"_s || suffix == u"jpeg"_s || suffix == u"png"_s || suffix == u"tif"_s ||
                suffix == u"tiff"_s || suffix == u"webp"_s) {
                m_photos << f.absoluteFilePath();
            }
        }
        QVERIFY2(!m_photos.isEmpty(), "no photos in PIXELFX_E2E_PHOTOS");
        auto loaded = pfx::load(m_photos.first().toStdString(), {800});
        QVERIFY(loaded.ok());
        m_sample = std::move(loaded).value().image;
        qInfo().noquote() << u"%1 photos from %2"_s.arg(m_photos.size()).arg(folder);
    }

    void everyPhotoOpensWithAPreviewAndAHistogram()
    {
        Document doc;
        QStringList broken;
        QElapsedTimer clock;
        qint64 slowest = 0;
        for (const QString& path : m_photos) {
            clock.start();
            if (!openAndWait(doc, path) || doc.preview().isNull() || !doc.info().size.isValid()) {
                broken << QFileInfo(path).fileName();
                continue;
            }
            slowest = std::max(slowest, clock.elapsed());
            QVERIFY(std::max(doc.preview().width(), doc.preview().height()) <= kPreview);
        }
        qInfo().noquote() << u"opened %1 of %2, slowest %3 ms"_s.arg(m_photos.size() - broken.size())
                                 .arg(m_photos.size())
                                 .arg(slowest);
        QVERIFY2(broken.isEmpty(), qPrintable(broken.join(u", "_s)));
    }

    void anEditChangesThePictureAndUndoBringsItBack()
    {
        Document doc;
        QVERIFY(openAndWait(doc, m_photos.first()));
        const QImage before = doc.preview();
        QSignalSpy ready(&doc, &Document::previewReady);
        pfx::Settings brighter = doc.settings();
        brighter.set("light.exposure", 1.0);
        brighter.set("light.contrast", 20.0);
        doc.setSettings(brighter);
        doc.commit(u"Exposure"_s);
        QVERIFY(ready.wait(30000));
        QVERIFY2(meanLevel(doc.preview()) > meanLevel(before) + 10.0, "one stop brighter is brighter");
        doc.undo();
        QVERIFY(ready.wait(30000));
        QVERIFY2(difference(doc.preview(), before) < 1.0, "undo gives the photo back");
        doc.redo();
        QVERIFY(ready.wait(30000));
        QVERIFY(meanLevel(doc.preview()) > meanLevel(before) + 10.0);
    }

    void everyBuiltInLookRendersAndChangesThePhoto()
    {
        const QImage original = shown(m_sample, {});
        QStringList unchanged;
        for (const pixelfx::core::LookEntry& look : pixelfx::core::builtInLooks()) {
            const QImage out = shown(m_sample, pixelfx::core::withLook({}, look.id, 100));
            QVERIFY2(!out.isNull(), qPrintable(look.name));
            if (difference(out, original) < 0.5) {
                unchanged << look.name;
            }
        }
        QVERIFY2(unchanged.isEmpty(), qPrintable(u"no visible change: "_s + unchanged.join(u", "_s)));
    }

    void everyBuiltInPresetRendersIncludingAutoEnhance()
    {
        const QTemporaryDir folder;
        const QImage original = shown(m_sample, {});
        for (const pixelfx::core::PresetEntry& p : pixelfx::core::PresetLibrary(folder.path()).entries()) {
            if (!p.isBuiltIn()) {
                continue;
            }
            pixelfx::services::BatchRecipe recipe;
            recipe.presets = {p.settings};
            recipe.autoFirst = p.autoEnhance;
            const pfx::Settings settings = pixelfx::services::BatchRunner::settingsFor(recipe, {}, m_sample);
            const QImage out = shown(m_sample, settings);
            QVERIFY2(!out.isNull(), qPrintable(p.name));
            QVERIFY2(difference(out, original) > 0.5, qPrintable(p.name + u": no visible change"_s));
        }
    }

    void cropTurnsAndStraighteningGiveTheirSizes()
    {
        const int w = m_sample.width();
        const int h = m_sample.height();
        pfx::Settings turned;
        pixelfx::core::turn(turned, true);
        const QImage t = shown(m_sample, turned);
        QCOMPARE(t.size(), QSize(h, w));
        pfx::Settings square;
        pixelfx::core::setCrop(square,
                               pixelfx::core::fitRatio(pixelfx::core::cropOf(square), QSize(w, h), 1.0));
        const QImage s = shown(m_sample, square);
        QVERIFY2(std::abs(s.width() - s.height()) <= 1,
                 qPrintable(u"%1x%2"_s.arg(s.width()).arg(s.height())));
        pfx::Settings level;
        pixelfx::core::setAngle(level, 5.0);
        const QImage l = shown(m_sample, level);
        QVERIFY(!l.isNull());
        QVERIFY(difference(l, shown(m_sample, {})) > 1.0);
    }

    void aHealedSpotChangesOnlyItsArea()
    {
        pfx::Spot spot;
        spot.x = 0.5;
        spot.y = 0.5;
        spot.sourceX = 0.3;
        spot.sourceY = 0.3;
        spot.radius = 0.05;
        pfx::Settings healed;
        healed.set("heal.spots", pfx::Spots{spot});
        const QImage before = shown(m_sample, {});
        const QImage after = shown(m_sample, healed);
        QCOMPARE(after.size(), before.size());
        const QRect corner(0, 0, before.width() / 6, before.height() / 6);
        QVERIFY2(difference(after.copy(corner), before.copy(corner)) < 0.5,
                 "away from the spot nothing moves");
        const QRect middle(before.width() * 45 / 100, before.height() * 45 / 100, before.width() / 10,
                           before.height() / 10);
        QVERIFY2(difference(after.copy(middle), before.copy(middle)) > 1.0, "the spot itself changes");
    }

    void exportWritesEveryFormatAtItsSize()
    {
        const QString source = m_photos.first();
        pfx::Settings edit;
        edit.set("light.exposure", 0.5);
        QStringList problems;
        for (const std::string& ext : pfx::writableExtensions()) {
            ExportOptions o;
            o.extension = QString::fromStdString(ext.substr(1));
            o.longEdge = 1000;
            o.folder = m_out.filePath(u"formats"_s);
            const QString written = exportOne(source, edit, o);
            if (written.isEmpty() || !written.endsWith(o.extension)) {
                problems << o.extension + u": not written"_s;
                continue;
            }
            const int edge = longEdgeOf(written);
            if (edge != std::min(1000, longEdgeOf(source))) {
                problems << u"%1: long edge %2"_s.arg(o.extension).arg(edge);
            }
        }
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(u"; "_s)));
    }

    void exportSizePresetsMetadataAndWatermark()
    {
        // A size preset gives its exact frame.
        const QString source = m_photos.first();
        ExportOptions o;
        o.folder = m_out.filePath(u"options"_s);
        o.sizePreset = u"instagram-portrait"_s;
        const QString framed = exportOne(source, {}, o);
        QVERIFY(!framed.isEmpty());
        const auto f = pfx::load(framed.toStdString());
        QVERIFY(f.ok());
        QCOMPARE(QSize(f.value().info.width, f.value().info.height), QSize(1080, 1350));

        // Metadata kept or left out, on a photo that carries camera details.
        QString withExif;
        for (const QString& photo : m_photos) {
            const auto loaded = pfx::load(photo.toStdString(), {64});
            if (loaded && !loaded.value().metadata.exif.empty()) {
                withExif = photo;
                break;
            }
        }
        o.sizePreset.clear();
        o.longEdge = 800;
        o.keepMetadata = false;
        o.nameTemplate = u"{name}-bare"_s;
        if (withExif.isEmpty()) {
            qInfo() << "no photo carries EXIF: keeping and leaving out camera details are not checked";
        } else {
            const auto bare = pfx::load(exportOne(withExif, {}, o).toStdString());
            QVERIFY(bare.ok());
            QVERIFY2(bare.value().metadata.exif.empty(), "Keep camera details off leaves EXIF out");
            o.keepMetadata = true;
            o.nameTemplate = u"{name}-kept"_s;
            const auto kept = pfx::load(exportOne(withExif, {}, o).toStdString());
            QVERIFY(kept.ok());
            QVERIFY2(!kept.value().metadata.exif.empty(), "Keep camera details keeps EXIF");
        }

        // A watermark lands in its corner and nowhere else.
        pixelfx::core::Watermark mark;
        mark.name = u"E2E"_s;
        mark.text = u"PixelFx E2E"_s;
        mark.size = 0.08;
        mark.opacity = 1.0;
        const QString saved = pixelfx::core::WatermarkLibrary(m_out.filePath(u"marks"_s)).save(mark);
        QVERIFY(!saved.isEmpty());
        o.keepMetadata = false;
        o.nameTemplate = u"{name}-plain"_s;
        const auto plain = pfx::load(exportOne(source, {}, o).toStdString());
        o.watermark = saved;
        o.nameTemplate = u"{name}-marked"_s;
        const auto marked = pfx::load(exportOne(source, {}, o).toStdString());
        QVERIFY(plain.ok() && marked.ok());
        const auto toQ = [](const pfx::Image& image) { return pixelfx::services::toDisplayImage(image); };
        const QImage a = toQ(plain.value().image);
        const QImage b = toQ(marked.value().image);
        const QRect corner(a.width() * 3 / 5, a.height() * 4 / 5, a.width() * 2 / 5, a.height() / 5);
        const QRect topLeft(0, 0, a.width() / 3, a.height() / 3);
        QVERIFY2(difference(a.copy(corner), b.copy(corner)) > 1.0, "the mark is in the bottom right");
        QVERIFY2(difference(a.copy(topLeft), b.copy(topLeft)) < 0.5, "and not elsewhere");
    }

    void autoEnhanceHistoryAndSnapshots()
    {
        Document doc;
        QVERIFY(openAndWait(doc, m_photos.at(m_photos.size() / 2)));
        QSignalSpy ready(&doc, &Document::previewReady);
        const QImage original = doc.preview();
        doc.autoAdjust();
        QVERIFY(ready.wait(30000));
        QVERIFY2(!doc.settings().keys().empty(), "Auto enhance sets something");
        const pfx::Settings automatic = doc.settings();
        doc.autoWhiteBalance();
        QVERIFY(ready.wait(30000));
        doc.saveSnapshot(u"Auto"_s);
        QCOMPARE(doc.snapshots().size(), std::size_t{1});
        // Back to the first step of the history: the photo as it opened.
        doc.goToStep(0);
        QVERIFY(ready.wait(30000));
        QVERIFY2(difference(doc.preview(), original) < 1.0, "the first step is the photo as opened");
        doc.applySnapshot(0);
        QVERIFY(ready.wait(30000));
        QVERIFY(doc.settings() != pfx::Settings{});
        QVERIFY(automatic.keys().size() <= doc.settings().keys().size());
    }

    void savedPresetsAndLooksComeBack()
    {
        const QTemporaryDir folder;
        pfx::Settings warm;
        warm.set("color.wb.temperature", 25.0);
        warm.set("color.vibrance", 15.0);
        const pixelfx::core::PresetLibrary presets(folder.filePath(u"presets"_s));
        const QString preset = presets.save(u"Warm test"_s, u"My presets"_s, warm);
        QVERIFY(!preset.isEmpty());
        bool found = false;
        for (const pixelfx::core::PresetEntry& p : presets.entries()) {
            if (p.name == u"Warm test"_s) {
                found = p.settings == warm;
            }
        }
        QVERIFY2(found, "the saved preset reads back with its settings");
        // A look of one's own, saved and applied by its id, as a look tile does.
        pfx::Look look;
        look.defaultStrength = 70;
        pfx::LookStep step;
        step.op = "vibrance_saturation";
        step.params.set("color.saturation", -60.0);
        look.steps.push_back(step);
        const pixelfx::core::LookLibrary looks(folder.filePath(u"looks"_s));
        const QString id = looks.save(u"Muted test"_s, look);
        QVERIFY(!id.isEmpty());
        pixelfx::core::setLookFolder(looks.folder()); // where the app finds the user's looks
        const QImage out = shown(m_sample, pixelfx::core::withLook({}, id, 100));
        QVERIFY2(!out.isNull() && difference(out, shown(m_sample, {})) > 1.0,
                 "the saved look renders by its id");
    }

    void editsAreRememberedAndCopyPasteCarriesThem()
    {
        const QTemporaryDir sidecars;
        pfx::Settings edit;
        {
            Document doc;
            doc.setSidecarFolder(sidecars.path());
            QVERIFY(openAndWait(doc, m_photos.first()));
            edit = doc.settings();
            edit.set("color.vibrance", 30.0);
            edit.set("light.shadows", 25.0);
            doc.setSettings(edit);
            doc.commit(u"Vibrance"_s);
            doc.flush();
        }
        Document again;
        again.setSidecarFolder(sidecars.path());
        QVERIFY(openAndWait(again, m_photos.first()));
        QCOMPARE(again.settings().number("color.vibrance", 0), 30.0);
        QCOMPARE(again.settings().number("light.shadows", 0), 25.0);
        // Copy, then paste onto another photo: the look travels, its own geometry stays.
        pfx::Settings other;
        pixelfx::core::turn(other, true);
        const auto copied = pixelfx::core::settingsFromClipboard(
            pixelfx::core::settingsToClipboard(again.settings(), QFileInfo(m_photos.first()).fileName()));
        if (!copied) {
            QFAIL("the copied settings do not read back");
            return;
        }
        const pfx::Settings pasted = pixelfx::core::pasted(other, *copied);
        QCOMPARE(pasted.number("color.vibrance", 0), 30.0);
        QCOMPARE(pixelfx::core::cropOf(pasted), pixelfx::core::cropOf(other));
    }

    void batchProcessesEveryPhotoWithALookASizeAndAWatermark()
    {
        pixelfx::core::Watermark mark;
        mark.name = u"Batch"_s;
        mark.text = u"(c) {year}"_s;
        pixelfx::services::BatchRecipe recipe;
        recipe.presets = {pixelfx::core::withLook({}, pixelfx::core::builtInLooks().front().id, 80)};
        recipe.keepEdits = false;
        recipe.output.folder = m_out.filePath(u"batch"_s);
        recipe.output.longEdge = 1024;
        recipe.output.watermark = pixelfx::core::WatermarkLibrary(m_out.filePath(u"marks"_s)).save(mark);
        pixelfx::services::BatchRunner runner;
        QSignalSpy finished(&runner, &pixelfx::services::BatchRunner::finished);
        QElapsedTimer clock;
        clock.start();
        runner.start(m_photos, recipe);
        QVERIFY(finished.wait(600000));
        QStringList failures;
        for (int i = 0; i < runner.total(); ++i) {
            if (runner.state(i) != pixelfx::services::BatchRunner::State::Saved) {
                failures << QFileInfo(m_photos.at(i)).fileName() + u": "_s + runner.reason(i);
                continue;
            }
            const int edge = longEdgeOf(runner.target(i));
            const int expected = std::min(1024, longEdgeOf(m_photos.at(i)));
            if (edge != expected) {
                failures << u"%1: long edge %2, not %3"_s.arg(QFileInfo(m_photos.at(i)).fileName())
                                .arg(edge)
                                .arg(expected);
            }
        }
        qInfo().noquote() << u"batch: %1 saved of %2 in %3 s"_s.arg(runner.saved())
                                 .arg(runner.total())
                                 .arg(static_cast<double>(clock.elapsed()) / 1000.0, 0, 'f', 1);
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(u"; "_s)));
    }

    void theCommandLineRunsABatchRecipe()
    {
        // pixelfx --batch recipe.pfxbatch photos... writes each photo and says so.
        pixelfx::services::BatchRecipe recipe;
        recipe.presets = {pixelfx::core::withLook({}, pixelfx::core::builtInLooks().back().id, 100)};
        recipe.keepEdits = false;
        recipe.output.folder = m_out.filePath(u"cli"_s);
        recipe.output.longEdge = 640;
        recipe.output.extension = u"png"_s;
        pfx::Document doc;
        doc.kind = pfx::DocumentKind::Batch;
        doc.name = "E2E";
        doc.batch = pixelfx::services::bodyFromRecipe(recipe);
        const QString file = m_out.filePath(u"e2e.pfxbatch"_s);
        QVERIFY(pfx::saveDocument(doc, file.toStdString()).ok());
        const QStringList some = m_photos.mid(0, 5);
        QProcess app;
        app.start(QStringLiteral(PIXELFX_BINARY), QStringList{u"--batch"_s, file} + some);
        QVERIFY(app.waitForFinished(300000));
        QCOMPARE(app.exitCode(), 0);
        const QStringList written = QDir(m_out.filePath(u"cli"_s)).entryList({u"*.png"_s}, QDir::Files);
        QCOMPARE(written.size(), some.size());
        for (const QString& name : written) {
            QVERIFY(longEdgeOf(m_out.filePath(u"cli/"_s + name)) <= 640);
        }
    }
};

QTEST_MAIN(TestEndToEnd)
#include "tst_e2e.moc"
