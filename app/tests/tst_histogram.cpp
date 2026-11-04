#include "core/histogram.h"

#include <QImage>
#include <QTest>

using pixelfx::core::histogramOf;

class TestHistogram : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsEveryChannel()
    {
        QImage img(4, 2, QImage::Format_RGBA8888);
        img.fill(QColor(10, 20, 30));
        img.setPixelColor(0, 0, QColor(255, 0, 128));
        const auto h = histogramOf(img);
        QCOMPARE(h.pixels, std::uint64_t{8});
        QCOMPARE(h.rgb[0][10], 7U);
        QCOMPARE(h.rgb[0][255], 1U);
        QCOMPARE(h.rgb[1][0], 1U);
        QCOMPARE(h.rgb[2][128], 1U);
        QCOMPARE(h.clippedHighlights, 1.0 / 8.0);
        QCOMPARE(h.clippedShadows, 1.0 / 8.0);
        QCOMPARE(h.peak(), 7U);
    }

    void transparentPixelsAndOtherFormats()
    {
        QImage img(2, 1, QImage::Format_ARGB32);
        img.setPixel(0, 0, qRgba(100, 100, 100, 255));
        img.setPixel(1, 0, qRgba(0, 0, 0, 0));
        const auto h = histogramOf(img);
        QCOMPARE(h.pixels, std::uint64_t{1});
        QVERIFY(histogramOf(QImage()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestHistogram)
#include "tst_histogram.moc"
