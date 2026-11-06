#include "core/plurals.h"
#include "services/batch_runner.h"
#include "ui/batch_page.h"
#include "ui/compress_compare.h"
#include "ui/compress_list.h"
#include "ui/compress_panel.h"
#include "ui/name_field.h"
#include "ui/style.h"

#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::BatchPage;

class TestBatchCompress : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        pixelfx::ui::applyStyle(*qApp);
        QVERIFY(pixelfx::core::installPluralForms(*qApp)); // as the app does
    }

    void oneOfACountIsSaidInTheSingular()
    {
        pixelfx::core::CompressJob job;
        pixelfx::ui::BatchPage page({}, job, true);
        page.compressPanel()->setFileCount(1);
        QCOMPARE(page.compressPanel()->compressButton()->text(), u"Compress 1 photo"_s);
        page.compressPanel()->setFileCount(2);
        QCOMPARE(page.compressPanel()->compressButton()->text(), u"Compress 2 photos"_s);
        QCOMPARE(QCoreApplication::translate("pixelfx::ui::BatchPage", "Process %n photos", nullptr, 1),
                 u"Process 1 photo"_s);
    }

    void theModeSwitchesThePanelAndTheList()
    {
        BatchPage page({}, {}, false);
        QSignalSpy mode(&page, &BatchPage::compressOnlyChanged);
        QVERIFY(!page.isCompressOnly());
        QVERIFY(page.compressPanel()->isHidden() || !page.compressPanel()->isVisibleTo(&page));
        page.setCompressOnly(true);
        QCOMPARE(mode.count(), 1);
        QVERIFY(page.compressPanel()->isVisibleTo(&page));
        QCOMPARE(page.summary(), u"No photos yet"_s); // one noun in both modes
        page.setCompressOnly(false);
        QCOMPARE(page.summary(), u"No photos yet"_s);
    }

    void savingOverTheOriginalsIsSaidAndTheListHoldsStillWhileARunGoes()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 3; ++i) {
            pfx::Image img(40, 30);
            img.fill(0.3F, 0.3F, 0.3F);
            files << dir.filePath(u"own%1.jpg"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        // Their own folder, their own name and format, and Overwrite: the originals would go.
        pixelfx::core::ExportOptions out;
        out.folder = dir.path();
        out.nameTemplate = u"{name}"_s;
        out.extension = u"jpg"_s;
        out.ifExists = pixelfx::core::ExportOptions::IfExists::Overwrite;
        BatchPage page(out, {}, false);
        page.resize(1200, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.addPaths(files);
        QVERIFY(page.nameField()->showsOverwrites());
        // While a batch runs nothing is added and nothing is taken off.
        out.folder = dir.filePath(u"out"_s);
        BatchPage safe(out, {}, false);
        safe.addPaths(files);
        QVERIFY(!safe.nameField()->showsOverwrites());
        safe.runner().setAtOnce(1);
        QSignalSpy running(&safe, &BatchPage::runningChanged);
        safe.process();
        QCOMPARE(running.count(), 1);
        QVERIFY(running.first().first().toBool());
        safe.addPaths({files.first()});
        QCOMPARE(safe.summary().left(1), u"3"_s); // still the three
        QSignalSpy finished(&safe.runner(), &pixelfx::services::BatchRunner::finished);
        QVERIFY(finished.wait(20000));
        QTRY_COMPARE(running.count(), 2);
    }

    void anEmptyListIsADropZoneUntilPhotosCome()
    {
        QTemporaryDir dir;
        const QString photo = dir.filePath(u"one.png"_s);
        pfx::Image img(40, 30);
        img.fill(0.3F, 0.3F, 0.3F);
        QVERIFY(pfx::save(img, photo.toStdString()).ok());
        BatchPage page({}, {}, false);
        page.resize(1200, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        const auto zone = [&page] { return page.findChild<QFrame*>(u"dropZone"_s); };
        QVERIFY(zone() != nullptr && zone()->isVisible());
        page.setCompressOnly(true);
        QVERIFY(zone()->isVisible()); // in both modes, in each one's words
        page.addPaths({photo});
        QVERIFY(!zone()->isVisible());
    }

    void theNameShowsTheFirstNamesAndSaysWhenTheyClash()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 5; ++i) {
            pfx::Image img(40, 30);
            img.fill(0.3F, 0.3F, 0.3F);
            files << dir.filePath(u"IMG_%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        BatchPage page({}, {}, false);
        page.addPaths(files);
        auto* name = page.nameField();
        QVERIFY(name);
        const QString ext = page.recipe().output.extension;
        name->field()->setText(u"trip-{n}"_s);
        QCOMPARE(name->shownNames(),
                 (QStringList{u"IMG_0.png → trip-1."_s + ext, u"IMG_1.png → trip-2."_s + ext,
                              u"IMG_2.png → trip-3."_s + ext}));
        QVERIFY(!name->showsClash());
        page.resize(1200, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        for (const QLabel* l : name->findChildren<QLabel*>()) {
            if (l->isVisible()) {
                QVERIFY2(l->geometry().right() < name->width(), qPrintable(l->text())); // nothing cut off
            }
        }
        // A name without a token: numbers come in, and the line says so.
        name->field()->setText(u"holiday"_s);
        QCOMPARE(name->shownNames().at(1), u"IMG_1.png → holiday (2)."_s + ext);
        QVERIFY(name->showsClash());
        // Compress only keeps each file's format.
        page.setCompressOnly(true);
        auto* compressName = page.compressPanel()->nameField();
        compressName->field()->setText(u"{name}-small"_s);
        QCOMPARE(compressName->shownNames().first(), u"IMG_0.png → IMG_0-small.png"_s);
    }

    void compressOnlyShowsEachSavingTheTotalsAndABeforeAndAfter()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 3; ++i) {
            pfx::Image img(200, 150);
            img.fill(0.1F * static_cast<float>(i + 2), 0.3F, 0.5F);
            files << dir.filePath(u"photo%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        pixelfx::core::CompressJob job;
        job.folder = dir.filePath(u"small"_s);
        BatchPage page({}, job, true);
        page.resize(1200, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.addPaths(files);
        QCOMPARE(page.compressPanel()->compressButton()->text(), u"Compress 3 photos"_s);
        QSignalSpy finished(&page.runner(), &pixelfx::services::BatchRunner::finished);
        page.process(); // Compress only: process compresses
        QVERIFY(finished.wait(20000));
        QCOMPARE(page.runner().saved(), 3);
        // The totals, and the rows' savings.
        bool totals = false;
        for (const QLabel* l : page.findChildren<QLabel*>()) {
            totals = totals ||
                     (l->isVisible() && l->text().startsWith(u"Saved "_s) && l->text().contains(u" of "_s));
        }
        QVERIFY(totals);
        QCOMPARE(page.compressPanel()->compressButton()->text(), u"Compress again"_s); // the one button
        QVERIFY(page.compressPanel()->compressButton()->isEnabled());
        // The first file done is selected, and its before and after shows.
        QVERIFY(!page.compressList()->selected().isEmpty());
        QTRY_VERIFY(page.compressCompare()->isVisible());
        QCOMPARE(pixelfx::ui::CompressList::describe(page.runner().compressed(0), files.at(0)),
                 u"PNG, lossless"_s);
    }
};

QTEST_MAIN(TestBatchCompress)
#include "tst_batch_compress.moc"
