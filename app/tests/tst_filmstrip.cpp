#include "ui/filmstrip.h"
#include "ui/style.h"

#include <QApplication>
#include <QEnterEvent>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;
using pixelfx::ui::Filmstrip;
using pixelfx::ui::FilmTile;

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

    void aTilesCrossAndMenuClosePhotosAndThumbnailsStay()
    {
        Filmstrip strip;
        strip.setFiles({u"/a.jpg"_s, u"/b.jpg"_s, u"/c.jpg"_s});
        strip.setCurrent(0);
        QImage picture(92, 64, QImage::Format_RGB32);
        picture.fill(Qt::red);
        strip.setThumbnail(u"/c.jpg"_s, picture, true);
        strip.show();
        QVERIFY(QTest::qWaitForWindowExposed(&strip));
        QSignalSpy close(&strip, &Filmstrip::closeRequested);
        // The × shows under the pointer and closes its own photo.
        FilmTile* second = strip.tiles().at(1);
        QVERIFY(!second->closeButton()->isVisible());
        QEnterEvent enter(QPointF(10, 10), QPointF(10, 10), QPointF(10, 10));
        QApplication::sendEvent(second, &enter);
        QVERIFY(second->closeButton()->isVisible());
        QCOMPARE(second->closeButton()->accessibleName(), u"Close b.jpg"_s);
        QCOMPARE(second->closeButton()->toolTip(), u"Close photo"_s);
        QCOMPARE(strip.tiles().at(0)->closeButton()->toolTip(), u"Close photo (Ctrl+W)"_s);
        QVERIFY(second->closeButton()->width() >= 24);
        second->closeButton()->click();
        QCOMPARE(close.count(), 1);
        QCOMPARE(close.last().first().value<QList<int>>(), QList<int>{1});
        // The menu: this photo, or the whole selection when the tile is in it, or all.
        QMenu* menu = strip.tileMenu(2);
        QCOMPARE(menu->actions().at(0)->text(), u"Close photo"_s);
        QCOMPARE(menu->actions().at(1)->text(), u"Close all photos"_s);
        QCOMPARE(strip.tileMenu(0)->actions().at(0)->shortcut(), QKeySequence(u"Ctrl+W"_s));
        menu->actions().at(1)->trigger();
        QCOMPARE(close.last().first().value<QList<int>>(), (QList<int>{0, 1, 2}));
        strip.toggle(2, false);
        QCOMPARE(strip.tileMenu(2)->actions().at(0)->text(), u"Close 2 photos"_s);
        strip.tileMenu(2)->actions().at(0)->trigger();
        QCOMPARE(close.last().first().value<QList<int>>(), (QList<int>{0, 2}));
        // A photo closed leaves the strip; the others keep their pictures.
        strip.setFiles({u"/a.jpg"_s, u"/c.jpg"_s});
        QVERIFY(strip.tiles().at(1)->isEdited());
    }
};

QTEST_MAIN(TestFilmstrip)
#include "tst_filmstrip.moc"
