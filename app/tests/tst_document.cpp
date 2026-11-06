#include "services/document.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::services::Document;

namespace {

QString writePhoto(const QTemporaryDir& dir, const QString& name, int w, int h)
{
    pfx::Image img(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float* p = img.pixel(x, y);
            p[0] = static_cast<float>(x) / static_cast<float>(w);
            p[1] = 0.2F;
            p[2] = static_cast<float>(y) / static_cast<float>(h);
            p[3] = 1.0F;
        }
    }
    const QString path = dir.filePath(name);
    if (!pfx::save(img, path.toStdString()).ok()) {
        return {};
    }
    return path;
}

} // namespace

class TestDocument : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void opensAndDeliversAPreview()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 300);
        QVERIFY(!path.isEmpty());
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 200);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.info().size, QSize(400, 300));      // full size from the file
        QCOMPARE(doc.preview().size(), QSize(200, 150)); // preview at most 200 on the long edge
        QVERIFY(doc.isOpen());
        QVERIFY(!doc.isBusy());
    }

    void editsRenderAndUndo()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 160, 120);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 200);
        QVERIFY(ready.wait(10000));
        const QRgb before = doc.preview().pixel(80, 60);

        pfx::Settings brighter;
        brighter.set("light.exposure", 1.0);
        doc.setSettings(brighter);
        doc.commit(u"Exposure +1"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.isBusy(), 10000);
        QVERIFY(qGray(doc.preview().pixel(80, 60)) > qGray(before));
        QVERIFY(doc.history().canUndo());

        QSignalSpy replaced(&doc, &Document::settingsReplaced);
        doc.undo();
        QCOMPARE(replaced.count(), 1);
        QVERIFY(doc.settings().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!doc.isBusy(), 10000);
        QCOMPARE(doc.preview().pixel(80, 60), before);
        doc.redo();
        QCOMPARE(doc.settings().number("light.exposure", 0), 1.0);
    }

    void closingLeavesNothingOpenAndSaysWhetherAnEditIsLost()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 160, 120);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 200);
        QVERIFY(ready.wait(10000));
        QVERIFY(!doc.isEdited());
        pfx::Settings brighter;
        brighter.set("light.exposure", 1.0);
        doc.setSettings(brighter);
        doc.commit(u"Exposure +1"_s);
        QVERIFY(doc.isEdited());
        doc.undo();
        QVERIFY(!doc.isEdited()); // back as it opened: nothing to lose
        QSignalSpy closed(&doc, &Document::closed);
        doc.close();
        QCOMPARE(closed.count(), 1);
        QVERIFY(!doc.isOpen());
        QVERIFY(!doc.isEdited());
        QVERIFY(doc.preview().isNull());
        QVERIFY(doc.settings().isEmpty());
        QVERIFY(!doc.history().canUndo());
        // An open still on its way when closed is dropped.
        QSignalSpy opened(&doc, &Document::opened);
        doc.open(path, 200);
        doc.close();
        QTest::qWait(500);
        QCOMPARE(opened.count(), 0);
        QVERIFY(!doc.isOpen());
    }

    void manyQuickChangesRenderTheLastOne()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 300);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 400);
        QVERIFY(ready.wait(10000));
        ready.clear();
        for (int i = 1; i <= 30; ++i) {
            pfx::Settings s;
            s.set("light.exposure", i / 30.0);
            doc.setSettings(s);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!doc.isBusy(), 15000);
        QVERIFY(ready.count() < 30); // coalesced, not one render per change
        pfx::Settings last;
        last.set("light.exposure", 1.0);
        Document reference;
        QSignalSpy refReady(&reference, &Document::previewReady);
        reference.open(path, 400);
        QVERIFY(refReady.wait(10000));
        reference.setSettings(last);
        QTRY_VERIFY_WITH_TIMEOUT(!reference.isBusy(), 10000);
        QCOMPARE(doc.preview(), reference.preview());
    }

    void applyIsOneStep()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 64, 48);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 64);
        QVERIFY(ready.wait(10000));
        pfx::Settings preset;
        preset.set("light.contrast", 30.0);
        preset.set("color.vibrance", 20.0);
        doc.apply(preset, u"Moody"_s);
        QCOMPARE(doc.history().undoLabel(), u"Moody"_s);
        doc.undo();
        QVERIFY(doc.settings().isEmpty());
    }

    void editsAreRememberedPerPhoto()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 64, 48);
        const QString edits = dir.filePath(u"edits"_s);
        {
            Document doc;
            doc.setSidecarFolder(edits);
            QSignalSpy ready(&doc, &Document::previewReady);
            doc.open(path, 64);
            QVERIFY(ready.wait(10000));
            pfx::Settings s;
            s.set("light.shadows", 40.0);
            doc.apply(s, u"Shadows +40"_s);
        } // destroying the document writes what is pending
        Document again;
        again.setSidecarFolder(edits);
        QSignalSpy opened(&again, &Document::opened);
        again.open(path, 64);
        QVERIFY(opened.wait(10000));
        QCOMPARE(again.settings().number("light.shadows", 0), 40.0);
        QVERIFY(!again.history().canUndo()); // the remembered edit is the starting point
        again.apply({}, u"Reset all"_s);
        again.flush();
        QVERIFY(QDir(edits).entryList(QDir::Files).isEmpty()); // nothing left to remember
    }

    void editsThatCannotBeRememberedAreSaid()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 64, 48);
        const QString edits = dir.filePath(u"edits"_s);
        QDir().mkpath(edits);
        QFile::setPermissions(edits, QFileDevice::ReadOwner | QFileDevice::ExeOwner); // cannot be written
        Document doc;
        doc.setSidecarFolder(edits);
        QSignalSpy ready(&doc, &Document::previewReady);
        QSignalSpy notRemembered(&doc, &Document::editsNotRemembered);
        doc.open(path, 64);
        QVERIFY(ready.wait(10000));
        pfx::Settings s;
        s.set("light.shadows", 40.0);
        doc.apply(s, u"Shadows +40"_s);
        doc.flush();
        QVERIFY(notRemembered.wait(5000));
        QCOMPARE(notRemembered.first().first().toString(), path);
        QFile::setPermissions(edits,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }

    void autoBrightensADarkPhotoAndKeepsCurves()
    {
        QTemporaryDir dir;
        pfx::Image dark(120, 80);
        for (int y = 0; y < 80; ++y) {
            for (int x = 0; x < 120; ++x) {
                const float v = 0.004F + 0.03F * static_cast<float>(x) / 120.0F;
                float* p = dark.pixel(x, y);
                p[0] = v;
                p[1] = v;
                p[2] = v;
                p[3] = 1.0F;
            }
        }
        const QString path = dir.filePath(u"dark.png"_s);
        pfx::SaveOptions sixteen;
        sixteen.bitsPerSample = 16;
        QVERIFY(pfx::save(dark, path.toStdString(), sixteen).ok());
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 120);
        QVERIFY(ready.wait(10000));
        pfx::Settings withCurve;
        withCurve.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.6}, {1, 1}});
        withCurve.set("light.exposure", -1.0);
        doc.apply(withCurve, u"Setup"_s);
        doc.autoAdjust();
        QVERIFY(doc.settings().number("light.exposure", 0) > 0.5);
        QVERIFY(doc.settings().contains("curves.rgb"));
        QCOMPARE(doc.history().undoLabel(), u"Auto"_s);
    }

    void cropEditingShowsTheWholePhoto()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 200);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        QSignalSpy original(&doc, &Document::originalReady);
        doc.open(path, 400);
        QVERIFY(ready.wait(10000));
        pfx::Settings s;
        s.set("geometry.crop.w", 0.5);
        s.set("geometry.crop.h", 0.5);
        doc.apply(s, u"Crop"_s);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.preview().size(), QSize(200, 100));
        QCOMPARE(doc.outputSize(), QSize(200, 100));
        // The original lines up with the edit: same crop, no other settings.
        QTRY_COMPARE_WITH_TIMEOUT(doc.original().size(), QSize(200, 100), 10000);

        doc.setCropEditing(true);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.preview().size(), QSize(400, 200));
        QCOMPARE(doc.displaySize(), QSize(400, 200));
        QCOMPARE(doc.outputSize(), QSize(200, 100));
        // Moving the crop box while cropping renders nothing.
        ready.clear();
        s.set("geometry.crop.x", 0.25);
        doc.setSettings(s);
        QTest::qWait(300);
        QCOMPARE(ready.count(), 0);
        doc.setCropEditing(false);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.preview().size(), QSize(200, 100));
    }

    void fullResolutionRendersTheWholePhotoWhenAsked()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 300);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 100);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.preview().size(), QSize(100, 75));
        QSignalSpy full(&doc, &Document::fullReady);
        QSignalSpy stale(&doc, &Document::fullStale);
        doc.setFullResolution(true);
        QVERIFY(full.wait(10000));
        QCOMPARE(full.last().first().value<QImage>().size(), QSize(400, 300));
        QCOMPARE(doc.fullImage().size(), QSize(400, 300));
        // An edit makes it stale at once and brings a new one a moment later.
        pfx::Settings s;
        s.set("light.exposure", 1.0);
        doc.setSettings(s);
        QCOMPARE(stale.count(), 1);
        QVERIFY(doc.fullImage().isNull());
        QVERIFY(full.wait(10000));
        QVERIFY(!doc.fullImage().isNull());
        doc.setFullResolution(false);
        QVERIFY(doc.fullImage().isNull());
    }

    void theFullSizeStateSaysLoadingThenUpdatingAndAPreviewIsDrawnSharpToo()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 300);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 100);
        QVERIFY(ready.wait(10000));
        QCOMPARE(doc.fullState(), Document::FullState::Off);
        QSignalSpy full(&doc, &Document::fullReady);
        doc.setFullResolution(true);
        QCOMPARE(doc.fullState(), Document::FullState::Loading); // the photo is read at full size
        QVERIFY(full.wait(10000));
        QCOMPARE(doc.fullState(), Document::FullState::Ready);
        const QImage edit = doc.fullImage();
        // A preset's preview, zoomed in, is drawn at full size too.
        pfx::Settings bright;
        bright.set("light.exposure", 1.5);
        doc.setHoverSettings(bright);
        QCOMPARE(doc.fullState(), Document::FullState::Updating);
        QVERIFY(full.wait(10000));
        QVERIFY(doc.fullImage() != edit);
        // When it ends, the edit's own sharp picture is back at once, without a render.
        const int before = static_cast<int>(full.count());
        doc.setHoverSettings(std::nullopt);
        QCOMPARE(static_cast<int>(full.count()), before + 1);
        QCOMPARE(doc.fullImage(), edit);
        QCOMPARE(doc.fullState(), Document::FullState::Ready);
        doc.setFullResolution(false);
        QCOMPARE(doc.fullState(), Document::FullState::Off);
    }

    void aHoverPreviewIsShownOnlyWithItsOwnPicture()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 400, 300);
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 300);
        QVERIFY(ready.wait(10000));
        const auto lightness = [&doc] { return qGray(doc.preview().pixel(150, 110)); };
        const int edit = lightness();
        pfx::Settings dark;
        dark.set("light.exposure", -3.0);
        pfx::Settings bright;
        bright.set("light.exposure", 3.0);
        // Moving on before the first tile's picture is made: only the last one reaches the photo.
        ready.clear();
        doc.setHoverSettings(dark);
        QCOMPARE(doc.hoverState(), Document::HoverState::Preparing);
        doc.setHoverSettings(bright);
        QTRY_COMPARE_WITH_TIMEOUT(doc.hoverState(), Document::HoverState::Shown, 10000);
        QCOMPARE(ready.count(), 1);
        QVERIFY(lightness() > edit);
        // Ending it before its picture is made: the photo goes straight back to the edit.
        doc.setHoverSettings(dark);
        doc.setHoverSettings(std::nullopt);
        QCOMPARE(doc.hoverState(), Document::HoverState::Off);
        ready.clear();
        QVERIFY(ready.wait(10000));
        QTest::qWait(200); // nothing later replaces it
        QCOMPARE(ready.count(), 1);
        QCOMPARE(lightness(), edit);
    }

    void whiteBalancePickerNeutralisesTheCast()
    {
        QTemporaryDir dir;
        pfx::Image warm(120, 80);
        warm.fill(0.45F, 0.30F, 0.18F); // a grey wall under tungsten light
        const QString path = dir.filePath(u"warm.png"_s);
        QVERIFY(pfx::save(warm, path.toStdString()).ok());
        Document doc;
        QSignalSpy ready(&doc, &Document::previewReady);
        doc.open(path, 200);
        QVERIFY(ready.wait(10000));
        pfx::Settings start;
        start.set("light.exposure", 0.3);
        doc.apply(start, u"Start"_s);
        QVERIFY(ready.wait(10000));
        doc.pickWhiteBalance({0.5, 0.5});
        QVERIFY(doc.settings().number("color.wb.temperature", 0) < -10); // cooled
        QCOMPARE(doc.settings().number("light.exposure", 0), 0.3);       // the rest stays
        QCOMPARE(doc.history().undoLabel(), u"White balance picker"_s);
        QVERIFY(ready.wait(10000));
        const QColor c = doc.preview().pixelColor(60, 40);
        QVERIFY2(std::abs(c.red() - c.blue()) <= 6,
                 qPrintable(u"%1 %2 %3"_s.arg(c.red()).arg(c.green()).arg(c.blue())));
        QVERIFY(std::abs(c.green() - c.blue()) <= 6);
        doc.autoWhiteBalance();
        QCOMPARE(doc.settings().number("light.exposure", 0), 0.3);
        QCOMPARE(doc.history().undoLabel(), u"Auto white balance"_s);
    }

    void snapshotsComeBackWithThePhoto()
    {
        QTemporaryDir dir;
        const QString path = writePhoto(dir, u"photo.png"_s, 60, 40);
        const QString edits = dir.filePath(u"edits"_s);
        {
            Document doc;
            doc.setSidecarFolder(edits);
            QSignalSpy ready(&doc, &Document::previewReady);
            doc.open(path, 100);
            QVERIFY(ready.wait(10000));
            pfx::Settings warm;
            warm.set("color.wb.temperature", 20.0);
            doc.apply(warm, u"Warm"_s);
            doc.saveSnapshot(u"Warm"_s);
            doc.apply({}, u"Clear"_s);
            doc.flush();
        }
        Document again;
        again.setSidecarFolder(edits);
        QSignalSpy opened(&again, &Document::opened);
        again.open(path, 100);
        QVERIFY(opened.wait(10000));
        QCOMPARE(again.snapshots().size(), std::size_t{1});
        again.applySnapshot(0);
        QCOMPARE(again.settings().number("color.wb.temperature", 0), 20.0);
        QCOMPARE(again.history().undoLabel(), u"Snapshot Warm"_s);
    }

    void failureSaysWhy()
    {
        Document doc;
        QSignalSpy failed(&doc, &Document::failed);
        doc.open(u"/no/such/photo.jpg"_s, 100);
        QVERIFY(failed.wait(10000));
        QVERIFY(!failed.first().at(1).toString().isEmpty());
        QVERIFY(!doc.isOpen());
    }

    void onlyTheNewestOpenIsDelivered()
    {
        QTemporaryDir dir;
        const QString first = writePhoto(dir, u"first.png"_s, 300, 200);
        const QString second = writePhoto(dir, u"second.png"_s, 120, 90);
        Document doc;
        QSignalSpy opened(&doc, &Document::opened);
        doc.open(first, 400);
        doc.open(second, 400);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.isBusy(), 10000);
        QCOMPARE(opened.count(), 1);
        QCOMPARE(doc.info().path, second);
    }
};

QTEST_MAIN(TestDocument)
#include "tst_document.moc"
