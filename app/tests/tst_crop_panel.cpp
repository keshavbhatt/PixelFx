#include "core/crop_geometry.h"
#include "core/settings/settings.h"
#include "services/document.h"
#include "ui/canvas.h"
#include "ui/corner_tool.h"
#include "ui/crop_panel.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::CropPanel;

namespace {

double pixelRatio(const pfx::Settings& s, const QSize& photo)
{
    const QSize out = pixelfx::core::outputSize(s, photo);
    return static_cast<double>(out.width()) / out.height();
}

} // namespace

class TestCropPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void ratioChipsFitTheLargestBox()
    {
        CropPanel panel;
        QSignalSpy changed(&panel, &CropPanel::settingsChanged);
        QSignalSpy ratio(&panel, &CropPanel::ratioChanged);
        panel.begin({}, QSize(600, 400));
        panel.chooseRatio(u"1:1"_s);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(panel.ratio(), 1.0);
        QCOMPARE(pixelfx::core::outputSize(panel.settings(), QSize(600, 400)), QSize(400, 400));
        panel.chooseRatio(u"Original"_s);
        QCOMPARE(panel.ratio(), 1.5);
        QVERIFY(panel.settings().isEmpty()); // the whole photo again
        panel.chooseRatio(u"Free"_s);
        QCOMPARE(panel.ratio(), 0.0);
        QVERIFY(ratio.count() >= 3);
    }

    void orientationSwaps()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.chooseRatio(u"3:2"_s);
        panel.swapOrientation();
        QCOMPARE(panel.ratio(), 2.0 / 3.0);
        QVERIFY(std::abs(pixelRatio(panel.settings(), QSize(600, 400)) - (2.0 / 3.0)) < 0.01);
        panel.chooseRatio(u"Free"_s);
        panel.swapOrientation(); // free: the box itself turns
        QVERIFY(pixelRatio(panel.settings(), QSize(600, 400)) > 1.0);
    }

    void straighteningKeepsTheRatio()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.chooseRatio(u"16:9"_s);
        panel.setAngle(8);
        QCOMPARE(panel.settings().number("geometry.angle", 0), 8.0);
        QVERIFY(std::abs(pixelRatio(panel.settings(), QSize(600, 400)) - (16.0 / 9.0)) < 0.02);
    }

    void turnsInvertTheRatio()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.chooseRatio(u"3:2"_s);
        panel.turn(true);
        QCOMPARE(panel.settings().number("geometry.rotate90", 0), 1.0);
        QCOMPARE(panel.ratio(), 2.0 / 3.0);
        QVERIFY(std::abs(pixelRatio(panel.settings(), QSize(600, 400)) - (2.0 / 3.0)) < 0.01);
    }

    void resetKeepsOtherSettings()
    {
        CropPanel panel;
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        s.set("geometry.angle", 3.0);
        panel.begin(s, QSize(600, 400));
        panel.flip(Qt::Horizontal);
        panel.reset();
        QCOMPARE(panel.settings().keys(), std::vector<std::string>{"light.exposure"});
    }

    void customRatio()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.setCustomRatio(7, 5);
        QCOMPARE(panel.ratio(), 1.4);
        QVERIFY(std::abs(pixelRatio(panel.settings(), QSize(600, 400)) - 1.4) < 0.01);
        bool customOn = false;
        for (QPushButton* b : panel.findChildren<QPushButton*>(u"chip"_s)) {
            if (b->text() == u"Custom"_s) {
                customOn = b->isChecked();
            }
        }
        QVERIFY(customOn);
    }

    void levelLineStraightens()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.levelTo(10); // a horizon tilting down to the right: turn the photo back
        QCOMPARE(panel.settings().number("geometry.angle", 0), -10.0);
        panel.levelTo(0);
        QCOMPARE(panel.settings().number("geometry.angle", 0), -10.0);
        panel.setAngle(0);
        panel.levelTo(80); // nearly upright: made vertical
        QCOMPARE(panel.settings().number("geometry.angle", 0), 10.0);
        panel.setAngle(0);
        panel.levelTo(-170); // drawn right to left
        QCOMPARE(panel.settings().number("geometry.angle", 0), -10.0);
    }

    void perspectiveSliders()
    {
        CropPanel panel;
        QSignalSpy changed(&panel, &CropPanel::settingsChanged);
        panel.begin({}, QSize(600, 400));
        panel.setPerspective(u"geometry.perspective.vertical"_s, 30);
        QCOMPARE(panel.settings().number("geometry.perspective.vertical", 0), 30.0);
        panel.setPerspective(u"geometry.perspective.vertical"_s, 0);
        QVERIFY(panel.settings().isEmpty());
        QCOMPARE(changed.count(), 2);
    }

    void fourCorners()
    {
        CropPanel panel;
        panel.begin({}, QSize(600, 400));
        panel.chooseRatio(u"1:1"_s);
        QSignalSpy requested(&panel, &CropPanel::cornersRequested);
        const auto button = [&panel](const QString& text) {
            for (QPushButton* b : panel.findChildren<QPushButton*>()) {
                if (b->text() == text && b->isVisibleTo(&panel)) {
                    return b;
                }
            }
            return static_cast<QPushButton*>(nullptr);
        };
        QPushButton* fix = button(u"Fix by four corners"_s);
        QVERIFY(fix != nullptr);
        QVERIFY(button(u"Remove"_s) == nullptr); // nothing to remove yet
        fix->click();
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toBool(), true);
        panel.setPlacingCorners(true);
        QVERIFY(fix->isChecked());
        QVERIFY(button(u"Reset corners"_s) != nullptr);
        // Placed: the crop starts again on the new picture and the sliders give way.
        const pixelfx::core::Corners placed{{{0.1, 0.1}, {0.9, 0.15}, {0.85, 0.9}, {0.15, 0.85}}};
        panel.setPlacingCorners(false);
        panel.setCorners(placed);
        QVERIFY(pixelfx::core::hasCorners(panel.settings()));
        QCOMPARE(pixelfx::core::cropOf(panel.settings()), QRectF(0, 0, 1, 1));
        QCOMPARE(panel.ratio(), 0.0);
        QVERIFY(button(u"Edit the four corners"_s) != nullptr);
        for (auto* slider : panel.findChildren<QWidget*>()) {
            if (slider->toolTip() == u"Off while four corners set the perspective"_s) {
                QVERIFY(!slider->isEnabled());
            }
        }
        // Keep, then Remove.
        panel.setKeepInside(true);
        QVERIFY(pixelfx::core::keepsInside(panel.settings()));
        button(u"Remove"_s)->click();
        QVERIFY(!pixelfx::core::hasCorners(panel.settings()));
        QVERIFY(button(u"Fix by four corners"_s) != nullptr);
    }

    void fourCornersInTheWindow()
    {
        QTemporaryDir dir;
        pfx::Image image(300, 200);
        image.fill(0.5F, 0.4F, 0.3F);
        const QString file = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(image, file.toStdString()).ok());
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QSignalSpy opened(&window.document(), &pixelfx::services::Document::opened);
        window.openFiles({file});
        QVERIFY(opened.wait(5000));
        window.enterCrop();
        window.startCorners();
        QVERIFY(window.cropPanel()->isPlacingCorners());
        QVERIFY(window.document().isCornerEditing());
        const pixelfx::core::Corners placed{{{0.1, 0.1}, {0.9, 0.15}, {0.85, 0.9}, {0.15, 0.85}}};
        window.cornerTool()->setCorners(placed);
        // Escape leaves the corners as they were; the window stays in Crop.
        QTest::keyClick(window.findChild<pixelfx::ui::Canvas*>(), Qt::Key_Escape);
        QVERIFY(!window.cropPanel()->isPlacingCorners());
        QVERIFY(window.isCropping());
        QVERIFY(!pixelfx::core::hasCorners(window.cropPanel()->settings()));
        // Enter applies them; Done keeps them as the edit.
        window.startCorners();
        window.cornerTool()->setCorners(placed);
        QTest::keyClick(window.findChild<pixelfx::ui::Canvas*>(), Qt::Key_Return);
        QVERIFY(!window.cropPanel()->isPlacingCorners());
        QVERIFY(!window.document().isCornerEditing());
        QVERIFY(window.isCropping());
        window.leaveCrop(true);
        QVERIFY(pixelfx::core::hasCorners(window.document().settings()));
        QVERIFY(window.document().outputSize() != QSize(300, 200));
    }

    void doneAndCancel()
    {
        CropPanel panel;
        QSignalSpy finished(&panel, &CropPanel::finished);
        for (QPushButton* b : panel.findChildren<QPushButton*>()) {
            if (b->text() == u"Done"_s || b->text() == u"Cancel"_s) {
                b->click();
            }
        }
        QCOMPARE(finished.count(), 2);
    }
};

QTEST_MAIN(TestCropPanel)
#include "tst_crop_panel.moc"
