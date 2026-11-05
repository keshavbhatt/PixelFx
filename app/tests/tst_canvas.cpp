#include "ui/canvas.h"
#include "ui/style.h"

#include <QApplication>
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
};

QTEST_MAIN(TestCanvas)
#include "tst_canvas.moc"
