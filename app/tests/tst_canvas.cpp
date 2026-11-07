#include "ui/canvas.h"
#include "ui/corner_tool.h"
#include "ui/style.h"

#include <QApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::ui::Canvas;

class TestCanvas : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void theBusyChipShowsOnlyAfterTheDelayAndBesideTheZoomBar()
    {
        Canvas canvas;
        canvas.resize(800, 600);
        QImage photo(400, 300, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        canvas.setImage(photo, photo.size());
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        QSignalSpy busy(&canvas, &Canvas::busyChanged);
        // A quick piece of work shows nothing.
        canvas.setBusy(tr("Loading"));
        QTest::qWait(Canvas::kBusyDelay / 3);
        canvas.setBusy(QString());
        QTest::qWait(Canvas::kBusyDelay);
        QCOMPARE(busy.count(), 0);
        QVERIFY(!canvas.showsBusy());
        // A longer one shows the chip, beside the bar, which does not move.
        auto* bar = canvas.findChild<QWidget*>(u"zoomBar"_s);
        QVERIFY(bar != nullptr);
        const QRect before = bar->geometry();
        canvas.setBusy(u"Updating"_s);
        QVERIFY(busy.wait(Canvas::kBusyDelay * 3));
        QVERIFY(canvas.showsBusy());
        auto* chip = canvas.findChild<QWidget*>(u"zoomChip"_s);
        QCOMPARE(bar->geometry(), before);
        QCOMPARE(chip->y(), bar->y());
        QVERIFY(chip->x() > bar->geometry().right());
        canvas.setBusy(QString());
        QVERIFY(!canvas.showsBusy());
        QCOMPARE(busy.last().first().toBool(), false);
    }

    void cornersAreDraggedAndMovedByKeys()
    {
        Canvas canvas;
        canvas.resize(800, 600);
        QImage photo(300, 200, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        canvas.setImage(photo, photo.size());
        canvas.setCropMode(true);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        pixelfx::ui::CornerTool tool(&canvas);
        QSignalSpy changed(&tool, &pixelfx::ui::CornerTool::cornersChanged);
        canvas.setTool(&tool);
        const QRectF photoRect(canvas.photoRect());
        const auto at = [&photoRect](double x, double y) {
            return QPoint(qRound(photoRect.left() + (x * photoRect.width())),
                          qRound(photoRect.top() + (y * photoRect.height())));
        };
        // Drag the top left corner in: the shape follows, one change when it is let go.
        QTest::mousePress(&canvas, Qt::LeftButton, {}, at(0, 0));
        QTest::mouseMove(&canvas, at(0.2, 0.1));
        QTest::mouseRelease(&canvas, Qt::LeftButton, {}, at(0.2, 0.1));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(tool.corners()[0].x, 0.2);
        // A drag that would fold the shape leaves the corner where it was.
        QTest::mousePress(&canvas, Qt::LeftButton, {}, at(0.2, 0.1));
        QTest::mouseMove(&canvas, at(0.95, 0.95));
        QTest::mouseRelease(&canvas, Qt::LeftButton, {}, at(0.95, 0.95));
        QVERIFY(pfx::cornersUsable(tool.corners()));
        QCOMPARE(tool.corners()[0].x, 0.2);
        // Tab picks the next corner, an arrow moves it a screen pixel (Shift: ten).
        canvas.setFocus();
        QTest::keyClick(&canvas, Qt::Key_Tab);
        QCOMPARE(tool.selected(), 1);
        const double before = tool.corners()[1].x;
        QTest::keyClick(&canvas, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(tool.corners()[1].x, before - (10.0 / photoRect.width()));
        // Keys the tool does not take still reach the window (Enter applies).
        QKeyEvent enter(QEvent::ShortcutOverride, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &enter);
        QVERIFY(!enter.isAccepted());
        tool.reset();
        QVERIFY(tool.corners() == pixelfx::core::ownCorners());
        canvas.setTool(nullptr);
    }
};

QTEST_MAIN(TestCanvas)
#include "tst_canvas.moc"
