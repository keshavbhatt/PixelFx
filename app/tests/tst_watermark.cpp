#include "core/watermark.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::core::Watermark;

class TestWatermark : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tokensFillFromExif()
    {
        pfx::ExifSummary exif;
        exif.make = "Canon";
        exif.model = "Canon EOS R5";
        exif.dateTaken = "2024:06:01 10:00:00";
        const auto t = pixelfx::core::tokensFrom(exif, QDate(2026, 1, 1));
        QCOMPARE(t.camera, u"Canon EOS R5"_s); // the make is not said twice
        QCOMPARE(pixelfx::core::expandTokens(u"© {year} {camera}, {date}"_s, t),
                 u"© 2024 Canon EOS R5, 2024-06-01"_s);
        exif.make = "NIKON CORPORATION";
        exif.model = "Z 6";
        exif.dateTaken.clear();
        const auto u = pixelfx::core::tokensFrom(exif, QDate(2026, 1, 1));
        QCOMPARE(u.camera, u"NIKON CORPORATION Z 6"_s);
        QCOMPARE(u.date, QDate(2026, 1, 1)); // no date in the photo: the fallback
    }

    void placementIsRelativeToTheLongEdge()
    {
        Watermark w;
        w.margin = 0.02;
        w.anchor = 8;
        const auto landscape = pixelfx::core::placements(w, QSizeF(1000, 500), QSizeF(100, 20));
        QCOMPARE(landscape.size(), std::size_t{1});
        QCOMPARE(landscape.front(), QRectF(1000 - 20 - 100, 500 - 20 - 20, 100, 20));
        w.anchor = 0;
        QCOMPARE(pixelfx::core::placements(w, QSizeF(500, 1000), QSizeF(100, 20)).front().topLeft(),
                 QPointF(20, 20));
        w.anchor = 4;
        QCOMPARE(pixelfx::core::placements(w, QSizeF(500, 1000), QSizeF(100, 20)).front().center(),
                 QPointF(250, 500));
        // Placed freely: centred there, but kept inside the photo.
        w.position = QPointF(1.0, 0.5);
        QCOMPARE(pixelfx::core::placements(w, QSizeF(500, 1000), QSizeF(100, 20)).front().right(), 500.0);
        // Repeating: a grid that covers the photo even when turned.
        w.repeat = true;
        const auto grid = pixelfx::core::placements(w, QSizeF(500, 1000), QSizeF(100, 20));
        QVERIFY(grid.size() > 20);
    }

    void autoColorReadsTheArea()
    {
        QCOMPARE(pixelfx::core::autoColor(0.02), QColor(Qt::white));
        QCOMPARE(pixelfx::core::autoColor(0.8), QColor(0x11, 0x11, 0x11));
        QImage light(10, 10, QImage::Format_RGB32);
        light.fill(QColor(240, 240, 240));
        QVERIFY(pixelfx::core::meanLuminance(light, light.rect()) > 0.8);
        QCOMPARE(pixelfx::core::meanLuminance(light, QRect(20, 20, 5, 5)), 0.0);
    }

    void overlayCoversOnlyTheMark()
    {
        Watermark w;
        w.text = u"Hello"_s;
        w.size = 0.05;
        w.shadow = false; // the shadow would add to the alpha
        const auto overlay = pixelfx::core::renderWatermark(w, QSize(2000, 1000), {QDate(2026, 1, 1), {}},
                                                            [](const QRect&) { return 0.0; });
        QVERIFY(!overlay.image.isNull());
        QVERIFY(overlay.image.width() < 1000);
        QVERIFY(overlay.origin.x() > 1000 && overlay.origin.y() > 700); // bottom right
        // Something was drawn, in white (dark photo), at about 70% opacity.
        int strongest = 0;
        for (int y = 0; y < overlay.image.height(); ++y) {
            for (int x = 0; x < overlay.image.width(); ++x) {
                strongest = std::max(strongest, qAlpha(overlay.image.pixel(x, y)));
            }
        }
        QVERIFY2(strongest > 150 && strongest < 200, qPrintable(QString::number(strongest)));
        // Empty text after the tokens: nothing at all.
        w.text = u"{camera}"_s;
        QVERIFY(pixelfx::core::renderWatermark(w, QSize(2000, 1000), {}, {}).image.isNull());
    }

    void fileRoundTripAndLibrary()
    {
        QTemporaryDir dir;
        Watermark w;
        w.name = u"Studio: signature"_s;
        w.kind = Watermark::Kind::Logo;
        w.colorMode = Watermark::ColorMode::Custom;
        w.color = QColor(0x12, 0x34, 0x56);
        w.size = 0.08;
        w.opacity = 0.5;
        w.margin = 0.03;
        w.anchor = 2;
        w.position = QPointF(0.25, 0.75);
        w.repeat = true;
        w.angle = 15;
        w.shadow = false;
        QImage logo(40, 20, QImage::Format_ARGB32);
        logo.fill(Qt::red);
        const QString logoPath = dir.filePath(u"logo.png"_s);
        QVERIFY(logo.save(logoPath));
        w.logo = logoPath;

        const pixelfx::core::WatermarkLibrary library(dir.filePath(u"marks"_s));
        const QString path = library.save(w);
        QVERIFY(!path.isEmpty());
        QVERIFY(QFile::remove(logoPath)); // the library keeps its own copy
        const auto entries = library.entries();
        QCOMPARE(entries.size(), std::size_t{1});
        Watermark back = entries.front().watermark;
        QVERIFY(QFile::exists(back.logo));
        QVERIFY(back.logo.startsWith(dir.filePath(u"marks"_s)));
        back.logo = w.logo;
        QCOMPARE(back, w);
        // The logo is sized by its height, its shape kept.
        const auto overlay =
            pixelfx::core::renderWatermark(entries.front().watermark, QSize(1000, 1000), {}, {});
        QVERIFY(!overlay.image.isNull());
        QVERIFY(library.remove(path));
        QVERIFY(library.entries().empty());
        QVERIFY(!QFile::exists(entries.front().watermark.logo));
    }

    void badFilesAreRefusedOrClamped()
    {
        QVERIFY(!pixelfx::core::watermarkFromJson("not json"));
        QVERIFY(!pixelfx::core::watermarkFromJson(R"({"format":"pixelfx","kind":"preset","version":1})"));
        QString error;
        QVERIFY(!pixelfx::core::watermarkFromJson(R"({"format":"pixelfx","kind":"watermark","version":9})",
                                                  &error));
        QVERIFY(error.contains(u"newer"_s));
        const auto w = pixelfx::core::watermarkFromJson(
            R"({"format":"pixelfx","kind":"watermark","version":1,"body":{"size":5,"opacity":-1,"anchor":42,"logo":"../../etc/x.png"}})");
        QVERIFY(w);
        QCOMPARE(w->size, 0.25);
        QCOMPARE(w->opacity, 0.0);
        QCOMPARE(w->anchor, 8);
        // Only a file name is kept for the logo: never a path out of the library.
        QTemporaryDir dir;
        QFile file(dir.filePath(u"x.pfxwatermark"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(pixelfx::core::watermarkToJson(*w).replace("x.png", "../../etc/x.png"));
        file.close();
        const auto loaded = pixelfx::core::WatermarkLibrary::load(file.fileName());
        QVERIFY(loaded);
        QCOMPARE(loaded->logo, dir.filePath(u"x.png"_s));
    }
};

QTEST_MAIN(TestWatermark)
#include "tst_watermark.moc"
