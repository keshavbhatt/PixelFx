#include "services/exporter.h"

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

    void failureSaysWhy()
    {
        Exporter exporter;
        QSignalSpy failed(&exporter, &Exporter::failed);
        exporter.start(u"/no/photo.jpg"_s, {}, ExportOptions{});
        QVERIFY(failed.wait(10000));
        QVERIFY(!failed.first().at(1).toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestExporter)
#include "tst_exporter.moc"
