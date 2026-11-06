#include "core/watermark.h"
#include "ui/export_sheet.h"
#include "ui/frame_picker.h"
#include "ui/message_line.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::core::ExportOptions;
using pixelfx::ui::ExportSheet;

class TestExportSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void untouchedSheetReturnsTheRememberedChoices()
    {
        ExportOptions in;
        in.extension = u"png"_s;
        in.quality = 77;
        in.bitsPerSample = 16;
        in.longEdge = 1600;
        in.keepMetadata = false;
        in.keepLocation = true;
        in.sizePreset = u"instagram-portrait"_s;
        in.longEdge = 0;
        in.fitInside = true;
        in.folder = u"/tmp/out"_s;
        in.nameTemplate = u"{name}-web"_s;
        in.ifExists = ExportOptions::IfExists::Skip;
        in.sharpen = pfx::OutputSharpen::Print;
        in.colorSpace = pfx::OutputSpace::DisplayP3;
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        QCOMPARE(sheet.options(), in);
    }

    void theFileSizeIsEstimatedFromThePreview()
    {
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(1200, 800), ExportOptions());
        QVERIFY(!sheet.note()->text().contains(u"About"_s)); // nothing to estimate from yet
        QImage preview(300, 200, QImage::Format_RGBA8888);
        for (int y = 0; y < preview.height(); ++y) {
            for (int x = 0; x < preview.width(); ++x) {
                preview.setPixelColor(x, y, QColor((x * 7) % 256, (y * 5) % 256, ((x + y) * 3) % 256));
            }
        }
        sheet.setPreview(preview);
        QTRY_VERIFY_WITH_TIMEOUT(sheet.note()->text().startsWith(u"About "_s), 10000);
        QVERIFY(sheet.note()->text().endsWith(u"Saved formats and choices are remembered."_s));
    }

    void theSizeListGroupsTheSizesAndKeepsMine()
    {
        ExportOptions in;
        in.sizePreset = u"mine:Shop listing"_s;
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        sheet.setCustomSizes({{u"Shop listing"_s, QSize(2000, 2000), 72}});
        QComboBox* list = sheet.sizeList();
        QCOMPARE(list->itemText(0), u"Keep"_s);
        QVERIFY(!(list->model()->flags(list->model()->index(0, 0)) & Qt::ItemIsEnabled)); // a heading
        QCOMPARE(list->itemText(1), u"Original · 6000 x 4000"_s);
        QVERIFY(list->findText(u"Instagram portrait · 1080 x 1350 · 4:5"_s) > 0);
        QVERIFY(list->findText(u"Web · 2048 long edge"_s) > 0);
        QVERIFY(list->findText(u"Mine"_s) > 0);
        // The kept size is chosen again, and gives an exact square at its resolution.
        QCOMPARE(list->currentText(), u"Shop listing · 2000 x 2000 · 1:1"_s);
        QCOMPARE(sheet.options().customSize, QSize(2000, 2000));
        QCOMPARE(sheet.options().dpi, 72);
        QCOMPARE(list->itemText(list->count() - 1), u"Custom size..."_s);
    }

    void theFrameChoosesWhatACropToFillKeeps()
    {
        ExportOptions in;
        in.sizePreset = u"instagram-portrait"_s;
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        QImage photo(300, 200, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        sheet.setPreview(photo);
        auto* picker = sheet.framePicker();
        QVERIFY(picker->isVisibleTo(&sheet)); // 4:5 from 3:2: something to choose
        QTest::keyClick(picker, Qt::Key_Left);
        QVERIFY(sheet.options().frameX < 0.5);
        QCOMPARE(sheet.options().frameY, 0.5);
        in.sizePreset = u"web"_s; // the photo's own shape: nothing to frame
        ExportSheet same(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        QVERIFY(!same.framePicker()->isVisibleTo(&same));
    }

    void theSheetFollowsTheFramingCardAndNeverSqueezesIt()
    {
        // Opened on the photo's own shape, then a size of another: the card shows and the sheet
        // grows for it, scrolling when the screen is too short, the picture at its full height.
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(3000, 4000), ExportOptions());
        QImage photo(150, 200, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        sheet.setPreview(photo);
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        auto* picker = sheet.framePicker();
        QVERIFY(!picker->isVisible());
        const int before = sheet.height();
        QComboBox* list = sheet.sizeList();
        list->setCurrentIndex(list->findText(u"Instagram portrait · 1080 x 1350 · 4:5"_s));
        QTRY_VERIFY(picker->isVisible());
        QTRY_VERIFY(sheet.height() > before);
        QVERIFY(sheet.height() <= sheet.heightLimit());
        QTRY_COMPARE(picker->height(), picker->sizeHint().height());
        // Back to the photo's shape: the card goes and the sheet shrinks to its content again.
        list->setCurrentIndex(1); // Original
        QTRY_VERIFY(!picker->isVisible());
        QTRY_COMPARE(sheet.height(), before);
    }

    void aSheetThatFitsDoesNotScroll()
    {
        // At a desktop's text size, room to spare: no scroll range, the content keeps the sheet's
        // full right padding, and no field is wider than the sheet (the size list was: its right
        // side was cut off).
        const QFont before = QApplication::font();
        QFont desktop = before;
        desktop.setPixelSize(13); // a desktop's text (KDE's 10 pt)
        QApplication::setFont(desktop);
        QWidget window;
        window.resize(1440, 900);
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), ExportOptions());
        sheet.setParent(&window, sheet.windowFlags());
        window.show();
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        const auto* scroll = sheet.findChild<QScrollArea*>(u"sheetScroll"_s);
        QVERIFY(scroll != nullptr);
        QTRY_COMPARE(scroll->verticalScrollBar()->maximum(), 0);
        QCOMPARE(scroll->widget()->layout()->contentsMargins().right(), 20);
        // Nothing in the form is wider than the sheet: the fields end 20 px from its edge.
        QCOMPARE(scroll->widget()->width(), scroll->viewport()->width());
        const QComboBox* size = sheet.sizeList();
        QCOMPARE(size->mapTo(&sheet, QPoint(size->width(), 0)).x(), sheet.width() - 1 - 20);
        QApplication::setFont(before);
    }

    void overwritingThePhotoItselfIsSaid()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath(u"photo.jpg"_s);
        ExportOptions in;
        in.folder = dir.path();
        in.nameTemplate = u"{name}"_s;
        in.extension = u"jpg"_s;
        in.ifExists = ExportOptions::IfExists::Overwrite;
        ExportSheet sheet(source, QSize(600, 400), in);
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        const auto said = [&sheet] {
            for (const QLabel* l : sheet.findChildren<QLabel*>(u"warningText"_s)) {
                if (l->isVisible() && l->text().contains(u"original"_s)) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(said());
        for (QLineEdit* field : sheet.findChildren<QLineEdit*>()) {
            if (field->text() == u"{name}"_s) {
                field->setText(u"{name}-edit"_s); // another name: nothing to warn about
            }
        }
        QVERIFY(!said());
    }

    void watermarkRowListsTheSavedOnes()
    {
        QTemporaryDir dir;
        pixelfx::core::Watermark mark;
        mark.name = u"Studio"_s;
        const QString saved = pixelfx::core::WatermarkLibrary(dir.path()).save(mark);
        QVERIFY(!saved.isEmpty());
        ExportOptions in;
        in.watermark = saved;
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        sheet.setWatermarkFolder(dir.path());
        QCOMPARE(sheet.options().watermark, saved);
        auto* picker = sheet.findChild<pixelfx::ui::WatermarkPicker*>();
        QVERIFY(picker);
        const QStringList items = picker->names();
        QCOMPARE(items, (QStringList{u"None"_s, u"Studio"_s, u"New watermark..."_s}));
        // One that is gone falls back to None.
        QVERIFY(QFile::remove(saved));
        sheet.setWatermarkFolder(dir.path());
        QVERIFY(sheet.options().watermark.isEmpty());
    }

    void copyToClipboardIsForOnePhoto()
    {
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), {});
        QPushButton* copy = nullptr;
        for (auto* b : sheet.findChildren<QPushButton*>()) {
            if (b->text() == u"Copy to clipboard"_s) {
                copy = b;
            }
        }
        QVERIFY(copy);
        QVERIFY(!copy->isHidden());
        sheet.setPhotoCount(3);
        QVERIFY(copy->isHidden());
    }

    void watermarkPreviewsOnTheBatchPhotos()
    {
        QTemporaryDir dir;
        pfx::Image tall(200, 400);
        tall.fill(0.5F, 0.5F, 0.5F);
        const QString path = dir.filePath(u"tall.png"_s);
        QVERIFY(pfx::save(tall, path.toStdString()).ok());
        QImage wide(300, 200, QImage::Format_RGB32);
        wide.fill(Qt::darkGray);
        pixelfx::ui::WatermarkSheet sheet(wide, {}, dir.filePath(u"marks"_s));
        QPushButton* batch = nullptr;
        for (auto* b : sheet.findChildren<QPushButton*>()) {
            if (b->text() == u"Batch photos"_s) {
                batch = b;
            }
        }
        QVERIFY(batch);
        QVERIFY(batch->isHidden()); // no batch, no button
        sheet.setBatchPhotos({path});
        QVERIFY(!batch->isHidden());
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        QVERIFY(sheet.preview()->composed().width() > sheet.preview()->composed().height());
        batch->click();
        QTRY_VERIFY(sheet.preview()->composed().width() <
                    sheet.preview()->composed().height()); // the tall one
    }

    void locationIsOffByDefaultAndFollowsMetadata()
    {
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        QVERIFY(!sheet.options().keepLocation);
        auto boxes = sheet.findChildren<QCheckBox*>();
        QCheckBox* metadata = nullptr;
        QCheckBox* location = nullptr;
        for (QCheckBox* b : boxes) {
            (b->text() == u"Keep location"_s ? location : metadata) = b;
        }
        QVERIFY(metadata && location);
        metadata->setChecked(false);
        QVERIFY(!location->isEnabled());
    }

    void switchingToJpegDropsSixteenBits()
    {
        ExportOptions in;
        in.extension = u"png"_s;
        in.bitsPerSample = 16;
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), in);
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->property("extension").toString() == u"jpg"_s) {
                b->click();
            }
        }
        const ExportOptions out = sheet.options();
        QCOMPARE(out.extension, u"jpg"_s);
        QCOMPARE(out.bitsPerSample, 8);
    }

    void onlyWritableFormatsAreOffered()
    {
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        QStringList offered;
        for (const QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            offered << b->property("extension").toString();
        }
        for (const char* ext : {"jpg", "png", "webp", "tif"}) {
            QVERIFY2(offered.contains(QString::fromLatin1(ext)), ext);
        }
    }

    void aLogoWatermarkWithoutALogoSaysSo()
    {
        QTemporaryDir dir;
        QImage photo(60, 40, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        pixelfx::ui::WatermarkSheet sheet(photo, {}, dir.filePath(u"marks"_s));
        for (auto* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"Logo"_s) {
                b->click();
            }
        }
        QVERIFY(!sheet.useWatermark());
        auto* message = sheet.findChild<pixelfx::ui::MessageLine*>();
        QVERIFY(message && message->isError());
        QVERIFY(message->text().contains(u"logo"_s));
    }

    void aTextWatermarkWithoutTextSaysSo()
    {
        QTemporaryDir dir;
        QImage photo(60, 40, QImage::Format_RGB32);
        photo.fill(Qt::gray);
        pixelfx::ui::WatermarkSheet sheet(photo, {}, dir.filePath(u"marks"_s));
        for (auto* field : sheet.findChildren<QLineEdit*>()) {
            if (field->accessibleName() == u"Watermark text"_s) {
                field->selectAll();
                QTest::keyClick(field, Qt::Key_Backspace); // as a person clears it
            }
        }
        QVERIFY(!sheet.useWatermark());
        auto* message = sheet.findChild<pixelfx::ui::MessageLine*>();
        QVERIFY(message && message->isError());
        QVERIFY(message->text().contains(u"text"_s));
    }

    void rareFormatsAreUnderMore()
    {
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        auto* menu = sheet.findChild<QMenu*>();
        QPushButton* more = nullptr;
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"More"_s) {
                more = b;
            }
        }
        if (more == nullptr || menu == nullptr) {
            QSKIP("this build writes no format that goes under More");
        }
        QAction* rare = menu->actions().first();
        rare->trigger();
        QCOMPARE(more->text(), rare->text());
        QVERIFY(more->isChecked());
        const QString chosen = sheet.options().extension;
        QVERIFY(chosen == u"jxl"_s || chosen == u"heic"_s);
        // Remembered: the sheet opens on it again.
        ExportOptions in;
        in.extension = chosen;
        const ExportSheet again(u"/p/photo.jpg"_s, QSize(600, 400), in);
        QCOMPARE(again.options().extension, chosen);
    }

    void closingMoreWithoutAChoiceKeepsTheFormat()
    {
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        QPushButton* more = nullptr;
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"More"_s) {
                more = b;
            }
        }
        if (more == nullptr) {
            QSKIP("this build writes no format that goes under More");
        }
        const QString before = sheet.options().extension;
        more->click();
        auto* menu = sheet.findChild<QMenu*>();
        QTRY_VERIFY(menu->isVisible());
        menu->hide();
        QTRY_VERIFY(!more->isChecked());
        QCOMPARE(sheet.options().extension, before);
    }

    void aCustomSizeIsTheLongEdgeInTheList()
    {
        ExportOptions in;
        in.longEdge = 1600;
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), in);
        auto* size = sheet.findChild<QComboBox*>(u"sizeList"_s);
        QVERIFY(size->currentText().contains(u"1600"_s));
        QCOMPARE(sheet.options().longEdge, 1600);
    }
};

QTEST_MAIN(TestExportSheet)
#include "tst_export_sheet.moc"
