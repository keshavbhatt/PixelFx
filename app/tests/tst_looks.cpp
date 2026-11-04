#include "core/looks.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestLooks : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void builtInLooksAreTheMocksAndRender()
    {
        const auto& looks = pixelfx::core::builtInLooks();
        QCOMPARE(looks.size(), std::size_t{13}); // editor.html's tiles, Original aside
        QSet<QString> ids;
        pfx::Image photo(32, 24);
        photo.fill(0.3F, 0.2F, 0.1F);
        for (const auto& e : looks) {
            QVERIFY(e.id.startsWith(u"builtin:"_s));
            ids.insert(e.id);
            QVERIFY(!e.look->steps.empty());
            QVERIFY(e.look->defaultStrength > 0);
            const auto out =
                pixelfx::core::renderPhoto(photo, pixelfx::core::withLook({}, e.id, e.look->defaultStrength));
            QVERIFY(out.ok());
            QVERIFY2(!(out.value() == photo), qPrintable(e.id)); // each look changes something
        }
        QCOMPARE(ids.size(), static_cast<qsizetype>(looks.size()));
    }

    void strengthAndNoLook()
    {
        pfx::Image photo(8, 8);
        photo.fill(0.4F, 0.2F, 0.1F);
        const auto mono = pixelfx::core::renderPhoto(photo, pixelfx::core::withLook({}, u"builtin:mono"_s, 100));
        QVERIFY(mono.ok());
        const float* p = mono.value().pixel(4, 4);
        QVERIFY(std::abs(p[0] - p[1]) < 1e-3F && std::abs(p[1] - p[2]) < 1e-3F); // grey
        const auto none = pixelfx::core::renderPhoto(photo, pixelfx::core::withLook({}, u"builtin:mono"_s, 0));
        QVERIFY(none.ok());
        QCOMPARE(none.value(), photo);
        // An id with no look renders without one; withLook with no id clears the group.
        QVERIFY(pixelfx::core::renderPhoto(photo, pixelfx::core::withLook({}, u"builtin:gone"_s, 100)).value() ==
                photo);
        const pfx::Settings cleared = pixelfx::core::withLook(pixelfx::core::withLook({}, u"builtin:mono"_s, 50), {}, 0);
        QVERIFY(cleared.isEmpty());
    }

    void importedLutIsAMineLook()
    {
        QTemporaryDir dir;
        const QString cube = dir.filePath(u"Teal punch.cube"_s);
        QFile file(cube);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n0 0 0\n"); // white goes black
        file.close();
        const QString folder = dir.filePath(u"looks"_s);
        pixelfx::core::setLookFolder(folder);
        const pixelfx::core::LookLibrary library(folder);
        QString error;
        const QString id = library.importLut(cube, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const auto entries = library.entries();
        QCOMPARE(entries.front().name, u"Teal punch"_s);
        QVERIFY(entries.front().isMine());
        QCOMPARE(entries.size(), pixelfx::core::builtInLooks().size() + 1);
        QVERIFY(pixelfx::core::findLook(id) != nullptr);
        pfx::Image white(4, 4);
        white.fill(1.0F, 1.0F, 1.0F);
        const auto out = pixelfx::core::renderPhoto(white, pixelfx::core::withLook({}, id, 100));
        QVERIFY(out.value().pixel(1, 1)[0] < 0.01F);
        QVERIFY(library.remove(entries.front().path));
        QVERIFY(pixelfx::core::findLook(id) == nullptr);
        QVERIFY(library.importLut(dir.filePath(u"nothing.cube"_s), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLooks)
#include "tst_looks.moc"
