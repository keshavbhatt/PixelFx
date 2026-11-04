#include "core/settings/settings.h"
#include "ui/adjust_panel.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/main_window.h"
#include "ui/straighten_dial.h"
#include "ui/style.h"

#include <QApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <numbers>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    } // edit files stay out of the real data folder

    void windowBuildsShowsAndOpensAPhoto()
    {
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        pfx::Image img(320, 200);
        img.fill(0.3F, 0.2F, 0.1F);
        const QString path = dir.filePath(u"photo.jpg"_s);
        QVERIFY(pfx::save(img, path.toStdString()).ok());
        QSignalSpy ready(&window.document(), &pixelfx::services::Document::previewReady);
        window.openFiles({dir.filePath(u"notes.txt"_s), path});
        QVERIFY(ready.wait(10000));
        QCOMPARE(window.canvas()->image().size(), QSize(320, 200));
        QVERIFY(window.canvas()->photoRect().isValid());
        QCOMPARE(window.windowTitle(), u"photo.jpg"_s);
        QCOMPARE(settings.recentFiles().first(), path);
        // Before and after: the backslash toggles the split, Space shows the original while held.
        QTest::keyClick(&window, Qt::Key_Backslash);
        QVERIFY(window.canvas()->isComparing());
        QTest::keyClick(&window, Qt::Key_Backslash);
        QVERIFY(!window.canvas()->isComparing());

        // Copy and paste: the settings travel, the crop stays with its photo.
        pfx::Settings look;
        look.set("light.exposure", 0.7);
        look.set("geometry.angle", 4.0);
        window.document().apply(look, u"Look"_s);
        window.copySettings();
        pfx::Settings other;
        other.set("color.vibrance", 30.0);
        window.document().apply(other, u"Other"_s);
        window.pasteSettings();
        QCOMPARE(window.document().settings().number("light.exposure", 0), 0.7);
        QVERIFY(!window.document().settings().contains("color.vibrance"));
        QVERIFY(!window.document().settings().contains("geometry.angle"));
        QCOMPARE(window.document().history().undoLabel(), u"Paste settings"_s);
        window.document().apply({}, u"Clear"_s);

        // Zoom: in from Fit, back with Ctrl+0; Crop mode always fits.
        QVERIFY(window.canvas()->isFit());
        QCOMPARE(window.canvas()->zoomText(), u"Fit"_s);
        window.canvas()->zoomIn();
        QVERIFY(!window.canvas()->isFit());
        QVERIFY(window.canvas()->scale() > 1.0);
        QVERIFY(window.canvas()->zoomText().endsWith(u'%'));
        QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(window.canvas()->isFit());
        window.canvas()->setZoom(3.0);
        QVERIFY(window.canvas()->visibleFraction().width() < 1.0); // 960 px of photo, smaller window
        QTest::keyClick(&window, Qt::Key_C);
        QVERIFY(window.canvas()->isFit());
        QTest::keyClick(&window, Qt::Key_Escape);

        // Crop mode: C enters, Escape drops the change, Enter keeps it as one step.
        const int steps = static_cast<int>(window.document().history().labels().size());
        QTest::keyClick(&window, Qt::Key_C);
        QVERIFY(window.isCropping());
        QVERIFY(window.canvas()->isCropping());
        window.cropPanel()->chooseRatio(u"1:1"_s);
        QCOMPARE(window.document().outputSize(), QSize(200, 200));
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!window.isCropping());
        QCOMPARE(window.document().outputSize(), QSize(320, 200));
        QTest::keyClick(&window, Qt::Key_C);
        // Draw a level line: a press and a release 10 degrees down to the right turn it back.
        window.canvas()->startLevelLine();
        const QPoint from = window.canvas()->rect().center() - QPoint(100, 0);
        const QPoint to =
            from + QPoint(200, static_cast<int>(std::lround(200 * std::tan(10 * std::numbers::pi / 180))));
        QTest::mousePress(window.canvas(), Qt::LeftButton, {}, from);
        QTest::mouseRelease(window.canvas(), Qt::LeftButton, {}, to);
        QVERIFY(std::abs(window.cropPanel()->settings().number("geometry.angle", 0) + 10.0) < 0.3);
        QVERIFY(std::abs(window.canvas()->dial()->angle() + 10.0) < 0.3);
        window.cropPanel()->setAngle(0);
        window.cropPanel()->chooseRatio(u"1:1"_s);
        QTest::keyClick(&window, Qt::Key_Return);
        QVERIFY(!window.isCropping());
        QCOMPARE(window.document().outputSize(), QSize(200, 200));
        QCOMPARE(window.document().history().labels().size(), steps + 1);
        QVERIFY(window.adjustPanel()->settings().contains("geometry.crop.w"));
        QTRY_COMPARE_WITH_TIMEOUT(window.canvas()->image().size(), QSize(200, 200), 10000);
        window.close();
    }

    void supportedExtensions()
    {
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"/a/B.JPG"_s));
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"x.webp"_s));
        QVERIFY(!pixelfx::ui::MainWindow::isSupported(u"x.txt"_s));
    }
};

QTEST_MAIN(TestSmoke)
#include "tst_smoke.moc"
