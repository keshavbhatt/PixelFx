#include "core/export_options.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

class TestExportOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tokensFillIn()
    {
        ExportOptions o;
        o.nameTemplate = u"{name}-{w}x{h}-{date}"_s;
        o.extension = u"webp"_s;
        QCOMPARE(exportFileName(u"/p/sunset.lake.JPG"_s, o, QSize(2048, 1365), QDate(2026, 11, 5)),
                 u"sunset.lake-2048x1365-2026-11-05.webp"_s);
    }

    void numbersDaysAndCamerasFillIn()
    {
        ExportOptions o;
        o.nameTemplate = u"trip-{n}-{taken}-{camera}"_s;
        NameFacts facts;
        facts.number = 7;
        facts.count = 300;
        facts.taken = QDate(2026, 8, 14);
        facts.camera = u"iPhone 15"_s;
        QCOMPARE(exportFileName(u"/p/IMG_0412.HEIC"_s, o, {}, QDate(2026, 11, 6), facts),
                 u"trip-007-2026-08-14-iPhone 15.jpg"_s);
        // No date in the photo: today; one photo: no padding.
        o.nameTemplate = u"{taken}-{n}"_s;
        QCOMPARE(exportFileName(u"a.jpg"_s, o, {}, QDate(2026, 11, 6), NameFacts{}), u"2026-11-06-1.jpg"_s);
        QVERIFY(namesDiffer(u"{name}-small"_s));
        QVERIFY(namesDiffer(u"trip-{n}"_s));
        QVERIFY(!namesDiffer(u"holiday-{date}"_s));
        QCOMPARE(nameTokens().first(), u"{name}"_s);
    }

    void theSavedSizeFollowsTheLongEdge()
    {
        ExportOptions o;
        o.longEdge = 1600;
        QCOMPARE(outputSize(o, QSize(4000, 3000)), QSize(1600, 1200));
        QCOMPARE(outputSize(o, QSize(800, 600)), QSize(800, 600)); // never enlarged
        o.noEnlarge = false;
        QCOMPARE(outputSize(o, QSize(800, 600)), QSize(1600, 1200));
        o.longEdge = 0;
        QCOMPARE(outputSize(o, QSize(800, 600)), QSize(800, 600));
    }

    void unsafeCharactersAndEmptyNames()
    {
        ExportOptions o;
        o.nameTemplate = u"a/b:c"_s;
        QCOMPARE(exportFileName(u"x.jpg"_s, o, {}, {}), u"a-b-c.jpg"_s);
        o.nameTemplate = u"  "_s;
        QCOMPARE(exportFileName(u"/p/photo.png"_s, o, {}, {}), u"photo.jpg"_s);
    }

    void namesNeitherHideNorRunPastWhatFileSystemsAllow()
    {
        ExportOptions o;
        o.nameTemplate = u".{name}"_s; // a leading dot would hide the photo
        QCOMPARE(exportFileName(u"/p/photo.jpg"_s, o, {}, {}), u"photo.jpg"_s);
        o.nameTemplate = u"{name}-edit"_s;
        const QString longName = QString(120, QChar(0x5199)); // 写, 3 bytes in UTF-8
        const QString saved = exportFileName(u"/p/"_s + longName + u".jpg"_s, o, {}, {});
        QVERIFY(saved.toUtf8().size() <= 255);
        QVERIFY(saved.endsWith(u".jpg"_s));
    }

    void ifExistsPolicies()
    {
        QTemporaryDir dir;
        ExportOptions o;
        o.folder = dir.path();
        const QString source = dir.filePath(u"photo.jpg"_s);
        const QString first = exportTarget(source, o, {}, {});
        QCOMPARE(first, dir.filePath(u"photo-edit.jpg"_s));
        QVERIFY(QFile(first).open(QIODevice::WriteOnly));
        QCOMPARE(exportTarget(source, o, {}, {}), dir.filePath(u"photo-edit (2).jpg"_s));
        o.ifExists = ExportOptions::IfExists::Skip;
        QVERIFY(exportTarget(source, o, {}, {}).isEmpty());
        o.ifExists = ExportOptions::IfExists::Overwrite;
        QCOMPARE(exportTarget(source, o, {}, {}), first);
    }

    void neverTheSourceByAccident()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath(u"photo.jpg"_s);
        QVERIFY(QFile(source).open(QIODevice::WriteOnly));
        ExportOptions o;
        o.folder = dir.path();
        o.nameTemplate = u"{name}"_s; // would be the source itself
        QCOMPARE(exportTarget(source, o, {}, {}), dir.filePath(u"photo (2).jpg"_s));
    }

    void emptyFolderMeansNextToTheSource()
    {
        ExportOptions o;
        o.folder.clear();
        const QString dir = QDir(QDir::tempPath()).filePath(u"nonexistent/dir"_s);
        QCOMPARE(exportTarget(dir + u"/a.jpg"_s, o, {}, {}), dir + u"/a-edit.jpg"_s);
    }

    void policyKeysRoundTrip()
    {
        for (const auto v : {ExportOptions::IfExists::AddNumber, ExportOptions::IfExists::Overwrite,
                             ExportOptions::IfExists::Skip}) {
            QCOMPARE(ifExistsFromKey(ifExistsKey(v)), v);
        }
    }

    void sharpenAndColorSpaceKeysRoundTrip()
    {
        for (const auto v :
             {pfx::OutputSharpen::None, pfx::OutputSharpen::Screen, pfx::OutputSharpen::Print}) {
            QCOMPARE(sharpenFromKey(sharpenKey(v)), v);
        }
        for (const auto v : {pfx::OutputSpace::Srgb, pfx::OutputSpace::DisplayP3, pfx::OutputSpace::AdobeRgb,
                             pfx::OutputSpace::Original}) {
            QCOMPARE(colorSpaceFromKey(colorSpaceKey(v)), v);
        }
        QCOMPARE(colorSpaceKey(pfx::OutputSpace::DisplayP3), u"display-p3"_s);
        QCOMPARE(sharpenFromKey(u"loud"_s), pfx::OutputSharpen::None);
        QCOMPARE(colorSpaceFromKey(QString()), pfx::OutputSpace::Srgb);
    }

    void saveOptionsCarryTheChoices()
    {
        ExportOptions o;
        o.quality = 70;
        o.dpi = 300;
        o.colorSpace = pfx::OutputSpace::Original;
        const pfx::SaveOptions save = saveOptions(o, {1, 2, 3});
        QCOMPARE(save.quality, 70);
        QCOMPARE(save.dpi, 300);
        QCOMPARE(save.space, pfx::OutputSpace::Original);
        QCOMPARE(save.originalProfile, (std::vector<std::uint8_t>{1, 2, 3}));
    }

    void printSizesTurnWithThePhoto()
    {
        const pixelfx::core::SizePreset* print = pixelfx::core::sizePreset(u"print-10x15"_s);
        QVERIFY(print != nullptr);
        QCOMPARE(pixelfx::core::exactSize(*print, QSize(4000, 6000)), QSize(1200, 1800));
        QCOMPARE(pixelfx::core::exactSize(*print, QSize(6000, 4000)), QSize(1800, 1200));
        const pixelfx::core::SizePreset* story = pixelfx::core::sizePreset(u"story"_s);
        QCOMPARE(pixelfx::core::exactSize(*story, QSize(6000, 4000)), QSize(1080, 1920)); // social stays
        QVERIFY(!pixelfx::core::exactSize(*pixelfx::core::sizePreset(u"web"_s), QSize(10, 10)).isValid());
        QVERIFY(pixelfx::core::sizePreset(u"nope"_s) == nullptr);
    }
};

QTEST_GUILESS_MAIN(TestExportOptions)
#include "tst_export_options.moc"
