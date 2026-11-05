// SPDX-License-Identifier: Apache-2.0
// Linear DNG (ADR-012): a small DNG is written here, uncompressed or JPEG XL like a phone's, and
// developed: levels, colour, baseline exposure, the gain table map, orientation and Exif.
#include "doctest/doctest.h"
#include "fixtures.h"
#include "io/compress_internal.h"
#include "pfx/io.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <vips/vips8>

namespace {

using Bytes = std::vector<std::uint8_t>;

// A little-endian TIFF writer: entries sorted by tag, values after the directory.
class TiffWriter
{
public:
    void shorts(std::uint16_t tag, const std::vector<std::uint16_t>& v) { add(tag, 3, v.size(), pack16(v)); }
    void longs(std::uint16_t tag, const std::vector<std::uint32_t>& v) { add(tag, 4, v.size(), pack32(v)); }
    void text(std::uint16_t tag, const std::string& s)
    {
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        add(tag, 2, b.size(), b);
    }
    void srationals(std::uint16_t tag, const std::vector<double>& v)
    {
        Bytes b;
        for (const double x : v) {
            append32(b, static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(x * 10000))));
            append32(b, 10000);
        }
        add(tag, 10, v.size(), b);
    }
    void rationals(std::uint16_t tag, const std::vector<double>& v)
    {
        Bytes b;
        for (const double x : v) {
            append32(b, static_cast<std::uint32_t>(std::lround(x * 10000)));
            append32(b, 10000);
        }
        add(tag, 5, v.size(), b);
    }
    void undefined(std::uint16_t tag, const Bytes& b) { add(tag, 7, b.size(), b); }
    // The image data, pointed at by StripOffsets.
    void strip(Bytes data)
    {
        m_strip = std::move(data);
        longs(279, {static_cast<std::uint32_t>(m_strip.size())});
        longs(273, {0}); // filled in by bytes()
    }

    [[nodiscard]] Bytes bytes() const
    {
        Bytes out = {'I', 'I', 42, 0, 8, 0, 0, 0};
        const std::size_t dir = 8;
        const std::size_t valuesStart = dir + 2 + (m_entries.size() * 12) + 4;
        Bytes values;
        Bytes entries;
        append16(entries, static_cast<std::uint16_t>(m_entries.size()));
        std::size_t stripOffsetField = 0;
        for (const auto& [tag, e] : m_entries) {
            append16(entries, tag);
            append16(entries, e.type);
            append32(entries, static_cast<std::uint32_t>(e.count));
            if (tag == 273) {
                stripOffsetField = dir + entries.size();
            }
            if (e.data.size() <= 4) {
                Bytes field = e.data;
                field.resize(4, 0);
                entries.insert(entries.end(), field.begin(), field.end());
            } else {
                append32(entries, static_cast<std::uint32_t>(valuesStart + values.size()));
                values.insert(values.end(), e.data.begin(), e.data.end());
                if (values.size() % 2 != 0) {
                    values.push_back(0);
                }
            }
        }
        append32(entries, 0);
        out.insert(out.end(), entries.begin(), entries.end());
        out.insert(out.end(), values.begin(), values.end());
        const auto stripAt = static_cast<std::uint32_t>(out.size());
        out.insert(out.end(), m_strip.begin(), m_strip.end());
        for (std::size_t i = 0; i < 4; ++i) {
            out[stripOffsetField + i] = static_cast<std::uint8_t>((stripAt >> (8 * i)) & 0xFFU);
        }
        return out;
    }

    static void append16(Bytes& b, std::uint16_t v)
    {
        b.push_back(static_cast<std::uint8_t>(v & 0xFFU));
        b.push_back(static_cast<std::uint8_t>(v >> 8U));
    }
    static void append32(Bytes& b, std::uint32_t v)
    {
        for (unsigned s = 0; s < 32; s += 8) {
            b.push_back(static_cast<std::uint8_t>((v >> s) & 0xFFU));
        }
    }

private:
    struct Entry
    {
        std::uint16_t type;
        std::size_t count;
        Bytes data;
    };
    void add(std::uint16_t tag, std::uint16_t type, std::size_t count, Bytes data)
    {
        m_entries[tag] = Entry{type, count, std::move(data)};
    }
    static Bytes pack16(const std::vector<std::uint16_t>& v)
    {
        Bytes b;
        for (const auto x : v) {
            append16(b, x);
        }
        return b;
    }
    static Bytes pack32(const std::vector<std::uint32_t>& v)
    {
        Bytes b;
        for (const auto x : v) {
            append32(b, x);
        }
        return b;
    }

    std::map<std::uint16_t, Entry> m_entries;
    Bytes m_strip;
};

// A 6 by 4 linear DNG: left half at 0.25 of white, right half at 0.5, above a black of 1000;
// identity colour, a neutral white, baseline exposure +1, turned 90 degrees (orientation 6).
Bytes linearDng(bool jpegXl, float gain)
{
    constexpr int kWidth = 6;
    constexpr int kHeight = 4;
    std::vector<std::uint16_t> samples;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const auto v = static_cast<std::uint16_t>(1000 + ((x < kWidth / 2 ? 0.25 : 0.5) * 64535));
            samples.insert(samples.end(), {v, v, v});
        }
    }
    Bytes data;
    if (jpegXl) {
        const vips::VImage img = vips::VImage::new_from_memory_copy(samples.data(), samples.size() * 2, kWidth, kHeight, 3,
                                                                    VIPS_FORMAT_USHORT)
                                     .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_RGB16));
        void* buffer = nullptr;
        std::size_t size = 0;
        img.write_to_buffer(".jxl", &buffer, &size, vips::VImage::option()->set("lossless", true));
        const auto* b = static_cast<const std::uint8_t*>(buffer);
        data.assign(b, b + size);
        g_free(buffer);
    } else {
        for (const auto v : samples) {
            TiffWriter::append16(data, v);
        }
    }
    TiffWriter w;
    w.longs(254, {0});
    w.longs(256, {kWidth});
    w.longs(257, {kHeight});
    w.shorts(258, {16, 16, 16});
    w.shorts(259, {static_cast<std::uint16_t>(jpegXl ? 52546 : 1)});
    w.shorts(262, {34892});
    w.text(271, "Testmaker");
    w.text(272, "Model 7");
    w.shorts(274, {6});
    w.shorts(277, {3});
    w.longs(278, {kHeight});
    w.strip(data);
    w.undefined(50706, {1, 6, 0, 0});
    w.longs(50714, {1000, 1000, 1000});
    w.longs(50717, {65535, 65535, 65535});
    w.srationals(50721, {1, 0, 0, 0, 1, 0, 0, 0, 1});
    w.rationals(50728, {1, 1, 1});
    w.srationals(50730, {1.0});
    w.shorts(50778, {21});
    // A gain table map of 1 by 1 points and 2 brightness steps, `gain` everywhere.
    Bytes map;
    TiffWriter::append32(map, 1);
    TiffWriter::append32(map, 1);
    const auto appendDouble = [&map](double d) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &d, 8);
        TiffWriter::append32(map, static_cast<std::uint32_t>(bits & 0xFFFFFFFFU));
        TiffWriter::append32(map, static_cast<std::uint32_t>(bits >> 32U));
    };
    const auto appendFloat = [&map](float f) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &f, 4);
        TiffWriter::append32(map, bits);
    };
    for (const double d : {1.0, 1.0, 0.5, 0.5}) {
        appendDouble(d);
    }
    TiffWriter::append32(map, 2);
    for (const float f : {0.33F, 0.33F, 0.34F, 0.0F, 0.0F, gain, gain}) {
        appendFloat(f);
    }
    w.undefined(52525, map);
    return w.bytes();
}

std::filesystem::path written(const fixtures::TempDir& dir, const std::string& name, const Bytes& bytes)
{
    const auto path = dir.path() / name;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); // NOLINT
    return path;
}

} // namespace

TEST_CASE("a linear DNG develops: levels, colour, baseline exposure, orientation, Exif")
{
    pfx::detail::startVips();
    const fixtures::TempDir dir;
    for (const bool jpegXl : {false, true}) {
        const auto path = written(dir, jpegXl ? "jxl.dng" : "plain.dng", linearDng(jpegXl, 1.0F));
        CHECK(pfx::isRawFile(path));
        const auto loaded = pfx::load(path);
        REQUIRE(loaded);
        const pfx::LoadedImage& l = loaded.value();
        CHECK(l.info.raw);
        CHECK(l.info.format == "dng");
        CHECK(l.image.width() == 4); // turned: 6 by 4 stored, 4 by 6 upright
        CHECK(l.image.height() == 6);
        // The stored left half (0.25 of white) is the top after the turn, doubled by +1 EV.
        CHECK(l.image.pixel(1, 0)[1] == doctest::Approx(0.5).epsilon(0.02));
        CHECK(l.image.pixel(1, 5)[1] == doctest::Approx(1.0).epsilon(0.02));
        CHECK(l.info.asShotKelvin > 4800);
        CHECK(l.info.asShotKelvin < 5600); // a neutral of 1, 1, 1 with an identity matrix: D50
        CHECK(pfx::readExif(l.metadata.exif).model == "Model 7");
    }
}

TEST_CASE("the gain table map is the camera's tone, applied only when asked")
{
    pfx::detail::startVips();
    const fixtures::TempDir dir;
    const auto path = written(dir, "gain.dng", linearDng(false, 2.0F));
    const auto camera = pfx::load(path);
    pfx::LoadOptions plain;
    plain.rawTone = false;
    const auto flat = pfx::load(path, plain);
    REQUIRE(camera);
    REQUIRE(flat);
    CHECK(camera.value().image.pixel(1, 0)[1] == doctest::Approx(2.0 * double{flat.value().image.pixel(1, 0)[1]}).epsilon(0.02));
}

TEST_CASE("a RAW without a readable image still has its preview")
{
    pfx::detail::startVips();
    const fixtures::TempDir dir;
    // A TIFF-based RAW holding only a JPEG preview, as a JPEG-compressed strip.
    const vips::VImage picture =
        vips::VImage::black(40, 30, vips::VImage::option()->set("bands", 3)).linear(0.0, 180.0).cast(VIPS_FORMAT_UCHAR);
    void* buffer = nullptr;
    std::size_t size = 0;
    picture.write_to_buffer(".jpg", &buffer, &size);
    const Bytes jpeg(static_cast<std::uint8_t*>(buffer), static_cast<std::uint8_t*>(buffer) + size);
    g_free(buffer);
    TiffWriter w;
    w.longs(256, {40});
    w.longs(257, {30});
    w.shorts(259, {7});
    w.strip(jpeg);
    const auto path = written(dir, "photo.nef", w.bytes());
    const auto preview = pfx::loadPreview(path);
    REQUIRE(preview);
    CHECK(preview.value().image.width() == 40);
    CHECK_FALSE(preview.value().info.raw);
    CHECK_FALSE(pfx::loadPreview(written(dir, "empty.nef", Bytes{'I', 'I', 42, 0, 8, 0, 0, 0, 0, 0})).ok());
}
