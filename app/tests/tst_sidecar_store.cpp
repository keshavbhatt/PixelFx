#include "core/sidecar_store.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

namespace {

QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    QFile f(dir.filePath(name));
    if (!f.open(QIODevice::WriteOnly)) {
        return {};
    }
    f.write(content);
    return f.fileName();
}

} // namespace

class TestSidecarStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fingerprintFollowsContentNotName()
    {
        QTemporaryDir dir;
        const QByteArray big(300 * 1024, 'x');
        const QString a = writeFile(dir, u"a.jpg"_s, big);
        const QString b = writeFile(dir, u"renamed.jpg"_s, big);
        QByteArray changed = big;
        changed[changed.size() - 1] = 'y'; // the last 64 KiB differ
        const QString c = writeFile(dir, u"c.jpg"_s, changed);
        QCOMPARE(fileFingerprint(a), fileFingerprint(b));
        QVERIFY(fileFingerprint(a) != fileFingerprint(c));
        QVERIFY(fileFingerprint(a).startsWith(u"sha1:"_s));
        QVERIFY(fileFingerprint(dir.filePath(u"missing.jpg"_s)).isEmpty());
    }

    void writeReadAndRemove()
    {
        QTemporaryDir dir;
        const QString photo = writeFile(dir, u"photo.jpg"_s, "pixels");
        const SidecarStore store(dir.filePath(u"edits"_s));
        const QString fp = fileFingerprint(photo);
        QVERIFY(!store.read(fp).has_value());
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        QVERIFY(store.write(photo, fp, s));
        QCOMPARE(store.read(fp).value(), s);
        QVERIFY(store.write(photo, fp, {})); // nothing left to remember: the file goes
        QVERIFY(!QFile::exists(store.pathFor(fp)));
    }

    void anUnreadableFileIsIgnored()
    {
        QTemporaryDir dir;
        const SidecarStore store(dir.path());
        QFile f(store.pathFor(u"sha1:0123"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{broken");
        f.close();
        QVERIFY(!store.read(u"sha1:0123"_s).has_value());
    }

    void snapshotsAreKeptAcrossWrites()
    {
        QTemporaryDir dir;
        const SidecarStore store(dir.filePath(u"edits"_s));
        pfx::Settings s;
        s.set("light.exposure", 0.2);
        const std::vector<pfx::BatchStep> snapshots = {{"Before colour", s}};
        QVERIFY(store.write(u"/p/a.jpg"_s, u"fp1"_s, s, &snapshots));
        pfx::Settings later;
        later.set("light.exposure", 0.9);
        QVERIFY(store.write(u"/p/a.jpg"_s, u"fp1"_s, later)); // a batch writing settings only
        QCOMPARE(store.readSnapshots(u"fp1"_s).size(), std::size_t{1});
        QCOMPARE(store.read(u"fp1"_s)->number("light.exposure", 0), 0.9);
        QVERIFY(store.write(u"/p/a.jpg"_s, u"fp1"_s, {})); // no settings, but a snapshot: the file stays
        QVERIFY(QFile::exists(store.pathFor(u"fp1"_s)));
        const std::vector<pfx::BatchStep> none;
        QVERIFY(store.write(u"/p/a.jpg"_s, u"fp1"_s, {}, &none));
        QVERIFY(!QFile::exists(store.pathFor(u"fp1"_s)));
    }
};

QTEST_GUILESS_MAIN(TestSidecarStore)
#include "tst_sidecar_store.moc"
