#include "ui/looks_panel.h"
#include "ui/style.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
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
