#include "ui/adjust_group.h"
#include "ui/adjust_panel.h"
#include "ui/color_target_tool.h"
#include "ui/color_wheel.h"
#include "ui/curve_editor.h"
#include "ui/region_curve.h"
#include "ui/setting_rows.h"
#include "ui/style.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>
#include <QToolButton>
#include <QWheelEvent>

#include <cmath>

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
#ifdef Q_OS_MACOS
        // Crashes inside the test run on the macOS CI runner only (SIGSEGV after a synthetic double
        // click); clean under ASan and UBSan on Linux and green on Windows. Open in PROGRESS.
        QSKIP("Synthetic mouse events crash on the macOS CI runner; see DOCS/PROGRESS.md");
#endif
        AdjustPanel panel;
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* editor = panel.curveEditor();
        // Points from the editor's laid-out size: the layout, not this test, decides it, and the
        // style's metrics differ between platforms.
        QVERIFY(editor->height() > 60);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        const QPoint middle = editor->rect().center();
        const QPoint up = middle - QPoint(0, editor->height() / 5);
        // No QTest::mouseMove: on macOS it moves the real cursor. A release ends the drag where it
        // happens, so press and release are enough.
        QTest::mousePress(editor, Qt::LeftButton, {}, middle);
        QTest::mouseRelease(editor, Qt::LeftButton, {}, up);
        QCOMPARE(editor->curve().size(), std::size_t{3});
        QVERIFY(panel.settings().contains("curves.rgb"));
        const pfx::CurvePoint dragged = panel.settings().curve("curves.rgb").at(1);
        QVERIFY2(dragged.y > dragged.x, "dragged up, above the diagonal"); // how far depends on the layout
        QCOMPARE(finished.last().first().toString(), u"Curve"_s);
        QTest::mouseDClick(editor, Qt::LeftButton, {}, editor->pointPosition(1).toPoint());
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

    void colorMixerShowsOneBandAtATime()
    {
        AdjustPanel panel;
        QCOMPARE(panel.mixerSlider()->key(), u"mixer.hue.red"_s);
        panel.showMixer(0, 2);
        pixelfx::ui::AdjustSlider* yellows = panel.mixerSlider();
        QCOMPARE(yellows->key(), u"mixer.hue.yellow"_s);
        QCOMPARE(yellows->label(), u"Yellows hue"_s);
        panel.showMixer(1, 3);
        QCOMPARE(panel.mixerSlider()->key(), u"mixer.saturation.green"_s);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        panel.mixerSlider()->findChild<QSlider*>()->setValue(-30); // a keyboard step: complete at once
        QCOMPARE(panel.settings().number("mixer.saturation.green", 0), -30.0);
        QCOMPARE(finished.last().first().toString(), u"Greens saturation -30"_s);
        // A setting from elsewhere shows on its slider even when another band is on show.
        pfx::Settings s;
        s.set("mixer.luminance.blue", 40.0);
        panel.setSettings(s);
        QCOMPARE(panel.slider(u"mixer.luminance.blue"_s)->value(), 40.0);
    }

    void gradingWheelPicksHueByAngleAndAmountByDistance()
    {
        AdjustPanel panel;
        panel.resize(316, 900);
        auto* wheel = panel.gradingWheel(); // Shadows is the range on show at first
        QVERIFY(wheel != nullptr);
        QCOMPARE(panel.gradingRange(), 0);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        const QPointF centre = QRectF(wheel->rect()).center();
        const double radius = (wheel->width() - 2) / 2.0;
        // Half way out, straight down: cyan (180 degrees), amount 50.
        const QPoint down = (centre + QPointF(0, radius / 2.0)).toPoint();
        QTest::mousePress(wheel, Qt::LeftButton, {}, down);
        QTest::mouseRelease(wheel, Qt::LeftButton, {}, down);
        QCOMPARE(panel.settings().number("grading.shadows.hue", -1), 180.0);
        QVERIFY(std::abs(panel.settings().number("grading.shadows.amount", -1) - 50.0) <= 1.0);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.first().first().toString().startsWith(u"Shadows colour 180°"_s));
        // Right of the centre is yellow-green (90 degrees); past the rim stays at 100.
        const auto [hue, amount] = wheel->colorAt(centre + QPointF(radius * 2.0, 0));
        QCOMPARE(hue, 90.0);
        QCOMPARE(amount, 100.0);
        // The puck is drawn where the value says.
        wheel->setColor(90.0, 100.0);
        QVERIFY(std::abs(wheel->puckPosition().x() - (centre.x() + radius)) < 0.5);
    }

    void gradingWheelKeysNudgeAndResetAndHeldKeysAreOneStep()
    {
        AdjustPanel panel;
        panel.showGradingRange(2); // Highlights
        auto* wheel = panel.gradingWheel();
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        QTest::keyClick(wheel, Qt::Key_Up, Qt::ShiftModifier); // amount 10
        QTest::keyClick(wheel, Qt::Key_Left);                  // hue 359
        QCOMPARE(panel.settings().number("grading.highlights.amount", 0), 10.0);
        QCOMPARE(panel.settings().number("grading.highlights.hue", 0), 359.0);
        QCOMPARE(finished.count(), 2); // one per press and release
        QTest::keyPress(wheel, Qt::Key_Right);
        QTest::keyEvent(QTest::Press, wheel, Qt::Key_Right, Qt::NoModifier, 1); // a repeat is just more
        QCOMPARE(finished.count(), 2);
        QTest::keyRelease(wheel, Qt::Key_Right);
        QCOMPARE(finished.count(), 3);
        QCOMPARE(panel.settings().number("grading.highlights.hue", 0), 1.0); // wrapped past 360
        QVERIFY(wheel->accessibleDescription().contains(u"amount 10"_s));
        QTest::keyClick(wheel, Qt::Key_Backspace);
        QVERIFY(!panel.settings().contains("grading.highlights.amount"));
        QVERIFY(!panel.settings().contains("grading.highlights.hue"));
    }

    void gradingGroupShowsAndResetsSettings()
    {
        AdjustPanel panel;
        pfx::Settings s;
        s.set("grading.shadows.hue", 200.0);
        s.set("grading.shadows.amount", 20.0);
        s.set("grading.balance", 10.0);
        QSignalSpy changed(&panel, &AdjustPanel::settingsChanged);
        panel.setSettings(s);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(panel.gradingWheel()->hue(), 200.0);
        QCOMPARE(panel.gradingWheel()->amount(), 20.0);
        QCOMPARE(panel.slider(u"grading.balance"_s)->value(), 10.0);
        QToolButton* reset = nullptr;
        for (auto* b : panel.findChildren<QToolButton*>(u"flatIcon"_s)) {
            if (b->accessibleName() == u"Reset Color grading"_s) {
                reset = b;
            }
        }
        QVERIFY(reset != nullptr);
        reset->click();
        QVERIFY(!panel.settings().contains("grading.shadows.hue"));
        QVERIFY(!panel.settings().contains("grading.shadows.amount"));
        QVERIFY(!panel.settings().contains("grading.balance"));
        QCOMPARE(panel.gradingWheel()->amount(), 0.0);
    }

    void geometryTurnsFlipsStraightensAndResets()
    {
        AdjustPanel panel;
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        QSignalSpy crop(&panel, &AdjustPanel::cropRequested);
        QToolButton* rotateRight = nullptr;
        QToolButton* flip = nullptr;
        for (auto* b : panel.findChildren<QToolButton*>(u"iconButton"_s)) {
            if (b->accessibleName() == u"Rotate right"_s) {
                rotateRight = b;
            } else if (b->accessibleName() == u"Flip horizontal"_s) {
                flip = b;
            }
        }
        QVERIFY(rotateRight != nullptr && flip != nullptr);
        rotateRight->click();
        flip->click();
        QCOMPARE(panel.settings().number("geometry.rotate90", 0), 1.0);
        QVERIFY(panel.settings().boolean("geometry.flipH", false) ||
                panel.settings().boolean("geometry.flipV", false));
        QCOMPARE(finished.count(), 2);
        panel.slider(u"geometry.angle"_s)->findChild<QSlider*>()->setValue(30); // tenths at most
        QVERIFY(panel.settings().contains("geometry.angle"));
        QToolButton* reset = nullptr;
        for (auto* b : panel.findChildren<QToolButton*>(u"flatIcon"_s)) {
            if (b->accessibleName() == u"Reset Geometry"_s) {
                reset = b;
            }
        }
        QVERIFY(reset != nullptr && !reset->isHidden());
        reset->click();
        for (const char* key : {"geometry.rotate90", "geometry.flipH", "geometry.flipV", "geometry.angle"}) {
            QVERIFY2(!panel.settings().contains(key), key);
        }
        QVERIFY(reset->isHidden());
        for (auto* b : panel.findChildren<QPushButton*>(u"link"_s)) {
            if (b->text() == u"Crop and perspective"_s) {
                b->click();
            }
        }
        QCOMPARE(crop.count(), 1);
    }

    void everyGroupResetsTheKeysItOwns()
    {
        AdjustPanel panel;
        pfx::Settings s;
        s.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.6}, {1, 1}});
        s.set("light.contrast", 20.0);
        panel.setSettings(s);
        QToolButton* resetCurves = nullptr;
        for (auto* b : panel.findChildren<QToolButton*>(u"flatIcon"_s)) {
            if (b->accessibleName() == u"Reset Curves"_s) {
                resetCurves = b;
            }
        }
        QVERIFY(resetCurves != nullptr && !resetCurves->isHidden());
        resetCurves->click();
        QVERIFY(!panel.settings().contains("curves.rgb"));
        QCOMPARE(panel.settings().number("light.contrast", 0), 20.0); // other groups untouched
        QVERIFY(panel.curveEditor()->isIdentity());
    }

    void slidersTakeTheWheelOnlyWithFocusAndResetFromTheirName()
    {
        AdjustPanel panel;
        panel.resize(316, 800);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* contrast = panel.slider(u"light.contrast"_s);
        auto* track = contrast->slider();
        const QPoint middle = track->rect().center();
        QWheelEvent wheel(middle, track->mapToGlobal(middle), {}, {0, 120}, Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(track, &wheel);
        QCOMPARE(contrast->value(), 0.0); // the wheel was passing over the panel
        track->setFocus();
        QApplication::sendEvent(track, &wheel);
        QVERIFY(contrast->value() != 0.0);
        // A click on the track goes there, not a page at a time.
        QTest::mouseClick(track, Qt::LeftButton, {}, QPoint(track->width() - 3, middle.y()));
        QVERIFY(contrast->value() > 90.0);
        // A double-click on the name resets.
        auto* name = contrast->findChild<QLabel*>(u"adjustName"_s);
        QTest::mouseDClick(name, Qt::LeftButton);
        QCOMPARE(contrast->value(), 0.0);
        // Exposure steps a twentieth of a stop.
        QCOMPARE(panel.slider(u"light.exposure"_s)->slider()->singleStep(), 5);
        // The value keeps its tabular figures under the style sheet.
        auto* value = contrast->findChild<QLabel*>(u"adjustValue"_s);
        QVERIFY(value->font().featureTags().contains(QFont::Tag("tnum")));
    }

    void aValueCanBeTypedInTheSlidersUnit()
    {
        AdjustPanel panel;
        panel.resize(316, 800);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* exposure = panel.slider(u"light.exposure"_s);
        QSignalSpy released(exposure, &pixelfx::ui::AdjustSlider::released);
        auto* value = exposure->findChild<QLabel*>(u"adjustValue"_s);
        QLineEdit* field = exposure->valueField();
        QTest::mouseClick(value, Qt::LeftButton);
        QVERIFY(field->isVisible());
        QVERIFY(field->hasSelectedText());
        field->setText(u"+0.35"_s);
        QTest::keyClick(field, Qt::Key_Return);
        QVERIFY(!field->isVisible());
        QCOMPARE(exposure->value(), 0.35);
        QCOMPARE(released.count(), 1); // one history step
        // Escape goes back.
        exposure->beginTyping();
        field->setText(u"2"_s);
        QTest::keyClick(field, Qt::Key_Escape);
        QCOMPARE(exposure->value(), 0.35);
        QVERIFY(!field->isVisible());
        // Up nudges by the slider's step, Shift by ten.
        exposure->beginTyping();
        QTest::keyClick(field, Qt::Key_Up);
        QCOMPARE(exposure->value(), 0.40);
        QTest::keyClick(field, Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(exposure->value(), -0.10);
        QTest::keyClick(field, Qt::Key_Escape);
        // Past the end goes to the end; words leave the value; a unit is allowed.
        QCOMPARE(exposure->parse(u"9"_s), std::optional<int>(500));
        QCOMPARE(exposure->parse(u"bright"_s), std::nullopt);
        auto* straighten = panel.slider(u"geometry.angle"_s);
        QCOMPARE(straighten->parse(u"-12\u00B0"_s), std::optional<int>(-12));
        // Enter on a focused slider opens the field.
        auto* contrast = panel.slider(u"light.contrast"_s);
        contrast->slider()->setFocus();
        QTest::keyClick(contrast->slider(), Qt::Key_Return);
        QVERIFY(contrast->valueField()->isVisible());
    }

    void openGroupsAreReadAndSet()
    {
        AdjustPanel panel;
        panel.setOpenGroups({u"Detail"_s, u"Curves"_s});
        QStringList open = panel.openGroups();
        open.sort();
        QCOMPARE(open, QStringList({u"Curves"_s, u"Detail"_s}));
        panel.setOpenGroups({});
        QVERIFY(panel.openGroups().isEmpty());
    }

    void focusShowsOnlyWhenTheKeyboardGaveIt()
    {
        AdjustPanel panel;
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.activateWindow();
        QSlider* slider = panel.slider(u"light.contrast"_s)->slider();
        slider->setFocus(Qt::MouseFocusReason);
        QTRY_VERIFY(slider->hasFocus());
        QVERIFY(!pixelfx::ui::showsFocusRing(slider)); // clicked: no ring
        slider->clearFocus();
        slider->setFocus(Qt::TabFocusReason);
        QTRY_VERIFY(slider->hasFocus());
        QVERIFY(pixelfx::ui::showsFocusRing(slider)); // tabbed to: a ring
    }

    void aSubmenuOpensBesideItsParent()
    {
        QMenu parent;
        parent.addAction(u"Open"_s);
        QMenu* sub = parent.addMenu(u"View"_s);
        sub->addAction(u"Fit"_s);
        for (const QPoint at :
             {QPoint(40, 40), QPoint(QGuiApplication::primaryScreen()->geometry().right() - 60, 40)}) {
            parent.popup(at);
            QVERIFY(QTest::qWaitForWindowExposed(&parent));
            parent.setActiveAction(sub->menuAction());
            QTRY_VERIFY(sub->isVisible());
            const QRect p = parent.geometry();
            const QRect s = sub->geometry();
            QVERIFY2(s.left() > p.right() || s.right() < p.left(),
                     qPrintable(u"the submenu %1,%2 %3x%4 covers its parent %5,%6 %7x%8"_s.arg(s.x())
                                    .arg(s.y())
                                    .arg(s.width())
                                    .arg(s.height())
                                    .arg(p.x())
                                    .arg(p.y())
                                    .arg(p.width())
                                    .arg(p.height())));
            sub->hide();
            parent.hide();
        }
    }

    void aListOpensAsTallAsItsChoices()
    {
        AdjustPanel panel;
        panel.resize(316, 800);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* box = panel.findChild<QComboBox*>(u"compact"_s);
        QVERIFY(box);
        box->showPopup();
        QWidget* popup = box->view()->window();
        QTRY_VERIFY(popup->isVisible());
        QVERIFY2(popup->height() < 40 * box->count(), qPrintable(QString::number(popup->height())));
        QVERIFY2(popup->geometry().top() >= box->mapToGlobal(QPoint(0, 0)).y(),
                 "the list opens below its box");
        box->hidePopup();
    }

    void blackAndWhiteTurnsTheMixerIntoTheBwMixAndGreysVibrance()
    {
        AdjustPanel panel;
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        QPushButton* bw = nullptr;
        for (auto* b : panel.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->accessibleName() == u"Black & white"_s) {
                bw = b;
            }
        }
        QVERIFY(bw != nullptr);
        QCOMPARE(bw->text(), u"Black && white"_s); // the & is shown, not a shortcut
        bw->click();
        QVERIFY(panel.isBlackAndWhite());
        QVERIFY(panel.settings().boolean("bw.enabled", false));
        QCOMPARE(finished.last().first().toString(), u"Black and white"_s);
        QCOMPARE(panel.group(u"Color mixer"_s)->name(), u"B&W mix"_s);
        QVERIFY(!panel.slider(u"color.vibrance"_s)->isEnabled());
        panel.showMixer(0, 5); // blues
        QCOMPARE(panel.mixerSlider()->key(), u"bw.mix.blue"_s);
        QCOMPARE(panel.mixerSlider()->label(), u"Blues in black and white"_s);
        panel.setBlackAndWhite(false);
        QVERIFY(!panel.settings().contains("bw.enabled"));
        QCOMPARE(panel.mixerSlider()->key(), u"mixer.hue.blue"_s);
        QVERIFY(panel.slider(u"color.vibrance"_s)->isEnabled());
    }

    void choicesAndChecksAreSettingsAndDefaultsAreNotStored()
    {
        AdjustPanel panel;
        pixelfx::ui::ChoiceRow* profile = panel.choiceRow(u"color.profile"_s);
        QVERIFY(profile != nullptr);
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        profile->list()->setCurrentIndex(1);
        Q_EMIT profile->list()->activated(1);
        QCOMPARE(panel.settings().text("color.profile", ""), std::string("vivid"));
        QCOMPARE(finished.last().first().toString(), u"Profile Vivid"_s);
        profile->list()->setCurrentIndex(0);
        Q_EMIT profile->list()->activated(0);
        QVERIFY(!panel.settings().contains("color.profile"));
        pixelfx::ui::CheckRow* ca = panel.checkRow(u"detail.ca.auto"_s);
        QVERIFY(ca != nullptr);
        ca->box()->click();
        QVERIFY(panel.settings().boolean("detail.ca.auto", false));
        QCOMPARE(finished.last().first().toString(), u"Remove chromatic aberration on"_s);
        pfx::Settings s;
        s.set("effects.vignette.style", std::string("paint"));
        panel.setSettings(s);
        QCOMPARE(panel.choiceRow(u"effects.vignette.style"_s)->value(), std::string("paint"));
        QVERIFY(!panel.checkRow(u"detail.ca.auto"_s)->isChecked());
    }

    void moreOpensTheFinerSettingsAndIsRemembered()
    {
        AdjustPanel panel;
        panel.resize(316, 900);
        panel.show();
        pixelfx::ui::AdjustGroup* detail = panel.group(u"Detail"_s);
        QVERIFY(detail->hasMore());
        QVERIFY(!panel.group(u"Light"_s)->hasMore());
        QVERIFY(panel.slider(u"detail.sharpen.detail"_s)->isHidden() ||
                !panel.slider(u"detail.sharpen.detail"_s)->isVisible());
        panel.setOpenMore({u"Detail"_s});
        QVERIFY(detail->isMoreOpen());
        QCOMPARE(panel.openMore(), QStringList{u"Detail"_s});
        QVERIFY(panel.slider(u"detail.sharpen.detail"_s)->isVisible());
        // A radius reads as the pixels of a 6000 px photo, as in Lightroom.
        QCOMPARE(panel.slider(u"detail.sharpen.radius"_s)->valueText(), u"2.4"_s);
        QCOMPARE(panel.slider(u"detail.sharpen.detail"_s)->label(), u"Sharpening detail"_s);
        QCOMPARE(pixelfx::ui::settingLabel(u"detail.sharpen.detail"_s), u"Sharpening detail"_s);
        // Optics and Calibration start closed.
        QVERIFY(!panel.group(u"Optics"_s)->isExpanded());
        QVERIFY(!panel.group(u"Calibration"_s)->isExpanded());
    }

    void gradingRangesEditTheirOwnKeys()
    {
        AdjustPanel panel;
        panel.showGradingRange(1); // midtones
        QCOMPARE(panel.gradingWheel()->accessibleName(), u"Midtones colour"_s);
        QTest::keyClick(panel.gradingWheel(), Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(panel.settings().number("grading.midtones.amount", 0), 10.0);
        QVERIFY(!panel.settings().contains("grading.shadows.amount"));
        QVERIFY(!panel.slider(u"grading.midtones.luminance"_s)->isHidden());
        QVERIFY(panel.slider(u"grading.shadows.luminance"_s)->isHidden());
        pfx::Settings s;
        s.set("grading.global.hue", 210.0);
        s.set("grading.global.amount", 30.0);
        panel.setSettings(s);
        panel.showGradingRange(3);
        QCOMPARE(panel.gradingWheel()->hue(), 210.0);
        QCOMPARE(panel.slider(u"grading.blending"_s)->value(), 50.0);
    }

    void curvesShowRegionsForARegionCurveAndSplitsMove()
    {
        AdjustPanel panel;
        QVERIFY(!panel.showsCurveRegions());
        pfx::Settings s;
        s.set("curves.parametric.shadows", 20.0);
        panel.setSettings(s);
        QVERIFY(panel.showsCurveRegions()); // a preset with only regions opens on them
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        pixelfx::ui::RegionCurveView* view = panel.regionCurve();
        Q_EMIT view->splitMoved(0, 30.0, true);
        QCOMPARE(panel.settings().number("curves.parametric.split.shadows", 0), 30.0);
        QCOMPARE(finished.last().first().toString(), u"Region split 30"_s);
        Q_EMIT view->splitMoved(0, 25.0, true);
        QVERIFY(!panel.settings().contains("curves.parametric.split.shadows")); // the default
        pfx::Settings points;
        points.set("curves.rgb", pfx::Curve{{0, 0}, {0.5, 0.6}, {1, 1}});
        panel.setSettings(points);
        QVERIFY(!panel.showsCurveRegions());
    }

    void regionMarksMoveWithKeysAndStayApart()
    {
        pixelfx::ui::RegionCurveView view;
        view.resize(280, view.sizeHint().height());
        QSignalSpy moved(&view, &pixelfx::ui::RegionCurveView::splitMoved);
        view.setFocus(Qt::TabFocusReason);
        QTest::keyClick(&view, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(view.split(0), 35.0);
        QCOMPARE(moved.last().at(2).toBool(), true); // released: a history step
        for (int i = 0; i < 5; ++i) {
            QTest::keyClick(&view, Qt::Key_Right, Qt::ShiftModifier);
        }
        QCOMPARE(view.split(0), 45.0); // 5 short of the next split
    }

    void aColourPickedOnThePhotoMovesItsBandsByTheirShares()
    {
        AdjustPanel panel;
        QSignalSpy finished(&panel, &AdjustPanel::editFinished);
        panel.setTargeting(true);
        QVERIFY(panel.isTargeting());
        QVERIFY(panel.group(u"Color mixer"_s)->isExpanded());
        const pfx::HueBands sky{4, 5, 0.7, 1.0}; // between aqua and blue, nearer blue
        QCOMPARE(panel.targetName(sky), u"Blues and aquas"_s);
        QCOMPARE(panel.targetName(pfx::HueBands{4, 5, 0.95, 1.0}), u"Blues"_s);
        QCOMPARE(panel.targetName(pfx::HueBands{4, 5, 0.5, 0.0}), u"No colour here"_s);
        panel.pickTarget(sky);
        QCOMPARE(panel.mixerSlider()->key(), u"mixer.hue.blue"_s); // the main band's swatch
        panel.dragTarget(24, true);
        QCOMPARE(panel.settings().number("mixer.hue.blue", 0), 24.0);
        QCOMPARE(panel.settings().number("mixer.hue.aqua", 0), 10.0); // 24 times 0.3 / 0.7
        QCOMPARE(finished.last().first().toString(), u"Blues and aquas hue +24"_s);
        panel.dragTarget(-6, true); // the next drag goes on from there
        QCOMPARE(panel.settings().number("mixer.hue.blue", 0), 18.0);
        // A grey picks nothing, and a drag then changes nothing.
        const pfx::Settings before = panel.settings();
        panel.pickTarget(pfx::HueBands{0, 1, 0.5, 0.0});
        panel.dragTarget(40, true);
        QCOMPARE(panel.settings(), before);
        // In black and white the drag moves the B&W mix.
        panel.setBlackAndWhite(true);
        panel.pickTarget(sky);
        panel.dragTarget(-40, true);
        QCOMPARE(panel.settings().number("bw.mix.blue", 0), -40.0);
        QCOMPARE(panel.settings().number("bw.mix.aqua", 0), -17.0);
        QCOMPARE(finished.last().first().toString(), u"Blues and aquas in black and white -40"_s);
        panel.setTargeting(false);
        QVERIFY(!panel.isTargeting());
    }

    void theTargetToolCountsStepsFromThePress()
    {
        pixelfx::ui::ColorTargetTool tool;
        QSignalSpy dragged(&tool, &pixelfx::ui::ColorTargetTool::dragged);
        QSignalSpy pressed(&tool, &pixelfx::ui::ColorTargetTool::pressed);
        const QRectF photo(0, 0, 400, 300);
        const auto mouse = [](QEvent::Type type, QPointF at, Qt::KeyboardModifiers mods = {}) {
            return QMouseEvent(type, at, at, Qt::LeftButton, Qt::LeftButton, mods);
        };
        QVERIFY(!tool.press(mouse(QEvent::MouseButtonPress, {500, 100}), photo)); // off the photo
        QVERIFY(tool.press(mouse(QEvent::MouseButtonPress, {100, 150}), photo));
        QCOMPARE(pressed.first().first().toPointF(), QPointF(0.25, 0.5));
        tool.move(mouse(QEvent::MouseMove, {300, 130}), photo); // only the height counts
        QCOMPARE(dragged.last().at(0).toDouble(), 10.0);
        tool.move(mouse(QEvent::MouseMove, {100, 190}, Qt::ShiftModifier), photo);
        QCOMPARE(dragged.last().at(0).toDouble(), -5.0); // 40 px down, finer
        tool.release(mouse(QEvent::MouseButtonRelease, {100, 190}), photo);
        QCOMPARE(dragged.last().at(1).toBool(), true);
        QCOMPARE(*tool.marker(), QPointF(0.25, 0.5)); // the marker stays on the colour picked
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
