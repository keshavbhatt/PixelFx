#include "services/qt_image_bridge.h"

#include <QColorSpace>
#include <QTest>

using pixelfx::services::toQImage;

class TestQtImageBridge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void copiesPixelsAndTagsSrgb()
    {
        pfx::Rgba8Image in;
        in.width = 3;
        in.height = 2;
        for (int i = 0; i < 6; ++i) {
            in.pixels.insert(in.pixels.end(), {static_cast<std::uint8_t>(i * 40), 20, 200, 255});
        }
        in.pixels[3] = 128; // first pixel half transparent
        const QImage out = toQImage(in);
        QCOMPARE(out.size(), QSize(3, 2));
        QCOMPARE(out.colorSpace(), QColorSpace(QColorSpace::SRgb));
        QCOMPARE(qRed(out.pixel(2, 1)), 200);
        QCOMPARE(qBlue(out.pixel(1, 0)), 200);
        QCOMPARE(out.pixelColor(0, 0).alpha(), 128);
    }

    void emptyGivesNull() { QVERIFY(toQImage(pfx::Rgba8Image{}).isNull()); }
};

QTEST_GUILESS_MAIN(TestQtImageBridge)
#include "tst_qt_image_bridge.moc"
