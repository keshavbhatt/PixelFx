#include "services/render_scheduler.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <pfx/image.h>

using pixelfx::services::RenderScheduler;

class TestRenderScheduler : public QObject
{
    Q_OBJECT

    static std::shared_ptr<const pfx::Image> source()
    {
        auto image = std::make_shared<pfx::Image>(400, 300);
        image->fill(0.4F, 0.3F, 0.2F);
        return image;
    }

    static pfx::Settings exposure(double ev)
    {
        pfx::Settings s;
        s.set("light.exposure", ev);
        return s;
    }

private Q_SLOTS:
    static void aDragIsDraftedThenDrawnSharp()
    {
        RenderScheduler scheduler;
        scheduler.setSource(source());
        QSignalSpy rendered(&scheduler, &RenderScheduler::rendered);
        scheduler.request(exposure(0.1), true); // the first step: full size
        QTRY_COMPARE(rendered.count(), 1);
        QCOMPARE(rendered.at(0).at(0).value<QImage>().size(), QSize(400, 300));
        scheduler.request(exposure(0.2), true); // the next step of a drag: half size
        QTRY_COMPARE(rendered.count(), 2);
        QCOMPARE(rendered.at(1).at(0).value<QImage>().size(), QSize(200, 150));
        QVERIFY(scheduler.isBusy()); // a full-size picture is still to come
        // Once the steps stop, the newest settings come back at full size.
        QTRY_COMPARE(rendered.count(), 3);
        QCOMPARE(rendered.at(2).at(0).value<QImage>().size(), QSize(400, 300));
        QCOMPARE(rendered.at(2).at(1).value<pfx::Settings>(), exposure(0.2));
        QVERIFY(!scheduler.isBusy());
        QTest::qWait(300);
        QCOMPARE(rendered.count(), 3); // and nothing more
    }

    static void changesMadeAtOnceAreNeverDrafted()
    {
        // Undo, a preset, a crop: each a whole change, drawn at full size however quick.
        RenderScheduler scheduler;
        scheduler.setSource(source());
        QSignalSpy rendered(&scheduler, &RenderScheduler::rendered);
        scheduler.request(exposure(0.1));
        QTRY_COMPARE(rendered.count(), 1);
        scheduler.request(exposure(0.2));
        QTRY_COMPARE(rendered.count(), 2);
        QCOMPARE(rendered.at(1).at(0).value<QImage>().size(), QSize(400, 300));
    }
};

QTEST_MAIN(TestRenderScheduler)
#include "tst_render_scheduler.moc"
