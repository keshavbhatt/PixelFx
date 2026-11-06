// SPDX-License-Identifier: Apache-2.0
// Removing location from metadata without a metadata library (ADR-003): the Exif GPS block is
// unlinked and zeroed, and XMP's exif:GPS properties are dropped. Anything malformed is left as
// it is rather than guessed at.
#include "io/compress_internal.h"
#include "pfx/io.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

namespace pfx {

namespace {

constexpr std::uint16_t kGpsPointer = 0x8825;

class Tiff
{
public:
    explicit Tiff(std::vector<std::uint8_t>& bytes)
        : m_bytes(bytes)
    {
        static constexpr std::array<std::uint8_t, 6> kExifHeader = {'E', 'x', 'i', 'f', 0, 0};
        if (bytes.size() >= 6 && std::equal(kExifHeader.begin(), kExifHeader.end(), bytes.begin())) {
            m_base = 6;
        }
        if (bytes.size() < m_base + 8) {
            return;
        }
        if (bytes[m_base] == 'I' && bytes[m_base + 1] == 'I') {
            m_little = true;
        } else if (bytes[m_base] != 'M' || bytes[m_base + 1] != 'M') {
            return;
        }
        m_valid = read16(2) == std::optional<std::uint32_t>{42}; // the TIFF magic number
    }

    [[nodiscard]] bool valid() const { return m_valid; }

    // Offsets are from the TIFF header, as in the file.
    [[nodiscard]] std::optional<std::uint32_t> read16(std::size_t at) const
    {
        const std::size_t p = m_base + at;
        if (p + 2 > m_bytes.size()) {
            return std::nullopt;
        }
        const auto first = static_cast<std::uint32_t>(m_bytes[p]);
        const auto second = static_cast<std::uint32_t>(m_bytes[p + 1]);
        return m_little ? (first | (second << 8U)) : ((first << 8U) | second);
    }

    [[nodiscard]] std::optional<std::uint32_t> read32(std::size_t at) const
    {
        const auto hi = read16(at + (m_little ? 2 : 0));
        const auto lo = read16(at + (m_little ? 0 : 2));
        if (!hi || !lo) {
            return std::nullopt;
        }
        return (*hi << 16U) | *lo;
    }

    void write16(std::size_t at, std::uint32_t value)
    {
        const std::size_t p = m_base + at;
        if (p + 2 > m_bytes.size()) {
            return;
        }
        const auto a = static_cast<std::uint8_t>(value & 0xFFU);
        const auto b = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        m_bytes[p] = m_little ? a : b;
        m_bytes[p + 1] = m_little ? b : a;
    }

    void write32(std::size_t at, std::uint32_t value)
    {
        write16(at + (m_little ? 0 : 2), value & 0xFFFFU);
        write16(at + (m_little ? 2 : 0), value >> 16U);
    }

    // Where the TIFF header starts in the bytes (after "Exif\0\0" when there is one).
    [[nodiscard]] std::size_t base() const { return m_base; }

    [[nodiscard]] std::optional<std::uint8_t> byte(std::size_t at) const
    {
        const std::size_t p = m_base + at;
        return p < m_bytes.size() ? std::optional<std::uint8_t>{m_bytes[p]} : std::nullopt;
    }

    bool zero(std::size_t at, std::size_t count)
    {
        const std::size_t p = m_base + at;
        if (p + count > m_bytes.size() || p + count < p) {
            return false;
        }
        std::fill_n(m_bytes.begin() + static_cast<std::ptrdiff_t>(p), count, std::uint8_t{0});
        return true;
    }

private:
    std::vector<std::uint8_t>& m_bytes;
    std::size_t m_base = 0;
    bool m_little = false;
    bool m_valid = false;
};

std::size_t typeSize(std::uint32_t type)
{
    switch (type) {
    case 1:
    case 2:
    case 6:
    case 7:
        return 1;
    case 3:
    case 8:
        return 2;
    case 4:
    case 9:
    case 11:
        return 4;
    case 5:
    case 10:
    case 12:
        return 8;
    default:
        return 0;
    }
}

// Zeroes the GPS directory at `offset`: each entry's out-of-line value, then the entries.
void zeroGpsDirectory(Tiff& tiff, std::uint32_t offset)
{
    const auto count = tiff.read16(offset);
    if (!count || *count > 512) {
        return;
    }
    for (std::uint32_t i = 0; i < *count; ++i) {
        const std::size_t entry = offset + 2 + (12 * static_cast<std::size_t>(i));
        const auto type = tiff.read16(entry + 2);
        const auto n = tiff.read32(entry + 4);
        const auto value = tiff.read32(entry + 8);
        if (type && n && value) {
            const std::size_t size = typeSize(*type) * *n;
            if (size > 4 && size < (1U << 20U)) {
                tiff.zero(*value, size);
            }
        }
    }
    tiff.zero(offset, 2 + (12 * static_cast<std::size_t>(*count)));
}

// One directory's entries by tag: type, count, and the value field's offset (values of four
// bytes or fewer sit in the field itself, longer ones at the offset it holds).
struct Entry
{
    std::uint32_t type = 0;
    std::uint32_t count = 0;
    std::size_t field = 0;
};

std::optional<Entry> findEntry(const Tiff& tiff, std::uint32_t directory, std::uint32_t tag)
{
    const auto count = tiff.read16(directory);
    if (!count || *count > 1024) {
        return std::nullopt;
    }
    for (std::uint32_t i = 0; i < *count; ++i) {
        const std::size_t entry = directory + 2 + (12 * static_cast<std::size_t>(i));
        if (tiff.read16(entry) == std::optional<std::uint32_t>{tag}) {
            const auto type = tiff.read16(entry + 2);
            const auto n = tiff.read32(entry + 4);
            if (!type || !n) {
                return std::nullopt;
            }
            return Entry{*type, *n, entry + 8};
        }
    }
    return std::nullopt;
}

std::size_t valueAt(const Tiff& tiff, const Entry& e)
{
    if (typeSize(e.type) * e.count <= 4) {
        return e.field;
    }
    return tiff.read32(e.field).value_or(0);
}

std::string readText(const Tiff& tiff, std::uint32_t directory, std::uint32_t tag)
{
    const auto e = findEntry(tiff, directory, tag);
    if (!e || e->type != 2 || e->count > 256) {
        return {};
    }
    std::string text;
    const std::size_t at = valueAt(tiff, *e);
    for (std::uint32_t i = 0; i < e->count; ++i) {
        const auto c = tiff.byte(at + i);
        if (!c || *c == 0) {
            break;
        }
        text.push_back(static_cast<char>(*c));
    }
    while (!text.empty() && text.back() == ' ') { // cameras pad with spaces
        text.pop_back();
    }
    return text;
}

double readNumber(const Tiff& tiff, std::uint32_t directory, std::uint32_t tag)
{
    const auto e = findEntry(tiff, directory, tag);
    if (!e || e->count == 0) {
        return 0.0;
    }
    switch (e->type) {
    case 3: // short
        return tiff.read16(e->field).value_or(0);
    case 4: // long
        return tiff.read32(e->field).value_or(0);
    case 5: { // rational
        const std::size_t at = tiff.read32(e->field).value_or(0);
        const auto num = tiff.read32(at);
        const auto den = tiff.read32(at + 4);
        return num && den && *den != 0 ? static_cast<double>(*num) / static_cast<double>(*den) : 0.0;
    }
    default:
        return 0.0;
    }
}

// Degrees from a GPS coordinate (three rationals: degrees, minutes, seconds) and its reference
// ("N", "S", "E", "W"); nullopt when it is missing or out of `range`.
std::optional<double> readCoordinate(const Tiff& tiff, std::uint32_t directory, std::uint32_t tag,
                                     std::uint32_t refTag, double range)
{
    const auto e = findEntry(tiff, directory, tag);
    if (!e || e->type != 5 || e->count != 3) {
        return std::nullopt;
    }
    const std::size_t at = tiff.read32(e->field).value_or(0);
    double degrees = 0.0;
    double unit = 1.0;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto num = tiff.read32(at + (8 * i));
        const auto den = tiff.read32(at + (8 * i) + 4);
        if (!num || !den || *den == 0) {
            return std::nullopt;
        }
        degrees += static_cast<double>(*num) / static_cast<double>(*den) * unit;
        unit /= 60.0;
    }
    const std::string ref = readText(tiff, directory, refTag);
    if (ref == "S" || ref == "W") {
        degrees = -degrees;
    }
    if (degrees < -range || degrees > range) {
        return std::nullopt;
    }
    return degrees;
}

} // namespace

ExifSummary readExif(const std::vector<std::uint8_t>& exif)
{
    std::vector<std::uint8_t> bytes = exif;
    const Tiff tiff(bytes);
    ExifSummary out;
    const auto ifd0 = tiff.valid() ? tiff.read32(4) : std::nullopt;
    if (!ifd0) {
        return out;
    }
    out.make = readText(tiff, *ifd0, 0x010F);
    out.model = readText(tiff, *ifd0, 0x0110);
    out.dateTaken = readText(tiff, *ifd0, 0x0132); // DateTime, unless the Exif IFD has better
    const auto sub = findEntry(tiff, *ifd0, 0x8769);
    if (const auto at = sub ? tiff.read32(sub->field) : std::nullopt) {
        if (std::string original = readText(tiff, *at, 0x9003); !original.empty()) {
            out.dateTaken = std::move(original);
        }
        out.lens = readText(tiff, *at, 0xA434);
        out.iso = static_cast<int>(readNumber(tiff, *at, 0x8827));
        out.exposureTime = readNumber(tiff, *at, 0x829A);
        out.fNumber = readNumber(tiff, *at, 0x829D);
        out.focalLength = readNumber(tiff, *at, 0x920A);
    }
    const auto gps = findEntry(tiff, *ifd0, kGpsPointer);
    if (const auto at = gps ? tiff.read32(gps->field) : std::nullopt) {
        const auto latitude = readCoordinate(tiff, *at, 0x0002, 0x0001, 90.0);
        const auto longitude = readCoordinate(tiff, *at, 0x0004, 0x0003, 180.0);
        if (latitude && longitude && (*latitude != 0.0 || *longitude != 0.0)) {
            out.hasLocation = true;
            out.latitude = *latitude;
            out.longitude = *longitude;
        }
    }
    return out;
}

std::vector<std::uint8_t> exifWithoutLocation(std::vector<std::uint8_t> exif)
{
    Tiff tiff(exif);
    if (!tiff.valid()) {
        return exif;
    }
    const auto ifd0 = tiff.read32(4);
    const auto count = ifd0 ? tiff.read16(*ifd0) : std::nullopt;
    if (!count || *count > 1024) {
        return exif;
    }
    for (std::uint32_t i = 0; i < *count; ++i) {
        const std::size_t entry = *ifd0 + 2 + (12 * static_cast<std::size_t>(i));
        if (tiff.read16(entry) != std::optional<std::uint32_t>{kGpsPointer}) {
            continue;
        }
        if (const auto gps = tiff.read32(entry + 8)) {
            zeroGpsDirectory(tiff, *gps);
        }
        // Unlink: later entries and the next-directory pointer after them (the thumbnail's
        // IFD1) move up one slot, the freed bytes are zeroed, the count drops.
        const std::size_t end = *ifd0 + 2 + (12 * static_cast<std::size_t>(*count)) + 4;
        for (std::size_t at = entry; at + 12 < end; at += 2) {
            if (const auto v = tiff.read16(at + 12)) {
                tiff.write16(at, *v);
            }
        }
        tiff.zero(end - 12, 12);
        tiff.write16(*ifd0, *count - 1);
        break;
    }
    return exif;
}

namespace {

constexpr std::uint32_t kExifPointer = 0x8769;
constexpr std::uint32_t kInteropPointer = 0xA005;

// The end of the bytes `directory` and what it points to use, as an offset from the TIFF header.
std::size_t usedEnd(const Tiff& tiff, std::uint32_t directory, int depth)
{
    const auto count = tiff.read16(directory);
    if (!count || *count > 1024 || depth > 3) {
        return 0;
    }
    std::size_t end = directory + 2 + (12 * static_cast<std::size_t>(*count)) + 4;
    for (std::uint32_t i = 0; i < *count; ++i) {
        const std::size_t entry = directory + 2 + (12 * static_cast<std::size_t>(i));
        const auto tag = tiff.read16(entry);
        const auto type = tiff.read16(entry + 2);
        const auto n = tiff.read32(entry + 4);
        const auto value = tiff.read32(entry + 8);
        if (!tag || !type || !n || !value) {
            continue;
        }
        const std::size_t size = typeSize(*type) * *n;
        if (size > 4) {
            end = std::max(end, std::size_t{*value} + size);
        }
        if (*tag == kExifPointer || *tag == kGpsPointer || *tag == kInteropPointer) {
            end = std::max(end, usedEnd(tiff, *value, depth + 1));
        }
    }
    return end;
}

} // namespace

std::vector<std::uint8_t> exifWithoutThumbnail(std::vector<std::uint8_t> exif)
{
    Tiff tiff(exif);
    if (!tiff.valid()) {
        return exif;
    }
    const auto ifd0 = tiff.read32(4);
    const auto count = ifd0 ? tiff.read16(*ifd0) : std::nullopt;
    if (!count || *count > 1024) {
        return exif;
    }
    const std::size_t next = *ifd0 + 2 + (12 * static_cast<std::size_t>(*count));
    const auto ifd1 = tiff.read32(next);
    if (!ifd1 || *ifd1 == 0) {
        return exif;
    }
    tiff.write32(next, 0); // unlinked: IFD1 holds the thumbnail
    // Writers put the thumbnail last; then the block can end where the rest ends.
    const std::size_t end = usedEnd(tiff, *ifd0, 0);
    if (end > 8 && *ifd1 >= end && tiff.base() + end < exif.size()) {
        exif.resize(tiff.base() + end);
    }
    return exif;
}

std::vector<std::uint8_t> xmpWithoutLocation(const std::vector<std::uint8_t>& xmp)
{
    std::string text(xmp.begin(), xmp.end());
    // exif:GPS... as attributes (exif:GPSLatitude="...") and as elements, simple or nested.
    static const std::regex kAttribute(R"(\s+exif:GPS[A-Za-z]*\s*=\s*("[^"]*"|'[^']*'))");
    static const std::regex kElement(R"(<exif:(GPS[A-Za-z]*)\b[^>]*?(/>|>[\s\S]*?</exif:\1\s*>))");
    text = std::regex_replace(text, kAttribute, "");
    text = std::regex_replace(text, kElement, "");
    return {text.begin(), text.end()};
}

Metadata withoutLocation(const Metadata& metadata)
{
    return {exifWithoutLocation(metadata.exif), xmpWithoutLocation(metadata.xmp), metadata.iptc};
}

namespace detail {

std::vector<std::uint8_t> exifWithoutExtras(std::vector<std::uint8_t> exif)
{
    return exifWithoutThumbnail(exifWithoutLocation(std::move(exif)));
}

std::vector<std::uint8_t> xmpWithoutExtras(const std::vector<std::uint8_t>& xmp)
{
    const std::vector<std::uint8_t> located = xmpWithoutLocation(xmp);
    std::string text(located.begin(), located.end());
    static const std::regex kThumbnails(R"(<(xmp|xap):Thumbnails\b[\s\S]*?</\1:Thumbnails\s*>)");
    text = std::regex_replace(text, kThumbnails, "");
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> photoshopWithoutPreviews(const std::vector<std::uint8_t>& payload)
{
    // "Photoshop 3.0\0", then 8BIM resources: id, a Pascal name padded to even, a size, the
    // data padded to even. 0x0409 and 0x040C are thumbnails.
    constexpr std::size_t kHeader = 14;
    if (payload.size() < kHeader) {
        return payload;
    }
    std::vector<std::uint8_t> out(payload.begin(), payload.begin() + kHeader);
    std::size_t at = kHeader;
    while (at < payload.size()) {
        if (at + 7 > payload.size() || !std::equal(payload.begin() + static_cast<std::ptrdiff_t>(at),
                                                   payload.begin() + static_cast<std::ptrdiff_t>(at + 4),
                                                   std::string_view("8BIM").begin())) {
            return payload;
        }
        const std::uint32_t id = (std::uint32_t{payload[at + 4]} << 8U) | payload[at + 5];
        std::size_t name = 1 + payload[at + 6];
        name += name % 2;
        const std::size_t sizeAt = at + 6 + name;
        if (sizeAt + 4 > payload.size()) {
            return payload;
        }
        std::size_t size = (std::size_t{payload[sizeAt]} << 24U) | (std::size_t{payload[sizeAt + 1]} << 16U) |
                           (std::size_t{payload[sizeAt + 2]} << 8U) | payload[sizeAt + 3];
        size += size % 2;
        const std::size_t end = sizeAt + 4 + size;
        if (end > payload.size() + 1) {
            return payload;
        }
        if (id != 0x0409 && id != 0x040C) {
            out.insert(out.end(), payload.begin() + static_cast<std::ptrdiff_t>(at),
                       payload.begin() + static_cast<std::ptrdiff_t>(std::min(end, payload.size())));
        }
        at = end;
    }
    return out;
}

} // namespace detail

} // namespace pfx
