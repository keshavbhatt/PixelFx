// Report a bug: names left out of what is attached, the issue's title, text and address, the
// crash file a real crash leaves and how it reads, and the sheet's attachments.
#include "core/bug_report.h"
#include "core/crash_handler.h"
#include "ui/crash_notice.h"
#include "ui/report_bug_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include <csignal>
#include <cstdlib>

using namespace Qt::StringLiterals;
namespace crash = pixelfx::core::crash;

class TestBugReport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
    }

    void namesAreLeftOut()
    {
        using pixelfx::core::withoutNames;
        QCOMPARE(withoutNames(u"opened \"/home/sam/Pictures/Trip 2026/beach day.jpg\" QSize(2048, 1322)"_s),
                 u"opened a .jpg file QSize(2048, 1322)"_s);
        QCOMPARE(withoutNames(u"settings: /home/sam/.config/ktechpit/pixelfx.conf log: ~/logs/"_s),
                 u"settings: a .conf file log: a folder"_s);
        QCOMPARE(withoutNames(u"saved C:\\Users\\Sam\\Desktop\\out.png"_s), u"saved a .png file"_s);
        // Not paths: exposure, fractions, web addresses.
        const QString kept = u"ISO 100 \u00B7 f/8 \u00B7 1/250 s, see https://github.com/x/y"_s;
        QCOMPARE(withoutNames(kept), kept);
    }

    void theRunBeforeIsFound()
    {
        const QStringList log = {u"a [pixelfx.app] info: pixelfx 2.0.0 (1) profile=default"_s, u"b"_s, u"c"_s,
                                 u"d [pixelfx.app] info: pixelfx 2.0.0 (1) profile=default"_s, u"e"_s, u"f"_s,
                                 u"g [pixelfx.app] info: pixelfx 2.0.0 (1) profile=default"_s, u"h"_s};
        QCOMPARE(pixelfx::core::previousRun(log, 10), QStringList({log.at(3), u"e"_s, u"f"_s}));
        QCOMPARE(pixelfx::core::previousRun(log, 2), QStringList({u"e"_s, u"f"_s}));
        QCOMPARE(pixelfx::core::previousRun({u"x"_s, u"y"_s}, 1), QStringList{u"y"_s});
    }

    void theIssueIsFilledIn()
    {
        pixelfx::core::BugReport r;
        QCOMPARE(pixelfx::core::reportTitle(r), u"Bug report"_s);
        r.crash = u"PixelFx 2.0.0 closed with a crash (SIGSEGV)."_s;
        QCOMPARE(pixelfx::core::reportTitle(r), u"PixelFx closed unexpectedly"_s);
        r.whatHappened = u"\n  Crop froze after a turn  \nthen it closed"_s;
        QCOMPARE(pixelfx::core::reportTitle(r), u"Crop froze after a turn"_s);
        r.whatHappened = QString(200, u'x');
        QCOMPARE(pixelfx::core::reportTitle(r).size(), 80);
        r.whatHappened = u"It closed"_s;
        r.diagnostics = u"PixelFx 2.0.0"_s;
        r.log = {u"line one"_s, u"line two"_s};
        const QString body = pixelfx::core::reportBody(r);
        QVERIFY(body.startsWith(u"### What happened\n\nIt closed\n"_s));
        QVERIFY(body.contains(u"### What PixelFx was doing when it closed\n\n```\nPixelFx 2.0.0 closed"_s));
        QVERIFY(body.contains(u"### Log\n\n```\nline one\nline two\n```"_s));
        const QUrl url = pixelfx::core::issueUrl(u"A title"_s, body);
        QVERIFY(url.toString().startsWith(u"https://github.com/keshavbhatt/PixelFx/issues/new?"_s));
        // Each field decodes to the text as written, "&" and "#" included.
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(u"body"_s, QUrl::FullyDecoded), body);
        QCOMPARE(pixelfx::core::issueUrl(u"A & B #1"_s, u"x"_s).query(QUrl::FullyEncoded).section(u'&', 0, 0),
                 u"title=A%20%26%20B%20%231"_s);
        // Too long for an address: none, and the fallback asks to paste.
        QVERIFY(pixelfx::core::issueUrl(u"T"_s, QString(9000, u'y')).isEmpty());
        QVERIFY(pixelfx::core::fallbackUrl(u"T"_s).toString(QUrl::FullyDecoded).contains(u"clipboard"_s));
    }

    void aCrashFileReads()
    {
        const crash::Report r =
            crash::parse(u"kind: SIGSEGV\ntime: 1794000000\nphoto: harbour.jpg\nstack:\n"
                         u"/opt/pixelfx/lib/"
                         u"libpixelfx_ui.so(_ZN7pixelfx2ui6Canvas10paintEventEP11QPaintEvent+0x2c) [0x7f00]\n"
                         u"/usr/lib/libc.so.6(+0x3c770) [0x7f01]\n"_s);
        QCOMPARE(r.kind, u"a crash (SIGSEGV)"_s);
        QCOMPARE(r.when, QDateTime::fromSecsSinceEpoch(1794000000));
        QCOMPARE(r.photo, u"harbour.jpg"_s);
        QCOMPARE(r.stack.size(), 2);
        QVERIFY2(r.stack.at(0).startsWith(u"libpixelfx_ui.so: "_s), qPrintable(r.stack.at(0)));
        QVERIFY(!r.stack.at(0).contains(u"/opt"_s));
        QCOMPARE(crash::parse(u"kind: exception 0xC0000005\n"_s).kind, u"an exception (0xC0000005)"_s);
        QCOMPARE(crash::parse(u"kind: abort\n"_s).kind, u"an abort"_s);
        // The report text never carries the photo's name.
        const QString text = crash::reportText(r, u"2.0.0"_s, {u"a line before"_s});
        QVERIFY(text.startsWith(u"PixelFx 2.0.0 closed with a crash (SIGSEGV)."_s));
        QVERIFY(!text.contains(u"harbour"_s));
        QVERIFY(text.contains(u"a line before"_s));
    }

    void aRealCrashLeavesItsFile()
    {
#ifdef Q_OS_WIN
        QSKIP("Faults are caught by the exception filter on Windows; this child raises a POSIX signal.");
#else
        QTemporaryDir dir;
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(u"PIXELFX_TEST_CRASH_INTO"_s, dir.path());
        child.setProcessEnvironment(env);
        child.start(QCoreApplication::applicationFilePath(), {});
        QVERIFY(child.waitForFinished(20000));
        QCOMPARE(child.exitStatus(), QProcess::CrashExit);
        const std::optional<crash::Report> found = crash::takeLastCrash(dir.path());
        QVERIFY(found.has_value());
        QCOMPARE(found->kind, u"a crash (SIGSEGV)"_s);
        QCOMPARE(found->photo, u"fotó 1.jpg"_s);
        QVERIFY(found->when.isValid());
        // Read once: moved aside, so the next start says nothing.
        QVERIFY(!crash::takeLastCrash(dir.path()).has_value());
        QVERIFY(QFile::exists(QDir(dir.path()).filePath(u"crash-previous.txt"_s)));
#endif
    }

    void theSheetSendsWhatIsTicked()
    {
        const pixelfx::ui::ReportBugSheet plain(std::nullopt);
        QCOMPARE(plain.attachments().size(), 2); // no crash: Diagnostics and the log
        const crash::Report r = crash::parse(u"kind: SIGABRT\ntime: 1794000000\nphoto: a.jpg\nstack:\n"_s);
        pixelfx::ui::ReportBugSheet sheet(r);
        QCOMPARE(sheet.attachments().size(), 3);
        sheet.setWhatHappened(u"It closed while saving"_s);
        QVERIFY(sheet.report().crash.contains(u"an abort"_s));
        QVERIFY(!sheet.report().diagnostics.isEmpty());
        for (QCheckBox* box : sheet.attachments()) {
            box->setChecked(false);
        }
        const pixelfx::core::BugReport none = sheet.report();
        QVERIFY(none.crash.isEmpty() && none.diagnostics.isEmpty() && none.log.isEmpty());
        QCOMPARE(none.whatHappened, u"It closed while saving"_s);
        // The notice names the photo, and says when its edits could not be kept.
        const pixelfx::ui::CrashNotice kept(r, true);
        const pixelfx::ui::CrashNotice lost(r, false);
        const auto says = [](const QWidget& w, const QString& part) {
            for (const QLabel* l : w.findChildren<QLabel*>()) {
                if (l->text().contains(part)) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(says(kept, u"a.jpg"_s));
        QVERIFY(says(kept, u"were kept"_s));
        QVERIFY(says(lost, u"could not be kept"_s));
    }
};

int main(int argc, char** argv)
{
    // The child of aRealCrashLeavesItsFile: install the handlers, then crash.
    if (const char* folder = std::getenv("PIXELFX_TEST_CRASH_INTO")) {
        const QCoreApplication app(argc, argv);
        crash::install(QString::fromLocal8Bit(folder));
        crash::setCurrentPhoto(u"fotó 1.jpg"_s);
        std::raise(SIGSEGV);
        return 0;
    }
    QApplication app(argc, argv);
    TestBugReport test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_bug_report.moc"
