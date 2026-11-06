#include "core/watermark.h"
#include "services/exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <memory>
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

    void severalPhotosOfOneNameAreAllKeptEvenWithOverwrite()
    {
        // The photos of one export of several share their names so far: two photo.png from two
        // folders become photo-edit.png and photo-edit (2).png, never one over the other.
        QTemporaryDir dir;
        QStringList sources;
        for (const QString& folder : {u"a"_s, u"b"_s}) {
            QDir().mkpath(dir.filePath(folder));
            pfx::Image img(40, 30);
            img.fill(folder == u"a"_s ? 0.2F : 0.8F, 0.3F, 0.4F);
            sources << dir.filePath(folder + u"/photo.png"_s);
            QVERIFY(pfx::save(img, sources.last().toStdString()).ok());
        }
        ExportOptions o;
        o.folder = dir.filePath(u"out"_s);
        o.extension = u"png"_s;
        o.ifExists = ExportOptions::IfExists::Overwrite;
        Exporter exporter;
        QSignalSpy done(&exporter, &Exporter::finished);
        const auto claims = std::make_shared<pixelfx::core::NameClaims>();
        for (const QString& source : sources) {
            exporter.start(source, {}, o, claims);
            QVERIFY(done.wait(15000));
        }
        QCOMPARE(QDir(o.folder).entryList(QDir::Files).size(), 2);
        QVERIFY(done.at(0).first().toString() != done.at(1).first().toString());
    }

    void aPreviewOfTheExportIsPreparedSmall()
    {
        QTemporaryDir dir;
        pfx::Image img(600, 400);
        img.fill(0.1F, 0.1F, 0.1F);
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, source.toStdString()).ok());
        ExportOptions o; // full size, no watermark
        pfx::Settings brighter;
        brighter.set("light.exposure", 2.0);
        const auto prepared = pixelfx::services::prepareExport(
            source, [&brighter](const pfx::Image&) { return brighter; }, o, {150});
        QVERIFY(prepared.error.isEmpty());
        QCOMPARE(prepared.image.width(), 150);
        QVERIFY(prepared.image.pixel(10, 10)[0] > 0.3F); // the settings are on it
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

    void copyGivesTheExportedPixels()
    {
        QTemporaryDir dir;
        pfx::Image img(600, 400);
        img.fill(0.2F, 0.2F, 0.2F);
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, source.toStdString()).ok());
        ExportOptions o;
        o.longEdge = 300;
        o.folder = dir.filePath(u"never"_s);
        Exporter exporter;
        QSignalSpy copied(&exporter, &Exporter::copied);
        exporter.startCopy(source, {}, o);
        QVERIFY(copied.wait(15000));
        const auto image = copied.first().first().value<QImage>();
        QCOMPARE(image.size(), QSize(300, 200));
        QVERIFY(!QFileInfo::exists(o.folder)); // nothing written
        // Linear 0.2 is about 124 in sRGB.
        QVERIFY(std::abs(qGreen(image.pixel(10, 10)) - 124) <= 2);
        QSignalSpy failed(&exporter, &Exporter::failed);
        exporter.startCopy(dir.filePath(u"missing.png"_s), {}, o);
        QVERIFY(failed.wait(15000));
    }

    void theColorSpaceIsEmbedded()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath(u"photo.png"_s);
        pfx::SaveOptions adobe;
        adobe.space = pfx::OutputSpace::AdobeRgb;
        pfx::Image img(60, 40);
        img.fill(0.2F, 0.3F, 0.1F);
        QVERIFY(pfx::save(img, source.toStdString(), adobe).ok());
        const auto original = pfx::load(source.toStdString());
        QVERIFY(original.ok());
        const auto exported = [&](pfx::OutputSpace space, const QString& name) {
            ExportOptions o;
            o.folder = dir.filePath(u"out"_s);
            o.extension = u"png"_s;
            o.nameTemplate = name;
            o.colorSpace = space;
            o.sharpen = pfx::OutputSharpen::Screen;
            const auto out =
                pixelfx::services::exportFile(source, [](const pfx::Image&) { return pfx::Settings{}; }, o);
            auto loaded = pfx::load(out.target.toStdString());
            return loaded ? loaded.value().profile : std::vector<std::uint8_t>{};
        };
        QCOMPARE(exported(pfx::OutputSpace::Original, u"same"_s), original.value().profile);
        const auto p3 = exported(pfx::OutputSpace::DisplayP3, u"p3"_s);
        QVERIFY(!p3.empty());
        QVERIFY(p3 != original.value().profile);
    }

    void theSizeEstimateIsNearTheWrittenFile()
    {
        // A photo-like picture: smooth shading with fine texture, the same at every size.
        const auto paint = [](int width, int height) {
            QImage image(width, height, QImage::Format_RGBA8888);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const double u = static_cast<double>(x) / width;
                    const double v = static_cast<double>(y) / height;
                    const double texture = 0.5 + (0.5 * std::sin(u * 90.0) * std::cos(v * 70.0));
                    image.setPixelColor(x, y,
                                        QColor::fromRgbF(static_cast<float>(0.3 + 0.5 * u),
                                                         static_cast<float>(0.2 + 0.3 * texture),
                                                         static_cast<float>(0.6 - 0.4 * v)));
                }
            }
            return image;
        };
        QTemporaryDir dir;
        const QString source = dir.filePath(u"photo.png"_s);
        QVERIFY(paint(1800, 1200).save(source));
        ExportOptions o;
        o.folder = dir.filePath(u"out"_s);
        o.keepMetadata = false;
        const auto out =
            pixelfx::services::exportFile(source, [](const pfx::Image&) { return pfx::Settings{}; }, o);
        QVERIFY(out.error.isEmpty());
        const auto written = static_cast<double>(QFileInfo(out.target).size());
        const auto estimate =
            static_cast<double>(pixelfx::services::estimateExportSize(paint(600, 400), QSize(1800, 1200), o));
        QVERIFY2(estimate > written * 0.6 && estimate < written * 1.6,
                 qPrintable(u"estimate %1, written %2"_s.arg(estimate).arg(written)));
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
