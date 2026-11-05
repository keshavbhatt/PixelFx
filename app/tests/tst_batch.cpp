#include "core/compress_job.h"
#include "core/sidecar_store.h"
#include "services/batch_runner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
};

QTEST_GUILESS_MAIN(TestBatch)
#include "tst_batch.moc"
