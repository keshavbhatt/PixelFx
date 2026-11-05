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
#include <QMenu>
#include <QPushButton>
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

    void rareFormatsAreUnderMore()
    {
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        QPushButton* more = nullptr;
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->menu() != nullptr) {
                more = b;
            }
        }
        if (more == nullptr) {
            QSKIP("this build writes no format that goes under More");
        }
        QAction* rare = more->menu()->actions().first();
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
