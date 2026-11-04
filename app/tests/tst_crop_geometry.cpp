#include "core/crop_geometry.h"

#include <QTest>

using namespace pixelfx::core;

namespace {

bool near(const QRectF& a, const QRectF& b)
{
    constexpr double e = 1e-9;
    return std::abs(a.x() - b.x()) < e && std::abs(a.y() - b.y()) < e &&
           std::abs(a.width() - b.width()) < e && std::abs(a.height() - b.height()) < e;
}

} // namespace

class TestCropGeometry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void wholeCropStoresNothing()
    {
        pfx::Settings s;
        setCrop(s, QRectF(0, 0, 1, 1));
        QVERIFY(s.isEmpty());
        setCrop(s, QRectF(0.1, 0.2, 0.5, 0.5));
        QCOMPARE(s.keys().size(), 4U);
        QVERIFY(near(cropOf(s), QRectF(0.1, 0.2, 0.5, 0.5)));
        QVERIFY(withoutCrop(s).isEmpty());
    }

    void fourTurnsComeBack()
    {
        pfx::Settings s;
        setCrop(s, QRectF(0.1, 0.2, 0.3, 0.4));
        const pfx::Settings start = s;
        for (int i = 0; i < 4; ++i) {
            turn(s, true);
        }
        QVERIFY(near(cropOf(s), cropOf(start)));
        QVERIFY(!s.contains("geometry.rotate90"));
        turn(s, true);
        turn(s, false);
        QVERIFY(near(cropOf(s), cropOf(start)));
    }

    void turnMovesTheCropWithThePhoto()
    {
        pfx::Settings s;
        setCrop(s, QRectF(0.0, 0.0, 0.5, 0.25)); // the top-left of the photo
        turn(s, true);
        QCOMPARE(s.number("geometry.rotate90", 0), 1.0);
        QVERIFY(near(cropOf(s), QRectF(0.75, 0.0, 0.25, 0.5))); // now top-right
    }

    void turnWithOneFlipGoesTheOtherWayUnderneath()
    {
        pfx::Settings s;
        flip(s, Qt::Horizontal);
        turn(s, true);
        QCOMPARE(s.number("geometry.rotate90", 0), 3.0);
        flip(s, Qt::Vertical); // both flips: a half turn, turns add up normally
        turn(s, true);
        QVERIFY(!s.contains("geometry.rotate90"));
    }

    void flipMirrorsCropAndAngle()
    {
        pfx::Settings s;
        setAngle(s, 5);
        setCrop(s, QRectF(0.1, 0.2, 0.3, 0.4));
        flip(s, Qt::Horizontal);
        QVERIFY(s.boolean("geometry.flipH", false));
        QCOMPARE(s.number("geometry.angle", 0), -5.0);
        QVERIFY(near(cropOf(s), QRectF(0.6, 0.2, 0.3, 0.4)));
        flip(s, Qt::Horizontal);
        QVERIFY(!s.contains("geometry.flipH"));
        QVERIFY(near(cropOf(s), QRectF(0.1, 0.2, 0.3, 0.4)));
        flip(s, Qt::Vertical);
        QVERIFY(near(cropOf(s), QRectF(0.1, 0.4, 0.3, 0.4)));
    }

    void sizesFollowTheLibrary()
    {
        pfx::Settings s;
        QCOMPARE(outputSize(s, QSize(600, 400)), QSize(600, 400));
        turn(s, true);
        QCOMPARE(outputSize(s, QSize(600, 400)), QSize(400, 600));
        setCrop(s, QRectF(0, 0, 0.5, 0.5));
        QCOMPARE(outputSize(s, QSize(600, 400)), QSize(200, 300));
        QCOMPARE(uncroppedSize(s, QSize(600, 400)), QSize(400, 600));
    }

    void fitRatioIsLargestAndInside()
    {
        const QSize size(600, 400);
        const QRectF square = fitRatio(QRectF(0, 0, 1, 1), size, 1.0);
        QVERIFY(near(square, QRectF(100.0 / 600, 0, 400.0 / 600, 1)));
        // Centred on a crop at the left edge: pushed inside.
        const QRectF left = fitRatio(QRectF(0, 0, 0.1, 1), size, 1.0);
        QCOMPARE(left.x(), 0.0);
        const QRectF wide = fitRatio(QRectF(0, 0, 1, 1), size, 16.0 / 9);
        QCOMPARE(wide.width(), 1.0);
        QCOMPARE(wide.height() * 400, 600 * 9.0 / 16);
        QVERIFY(near(fitRatio(QRectF(0.1, 0.1, 0.2, 0.2), size, 0), QRectF(0.1, 0.1, 0.2, 0.2)));
    }

    void conformKeepsWidth()
    {
        const QRectF c = conformRatio(QRectF(0.25, 0.25, 0.5, 0.5), QSize(600, 400), 1.0);
        QCOMPARE(c.width() * 600, 300.0);
        QCOMPARE(c.height() * 400, 300.0);
        QCOMPARE(c.center(), QPointF(0.5, 0.5));
    }

    void swapTurnsTheBox()
    {
        const QSize size(600, 400);
        const QRectF wide(0.25, 0.25, 0.5, 0.5); // 300 x 200 px
        const QRectF tall = swapCrop(wide, size);
        QCOMPARE(tall.width() * 600, 200.0);
        QCOMPARE(tall.height() * 400, 300.0);
        QCOMPARE(tall.center(), QPointF(0.5, 0.5));
        const QRectF whole = swapCrop(QRectF(0, 0, 1, 1), size); // 600 x 400 becomes 267 x 400
        QCOMPARE(whole.height(), 1.0);
        QVERIFY(std::abs((whole.width() * 600) - (400.0 * 400 / 600)) < 1e-9);
    }

    void dragFree()
    {
        const QRectF bounds(0, 0, 600, 400);
        const QRectF box(100, 100, 200, 100);
        QCOMPARE(dragCrop(box, CropHandle::Right, {50, 30}, bounds, 0, 20), QRectF(100, 100, 250, 100));
        QCOMPARE(dragCrop(box, CropHandle::TopLeft, {-500, -500}, bounds, 0, 20), QRectF(0, 0, 300, 200));
        QCOMPARE(dragCrop(box, CropHandle::Left, {500, 0}, bounds, 0, 20), QRectF(280, 100, 20, 100));
        QCOMPARE(dragCrop(box, CropHandle::Move, {1000, -1000}, bounds, 0, 20), QRectF(400, 0, 200, 100));
    }

    void dragKeepsRatio()
    {
        const QRectF bounds(0, 0, 600, 400);
        const QRectF box(100, 100, 200, 100);
        const QRectF corner = dragCrop(box, CropHandle::BottomRight, {100, 10}, bounds, 2.0, 20);
        QCOMPARE(corner, QRectF(100, 100, 300, 150));
        const QRectF capped = dragCrop(box, CropHandle::BottomRight, {1000, 1000}, bounds, 2.0, 20);
        QCOMPARE(capped, QRectF(100, 100, 500, 250));
        const QRectF side = dragCrop(box, CropHandle::Right, {100, 0}, bounds, 2.0, 20);
        QCOMPARE(side, QRectF(100, 75, 300, 150));
        const QRectF top = dragCrop(box, CropHandle::Top, {0, -50}, bounds, 2.0, 20);
        QCOMPARE(top, QRectF(50, 50, 300, 150));
    }
};

QTEST_GUILESS_MAIN(TestCropGeometry)
#include "tst_crop_geometry.moc"
