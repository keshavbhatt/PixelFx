#include "services/batch_runner.h"
#include "ui/batch_page.h"
#include "ui/compress_compare.h"
#include "ui/compress_list.h"
#include "ui/compress_panel.h"
#include "ui/style.h"

#include <QApplication>
#include <QFileInfo>
#include <QLabel>
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
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void theModeSwitchesThePanelAndTheList()
    {
        BatchPage page({}, {}, false);
        QSignalSpy mode(&page, &BatchPage::compressOnlyChanged);
        QVERIFY(!page.isCompressOnly());
        QVERIFY(page.compressPanel()->isHidden() || !page.compressPanel()->isVisibleTo(&page));
        page.setCompressOnly(true);
        QCOMPARE(mode.count(), 1);
        QVERIFY(page.compressPanel()->isVisibleTo(&page));
        QCOMPARE(page.summary(), u"No files yet"_s);
        page.setCompressOnly(false);
        QCOMPARE(page.summary(), u"No photos yet"_s);
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
        QCOMPARE(page.compressPanel()->compressButton()->text(), u"Compress 3 files"_s);
        QVERIFY(!page.compressPanel()->runAgainButton()->isEnabled());
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
        QVERIFY(page.compressPanel()->runAgainButton()->isEnabled());
        // The first file done is selected, and its before and after shows.
        QVERIFY(!page.compressList()->selected().isEmpty());
        QTRY_VERIFY(page.compressCompare()->isVisible());
        QCOMPARE(pixelfx::ui::CompressList::describe(page.runner().compressed(0), files.at(0)),
                 u"PNG, lossless"_s);
    }
};

QTEST_MAIN(TestBatchCompress)
#include "tst_batch_compress.moc"
