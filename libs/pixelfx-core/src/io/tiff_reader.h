// SPDX-License-Identifier: Apache-2.0
// Reading TIFF structures (DNG and the camera RAW files built on TIFF): directories, their
// entries and typed values, without decoding any image data.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pfx::detail {

struct TiffEntry
{
    std::uint16_t type = 0;
    std::uint32_t count = 0;
    std::size_t offset = 0; // where the value is (in the entry itself when it fits in four bytes)
};

using TiffDirectory = std::map<std::uint16_t, TiffEntry>;

class TiffReader
{
public:
    explicit TiffReader(const std::vector<std::uint8_t>& file);

    [[nodiscard]] bool valid() const { return m_valid; }
    [[nodiscard]] bool littleEndian() const { return m_little; }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return m_file; }
    /// IFD0, its SubIFDs and the IFDs chained after it, in that order.
    [[nodiscard]] const std::vector<TiffDirectory>& directories() const { return m_directories; }
    /// The directory a SubIFD or Exif pointer leads to.
    [[nodiscard]] std::optional<TiffDirectory> directoryAt(std::size_t offset) const;

    [[nodiscard]] std::optional<std::uint32_t> u32(const TiffDirectory& d, std::uint16_t tag,
                                                   std::size_t i = 0) const;
    [[nodiscard]] std::vector<std::uint32_t> u32s(const TiffDirectory& d, std::uint16_t tag) const;
    /// RATIONAL, SRATIONAL, FLOAT, DOUBLE, SHORT or LONG values as doubles.
    [[nodiscard]] std::vector<double> reals(const TiffDirectory& d, std::uint16_t tag) const;
    [[nodiscard]] std::string text(const TiffDirectory& d, std::uint16_t tag) const;
    /// BYTE or UNDEFINED data, as stored.
    [[nodiscard]] std::vector<std::uint8_t> data(const TiffDirectory& d, std::uint16_t tag) const;

    [[nodiscard]] std::uint16_t read16(std::size_t at) const;
    [[nodiscard]] std::uint32_t read32(std::size_t at) const;
    [[nodiscard]] float readFloat(std::size_t at) const;
    [[nodiscard]] double readDouble(std::size_t at) const;

    [[nodiscard]] static std::size_t typeSize(std::uint16_t type);

private:
    void walk(std::size_t offset, int depth);

    const std::vector<std::uint8_t>& m_file;
    bool m_little = true;
    bool m_valid = false;
    std::vector<TiffDirectory> m_directories;
};

} // namespace pfx::detail
