#include "core/compress_job.h"
#include "core/sidecar_store.h"
#include "services/batch_runner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::services::BatchRecipe;
using pixelfx::services::BatchRunner;

class TestBatch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void settingsStackInOrder()
    {
        BatchRecipe r;
        pfx::Settings light;
        light.set("light.exposure", 0.5);
        pfx::Settings colour;
        colour.set("color.vibrance", 20.0);
        r.presets = {light, colour};
        pfx::Settings own;
        own.set("detail.clarity", 10.0);
        own.set("light.exposure", -1.0);
        const pfx::Image photo(8, 8);
        const pfx::Settings s = BatchRunner::settingsFor(r, own, photo);
        QCOMPARE(s.number("light.exposure", 0), 0.5); // the preset's Light replaces the photo's
        QCOMPARE(s.number("color.vibrance", 0), 20.0);
        QCOMPARE(s.number("detail.clarity", 0), 10.0); // kept: no preset holds Detail
        r.keepEdits = false;
        QVERIFY(!BatchRunner::settingsFor(r, own, photo).contains("detail.clarity"));
    }

    void compressTargetsKeepTheFormatAndNeverTakeAnotherFile()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath(u"photo.JPG"_s);
        QFile touch(source);
        QVERIFY(touch.open(QIODevice::WriteOnly));
        touch.close();
        pixelfx::core::CompressJob job;
        job.folder = dir.filePath(u"out"_s);
        const QDate day(2026, 11, 6);
        QCOMPARE(pixelfx::core::compressTarget(source, job, {}, day), dir.filePath(u"out/photo.JPG"_s));
        // A file already there gets a number, unless the last run of this list wrote it.
        QDir().mkpath(job.folder);
        QFile existing(dir.filePath(u"out/photo.JPG"_s));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();
        QCOMPARE(pixelfx::core::compressTarget(source, job, {}, day), dir.filePath(u"out/photo (2).JPG"_s));
        QCOMPARE(pixelfx::core::compressTarget(source, job, {dir.filePath(u"out/photo.JPG"_s)}, day),
                 dir.filePath(u"out/photo.JPG"_s));
        // The source's own folder: the source is never the target...
        job.folder = dir.path();
        QCOMPARE(pixelfx::core::compressTarget(source, job, {}, day), dir.filePath(u"photo (2).JPG"_s));
        // ...unless replacing the originals.
        job.replaceOriginals = true;
        QCOMPARE(pixelfx::core::compressTarget(source, job, {}, day), QFileInfo(source).absoluteFilePath());
        job.replaceOriginals = false;
        job.folder = dir.filePath(u"out"_s);
        job.nameTemplate = u"{name}-{date}"_s;
        QCOMPARE(pixelfx::core::compressTarget(source, job, {}, day),
                 dir.filePath(u"out/photo-2026-11-06.JPG"_s));
    }

    void compressOnlyWritesSmallerFilesAndRunsAgainInPlace()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 2; ++i) {
            pfx::Image img(160, 120);
            img.fill(0.2F, 0.1F * static_cast<float>(i + 1), 0.5F); // few colours: a palette fits
            files << dir.filePath(u"flat%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        const QString broken = dir.filePath(u"broken.png"_s);
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\x89PNG not really");
        f.close();
        files << broken;

        BatchRecipe r;
        pixelfx::core::CompressJob job;
        job.folder = dir.filePath(u"small"_s);
        r.compress = job;
        BatchRunner runner;
        QSignalSpy finished(&runner, &BatchRunner::finished);
        runner.start(files, r);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 2);
        QCOMPARE(runner.state(2), BatchRunner::State::Failed);
        QVERIFY(!runner.reason(2).isEmpty());
        for (int i = 0; i < 2; ++i) {
            const auto& c = runner.compressed(i);
            QCOMPARE(c.before, QFileInfo(files.at(i)).size());
            QVERIFY(c.after < c.before);
            QCOMPARE(QFileInfo(runner.target(i)).size(), c.after);
            QCOMPARE(c.method, pfx::CompressMethod::PngLossless);
        }
        // Run again with the same list: the files it wrote are replaced, not numbered.
        const QString first = runner.target(0);
        runner.start(files, r);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.target(0), first);
        QCOMPARE(QDir(job.folder).entryList(QDir::Files).size(), 2);

        // Replacing the originals: the file itself gets smaller, and its spot is kept.
        const qint64 before = QFileInfo(files.at(0)).size();
        job.replaceOriginals = true;
        r.compress = job;
        runner.start({files.at(0)}, r);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.target(0), QFileInfo(files.at(0)).absoluteFilePath());
        QVERIFY(QFileInfo(files.at(0)).size() < before);
        QVERIFY(!runner.compressed(0).beforeSpot.isEmpty());
    }

    void photosOfOneNameFromTwoFoldersAreBothKept()
    {
        // Several at once into one folder: a.jpg from two folders must not both become
        // a-edit.jpg, the second over the first. Many pairs, so workers run side by side.
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 8; ++i) {
            for (const char* folder : {"one", "two"}) {
                QDir().mkpath(dir.filePath(QString::fromLatin1(folder)));
                pfx::Image img(400, 300);
                img.fill(0.1F * static_cast<float>(i % 5), 0.3F, folder[0] == 'o' ? 0.2F : 0.8F);
                files << dir.filePath(u"%1/photo%2.png"_s.arg(QString::fromLatin1(folder)).arg(i));
                QVERIFY(pfx::save(img, files.last().toStdString()).ok());
            }
        }
        for (const bool compress : {false, true}) {
            BatchRecipe r;
            const QString out = dir.filePath(compress ? u"small"_s : u"out"_s);
            if (compress) {
                pixelfx::core::CompressJob job;
                job.folder = out;
                r.compress = job;
            } else {
                r.output.folder = out;
                r.output.extension = u"png"_s;
            }
            BatchRunner runner;
            runner.setAtOnce(4);
            QSignalSpy finished(&runner, &BatchRunner::finished);
            runner.start(files, r);
            QVERIFY(finished.wait(60000));
            QCOMPARE(runner.failed(), 0);
            QSet<QString> targets;
            for (int i = 0; i < files.size(); ++i) {
                targets.insert(runner.target(i));
            }
            QCOMPARE(targets.size(), files.size());
            QCOMPARE(QDir(out).entryList(QDir::Files).size(), files.size());
        }
    }

    void runsEveryPhotoAndSaysWhyOneFailed()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 3; ++i) {
            pfx::Image img(40, 30);
            img.fill(0.1F * static_cast<float>(i + 1), 0.3F, 0.4F);
            files << dir.filePath(u"p%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        const QString broken = dir.filePath(u"broken.jpg"_s);
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a photo");
        f.close();
        files.insert(1, broken);

        BatchRecipe r;
        pfx::Settings light;
        light.set("light.exposure", 0.5);
        r.presets = {light};
        r.output.folder = dir.filePath(u"out"_s);
        r.output.extension = u"jpg"_s;
        r.output.longEdge = 20;
        BatchRunner runner;
        QSignalSpy finished(&runner, &BatchRunner::finished);
        runner.start(files, r);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 3);
        QCOMPARE(runner.failed(), 1);
        QCOMPARE(runner.state(1), BatchRunner::State::Failed);
        QVERIFY(!runner.reason(1).isEmpty());
        const QStringList out = QDir(dir.filePath(u"out"_s)).entryList({u"*.jpg"_s});
        QCOMPARE(out.size(), 3);
        const auto probe = pfx::probe(runner.target(0).toStdString());
        QVERIFY(probe.ok());
        QCOMPARE(std::max(probe.value().width, probe.value().height), 20); // resized
    }

    void everyRowWaitsAtTheStartAndStopLeavesTheRestToProcess()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 4; ++i) {
            pfx::Image img(40, 30);
            img.fill(0.2F, 0.3F, 0.4F);
            files << dir.filePath(u"w%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        BatchRecipe r;
        r.output.folder = dir.filePath(u"out"_s);
        r.output.extension = u"jpg"_s;
        BatchRunner runner;
        runner.setAtOnce(1);
        QSignalSpy changed(&runner, &BatchRunner::photoChanged);
        QSignalSpy finished(&runner, &BatchRunner::finished);
        runner.start(files, r);
        // Every row hears at once that it waits (a second run leaves nothing of the first).
        QSet<int> told;
        for (const QList<QVariant>& c : changed) {
            told.insert(c.first().toInt());
        }
        QCOMPARE(told.size(), 4);
        runner.stop(); // the one under way finishes; the rest are not reached
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 1);
        QCOMPARE(runner.waiting(), 3);
        finished.clear();
        runner.processRest();
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 4);
        QCOMPARE(runner.waiting(), 0);
    }

    void aLimitedRunLeavesTheRestWaitingAndTheNextRunTakesTheNextOnes()
    {
        QTemporaryDir dir;
        QStringList files;
        for (int i = 0; i < 5; ++i) {
            pfx::Image img(40, 30);
            img.fill(0.2F, 0.3F, 0.4F);
            files << dir.filePath(u"l%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        BatchRecipe r;
        r.output.folder = dir.filePath(u"out"_s);
        r.output.extension = u"jpg"_s;
        BatchRunner runner;
        QSignalSpy finished(&runner, &BatchRunner::finished);
        runner.start(files, r, 2);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 2);
        QCOMPARE(runner.waiting(), 3);
        QCOMPARE(runner.state(2), BatchRunner::State::Waiting);
        finished.clear();
        runner.processRest(2);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.saved(), 4);
        QCOMPARE(runner.state(4), BatchRunner::State::Waiting);
    }

    void aRecipeKeepsItsSharpeningAndColourSpace()
    {
        BatchRecipe r;
        r.output.sharpen = pfx::OutputSharpen::Screen;
        r.output.colorSpace = pfx::OutputSpace::AdobeRgb;
        const pfx::BatchBody body = pixelfx::services::bodyFromRecipe(r);
        QCOMPARE(body.sharpen, std::string("screen"));
        QCOMPARE(body.colorSpace, std::string("adobe-rgb"));
        const BatchRecipe back = pixelfx::services::recipeFromBody(body, {});
        QCOMPARE(back.output.sharpen, pfx::OutputSharpen::Screen);
        QCOMPARE(back.output.colorSpace, pfx::OutputSpace::AdobeRgb);
    }

    void compressSaysAFileIsNoPhoto()
    {
        QTemporaryDir dir;
        const QString broken = dir.filePath(u"damaged.jpg"_s);
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("abcdef");
        f.close();
        BatchRecipe r;
        pixelfx::core::CompressJob job;
        job.folder = dir.filePath(u"small"_s);
        r.compress = job;
        BatchRunner runner;
        QSignalSpy finished(&runner, &BatchRunner::finished);
        runner.start({broken}, r);
        QVERIFY(finished.wait(20000));
        QCOMPARE(runner.state(0), BatchRunner::State::Failed); // not "no change"
        QVERIFY(!runner.reason(0).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestBatch)
#include "tst_batch.moc"
