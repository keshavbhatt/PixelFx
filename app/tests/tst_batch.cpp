#include "core/sidecar_store.h"
#include "services/batch_runner.h"

#include <QDir>
#include <QFile>
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
