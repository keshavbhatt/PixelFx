#include "ui/adjust_panel.h"
#include "ui/curve_editor.h"
#include "ui/style.h"

#include <QApplication>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::ui::AdjustPanel;

class TestAdjustPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void everyControlIsInTheRegistry()
    {
        AdjustPanel panel;
        for (const char* key : {"light.exposure", "light.shadows", "color.wb.temperature", "color.saturation",
                                "detail.clarity", "detail.sharpen.amount", "detail.denoise.color"}) {
            QVERIFY2(panel.slider(QString::fromLatin1(key)) != nullptr, key);
        }
    }

    void movingASliderEditsTheSettingsAndReleaseNamesTheStep()
    {
        AdjustPanel panel;
        QSignalSpy changed(&panel, &AdjustPanel::settingsChanged);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        auto* shadows = panel.slider(u"light.shadows"_s);
        auto* slider = shadows->findChild<QSlider*>();
        slider->setSliderDown(true);
        slider->setValue(28);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(finished.count(), 0); // still dragging
        slider->setSliderDown(false);  // emits sliderReleased
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().toString(), u"Shadows +28"_s);
        QCOMPARE(panel.settings().number("light.shadows", 0), 28.0);
        QVERIFY(shadows->isChanged());
    }

    void exposureUsesHundredths()
    {
        AdjustPanel panel;
        auto* exposure = panel.slider(u"light.exposure"_s);
        exposure->findChild<QSlider*>()->setValue(35);
        QCOMPARE(panel.settings().number("light.exposure", 0), 0.35);
        QCOMPARE(exposure->valueText(), u"+0.35"_s);
    }

    void defaultsAreNotStoredAndResetClearsEverything()
    {
        AdjustPanel panel;
        auto* vibrance = panel.slider(u"color.vibrance"_s)->findChild<QSlider*>();
        vibrance->setValue(20);
        vibrance->setValue(0);
        QVERIFY(!panel.settings().contains("color.vibrance"));
        pfx::Settings s;
        s.set("light.contrast", 18.0);
        s.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.6}, {1, 1}}); // not in this panel yet
        panel.setSettings(s);
        QCOMPARE(panel.slider(u"light.contrast"_s)->value(), 18.0);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        panel.resetAll();
        QVERIFY(panel.settings().isEmpty());
        QCOMPARE(finished.first().first().toString(), u"Reset all"_s);
    }

    void curvePointsAreAddedDraggedAndRemovedWithTheMouse()
    {
        AdjustPanel panel;
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* editor = panel.curveEditor();
        editor->resize(280, 180);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        const QPoint middle(140, 90);
        QTest::mousePress(editor, Qt::LeftButton, {}, middle);
        QTest::mouseMove(editor, QPoint(140, 60));
        QTest::mouseRelease(editor, Qt::LeftButton, {}, QPoint(140, 60));
        QCOMPARE(editor->curve().size(), std::size_t{3});
        QVERIFY(panel.settings().contains("curves.rgb"));
        QCOMPARE(panel.settings().curve("curves.rgb").at(1).y > 0.55, true); // dragged up
        QCOMPARE(finished.last().first().toString(), u"Curve"_s);
        QTest::mouseDClick(editor, Qt::LeftButton, {}, QPoint(140, 60));
        QCOMPARE(editor->curve().size(), std::size_t{2});
        QVERIFY(!panel.settings().contains("curves.rgb")); // back to identity: not stored
    }

    void eachChannelHasItsOwnCurve()
    {
        AdjustPanel panel;
        pfx::Settings s;
        s.set("curves.r", pfx::Curve{{0, 0}, {0.5, 0.7}, {1, 1}});
        panel.setSettings(s);
        QCOMPARE(panel.curveEditor()->curve().size(), std::size_t{2}); // RGB shown, identity
        panel.showCurve(u"curves.r"_s);
        QCOMPARE(panel.curveEditor()->curve().size(), std::size_t{3});
    }

    void setSettingsDoesNotEmit()
    {
        AdjustPanel panel;
        QSignalSpy changed(&panel, &AdjustPanel::settingsChanged);
        pfx::Settings s;
        s.set("light.exposure", -1.0);
        panel.setSettings(s);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(panel.slider(u"light.exposure"_s)->valueText(), u"-1.00"_s);
    }
};

QTEST_MAIN(TestAdjustPanel)
#include "tst_adjust_panel.moc"
