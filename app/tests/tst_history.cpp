#include "core/history.h"

#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::History;

namespace {

pfx::Settings exposure(double ev)
{
    pfx::Settings s;
    s.set("light.exposure", ev);
    return s;
}

} // namespace

class TestHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void undoAndRedoWalkTheSteps()
    {
        History h;
        h.reset({}, u"Opened"_s);
        QVERIFY(!h.canUndo());
        QVERIFY(h.push(u"Exposure +0.5"_s, exposure(0.5)));
        QVERIFY(h.push(u"Exposure +1"_s, exposure(1.0)));
        QCOMPARE(h.undoLabel(), u"Exposure +1"_s);
        QCOMPARE(h.undo().number("light.exposure", 0), 0.5);
        QCOMPARE(h.redoLabel(), u"Exposure +1"_s);
        QCOMPARE(h.undo().number("light.exposure", 0), 0.0);
        QVERIFY(!h.canUndo());
        QCOMPARE(h.redo().number("light.exposure", 0), 0.5);
        QCOMPARE(h.labels(), QStringList({u"Opened"_s, u"Exposure +0.5"_s, u"Exposure +1"_s}));
        QCOMPARE(h.currentIndex(), 1);
    }

    void goToJumpsAndKeepsTheSteps()
    {
        History h;
        h.reset({}, u"Opened"_s);
        h.push(u"a"_s, exposure(1));
        h.push(u"b"_s, exposure(2));
        QCOMPARE(h.goTo(0).number("light.exposure", 0), 0.0);
        QVERIFY(h.canRedo());
        QCOMPARE(h.goTo(2).number("light.exposure", 0), 2.0);
        QCOMPARE(h.goTo(99).number("light.exposure", 0), 2.0); // out of range: unchanged
        QCOMPARE(h.labels().size(), 3);
    }

    void aNewStepDropsTheRedoSteps()
    {
        History h;
        h.reset({}, u"Opened"_s);
        h.push(u"a"_s, exposure(1));
        h.push(u"b"_s, exposure(2));
        h.undo();
        h.push(u"c"_s, exposure(3));
        QVERIFY(!h.canRedo());
        QCOMPARE(h.labels(), QStringList({u"Opened"_s, u"a"_s, u"c"_s}));
    }

    void anUnchangedStepIsIgnored()
    {
        History h;
        h.reset(exposure(1), u"Opened"_s);
        QVERIFY(!h.push(u"same"_s, exposure(1)));
        QVERIFY(!h.canUndo());
    }

    void stepsAreCapped()
    {
        History h;
        h.reset({}, u"Opened"_s);
        for (int i = 1; i <= History::kMaxSteps + 10; ++i) {
            h.push(u"step %1"_s.arg(i), exposure(i / 100.0));
        }
        QCOMPARE(h.labels().size(), History::kMaxSteps);
        QCOMPARE(h.current().number("light.exposure", 0), (History::kMaxSteps + 10) / 100.0);
    }
    void changedGroupNamesWhatAStepChanged()
    {
        History h;
        pfx::Settings s;
        h.reset(s, u"Opened"_s);
        s.set("light.shadows", 28.0);
        h.push(u"Shadows +28"_s, s);
        s.set("look.id", std::string("builtin:noir"));
        h.push(u"Look: Noir 100"_s, s);
        s.erase("light.shadows");
        h.push(u"Reset Shadows"_s, s);
        QCOMPARE(h.changedGroup(0), QString());
        QCOMPARE(h.changedGroup(1), u"light"_s);
        QCOMPARE(h.changedGroup(2), u"look"_s);
        QCOMPARE(h.changedGroup(3), u"light"_s); // a removed key counts too
        QCOMPARE(h.changedGroup(9), QString());
    }
};

QTEST_GUILESS_MAIN(TestHistory)
#include "tst_history.moc"
