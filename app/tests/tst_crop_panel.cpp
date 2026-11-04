#include "core/crop_geometry.h"
#include "ui/crop_panel.h"
#include "ui/style.h"

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

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
