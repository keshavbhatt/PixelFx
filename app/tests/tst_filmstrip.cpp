#include "ui/filmstrip.h"
#include "ui/style.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::ui::Filmstrip;

class TestFilmstrip : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void clicksSwitchAndModifiersSelect()
    {
        Filmstrip strip;
        strip.setFiles({u"/a.jpg"_s, u"/b.jpg"_s, u"/c.jpg"_s, u"/d.jpg"_s});
        strip.setCurrent(0);
        QCOMPARE(strip.tiles().size(), 4);
        QVERIFY(strip.selection().isEmpty()); // one photo on its own is not a selection
        QSignalSpy current(&strip, &Filmstrip::currentRequested);
        strip.show();
        QVERIFY(QTest::qWaitForWindowExposed(&strip));
        QTest::mouseClick(strip.tiles().at(2), Qt::LeftButton, {}, QPoint(10, 10));
        QCOMPARE(current.count(), 1);
        QCOMPARE(current.first().first().toInt(), 2);

        QSignalSpy selected(&strip, &Filmstrip::selectionChanged);
        QTest::mouseClick(strip.tiles().at(2), Qt::LeftButton, Qt::ControlModifier, QPoint(10, 10));
        QCOMPARE(strip.selection(), (QList<int>{0, 2})); // the current photo is part of it
        strip.toggle(3, true);                           // Shift: a range from the last one
        QCOMPARE(strip.selection(), (QList<int>{0, 2, 3}));
        strip.clearSelection();
        QVERIFY(strip.selection().isEmpty());
        strip.selectAll();
        QCOMPARE(strip.selection().size(), 4);
        QVERIFY(selected.count() >= 3);
    }
};

QTEST_MAIN(TestFilmstrip)
#include "tst_filmstrip.moc"
