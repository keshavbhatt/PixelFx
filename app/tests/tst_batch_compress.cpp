#include "core/plurals.h"
#include "services/batch_runner.h"
#include "ui/batch_page.h"
#include "ui/compress_compare.h"
#include "ui/compress_list.h"
#include "ui/compress_panel.h"
#include "ui/name_field.h"
#include "ui/style.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::BatchPage;

namespace {

// The widest widgets inside `root` by the width they insist on, for a failure message: what
// pushes a panel or a form wider than its room.
QString widestWidgets(const QWidget* root)
{
    QList<const QWidget*> all;
    for (const QWidget* w : root->findChildren<QWidget*>()) {
        if (w->isVisibleTo(root)) {
            all << w;
        }
    }
    std::ranges::sort(all, [](const QWidget* a, const QWidget* b) {
        return a->minimumSizeHint().width() > b->minimumSizeHint().width();
    });
    QStringList out;
    for (const QWidget* w : all.mid(0, 6)) {
        const auto* label = qobject_cast<const QLabel*>(w);
        const auto* button = qobject_cast<const QAbstractButton*>(w);
        const QString text = label != nullptr    ? label->text()
                             : button != nullptr ? button->text()
                                                 : QString();
        out << QStringLiteral("%1 %2 \"%3\" min %4")
                   .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName(), text.left(40))
                   .arg(w->minimumSizeHint().width());
    }
    return out.join(QStringLiteral("; "));
}

} // namespace

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

    void theRecipeCardSaysWhatReallyHappens()
    {
        const auto shown = [](const BatchPage& page) {
            QStringList texts;
            for (const QLabel* l : page.findChildren<QLabel*>()) {
                texts << l->text();
            }
            return texts;
        };
        // Nothing to do at the original size: nothing is resized.
        BatchPage page({}, {}, false);
        QVERIFY(shown(page).contains(u"Convert and resize only"_s));
        QVERIFY(shown(page).contains(u"No edits: each photo is saved as below at the original size"_s));
        // A preset on top is the recipe, and is named.
        page.chooseSecond(u"Soft ember"_s);
        QVERIFY(shown(page).contains(u"Soft ember"_s));
        QVERIFY(shown(page).contains(u"No look or preset first, then saved as below at the original size"_s));
        QVERIFY(!shown(page).contains(u"No look or preset"_s));
        // A size: then it is resized, to that size.
        pixelfx::core::ExportOptions web;
        web.longEdge = 2048;
        const BatchPage sized(web, {}, false);
        bool resized = false;
        for (const QString& t : shown(sized)) {
            resized =
                resized || (t.startsWith(u"No edits: each photo is resized to "_s) && t.contains(u"2048"_s));
        }
        QVERIFY(resized);
    }

    void thePanelFitsItsWidthAtTheDesktopsTextSize()
    {
        // At 13 px, a desktop's text (KDE's 10 pt), nothing in the right panel may be wider than
        // the panel, or its right side is cut off. A list's longest choice did that.
        const QFont before = QApplication::font();
        QFont desktop = before;
        desktop.setPixelSize(13);
        QApplication::setFont(desktop);
        QTemporaryDir dir;
        pfx::Image img(40, 30);
        img.fill(0.3F, 0.3F, 0.3F);
        const QString photo = dir.filePath(u"a-photo-with-a-long-name-from-a-camera-0412.png"_s);
        QVERIFY(pfx::save(img, photo.toStdString()).ok());
        for (const bool compressOnly : {false, true}) {
            BatchPage page({}, {}, compressOnly);
            page.resize(1440, 860);
            page.addPaths({photo});
            page.show();
            QVERIFY(QTest::qWaitForWindowExposed(&page));
            for (const QScrollArea* area : page.findChildren<QScrollArea*>()) {
                const QWidget* content = area->widget();
                if (content != nullptr && content->objectName() == u"batchPanelContent"_s &&
                    area->isVisible()) {
                    QVERIFY2(
                        content->minimumSizeHint().width() <= area->viewport()->width(),
                        qPrintable(u"%1 px of content in %2 px: %3"_s.arg(content->minimumSizeHint().width())
                                       .arg(area->viewport()->width())
                                       .arg(widestWidgets(content))));
                }
            }
        }
        QApplication::setFont(before);
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
