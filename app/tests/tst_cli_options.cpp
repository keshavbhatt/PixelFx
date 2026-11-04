#include "app/cli_options.h"

#include <QJsonArray>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::app;

class TestCliOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void filesBecomeAbsolute()
    {
        const auto r =
            parseCliOptions({u"pixelfx"_s, u"a.jpg"_s, u"../b.png"_s, u"/x/c.tif"_s}, u"/home/u/pics"_s);
        QVERIFY(r.ok());
        QCOMPARE(r.options.files, QStringList({u"/home/u/pics/a.jpg"_s, u"/home/u/b.png"_s, u"/x/c.tif"_s}));
    }

    void optionsParse()
    {
        const auto r = parseCliOptions(
            {u"pixelfx"_s, u"--profile"_s, u"test"_s, u"--quit"_s, u"--no-log-file"_s}, u"/"_s);
        QVERIFY(r.ok());
        QCOMPARE(r.options.profile, u"test"_s);
        QVERIFY(r.options.quit);
        QVERIFY(r.options.noLogFile);
    }

    void helpVersionAndErrors()
    {
        QVERIFY(parseCliOptions({u"pixelfx"_s, u"--help"_s}, u"/"_s).helpRequested);
        QVERIFY(parseCliOptions({u"pixelfx"_s, u"--version"_s}, u"/"_s).versionRequested);
        QVERIFY(!parseCliOptions({u"pixelfx"_s, u"--nope"_s}, u"/"_s).ok());
        QVERIFY(!parseCliOptions({u"pixelfx"_s, u"--profile"_s, u"a/b"_s}, u"/"_s).ok());
    }

    void commandsForForwarding()
    {
        CliOptions none;
        QCOMPARE(commandsFor(none).size(), 1);
        QCOMPARE(commandsFor(none).first().value(u"cmd"_s).toString(), u"raise"_s);
        CliOptions both;
        both.files = {u"/a.jpg"_s};
        both.quit = true;
        const auto cmds = commandsFor(both);
        QCOMPARE(cmds.size(), 2);
        QCOMPARE(cmds.at(0).value(u"cmd"_s).toString(), u"open"_s);
        QCOMPARE(cmds.at(0).value(u"files"_s).toArray().first().toString(), u"/a.jpg"_s);
        QCOMPARE(cmds.at(1).value(u"cmd"_s).toString(), u"quit"_s);
    }
};

QTEST_GUILESS_MAIN(TestCliOptions)
#include "tst_cli_options.moc"
