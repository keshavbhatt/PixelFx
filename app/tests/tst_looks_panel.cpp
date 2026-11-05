#include "ui/look_editor.h"
#include "ui/looks_panel.h"
#include "ui/message_line.h"
#include "ui/step_list.h"
#include "ui/style.h"

#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::ui::LooksPanel;

class TestLooksPanel : public QObject
{
    Q_OBJECT

    static QPushButton* chip(LooksPanel& panel, const QString& text)
    {
        for (auto* b : panel.findChildren<QPushButton*>(u"chip"_s)) {
            if (b->text() == text) {
                return b;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void tilesFilterAndChoose()
    {
        LooksPanel panel;
        panel.resize(272, 700);
        panel.setEntries(pixelfx::core::builtInLooks());
        QCOMPARE(panel.visibleTiles().size(), std::size_t{14}); // Original and the 13 looks
        QVERIFY(chip(panel, u"B&&W"_s));
        QTest::mouseClick(chip(panel, u"B&&W"_s), Qt::LeftButton);
        QCOMPARE(panel.visibleTiles().size(), std::size_t{3}); // Mono, Noir, Sepia
        QTest::mouseClick(chip(panel, u"All"_s), Qt::LeftButton);
        auto* search = panel.findChild<QLineEdit*>();
        search->setText(u"gold"_s);
        QCOMPARE(panel.visibleTiles().size(), std::size_t{1});
        QSignalSpy chosen(&panel, &LooksPanel::lookChosen);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto* tile = panel.visibleTiles().front();
        QTest::mouseClick(tile, Qt::LeftButton);
        QCOMPARE(chosen.size(), 1);
        QCOMPARE(chosen.first().at(0).toString(), u"builtin:golden-hour"_s);
        QCOMPARE(chosen.first().at(1).toInt(), 72);
        // Keyboard: Enter on a focused tile chooses it too.
        tile->setFocus();
        QTest::keyClick(tile, Qt::Key_Return);
        QCOMPARE(chosen.size(), 2);
    }

    void lookEditorSaysWhatWentWrongInItsFooter()
    {
        QTemporaryDir dir;
        auto photo = std::make_shared<pfx::Image>(40, 30);
        pixelfx::ui::LookEditor editor(photo, {}, {}, dir.filePath(u"looks"_s));
        editor.addStep(u"grain"_s);
        auto* message = editor.findChild<pixelfx::ui::MessageLine*>();
        QVERIFY(message);
        auto* name = editor.findChild<QLineEdit*>();
        name->clear();
        QVERIFY(!editor.saveLook());
        QVERIFY(message->isError());
        QCOMPARE(message->text(), u"Name the look first."_s);
        QTest::keyClicks(name, u"Dusk"_s); // typing a name puts it right
        QVERIFY(!message->isError());
    }

    void lookEditorMakesALook()
    {
        QTemporaryDir dir;
        auto photo = std::make_shared<pfx::Image>(120, 80);
        photo->fill(0.2F, 0.2F, 0.2F);
        pixelfx::ui::LookEditor editor(photo, {}, {}, dir.filePath(u"looks"_s));
        QSignalSpy rendered(&editor, &pixelfx::ui::LookEditor::previewRendered);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QVERIFY(rendered.wait(5000)); // the photo with an empty look
        editor.addStep(u"exposure"_s);
        editor.addStep(u"grain"_s);
        QCOMPARE(editor.stepList()->count(), 2);
        // The exposure step's slider: the selected step is grain; pick exposure and move its first slider.
        editor.stepList()->setCurrent(0);
        QSlider* exposure = nullptr;
        for (auto* slider : editor.findChildren<QSlider*>()) {
            if (slider->accessibleName() == u"Exposure"_s) {
                exposure = slider;
            }
        }
        QVERIFY(exposure);
        exposure->setValue(100); // hundredths of a stop: +1 EV
        QCOMPARE(editor.look().steps.front().params.number("light.exposure", 0), 1.0);
        rendered.clear();
        QVERIFY(rendered.wait(5000));
        const QColor shown = editor.previewImage().pixelColor(60, 40);
        QVERIFY2(shown.red() > 130,
                 qPrintable(QString::number(shown.red()))); // brighter than 0.2 linear (124)
        // Moving a step moves it in the look.
        editor.stepList()->moveRow(1, 0);
        QCOMPARE(editor.look().steps.front().op, std::string("grain"));
        editor.stepList()->moveRow(0, 1);
        // Switching a step off leaves it in the look, not run.
        editor.stepList()->setOn(1, false);
        QVERIFY(!editor.look().steps.back().enabled);
        QVERIFY(editor.saveLook());
        const auto entries = pixelfx::core::LookLibrary(dir.filePath(u"looks"_s)).entries();
        QCOMPARE(entries.front().name, u"My look"_s);
        QVERIFY(entries.front().isMine());
        QCOMPARE(entries.front().id, editor.savedId());
        QCOMPARE(entries.front().look->steps.size(), std::size_t{2});
    }

    void currentLookSelectsItsTileAndTheSlider()
    {
        LooksPanel panel;
        panel.setEntries(pixelfx::core::builtInLooks());
        QVERIFY(!panel.strengthSlider()->isEnabled()); // no look: nothing to weigh
        panel.setCurrent(u"builtin:noir"_s, 64);
        QVERIFY(panel.strengthSlider()->isEnabled());
        QCOMPARE(panel.strengthSlider()->value(), 64);
        int selected = 0;
        for (auto* t : panel.visibleTiles()) {
            if (t->isSelected()) {
                ++selected;
                QCOMPARE(t->id(), u"builtin:noir"_s);
            }
        }
        QCOMPARE(selected, 1);
        // The slider's keys weigh the look one step at a time, each a history step.
        QSignalSpy released(&panel, &LooksPanel::strengthReleased);
        panel.strengthSlider()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        QVERIFY(released.wait(1000));
        QCOMPARE(released.first().first().toInt(), 65);
    }
};

QTEST_MAIN(TestLooksPanel)
#include "tst_looks_panel.moc"
