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

    void unsafeCharactersAndEmptyNames()
    {
        ExportOptions o;
        o.nameTemplate = u"a/b:c"_s;
        QCOMPARE(exportFileName(u"x.jpg"_s, o, {}, {}), u"a-b-c.jpg"_s);
        o.nameTemplate = u"  "_s;
        QCOMPARE(exportFileName(u"/p/photo.png"_s, o, {}, {}), u"photo.jpg"_s);
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
};

QTEST_GUILESS_MAIN(TestExportOptions)
#include "tst_export_options.moc"
