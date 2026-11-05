#include "ui/canvas.h"
#include "ui/hover_preview.h"
#include "ui/look_editor.h"
#include "ui/looks_panel.h"
#include "ui/message_line.h"
#include "ui/photo_tile.h"
#include "ui/preview_viewer.h"
#include "ui/slider_row.h"
#include "ui/step_list.h"
#include "ui/style.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

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

    void favoritesAreStarredAndListed()
    {
        LooksPanel panel;
        panel.resize(272, 700);
        panel.setEntries(pixelfx::core::builtInLooks());
        QSignalSpy changed(&panel, &LooksPanel::favoritesChanged);
        QTest::mouseClick(chip(panel, u"Favorites"_s), Qt::LeftButton);
        QCOMPARE(panel.visibleTiles().size(), std::size_t{0});
        const QList<QLabel*> labels = panel.findChildren<QLabel*>();
        QVERIFY(std::ranges::any_of(
            labels, [](const QLabel* l) { return l->text().startsWith(u"No favourites yet"_s); }));
        QTest::mouseClick(chip(panel, u"All"_s), Qt::LeftButton);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        pixelfx::ui::PhotoTile* vivid = nullptr;
        for (pixelfx::ui::PhotoTile* t : panel.visibleTiles()) {
            if (t->id() == u"builtin:vivid"_s) {
                vivid = t;
            }
        }
        QVERIFY(vivid);
        vivid->setFocus();
        QTest::keyClick(vivid, Qt::Key_F); // F stars it
        QCOMPARE(changed.size(), 1);
        QCOMPARE(panel.favorites(), QStringList{u"builtin:vivid"_s});
        QTest::mouseClick(chip(panel, u"Favorites"_s), Qt::LeftButton);
        QCOMPARE(panel.visibleTiles().size(), std::size_t{1});
        QCOMPARE(panel.visibleTiles().front()->id(), u"builtin:vivid"_s);
        panel.setFavorites({}); // remembered favourites replace the list
        QCOMPARE(panel.visibleTiles().size(), std::size_t{0});
    }

    void aSingleLookOfMyOwnIsAsWideAsTheOthers()
    {
        auto entries = pixelfx::core::builtInLooks();
        pixelfx::core::LookEntry mine = entries.front();
        mine.id = u"user:mine"_s;
        mine.name = u"Mine"_s;
        mine.path = u"/looks/mine.pfxlook"_s;
        entries.insert(entries.begin(), mine);
        LooksPanel panel;
        panel.resize(272, 800);
        panel.setEntries(entries);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        int mineWidth = 0;
        int builtInWidth = 0;
        for (auto* t : panel.visibleTiles()) {
            (t->id() == u"user:mine"_s ? mineWidth : builtInWidth) = t->width();
        }
        QVERIFY(mineWidth > 0);
        QCOMPARE(mineWidth, builtInWidth);
    }

    void theLargePreviewStepsThroughPhotosAndComparesThem()
    {
        // Three photos of one colour each; before is darker than after.
        const auto render = [](int index, int longEdge) {
            pixelfx::ui::PreviewViewer::Photo photo;
            photo.fullSize = QSize(600, 400);
            photo.before = QImage(longEdge / 8, longEdge / 12, QImage::Format_RGB32);
            photo.after = photo.before.copy();
            photo.before.fill(QColor(20, 20, 20 + index));
            photo.after.fill(QColor(200, 200, 200 + index));
            return photo;
        };
        pixelfx::ui::PreviewViewer viewer(
            3, [](int i) { return u"photo %1"_s.arg(i + 1); }, u"Batch preview"_s, render, nullptr);
        QSignalSpy shown(&viewer, &pixelfx::ui::PreviewViewer::photoShown);
        viewer.show();
        QVERIFY(QTest::qWaitForWindowExposed(&viewer));
        QVERIFY(shown.wait(5000));
        QCOMPARE(viewer.current(), 0);
        QCOMPARE(viewer.canvas()->image().pixelColor(0, 0).red(), 200); // after
        QTest::keyClick(&viewer, Qt::Key_Right);
        QVERIFY(shown.wait(5000));
        QCOMPARE(viewer.current(), 1);
        QCOMPARE(viewer.canvas()->image().pixelColor(0, 0).blue(), 201);
        // Space held shows before; \ turns the split on.
        QTest::keyPress(&viewer, Qt::Key_Space);
        QCOMPARE(viewer.canvas()->image().pixelColor(0, 0).red(), 20);
        QTest::keyRelease(&viewer, Qt::Key_Space);
        QCOMPARE(viewer.canvas()->image().pixelColor(0, 0).red(), 200);
        QTest::keyClick(&viewer, Qt::Key_Backslash);
        QVERIFY(viewer.canvas()->isComparing());
        // The first photo has no previous one.
        QTest::keyClick(&viewer, Qt::Key_Left);
        QVERIFY(shown.wait(5000));
        QTest::keyClick(&viewer, Qt::Key_Left);
        QCOMPARE(viewer.current(), 0);
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

    void addStepOffersLookStepsByName()
    {
        QTemporaryDir dir;
        auto photo = std::make_shared<pfx::Image>(40, 30);
        pixelfx::ui::LookEditor editor(photo, {}, {}, dir.filePath(u"looks"_s));
        QStringList offered;
        for (const QPushButton* b : editor.findChildren<QPushButton*>()) {
            if (b->menu() != nullptr && b->text() == u"Add step"_s) {
                for (const QAction* a : b->menu()->actions()) {
                    offered << a->text();
                }
            }
        }
        QVERIFY(offered.contains(u"Chromatic aberration"_s));
        QVERIFY(offered.contains(u"Black and white"_s));
        QVERIFY(offered.contains(u"Sepia"_s));
        QVERIFY(offered.contains(u"Edges"_s));
        for (const QString& name : offered) {
            QVERIFY2(!name.contains(u'_') && name.front().isUpper(), qPrintable(name)); // a name, not an id
        }
        for (const char* never : {"geometry", "heal", "lens_profile", "Geometry"}) {
            QVERIFY(!offered.contains(QString::fromLatin1(never)));
        }
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
        // An editor sheet can be made larger than it opens, not smaller.
        QCOMPARE(editor.minimumSize(), QSize(980, 640));
        QVERIFY(editor.maximumWidth() > 980 && editor.maximumHeight() > 640);
        // Steps added while the sheet is open all show.
        QApplication::processEvents();
        const QWidget* viewport = editor.stepList()->parentWidget();
        QVERIFY2(viewport->height() >= 2 * 40, qPrintable(QString::number(viewport->height())));
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

    void restingOnALookPreviewsItAndTheAppliedLookStays()
    {
        LooksPanel panel;
        panel.setEntries(pixelfx::core::builtInLooks());
        const pixelfx::core::LookEntry golden = pixelfx::core::builtInLooks().at(5);
        panel.setCurrent(pixelfx::core::builtInLooks().front().id, 72);
        QSignalSpy preview(&panel, &LooksPanel::previewRequested);
        QSignalSpy ended(&panel, &LooksPanel::previewEnded);
        QSignalSpy chosen(&panel, &LooksPanel::lookChosen);
        pixelfx::ui::PhotoTile* tile = nullptr;
        for (pixelfx::ui::PhotoTile* t : panel.visibleTiles()) {
            if (t->id() == golden.id) {
                tile = t;
            }
        }
        QVERIFY(tile != nullptr);
        Q_EMIT tile->hovered(true);
        QTest::qWait(pixelfx::ui::HoverPreview::kWait / 2);
        QCOMPARE(preview.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(preview.count(), 1, 1000);
        QCOMPARE(preview.first().at(0).toString(), golden.id);
        QCOMPARE(preview.first().at(1).toInt(), golden.look->defaultStrength); // what a click gives
        QVERIFY(panel.isPreviewing());
        QCOMPARE(panel.current(), pixelfx::core::builtInLooks().front().id); // nothing applied yet
        // A click applies it and ends the preview.
        QTest::mouseClick(tile, Qt::LeftButton);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(chosen.count(), 1);
        QVERIFY(!panel.isPreviewing());
        // The applied look is named above its strength; none greys the slider.
        auto labels = panel.findChildren<QLabel*>(u"itemTitle"_s);
        QVERIFY(std::ranges::any_of(
            labels, [&](QLabel* l) { return l->text() == pixelfx::core::builtInLooks().front().name; }));
        panel.setCurrent({}, 0);
        QVERIFY(std::ranges::any_of(labels, [](QLabel* l) { return l->text() == u"None"_s; }));
        QVERIFY(!panel.strengthSlider()->isEnabled());
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
        QCOMPARE(released.size(), 1);
        QCOMPARE(released.first().first().toInt(), 65);
        // The value can be typed, as on every slider.
        auto* row = panel.findChild<pixelfx::ui::SliderRow*>();
        QVERIFY(row);
        row->beginTyping();
        row->valueField()->setText(u"40"_s);
        QTest::keyClick(row->valueField(), Qt::Key_Return);
        QCOMPARE(released.last().first().toInt(), 40);
    }
};

QTEST_MAIN(TestLooksPanel)
#include "tst_looks_panel.moc"
