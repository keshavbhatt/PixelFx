#include "core/reveal_dust.h"
#include "core/settings/settings.h"
#include "core/settings_clipboard.h"
#include "services/document.h"
#include "ui/canvas.h"
#include "ui/heal_panel.h"
#include "ui/heal_tool.h"
#include "ui/list_row.h"
#include "ui/main_window.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QApplication>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::HealTool;

namespace {

// A canvas showing a 400 x 200 picture with the heal tool on it.
struct Rig
{
    pixelfx::ui::Canvas canvas;
    HealTool tool;
    Rig()
    {
        QImage image(400, 200, QImage::Format_RGB32);
        image.fill(Qt::gray);
        canvas.resize(800, 500);
        canvas.setImage(image, image.size());
        canvas.setTool(&tool);
        canvas.show();
    }
    [[nodiscard]] QPoint at(double fx, double fy) const
    {
        const QRect r = canvas.photoRect();
        return {r.left() + qRound(fx * r.width()), r.top() + qRound(fy * r.height())};
    }
};

} // namespace

class TestHeal : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
    }

    void aClickHealsASpotAndPicksItsPatch()
    {
        Rig rig;
        rig.tool.setSourceFinder([](pfx::Spot s) {
            s.sourceX = s.x + 0.1;
            return s;
        });
        QSignalSpy changed(&rig.tool, &HealTool::spotsChanged);
        QTest::mouseClick(&rig.canvas, Qt::LeftButton, {}, rig.at(0.25, 0.5));
        QCOMPARE(rig.tool.spots().size(), std::size_t{1});
        const pfx::Spot& s = rig.tool.spots().front();
        QVERIFY(std::abs(s.x - 0.25) < 0.01);
        QVERIFY(std::abs(s.y - 0.5) < 0.01);
        QVERIFY(std::abs(s.sourceX - 0.35) < 0.01);
        QCOMPARE(s.radius, 0.02); // the brush's 4% across
        QVERIFY(s.stroke.empty());
        QCOMPARE(rig.tool.selected(), 0);
        QCOMPARE(changed.count(), 1);
        QVERIFY(changed.first().at(1).toBool());
    }

    void aDragInBrushModeHealsAStroke()
    {
        Rig rig;
        rig.tool.setMode(HealTool::Mode::Brush);
        QTest::mousePress(&rig.canvas, Qt::LeftButton, {}, rig.at(0.2, 0.5));
        for (int i = 1; i <= 10; ++i) {
            QTest::mouseMove(&rig.canvas, rig.at(0.2 + (0.05 * i), 0.5));
        }
        QTest::mouseRelease(&rig.canvas, Qt::LeftButton, {}, rig.at(0.7, 0.5));
        QCOMPARE(rig.tool.spots().size(), std::size_t{1});
        QVERIFY(rig.tool.spots().front().stroke.size() >= 5);
        // A click in Brush mode is still a spot.
        QTest::mouseClick(&rig.canvas, Qt::LeftButton, {}, rig.at(0.8, 0.2));
        QVERIFY(rig.tool.spots().back().stroke.empty());
    }

    void theSelectedSpotMovesItsPatchSizesAndGoes()
    {
        Rig rig;
        rig.tool.setSourceFinder([](pfx::Spot s) {
            s.sourceX = s.x + 0.2;
            return s;
        });
        QTest::mouseClick(&rig.canvas, Qt::LeftButton, {}, rig.at(0.25, 0.5));
        // Drag the hollow source circle somewhere else.
        QTest::mousePress(&rig.canvas, Qt::LeftButton, {}, rig.at(0.45, 0.5));
        QTest::mouseMove(&rig.canvas, rig.at(0.45, 0.2));
        QTest::mouseRelease(&rig.canvas, Qt::LeftButton, {}, rig.at(0.45, 0.2));
        const pfx::Spot& s = rig.tool.spots().front();
        QVERIFY(std::abs(s.sourceY - 0.2) < 0.01);
        QVERIFY(std::abs(s.y - 0.5) < 0.01); // the spot stayed
        // ] makes the selected spot larger, not the brush.
        rig.tool.resizeStep(true);
        QVERIFY(rig.tool.spots().front().radius > 0.02);
        rig.tool.select(-1);
        QCOMPARE(rig.tool.size(), 0.04);
        rig.tool.select(0);
        rig.tool.nudge({0.1, 0});
        QVERIFY(std::abs(rig.tool.spots().front().x - 0.35) < 0.01);
        rig.tool.removeSelected();
        QVERIFY(rig.tool.spots().empty());
        QCOMPARE(rig.tool.selected(), -1);
    }

    void thePanelListsAndSelects()
    {
        HealTool tool;
        pixelfx::ui::HealPanel panel(&tool);
        tool.addSpotAt({0.3, 0.3});
        tool.addSpotAt({0.6, 0.6});
        QCOMPARE(panel.spotRows().size(), std::size_t{2});
        QCOMPARE(panel.spotRows().front()->title(), u"Spot 1"_s);
        Q_EMIT panel.spotRows().front()->clicked();
        QCOMPARE(tool.selected(), 0);
        QVERIFY(panel.spotRows().front()->isSelected());
        panel.sizeSlider()->slider()->setValue(100); // 10%
        QCOMPARE(tool.spots().front().radius, 0.05);
        Q_EMIT panel.spotRows().back()->actionClicked(); // the row's ×
        QCOMPARE(tool.spots().size(), std::size_t{1});
        tool.removeAll();
        QCOMPARE(panel.spotRows().size(), std::size_t{0});
    }

    void revealDustShowsSpecksOnGrey()
    {
        QImage sky(200, 100, QImage::Format_RGB32);
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 200; ++x) {
                sky.setPixel(x, y, qRgb(80 + (x / 4), 120 + (x / 4), 200)); // a smooth gradient
            }
        }
        sky.setPixel(100, 50, qRgb(60, 90, 160)); // a speck of dust
        const QImage view = pixelfx::core::revealDust(sky);
        QCOMPARE(view.size(), sky.size());
        QVERIFY(std::abs(qGray(view.pixel(20, 20)) - 128) < 10); // the gradient goes grey
        QVERIFY(qGray(view.pixel(100, 50)) < 40);                // the speck stands out
    }

    void spotsStayWithTheirPhoto()
    {
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        s.set("heal.spots", pfx::Spots{pfx::Spot{}});
        const auto copied = pixelfx::core::settingsFromClipboard(pixelfx::core::settingsToClipboard(s));
        QVERIFY(copied);
        QVERIFY(!copied->contains("heal.spots"));
        pfx::Settings mine;
        mine.set("heal.spots", pfx::Spots{pfx::Spot{}, pfx::Spot{}});
        const pfx::Settings after = pixelfx::core::pasted(mine, *copied);
        QCOMPARE(after.spots("heal.spots").size(), std::size_t{2}); // pasting keeps my spots
    }

    void healModeShowsTheUprightPhotoAndEachSpotIsAStep()
    {
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        pfx::Image img(320, 200);
        img.fill(0.3F, 0.2F, 0.1F);
        const QString path = dir.filePath(u"photo.png"_s);
        QVERIFY(pfx::save(img, path.toStdString()).ok());
        QSignalSpy opened(&window.document(), &pixelfx::services::Document::opened);
        window.openFiles({path});
        QVERIFY(opened.wait(10000));
        // A crop, then Heal: the whole upright photo is shown, the crop kept.
        pfx::Settings cropped = window.document().settings();
        cropped.set("geometry.crop.w", 0.5);
        window.document().apply(cropped, u"Crop"_s);
        window.enterHeal();
        QVERIFY(window.isHealing());
        QCOMPARE(window.document().displaySize(), QSize(320, 200));
        window.healTool()->addSpotAt({0.25, 0.5});
        window.healTool()->addSpotAt({0.75, 0.5});
        QCOMPARE(window.document().settings().spots("heal.spots").size(), std::size_t{2});
        QCOMPARE(window.document().settings().number("geometry.crop.w", 1), 0.5);
        // Undo takes the last spot off, in Heal too.
        window.document().undo();
        QCOMPARE(window.healTool()->spots().size(), std::size_t{1});
        window.leaveHeal();
        QVERIFY(!window.isHealing());
        QCOMPARE(window.document().displaySize(), QSize(160, 200));
        QCOMPARE(window.document().settings().spots("heal.spots").size(), std::size_t{1});
    }
};

QTEST_MAIN(TestHeal)
#include "tst_heal.moc"
