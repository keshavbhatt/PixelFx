// SPDX-License-Identifier: Apache-2.0
// A PNG writer on zlib (ADR-010): the container, the five row filters and the palette chunks are
// small enough to own, which keeps every chunk of the original in its required place.
#include "io/compress_internal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <future>
#include <limits>
#include <utility>
#include <zlib.h>

namespace pfx::detail {

namespace {

constexpr std::array<std::uint8_t, 8> kSignature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

void put32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>(v >> 24U));
    out.push_back(static_cast<std::uint8_t>((v >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
}

std::uint32_t get32(const std::uint8_t* p)
{
    return (std::uint32_t{p[0]} << 24U) | (std::uint32_t{p[1]} << 16U) | (std::uint32_t{p[2]} << 8U) |
           std::uint32_t{p[3]};
}

void writeChunk(std::vector<std::uint8_t>& out, const std::string& type, const std::uint8_t* data,
                std::size_t size)
{
    put32(out, static_cast<std::uint32_t>(size));
    const std::size_t start = out.size();
    out.insert(out.end(), type.begin(), type.begin() + 4);
    if (size > 0) {
        out.insert(out.end(), data, data + size);
    }
    const uLong crc = crc32(0L, out.data() + start, static_cast<uInt>(out.size() - start));
    put32(out, static_cast<std::uint32_t>(crc));
}

int channelsOf(int colourType)
{
    switch (colourType) {
    case 0:
    case 3:
        return 1;
    case 2:
        return 3;
    case 4:
        return 2;
    default:
        return 4;
    }
}

std::size_t rowBytes(const PngRaster& r)
{
    const auto bits = static_cast<std::size_t>(r.width) * static_cast<std::size_t>(channelsOf(r.colourType)) *
                      static_cast<std::size_t>(r.bitDepth);
    return (bits + 7) / 8;
}

// The rows as the format stores them (indexes packed below 8 bits), without filter bytes.
std::vector<std::uint8_t> packRows(const PngRaster& r)
{
    const std::size_t stride = rowBytes(r);
    if (r.colourType != 3 || r.bitDepth == 8) {
        return r.samples;
    }
    std::vector<std::uint8_t> out(stride * static_cast<std::size_t>(r.height), 0);
    const auto perByte = static_cast<std::size_t>(8 / r.bitDepth);
    const auto width = static_cast<std::size_t>(r.width);
    for (std::size_t y = 0; std::cmp_less(y, r.height); ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto shift =
                static_cast<unsigned>(8 - (static_cast<std::size_t>(r.bitDepth) * ((x % perByte) + 1)));
            out[(y * stride) + (x / perByte)] |=
                static_cast<std::uint8_t>(r.samples[(y * width) + x] << shift);
        }
    }
    return out;
}

std::uint8_t paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) {
        return static_cast<std::uint8_t>(a);
    }
    return static_cast<std::uint8_t>(pb <= pc ? b : c);
}

void filterRow(int type, const std::uint8_t* row, const std::uint8_t* prev, std::size_t size, std::size_t bpp,
               std::uint8_t* out)
{
    for (std::size_t i = 0; i < size; ++i) {
        const int a = i >= bpp ? row[i - bpp] : 0;
        const int b = prev != nullptr ? prev[i] : 0;
        const int c = (i >= bpp && prev != nullptr) ? prev[i - bpp] : 0;
        int predicted = 0;
        switch (type) {
        case 1:
            predicted = a;
            break;
        case 2:
            predicted = b;
            break;
        case 3:
            predicted = (a + b) / 2;
            break;
        case 4:
            predicted = paeth(a, b, c);
            break;
        default:
            break;
        }
        out[i] = static_cast<std::uint8_t>(row[i] - predicted);
    }
}

// Filtered rows, each with its filter byte: one filter for all, or per row the one whose output
// has the smallest sum of absolute values (libpng's heuristic). `adaptive` false means None.
std::vector<std::uint8_t> filterRows(const std::vector<std::uint8_t>& packed, const PngRaster& r,
                                     bool adaptive)
{
    const std::size_t stride = rowBytes(r);
    const std::size_t bpp =
        std::max<std::size_t>(1, static_cast<std::size_t>(channelsOf(r.colourType) * r.bitDepth) / 8);
    std::vector<std::uint8_t> out((stride + 1) * static_cast<std::size_t>(r.height));
    std::vector<std::uint8_t> trial(stride);
    for (std::size_t y = 0; std::cmp_less(y, r.height); ++y) {
        const std::uint8_t* row = packed.data() + (y * stride);
        const std::uint8_t* prev = y > 0 ? row - stride : nullptr;
        std::uint8_t* dst = out.data() + (y * (stride + 1));
        if (!adaptive) {
            dst[0] = 0;
            std::memcpy(dst + 1, row, stride);
            continue;
        }
        std::size_t bestScore = std::numeric_limits<std::size_t>::max();
        for (int type = 0; type < 5; ++type) {
            filterRow(type, row, prev, stride, bpp, trial.data());
            std::size_t score = 0;
            for (const std::uint8_t v : trial) {
                score += static_cast<std::size_t>(std::abs(static_cast<int>(static_cast<std::int8_t>(v))));
            }
            if (score < bestScore) {
                bestScore = score;
                dst[0] = static_cast<std::uint8_t>(type);
                std::memcpy(dst + 1, trial.data(), stride);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> deflateAll(const std::vector<std::uint8_t>& data, int strategy)
{
    z_stream z{};
    if (deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, 15, 9, strategy) != Z_OK) {
        return {};
    }
    std::vector<std::uint8_t> out(deflateBound(&z, static_cast<uLong>(data.size())));
    // zlib's interface is not const-correct; it only reads the input.
    z.next_in = const_cast<Bytef*>(data.data()); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    z.avail_in = static_cast<uInt>(data.size());
    z.next_out = out.data();
    z.avail_out = static_cast<uInt>(out.size());
    const int result = deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    return result == Z_STREAM_END ? out : std::vector<std::uint8_t>{};
}

// Puts entries with alpha below 255 first, so tRNS can stop at the last of them.
void orderPalette(PngRaster& r)
{
    std::vector<std::size_t> order(r.palette.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::stable_sort(order, [&r](std::size_t x, std::size_t y) {
        return (r.palette[x][3] < 255) && (r.palette[y][3] == 255);
    });
    std::vector<std::uint8_t> where(r.palette.size());
    std::vector<std::array<std::uint8_t, 4>> palette(r.palette.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        where[order[i]] = static_cast<std::uint8_t>(i);
        palette[i] = r.palette[order[i]];
    }
    for (std::uint8_t& index : r.samples) {
        index = where[index];
    }
    r.palette = std::move(palette);
}

// PLTE, and tRNS up to the last entry that is not opaque (orderPalette put those first).
void writePalette(std::vector<std::uint8_t>& out, const std::vector<std::array<std::uint8_t, 4>>& palette)
{
    std::vector<std::uint8_t> plte;
    std::vector<std::uint8_t> trns;
    plte.reserve(palette.size() * 3);
    for (const auto& entry : palette) {
        plte.insert(plte.end(), {entry[0], entry[1], entry[2]});
        if (entry[3] < 255) {
            trns.push_back(entry[3]);
        }
    }
    writeChunk(out, "PLTE", plte.data(), plte.size());
    if (!trns.empty()) {
        writeChunk(out, "tRNS", trns.data(), trns.size());
    }
}

// The smallest image data of the filter and deflate `trials`, each run on its own thread.
std::vector<std::uint8_t> smallestStream(const std::vector<std::uint8_t>& packed, const PngRaster& raster,
                                         const std::vector<std::pair<bool, int>>& trials)
{
    std::vector<std::future<std::vector<std::uint8_t>>> runs;
    runs.reserve(trials.size());
    for (const auto& [adaptive, strategy] : trials) {
        runs.push_back(std::async(std::launch::async, [&packed, &raster, adaptive, strategy] {
            return deflateAll(filterRows(packed, raster, adaptive), strategy);
        }));
    }
    std::vector<std::uint8_t> best;
    for (auto& run : runs) {
        std::vector<std::uint8_t> stream = run.get();
        if (!stream.empty() && (best.empty() || stream.size() < best.size())) {
            best = std::move(stream);
        }
    }
    return best;
}

} // namespace

std::optional<std::vector<PngChunk>> readPngChunks(const std::vector<std::uint8_t>& file)
{
    if (file.size() < kSignature.size() || !std::equal(kSignature.begin(), kSignature.end(), file.begin())) {
        return std::nullopt;
    }
    std::vector<PngChunk> chunks;
    std::size_t at = kSignature.size();
    while (at + 12 <= file.size()) {
        const std::size_t size = get32(file.data() + at);
        if (size > file.size() - at - 12) {
            return std::nullopt;
        }
        PngChunk chunk;
        chunk.type.assign(file.begin() + static_cast<std::ptrdiff_t>(at + 4),
                          file.begin() + static_cast<std::ptrdiff_t>(at + 8));
        chunk.data.assign(file.begin() + static_cast<std::ptrdiff_t>(at + 8),
                          file.begin() + static_cast<std::ptrdiff_t>(at + 8 + size));
        at += 12 + size;
        const bool end = chunk.type == "IEND";
        chunks.push_back(std::move(chunk));
        if (end) {
            return chunks;
        }
    }
    return std::nullopt;
}

PngRaster paletteRaster(const IndexedImage& image)
{
    PngRaster r;
    r.width = image.width;
    r.height = image.height;
    r.colourType = 3;
    const std::size_t n = image.palette.size();
    r.bitDepth = 8;
    for (const int bits : {4, 2, 1}) {
        if (n <= (std::size_t{1} << static_cast<unsigned>(bits))) {
            r.bitDepth = bits; // the smallest that holds every entry
        }
    }
    r.samples = image.indexes;
    r.palette = image.palette;
    return r;
}

std::vector<std::uint8_t> writePng(PngRaster raster, const std::vector<PngChunk>& early,
                                   const std::vector<PngChunk>& late, bool thorough)
{
    if (raster.colourType == 3) {
        orderPalette(raster);
    }
    const std::vector<std::uint8_t> packed = packRows(raster);
    // Palettes and low bit depths usually do best unfiltered, photos with adaptive filters.
    const bool paletted = raster.colourType == 3 || raster.bitDepth < 8;
    std::vector<std::pair<bool, int>> trials = {{!paletted, Z_DEFAULT_STRATEGY}};
    if (thorough) {
        trials.emplace_back(paletted, Z_DEFAULT_STRATEGY);
        trials.emplace_back(!paletted, Z_FILTERED);
    }
    const std::vector<std::uint8_t> best = smallestStream(packed, raster, trials);

    std::vector<std::uint8_t> out(kSignature.begin(), kSignature.end());
    std::vector<std::uint8_t> header;
    put32(header, static_cast<std::uint32_t>(raster.width));
    put32(header, static_cast<std::uint32_t>(raster.height));
    header.push_back(static_cast<std::uint8_t>(raster.bitDepth));
    header.push_back(static_cast<std::uint8_t>(raster.colourType));
    header.insert(header.end(), {0, 0, 0}); // deflate, adaptive filtering, not interlaced
    writeChunk(out, "IHDR", header.data(), header.size());
    for (const PngChunk& c : early) {
        writeChunk(out, c.type, c.data.data(), c.data.size());
    }
    if (raster.colourType == 3) {
        writePalette(out, raster.palette);
    }
    for (const PngChunk& c : late) {
        writeChunk(out, c.type, c.data.data(), c.data.size());
    }
    constexpr std::size_t kIdatSize = std::size_t{1} << 20U;
    for (std::size_t at = 0; at < best.size(); at += kIdatSize) {
        writeChunk(out, "IDAT", best.data() + at, std::min(kIdatSize, best.size() - at));
    }
    writeChunk(out, "IEND", nullptr, 0);
    return out;
}

std::optional<std::vector<std::uint8_t>> inflateAll(const std::uint8_t* data, std::size_t size)
{
    z_stream z{};
    if (inflateInit(&z) != Z_OK) {
        return std::nullopt;
    }
    z.next_in = const_cast<Bytef*>(data); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    z.avail_in = static_cast<uInt>(size);
    std::vector<std::uint8_t> out;
    std::array<std::uint8_t, 16384> buffer{};
    int result = Z_OK;
    while (result == Z_OK) {
        z.next_out = buffer.data();
        z.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&z, Z_NO_FLUSH);
        out.insert(out.end(), buffer.begin(),
                   buffer.begin() + static_cast<std::ptrdiff_t>(buffer.size() - z.avail_out));
        constexpr std::size_t kLimit = std::size_t{64} << 20U; // a text chunk, not a bomb
        if (out.size() > kLimit) {
            result = Z_DATA_ERROR;
        }
    }
    inflateEnd(&z);
    if (result != Z_STREAM_END) {
        return std::nullopt;
    }
    return out;
}

} // namespace pfx::detail
