#include "core/settings/settings.h"
#include "core/sidecar_store.h"
#include "ui/adjust_panel.h"
#include "ui/batch_page.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/filmstrip.h"
#include "ui/looks_panel.h"
#include "ui/main_window.h"
#include "ui/photo_tile.h"
#include "ui/presets_panel.h"
#include "ui/slider_row.h"
#include "ui/straighten_dial.h"
#include "ui/style.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    } // edit files stay out of the real data folder

    void windowBuildsShowsAndOpensAPhoto()
    {
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        pfx::Image img(320, 200);
        img.fill(0.3F, 0.2F, 0.1F);
        const QString path = dir.filePath(u"photo.jpg"_s);
        QVERIFY(pfx::save(img, path.toStdString()).ok());
        QSignalSpy ready(&window.document(), &pixelfx::services::Document::previewReady);
        window.openFiles({dir.filePath(u"notes.txt"_s), path});
        QVERIFY(ready.wait(10000));
        QCOMPARE(window.canvas()->image().size(), QSize(320, 200));
        QVERIFY(window.canvas()->photoRect().isValid());
        QCOMPARE(window.windowTitle(), u"photo.jpg"_s);
        QCOMPARE(settings.recentFiles().first(), path);
        // Before and after: the backslash toggles the split, Space shows the original while held.
        QTest::keyClick(&window, Qt::Key_Backslash);
        QVERIFY(window.canvas()->isComparing());
        QTest::keyClick(&window, Qt::Key_Backslash);
        QVERIFY(!window.canvas()->isComparing());
        // Side by side: the menu item and the header's compare button agree, and the button says
        // which comparison it shows.
        QAction* sideBySide = window.namedAction(u"sideBySide"_s);
        QAction* compare = window.namedAction(u"compare"_s);
        sideBySide->setChecked(true);
        QVERIFY(window.canvas()->isComparing());
        QVERIFY(compare->isChecked());
        QToolButton* compareButton = nullptr;
        for (auto* b : window.findChildren<QToolButton*>(u"headerButton"_s)) {
            if (b->isCheckable() && b->isChecked()) {
                compareButton = b;
            }
        }
        QVERIFY(compareButton);
        QVERIFY(compareButton->toolTip().startsWith(u"Side by side"_s));
        sideBySide->setChecked(false);
        QVERIFY(compareButton->toolTip().startsWith(u"Before and after"_s));
        QTest::keyClick(&window, Qt::Key_Backslash);
        QVERIFY(!window.canvas()->isComparing());
        QVERIFY(!compare->isChecked());
        // Tab hides the panels while the photo has the focus, and brings them back.
        window.canvas()->setFocus();
        QVERIFY(window.adjustPanel()->isVisible());
        QTest::keyClick(window.canvas(), Qt::Key_Tab);
        QVERIFY(window.panelsHidden());
        QVERIFY(!window.adjustPanel()->isVisible());
        QVERIFY(!window.filmstrip()->isVisible());
        QTest::keyClick(window.canvas(), Qt::Key_Tab);
        QVERIFY(!window.panelsHidden());
        QVERIFY(window.adjustPanel()->isVisible());
        // ...but not while a field has it: there Tab moves the focus.
        auto* field = window.presetsPanel()->findChild<QLineEdit*>();
        QVERIFY(field);
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Tab);
        QVERIFY(!window.panelsHidden());
        // A look from the Looks tab is one history step, and undo takes it off.
        Q_EMIT window.presetsPanel()->looksPanel()->lookChosen(u"builtin:mono"_s, 100);
        QCOMPARE(window.document().settings().text("look.id", ""), std::string("builtin:mono"));
        QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(!window.document().settings().contains("look.id"));
        // The main menu lists the window's own actions, with their shortcuts (menu.html).
        QStringList entries;
        for (auto* button : window.findChildren<QToolButton*>()) {
            if (button->menu() != nullptr) {
                for (QAction* a : button->menu()->actions()) {
                    entries << a->text();
                }
            }
        }
        QVERIFY2(entries.contains(u"Open recent"_s) && entries.contains(u"View"_s),
                 qPrintable(entries.join(u'|')));

        // Copy and paste: the settings travel, the crop stays with its photo.
        pfx::Settings look;
        look.set("light.exposure", 0.7);
        look.set("geometry.angle", 4.0);
        window.document().apply(look, u"Look"_s);
        window.copySettings();
        pfx::Settings other;
        other.set("color.vibrance", 30.0);
        window.document().apply(other, u"Other"_s);
        window.pasteSettings();
        QCOMPARE(window.document().settings().number("light.exposure", 0), 0.7);
        QVERIFY(!window.document().settings().contains("color.vibrance"));
        QVERIFY(!window.document().settings().contains("geometry.angle"));
        QCOMPARE(window.document().history().undoLabel(), u"Paste settings"_s);
        window.document().apply({}, u"Clear"_s);

        // The white balance picker: W, then a click on the photo, is one step.
        QTest::keyClick(&window, Qt::Key_W);
        QVERIFY(window.canvas()->isPicking());
        QTest::mouseClick(window.canvas(), Qt::LeftButton, {}, window.canvas()->photoRect().center());
        QVERIFY(!window.canvas()->isPicking());
        QCOMPARE(window.document().history().undoLabel(), u"White balance picker"_s);
        window.document().apply({}, u"Clear"_s);

        // J turns the clipping warnings on and off, through the settings.
        QTest::keyClick(&window, Qt::Key_J);
        QVERIFY(settings.clippingWarnings());
        QVERIFY(window.canvas()->clippingWarnings());
        QTest::keyClick(&window, Qt::Key_J);
        QVERIFY(!window.canvas()->clippingWarnings());

        // A preset from the library applies as one step and keeps what it does not hold.
        {
            pfx::Settings preset;
            preset.set("detail.clarity", 25.0);
            const QString presetPath =
                pixelfx::core::PresetLibrary(pixelfx::core::PresetLibrary::defaultFolder())
                    .save(u"Crisp"_s, u"My presets"_s, preset);
            QVERIFY(!presetPath.isEmpty());
            window.refreshPresets();
            pfx::Settings before;
            before.set("light.exposure", 0.4);
            window.document().apply(before, u"Before"_s);
            for (pixelfx::ui::PhotoTile* tile : window.presetsPanel()->tiles()) {
                if (tile->name() == u"Crisp"_s) {
                    QTest::mouseClick(tile, Qt::LeftButton, {}, QPoint(40, 10));
                }
            }
            QCOMPARE(window.document().settings().number("detail.clarity", 0), 25.0);
            QCOMPARE(window.document().settings().number("light.exposure", 0), 0.4);
            QCOMPARE(window.document().history().undoLabel(), u"Preset Crisp"_s);
            // The Amount blends the preset back toward the photo as it was: half the clarity.
            pixelfx::ui::SliderRow* amount = window.presetsPanel()->amountRow();
            QVERIFY(!amount->parentWidget()->isHidden());
            amount->slider()->setValue(50); // a keyboard step: complete at once
            QCOMPARE(window.document().settings().number("detail.clarity", 0), 12.5);
            QCOMPARE(window.document().settings().number("light.exposure", 0), 0.4);
            QCOMPARE(window.document().history().undoLabel(), u"Crisp at 50%"_s);
            window.document().undo();
            QVERIFY(amount->parentWidget()->isHidden()); // the Amount belonged to that step
            // T picks a colour on the photo, Escape or T again stops; Crop stops it too.
            window.namedAction(u"colorTarget"_s)->trigger();
            QVERIFY(window.isColorTargeting());
            window.namedAction(u"colorTarget"_s)->trigger();
            QVERIFY(!window.isColorTargeting());
            window.startColorTarget();
            window.enterCrop();
            QVERIFY(!window.isColorTargeting());
            window.leaveCrop(false);
            // A hover preview names the photo only once its picture is on it.
            const auto& shown = window.presetsPanel()->entries();
            const auto crisp =
                std::find_if(shown.begin(), shown.end(), [](const auto& e) { return e.name == u"Crisp"_s; });
            QVERIFY(crisp != shown.end());
            Q_EMIT window.presetsPanel()->previewRequested(&*crisp);
            QVERIFY(window.canvas()->previewName().isEmpty());
            QCOMPARE(window.document().hoverState(), pixelfx::services::Document::HoverState::Preparing);
            QVERIFY(ready.wait(10000));
            QCOMPARE(window.canvas()->previewName(), u"Crisp"_s);
            Q_EMIT window.presetsPanel()->previewRequested(nullptr);
            QVERIFY(window.canvas()->previewName().isEmpty());
            QFile::remove(presetPath);
            window.document().apply({}, u"Clear"_s);
        }

        // Zoom: in from Fit, back with Ctrl+0; Crop mode always fits.
        QVERIFY(window.canvas()->isFit());
        QCOMPARE(window.canvas()->zoomText(), u"Fit"_s);
        window.canvas()->zoomIn();
        QVERIFY(!window.canvas()->isFit());
        QVERIFY(window.canvas()->scale() > 1.0);
        QVERIFY(window.canvas()->zoomText().endsWith(u'%'));
        QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(window.canvas()->isFit());
        window.canvas()->setZoom(3.0);
        QVERIFY(window.canvas()->visibleFraction().width() < 1.0); // 960 px of photo, smaller window
        QTest::keyClick(&window, Qt::Key_C);
        QVERIFY(window.canvas()->isFit());
        QTest::keyClick(&window, Qt::Key_Escape);

        // Crop mode: C enters, Escape drops the change, Enter keeps it as one step.
        const int steps = static_cast<int>(window.document().history().labels().size());
        QTest::keyClick(&window, Qt::Key_C);
        QVERIFY(window.isCropping());
        QVERIFY(window.canvas()->isCropping());
        window.cropPanel()->chooseRatio(u"1:1"_s);
        QCOMPARE(window.document().outputSize(), QSize(200, 200));
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!window.isCropping());
        QCOMPARE(window.document().outputSize(), QSize(320, 200));
        QTest::keyClick(&window, Qt::Key_C);
        // Draw a level line: a press and a release 10 degrees down to the right turn it back.
        window.canvas()->startLevelLine();
        const QPoint from = window.canvas()->rect().center() - QPoint(100, 0);
        const QPoint to =
            from + QPoint(200, static_cast<int>(std::lround(200 * std::tan(10 * std::numbers::pi / 180))));
        QTest::mousePress(window.canvas(), Qt::LeftButton, {}, from);
        QTest::mouseRelease(window.canvas(), Qt::LeftButton, {}, to);
        QVERIFY(std::abs(window.cropPanel()->settings().number("geometry.angle", 0) + 10.0) < 0.3);
        QVERIFY(std::abs(window.canvas()->dial()->angle() + 10.0) < 0.3);
        window.cropPanel()->setAngle(0);
        window.cropPanel()->chooseRatio(u"1:1"_s);
        QTest::keyClick(&window, Qt::Key_Return);
        QVERIFY(!window.isCropping());
        QCOMPARE(window.document().outputSize(), QSize(200, 200));
        QCOMPARE(window.document().history().labels().size(), steps + 1);
        QVERIFY(window.adjustPanel()->settings().contains("geometry.crop.w"));
        QTRY_COMPARE_WITH_TIMEOUT(window.canvas()->image().size(), QSize(200, 200), 10000);
        window.close();
    }

    void pastingAPictureOpensIt()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QImage picture(64, 48, QImage::Format_RGB32);
        picture.fill(QColor(200, 120, 40));
        QGuiApplication::clipboard()->setImage(picture);
        QSignalSpy opened(&window.document(), &pixelfx::services::Document::opened);
        QTest::keyClick(&window, Qt::Key_V, Qt::ControlModifier);
        QVERIFY(opened.wait(10000));
        QCOMPARE(window.document().info().size, QSize(64, 48));
        QVERIFY(QFileInfo(window.document().info().path).fileName().startsWith(u"pasted-"_s));
        QFile::remove(window.document().info().path);
    }

    void severalPhotosShareThePresetThroughTheStrip()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QStringList files;
        for (int i = 0; i < 2; ++i) {
            pfx::Image img(64, 48);
            img.fill(0.2F + (0.2F * static_cast<float>(i)), 0.3F, 0.4F); // different pixels, fingerprints
            files << dir.filePath(u"p%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        QSignalSpy opened(&window.document(), &pixelfx::services::Document::opened);
        window.openFiles(files);
        QVERIFY(opened.wait(10000));
        QCOMPARE(window.filmstrip()->files().size(), 2);
        QCOMPARE(window.document().info().path, files.at(0));
        QTest::keyClick(&window, Qt::Key_Right);
        QVERIFY(opened.wait(10000));
        QCOMPARE(window.document().info().path, files.at(1));

        window.filmstrip()->selectAll();
        pfx::Settings preset;
        preset.set("light.exposure", 0.8);
        pixelfx::core::PresetEntry entry;
        entry.name = u"Bright"_s;
        entry.settings = preset;
        Q_EMIT window.presetsPanel()->applyRequested(entry);
        QCOMPARE(window.document().settings().number("light.exposure", 0), 0.8);
        // The other photo got it through its remembered edits.
        const pixelfx::core::SidecarStore store(pixelfx::core::SidecarStore::defaultFolder());
        const auto other = store.read(pixelfx::core::fileFingerprint(files.at(0)));
        QVERIFY(other.has_value());
        QCOMPARE(other->number("light.exposure", 0), 0.8);
        // Export with the selection writes every photo, each with its own settings.
        auto options = settings.exportOptions();
        options.folder = dir.filePath(u"out"_s);
        options.extension = u"png"_s;
        settings.setExportOptions(options);
        QTimer::singleShot(200, &window, [] {
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (auto* sheet = qobject_cast<QDialog*>(w); sheet != nullptr && sheet->isVisible()) {
                    sheet->accept();
                }
            }
        });
        window.exportPhoto();
        QTRY_COMPARE_WITH_TIMEOUT(QDir(dir.filePath(u"out"_s)).entryList({u"*.png"_s}).size(), 2, 20000);
        QVERIFY(QFile::remove(store.pathFor(pixelfx::core::fileFingerprint(files.at(0)))));
    }

    void batchModeComesAndGoes()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::keyClick(&window, Qt::Key_B);
        QVERIFY(window.isBatching());
        window.batchPage()->addPaths({dir.path()}); // an empty folder adds nothing
        QVERIFY(window.batchPage()->files().isEmpty());
        // The page as a batch recipe, and back.
        const QString recipe = dir.filePath(u"job.pfxbatch"_s);
        QVERIFY(window.batchPage()->saveRecipe(recipe));
        QVERIFY(window.batchPage()->loadRecipe(recipe).isEmpty());
        QVERIFY(!window.batchPage()->loadRecipe(dir.filePath(u"s.ini"_s)).isEmpty());
        // A look is a choice of the recipe ("Preset or look", batch.html), at its own strength.
        window.batchPage()->chooseRecipe(u"look:builtin:noir"_s);
        const auto withLook = window.batchPage()->recipe();
        QCOMPARE(withLook.presets.size(), std::size_t{1});
        QCOMPARE(withLook.presets.front().text("look.id", ""), std::string("builtin:noir"));
        QTest::keyClick(&window, Qt::Key_E);
        QVERIFY(!window.isBatching());
    }

    void supportedExtensions()
    {
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"/a/B.JPG"_s));
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"x.webp"_s));
        QVERIFY(!pixelfx::ui::MainWindow::isSupported(u"x.txt"_s));
    }
};

QTEST_MAIN(TestSmoke)
#include "tst_smoke.moc"
