#include "core/watermark.h"
#include "services/exporter.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::core::ExportOptions;
using pixelfx::services::Exporter;

class TestExporter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void writesTheEditedPhotoResized()
    {
        QTemporaryDir dir;
        pfx::Image img(600, 400);
        img.fill(0.1F, 0.1F, 0.1F);
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, source.toStdString()).ok());

        ExportOptions o;
        o.folder = dir.filePath(u"out"_s); // created on demand
        o.longEdge = 300;
        o.extension = u"png"_s;
        pfx::Settings brighter;
        brighter.set("light.exposure", 2.0);
        Exporter exporter;
        QSignalSpy done(&exporter, &Exporter::finished);
        exporter.start(source, brighter, o);
        QVERIFY(done.wait(15000));
        const QString target = done.first().first().toString();
        QCOMPARE(target, dir.filePath(u"out/photo-edit.png"_s));
        const auto written = pfx::load(target.toStdString());
        QVERIFY(written.ok());
        QCOMPARE(written.value().image.width(), 300);
        QVERIFY(written.value().image.pixel(10, 10)[0] > 0.3F); // 0.1 * 4
    }

    void sizePresetsGiveExactFrames()
    {
        QTemporaryDir dir;
        pfx::Image img(600, 400); // 3:2 landscape
        img.fill(0.3F, 0.3F, 0.3F);
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, source.toStdString()).ok());
        const auto run = [&](const QString& preset, bool fit) {
            ExportOptions o;
            o.folder = dir.filePath(u"out"_s);
            o.extension = u"png"_s;
            o.sizePreset = preset;
            o.fitInside = fit;
            o.ifExists = ExportOptions::IfExists::Overwrite;
            Exporter exporter;
            QSignalSpy done(&exporter, &Exporter::finished);
            exporter.start(source, {}, o);
            if (!done.wait(15000)) {
                return QSize();
            }
            const auto info = pfx::probe(done.first().first().toString().toStdString());
            return info ? QSize(info.value().width, info.value().height) : QSize();
        };
        QCOMPARE(run(u"instagram-square"_s, false), QSize(1080, 1080));
        QCOMPARE(run(u"instagram-portrait"_s, true), QSize(1080, 1350));
        QCOMPARE(run(u"print-10x15"_s, false), QSize(1800, 1200)); // follows the landscape photo
        QCOMPARE(run(u"email"_s, false), QSize(600, 400));         // a long edge never enlarges
    }

    void watermarkGoesOnTheExport()
    {
        QTemporaryDir dir;
        pfx::Image img(800, 400); // dark: Auto picks a light mark
        img.fill(0.02F, 0.02F, 0.02F);
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, source.toStdString()).ok());
        pixelfx::core::Watermark mark;
        mark.name = u"Studio"_s;
        mark.text = u"PIXELFX {year}"_s;
        mark.size = 0.1;
        mark.opacity = 1.0;
        mark.shadow = false;
        const pixelfx::core::WatermarkLibrary library(dir.filePath(u"marks"_s));
        ExportOptions o;
        o.folder = dir.filePath(u"out"_s);
        o.extension = u"png"_s;
        o.longEdge = 400; // the mark is sized on the final pixels
        o.watermark = library.save(mark);
        QVERIFY(!o.watermark.isEmpty());
        Exporter exporter;
        QSignalSpy done(&exporter, &Exporter::finished);
        exporter.start(source, {}, o);
        QVERIFY(done.wait(15000));
        const auto written = pfx::load(done.first().first().toString().toStdString());
        QVERIFY(written.ok());
        const pfx::Image& out = written.value().image;
        QCOMPARE(out.width(), 400);
        // Light pixels in the bottom right corner, the top left untouched.
        float brightest = 0.0F;
        for (int y = 140; y < 200; ++y) {
            for (int x = 200; x < 400; ++x) {
                brightest = std::max(brightest, out.pixel(x, y)[1]);
            }
        }
        QVERIFY2(brightest > 0.5F, qPrintable(QString::number(brightest)));
        QVERIFY(out.pixel(20, 20)[1] < 0.05F);

        // A watermark that went missing fails the export rather than leaving the mark off.
        QVERIFY(QFile::remove(o.watermark));
        QSignalSpy failed(&exporter, &Exporter::failed);
        exporter.start(source, {}, o);
        QVERIFY(failed.wait(15000));
    }

    void failureSaysWhy()
    {
        Exporter exporter;
        QSignalSpy failed(&exporter, &Exporter::failed);
        exporter.start(u"/no/photo.jpg"_s, {}, ExportOptions{});
        QVERIFY(failed.wait(10000));
        QVERIFY(!failed.first().at(1).toString().isEmpty());
    }
};

QTEST_MAIN(TestExporter) // a watermark draws text, which needs a GUI application
#include "tst_exporter.moc"
