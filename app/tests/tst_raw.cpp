#include "services/document.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <map>
#include <pfx/io.h>
#include <vector>

using namespace Qt::StringLiterals;
using pixelfx::services::Document;

namespace {

using Bytes = std::vector<std::uint8_t>;

void put16(Bytes& b, unsigned v)
{
    b.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    b.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
}

void put32(Bytes& b, unsigned v)
{
    put16(b, v & 0xFFFFU);
    put16(b, v >> 16U);
}

// A tiny uncompressed linear DNG (40 by 30, mid grey, identity colour), as phones write them.
QByteArray linearDng()
{
    constexpr unsigned kW = 40;
    constexpr unsigned kH = 30;
    Bytes strip;
    for (unsigned i = 0; i < kW * kH * 3; ++i) {
        put16(strip, 20000);
    }
    struct Entry
    {
        unsigned type;
        unsigned count;
        Bytes data;
    };
    std::map<unsigned, Entry> e;
    const auto shorts = [&e](unsigned tag, std::vector<unsigned> v) {
        Bytes b;
        for (const unsigned x : v) {
            put16(b, x);
        }
        e[tag] = {3, static_cast<unsigned>(v.size()), b};
    };
    const auto longs = [&e](unsigned tag, std::vector<unsigned> v) {
        Bytes b;
        for (const unsigned x : v) {
            put32(b, x);
        }
        e[tag] = {4, static_cast<unsigned>(v.size()), b};
    };
    const auto srationals = [&e](unsigned tag, std::vector<int> v) {
        Bytes b;
        for (const int x : v) {
            put32(b, static_cast<unsigned>(x));
            put32(b, 1);
        }
        e[tag] = {10, static_cast<unsigned>(v.size()), b};
    };
    longs(256, {kW});
    longs(257, {kH});
    shorts(258, {16, 16, 16});
    shorts(259, {1});
    shorts(262, {34892});
    longs(273, {0});
    shorts(277, {3});
    longs(278, {kH});
    longs(279, {static_cast<unsigned>(strip.size())});
    e[50706] = {1, 4, {1, 6, 0, 0}};
    longs(50717, {65535, 65535, 65535});
    srationals(50721, {1, 0, 0, 0, 1, 0, 0, 0, 1});
    Bytes out = {'I', 'I', 42, 0, 8, 0, 0, 0};
    const std::size_t values = 8 + 2 + (e.size() * 12) + 4;
    Bytes dir;
    Bytes data;
    put16(dir, static_cast<unsigned>(e.size()));
    std::size_t stripField = 0;
    for (auto& [tag, entry] : e) {
        put16(dir, tag);
        put16(dir, entry.type);
        put32(dir, entry.count);
        if (tag == 273) {
            stripField = 8 + dir.size();
        }
        if (entry.data.size() <= 4) {
            entry.data.resize(4, 0);
            dir.insert(dir.end(), entry.data.begin(), entry.data.end());
        } else {
            put32(dir, static_cast<unsigned>(values + data.size()));
            data.insert(data.end(), entry.data.begin(), entry.data.end());
        }
    }
    put32(dir, 0);
    out.insert(out.end(), dir.begin(), dir.end());
    out.insert(out.end(), data.begin(), data.end());
    const auto at = static_cast<unsigned>(out.size());
    out.insert(out.end(), strip.begin(), strip.end());
    for (unsigned i = 0; i < 4; ++i) {
        out[stripField + i] = static_cast<std::uint8_t>((at >> (8 * i)) & 0xFFU);
    }
    return {reinterpret_cast<const char*>(out.data()), static_cast<qsizetype>(out.size())}; // NOLINT
}

} // namespace

class TestRaw : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aRawOpensOnCameraAndDevelopsAgainOffIt()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"phone.dng"_s);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(linearDng());
        f.close();

        Document doc;
        QSignalSpy opened(&doc, &Document::opened);
        QSignalSpy developing(&doc, &Document::developingChanged);
        doc.open(path, 200);
        QVERIFY(doc.isDeveloping());
        QVERIFY(opened.wait(10000));
        QVERIFY(!doc.isDeveloping());
        QVERIFY(doc.info().raw);
        QCOMPARE(doc.info().size, QSize(40, 30));
        QVERIFY(doc.info().asShotKelvin > 0);
        QCOMPARE(doc.settings().text("color.profile", ""), std::string("camera"));

        // Standard is PixelFx's rendering without the file's tone: the RAW is developed again.
        developing.clear();
        pfx::Settings standard = doc.settings();
        standard.set("color.profile", std::string("standard"));
        doc.apply(standard, u"Profile Standard"_s);
        QVERIFY(doc.isDeveloping());
        QTRY_VERIFY_WITH_TIMEOUT(!doc.isDeveloping(), 10000);
        QCOMPARE(developing.count(), 2);
    }
};

QTEST_MAIN(TestRaw)
#include "tst_raw.moc"
