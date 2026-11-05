// SPDX-License-Identifier: Apache-2.0
#include "io/tiff_reader.h"

#include <bit>
#include <cstring>
#include <set>

namespace pfx::detail {

namespace {

constexpr std::uint16_t kSubIfds = 330;
constexpr int kMaxDepth = 4;
constexpr std::size_t kMaxEntries = 4096;

} // namespace

TiffReader::TiffReader(const std::vector<std::uint8_t>& file)
    : m_file(file)
{
    if (file.size() < 8) {
        return;
    }
    if (file[0] == 'I' && file[1] == 'I') {
        m_little = true;
    } else if (file[0] == 'M' && file[1] == 'M') {
        m_little = false;
    } else {
        return;
    }
    if (read16(2) != 42) {
        return;
    }
    m_valid = true;
    walk(read32(4), 0);
}

std::uint16_t TiffReader::read16(std::size_t at) const
{
    if (at + 2 > m_file.size()) {
        return 0;
    }
    const unsigned a = m_file[at];
    const unsigned b = m_file[at + 1];
    return static_cast<std::uint16_t>(m_little ? (a | (b << 8U)) : ((a << 8U) | b));
}

std::uint32_t TiffReader::read32(std::size_t at) const
{
    const std::uint32_t a = read16(at);
    const std::uint32_t b = read16(at + 2);
    return m_little ? (a | (b << 16U)) : ((a << 16U) | b);
}

float TiffReader::readFloat(std::size_t at) const
{
    return std::bit_cast<float>(read32(at));
}

double TiffReader::readDouble(std::size_t at) const
{
    const std::uint64_t a = read32(at);
    const std::uint64_t b = read32(at + 4);
    return std::bit_cast<double>(m_little ? (a | (b << 32U)) : ((a << 32U) | b));
}

std::size_t TiffReader::typeSize(std::uint16_t type)
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
    case 13:
        return 4;
    case 5:
    case 10:
    case 12:
    case 16:
        return 8;
    default:
        return 0;
    }
}

std::optional<TiffDirectory> TiffReader::directoryAt(std::size_t offset) const
{
    if (offset + 2 > m_file.size()) {
        return std::nullopt;
    }
    const std::size_t count = read16(offset);
    if (count == 0 || count > kMaxEntries || offset + 2 + (count * 12) > m_file.size()) {
        return std::nullopt;
    }
    TiffDirectory d;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = offset + 2 + (i * 12);
        TiffEntry e;
        e.type = read16(at + 2);
        e.count = read32(at + 4);
        const std::size_t size = typeSize(e.type) * e.count;
        e.offset = size <= 4 ? at + 8 : read32(at + 8);
        if (size > 4 && (e.offset > m_file.size() || size > m_file.size() - e.offset)) {
            continue; // a value outside the file
        }
        d[read16(at)] = e;
    }
    return d;
}

void TiffReader::walk(std::size_t offset, int depth)
{
    std::set<std::size_t> seen;
    while (offset != 0 && depth <= kMaxDepth && seen.insert(offset).second) {
        auto d = directoryAt(offset);
        if (!d) {
            return;
        }
        m_directories.push_back(*d);
        for (const std::uint32_t sub : u32s(*d, kSubIfds)) {
            walk(sub, depth + 1);
        }
        const std::size_t next = offset + 2 + (std::size_t{read16(offset)} * 12);
        offset = depth == 0 ? read32(next) : 0;
    }
}

std::optional<std::uint32_t> TiffReader::u32(const TiffDirectory& d, std::uint16_t tag, std::size_t i) const
{
    const auto values = u32s(d, tag);
    if (i >= values.size()) {
        return std::nullopt;
    }
    return values[i];
}

std::vector<std::uint32_t> TiffReader::u32s(const TiffDirectory& d, std::uint16_t tag) const
{
    const auto it = d.find(tag);
    if (it == d.end()) {
        return {};
    }
    const TiffEntry& e = it->second;
    std::vector<std::uint32_t> out;
    out.reserve(e.count);
    for (std::uint32_t i = 0; i < e.count; ++i) {
        if (e.type == 3) {
            out.push_back(read16(e.offset + (2 * std::size_t{i})));
        } else if (e.type == 4 || e.type == 13) {
            out.push_back(read32(e.offset + (4 * std::size_t{i})));
        } else if (e.type == 1) {
            out.push_back(m_file[e.offset + i]);
        }
    }
    return out;
}

std::vector<double> TiffReader::reals(const TiffDirectory& d, std::uint16_t tag) const
{
    const auto it = d.find(tag);
    if (it == d.end()) {
        return {};
    }
    const TiffEntry& e = it->second;
    std::vector<double> out;
    out.reserve(e.count);
    for (std::uint32_t i = 0; i < e.count; ++i) {
        const std::size_t at = e.offset + (typeSize(e.type) * i);
        switch (e.type) {
        case 5: {
            const std::uint32_t den = read32(at + 4);
            out.push_back(den == 0 ? 0.0 : static_cast<double>(read32(at)) / den);
            break;
        }
        case 10: {
            const auto den = static_cast<std::int32_t>(read32(at + 4));
            out.push_back(den == 0 ? 0.0 : static_cast<double>(static_cast<std::int32_t>(read32(at))) / den);
            break;
        }
        case 11:
            out.push_back(double{readFloat(at)});
            break;
        case 12:
            out.push_back(readDouble(at));
            break;
        case 3:
            out.push_back(read16(at));
            break;
        case 4:
            out.push_back(read32(at));
            break;
        default:
            return out;
        }
    }
    return out;
}

std::string TiffReader::text(const TiffDirectory& d, std::uint16_t tag) const
{
    const auto it = d.find(tag);
    if (it == d.end() || it->second.type != 2) {
        return {};
    }
    const auto* start = reinterpret_cast<const char*>(m_file.data() + it->second.offset); // NOLINT
    std::string s(start, it->second.count);
    s.erase(s.find('\0') == std::string::npos ? s.size() : s.find('\0'));
    return s;
}

std::vector<std::uint8_t> TiffReader::data(const TiffDirectory& d, std::uint16_t tag) const
{
    const auto it = d.find(tag);
    if (it == d.end()) {
        return {};
    }
    const std::size_t size = typeSize(it->second.type) * it->second.count;
    const auto first = m_file.begin() + static_cast<std::ptrdiff_t>(it->second.offset);
    return {first, first + static_cast<std::ptrdiff_t>(size)};
}

} // namespace pfx::detail
