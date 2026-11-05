#include "core/about.h"
#include "core/changelog.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

namespace {

QString normalised(QString name)
{
    name = name.section(u'(', 0, 0).trimmed().toLower();
    name.remove(u' ');
    return name;
}

} // namespace

class TestAbout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyShippedComponentHasItsNoticeBuiltIn()
    {
        QFile file(QString::fromLatin1(PIXELFX_THIRD_PARTY_PATH));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QSet<QString> known;
        for (const Notice& n : notices()) {
            known.insert(normalised(n.name));
            QVERIFY2(noticeText(n).size() > 500, qPrintable(n.name));
        }
        // Table rows of the libpixelfx and app sections; doctest (tests only) and the mocks are
        // not shipped.
        bool shipped = false;
        int rows = 0;
        for (const QString& line : QString::fromUtf8(file.readAll()).split(u'\n')) {
            if (line.startsWith(u"## "_s)) {
                shipped = !line.contains(u"Mocks"_s);
                continue;
            }
            if (!shipped || !line.startsWith(u"| "_s) || line.startsWith(u"| Component"_s)) {
                continue;
            }
            const QString name = line.section(u'|', 1, 1);
            if (name.contains(u"doctest"_s) || line.contains(u"not shipped"_s)) {
                continue;
            }
            ++rows;
            QVERIFY2(known.contains(normalised(name)), qPrintable(u"no notice for "_s + name));
        }
        QVERIFY(rows >= 10);
    }

    void formatNamesInAFixedOrder()
    {
        QCOMPARE(formatNames({u".png"_s, u".jpeg"_s, u".cr2"_s, u".nef"_s, u".jxl"_s, u".pdf"_s}),
                 (QStringList{u"JPEG"_s, u"PNG"_s, u"JPEG XL"_s, u"RAW"_s}));
        QVERIFY(formatNames({}).isEmpty());
    }

    void diagnosticsHoldVersionsAndNothingPersonal()
    {
        const DiagnosticsFacts facts = collectDiagnostics(u"2.0.0"_s, u"abc1234"_s);
        const QString text = diagnosticsText(facts);
        QVERIFY(text.startsWith(u"PixelFx 2.0.0 (abc1234), libpixelfx "_s));
        QVERIFY(text.contains(u"Qt "_s + QString::fromLatin1(qVersion())));
        QVERIFY(text.contains(u"libvips "_s));
        QVERIFY(text.contains(u"Little CMS "_s));
        QVERIFY(text.contains(u"Reads: JPEG, PNG"_s));
        QVERIFY(text.contains(u"Writes: JPEG, PNG"_s));
        QVERIFY(!text.contains(QDir::homePath()));
        QVERIFY(!text.contains(QDir::home().dirName())); // the user name
        static const QRegularExpression kForbidden(u"licen[cs]e|\\baccounts?\\b|activation"_s,
                                                   QRegularExpression::CaseInsensitiveOption);
        QVERIFY(!kForbidden.match(text).hasMatch());
        // An unknown revision is left out rather than written as "unknown".
        QVERIFY(
            diagnosticsText(collectDiagnostics(u"2.0.0"_s, u"unknown"_s)).startsWith(u"PixelFx 2.0.0, "_s));
    }

    void changelogIsBuiltIn()
    {
        QFile file(u":/CHANGELOG.md"_s);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!changelogReleases(QString::fromUtf8(file.readAll())).isEmpty());
    }
};

QTEST_MAIN(TestAbout)
#include "tst_about.moc"
