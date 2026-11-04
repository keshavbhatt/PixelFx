// End to end: the real pixelfx binary under a throwaway profile and throwaway config and data
// folders. A second launch forwards its photo to the first; --quit makes the first exit.
#include <QProcess>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

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
};

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
