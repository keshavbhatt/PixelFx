// SPDX-License-Identifier: Apache-2.0
// WebP's RIFF container: which kind of image it holds, and its metadata chunks cleaned without
// touching the image data (ADR-010).
#include "io/compress_internal.h"

#include <algorithm>
#include <string_view>

namespace pfx::detail {

namespace {

struct Chunk
{
    std::string fourcc;
    std::vector<std::uint8_t> data;
};

std::uint32_t get32le(const std::uint8_t* p)
{
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8U) | (std::uint32_t{p[2]} << 16U) |
           (std::uint32_t{p[3]} << 24U);
}

void put32le(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFU));
    }
}

std::optional<std::vector<Chunk>> readChunks(const std::vector<std::uint8_t>& file)
{
    if (file.size() < 12 || !std::equal(file.begin(), file.begin() + 4, std::string_view("RIFF").begin()) ||
        !std::equal(file.begin() + 8, file.begin() + 12, std::string_view("WEBP").begin())) {
        return std::nullopt;
    }
    std::vector<Chunk> chunks;
    std::size_t at = 12;
    while (at + 8 <= file.size()) {
        const std::size_t size = get32le(file.data() + at + 4);
        if (size > file.size() - at - 8) {
            return std::nullopt;
        }
        Chunk c;
        c.fourcc.assign(file.begin() + static_cast<std::ptrdiff_t>(at),
                        file.begin() + static_cast<std::ptrdiff_t>(at + 4));
        c.data.assign(file.begin() + static_cast<std::ptrdiff_t>(at + 8),
                      file.begin() + static_cast<std::ptrdiff_t>(at + 8 + size));
        chunks.push_back(std::move(c));
        at += 8 + size + (size % 2);
    }
    return chunks;
}

} // namespace

std::optional<WebpKind> webpKind(const std::vector<std::uint8_t>& file)
{
    const auto chunks = readChunks(file);
    if (!chunks) {
        return std::nullopt;
    }
    WebpKind kind;
    for (const Chunk& c : *chunks) {
        kind.lossless = kind.lossless || c.fourcc == "VP8L";
        kind.animated = kind.animated || c.fourcc == "ANIM";
    }
    return kind;
}

std::optional<std::vector<std::uint8_t>> webpWithoutExtras(const std::vector<std::uint8_t>& file)
{
    auto chunks = readChunks(file);
    if (!chunks) {
        return std::nullopt;
    }
    constexpr std::uint8_t kExifFlag = 0x08;
    constexpr std::uint8_t kXmpFlag = 0x04;
    std::uint8_t dropped = 0;
    std::vector<Chunk> kept;
    for (Chunk& c : *chunks) {
        if (c.fourcc == "EXIF") {
            c.data = exifWithoutExtras(std::move(c.data));
        } else if (c.fourcc == "XMP ") {
            c.data = xmpWithoutExtras(c.data);
        }
        if ((c.fourcc == "EXIF" || c.fourcc == "XMP ") && c.data.empty()) {
            dropped |= c.fourcc == "EXIF" ? kExifFlag : kXmpFlag;
            continue;
        }
        kept.push_back(std::move(c));
    }
    std::vector<std::uint8_t> body = {'W', 'E', 'B', 'P'};
    for (Chunk& c : kept) {
        if (c.fourcc == "VP8X" && !c.data.empty()) {
            c.data[0] = static_cast<std::uint8_t>(c.data[0] & static_cast<std::uint8_t>(~dropped));
        }
        body.insert(body.end(), c.fourcc.begin(), c.fourcc.end());
        put32le(body, static_cast<std::uint32_t>(c.data.size()));
        body.insert(body.end(), c.data.begin(), c.data.end());
        if (c.data.size() % 2 != 0) {
            body.push_back(0);
        }
    }
    std::vector<std::uint8_t> out = {'R', 'I', 'F', 'F'};
    put32le(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

} // namespace pfx::detail
