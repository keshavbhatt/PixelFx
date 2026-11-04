// End to end: the real pixelfx binary under a throwaway profile and throwaway config and data
// folders. A second launch forwards its photo to the first; --quit makes the first exit.
#include <QDir>
#include <QProcess>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/document.h>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

class TestInstance : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_home;
    QString m_profile;

    [[nodiscard]] QProcessEnvironment environment() const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
        env.insert(u"XDG_CONFIG_HOME"_s, m_home.filePath(u"config"_s));
        env.insert(u"XDG_DATA_HOME"_s, m_home.filePath(u"data"_s));
        env.insert(u"XDG_CACHE_HOME"_s, m_home.filePath(u"cache"_s));
        env.insert(u"QT_LOGGING_RULES"_s, u"pixelfx.*.debug=false;pixelfx.*.info=true"_s);
        return env;
    }

    void start(QProcess& process, const QStringList& args) const
    {
        process.setProcessEnvironment(environment());
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QString::fromLatin1(PIXELFX_BINARY), QStringList{u"--profile"_s, m_profile} + args);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        m_profile = u"test%1"_s.arg(QRandomGenerator::global()->generate());
    }

    void secondLaunchForwardsAndQuitEnds()
    {
        QProcess primary;
        // Always stop the process this test started, even when a check fails early.
        const auto stopPrimary = qScopeGuard([&primary] {
            if (primary.state() != QProcess::NotRunning) {
                primary.kill();
                primary.waitForFinished(5000);
            }
        });
        start(primary, {});
        QVERIFY(primary.waitForStarted(10000));
        QByteArray log;
        // The info line Application writes once it is the primary and set up.
        QTRY_VERIFY_WITH_TIMEOUT(
            (log += primary.readAll()).contains(QByteArray("profile=").append(m_profile.toUtf8())) ||
                primary.state() != QProcess::Running,
            15000);
        QCOMPARE(primary.state(), QProcess::Running);

        QProcess second;
        start(second, {QString::fromLatin1(PIXELFX_TEST_PHOTO)});
        QVERIFY(second.waitForFinished(15000));
        QCOMPARE(second.exitCode(), 0);
        QTRY_VERIFY_WITH_TIMEOUT((log += primary.readAll()).contains("opened"), 15000);

        QProcess quit;
        start(quit, {u"--quit"_s});
        QVERIFY(quit.waitForFinished(15000));
        QCOMPARE(quit.exitCode(), 0);
        QVERIFY2(primary.waitForFinished(10000), "the running PixelFx did not quit on --quit");
        QCOMPARE(primary.exitStatus(), QProcess::NormalExit);
    }

    void batchRecipeRunsFromTheCommandLine()
    {
        QTemporaryDir work;
        pfx::Document recipe;
        recipe.kind = pfx::DocumentKind::Batch;
        pfx::BatchStep step;
        step.name = "Bright";
        step.settings.set("light.exposure", 0.5);
        recipe.batch.stack = {step};
        recipe.batch.format = "png";
        recipe.batch.longEdge = 16;
        recipe.batch.folder = work.filePath(u"out"_s).toStdString();
        const QString recipePath = work.filePath(u"web.pfxbatch"_s);
        QVERIFY(pfx::saveDocument(recipe, recipePath.toStdString()).ok());
        QStringList photos;
        for (int i = 0; i < 2; ++i) {
            pfx::Image img(32, 24);
            img.fill(0.2F, 0.3F + (0.1F * static_cast<float>(i)), 0.4F);
            photos << work.filePath(u"p%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, photos.last().toStdString()).ok());
        }
        QProcess batch;
        start(batch, QStringList{u"--batch"_s, recipePath} + photos);
        QVERIFY(batch.waitForFinished(60000));
        const QString output = QString::fromUtf8(batch.readAll());
        QVERIFY2(batch.exitCode() == 0, qPrintable(output));
        QVERIFY2(output.contains(u"2 saved, 0 not saved"_s), qPrintable(output));
        QCOMPARE(QDir(work.filePath(u"out"_s)).entryList({u"*.png"_s}).size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
