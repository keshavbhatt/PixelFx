#include "services/document.h"

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
