// SPDX-License-Identifier: Apache-2.0
// Removing location from metadata without a metadata library (ADR-003): the Exif GPS block is
// unlinked and zeroed, and XMP's exif:GPS properties are dropped. Anything malformed is left as
// it is rather than guessed at.
#include "pfx/io.h"

#include <array>
#include <cstddef>
#include <optional>
#include <regex>
#include <string>

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

} // namespace

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

} // namespace pfx
