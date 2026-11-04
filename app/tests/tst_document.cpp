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
