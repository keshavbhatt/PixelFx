// SPDX-License-Identifier: Apache-2.0
// A GIF89a writer (ADR-010). Frames are stored without loss: a frame keeps only the rectangle
// that changed, unchanged pixels inside it transparent, when no pixel turns transparent along
// the way; otherwise every frame is whole and clears the one before it.
#include "io/compress_internal.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace pfx::detail {

namespace {

constexpr std::int64_t kTransparent = -1;

std::uint32_t rgbOf(const std::uint8_t* p)
{
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8U) | (std::uint32_t{p[2]} << 16U);
}

void put16(std::vector<std::uint8_t>& out, int value)
{
    const auto v = static_cast<std::uint32_t>(value);
    out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
}

// 2 to the power `bits`.
int powerOfTwo(int bits)
{
    return static_cast<int>(1U << static_cast<unsigned>(bits));
}

// Packed GIF fields: an 8-bit value built from flags and small numbers.
std::uint8_t packed(std::initializer_list<int> parts)
{
    unsigned v = 0;
    for (const int p : parts) {
        v |= static_cast<unsigned>(p);
    }
    return static_cast<std::uint8_t>(v & 0xFFU);
}

class BitWriter
{
public:
    explicit BitWriter(std::vector<std::uint8_t>& out)
        : m_out(out)
    {}

    void put(int symbol, int bits)
    {
        m_bits |= static_cast<std::uint32_t>(symbol) << static_cast<unsigned>(m_count);
        m_count += bits;
        while (m_count >= 8) {
            m_out.push_back(static_cast<std::uint8_t>(m_bits & 0xFFU));
            m_bits >>= 8U;
            m_count -= 8;
        }
    }

    void flush()
    {
        if (m_count > 0) {
            m_out.push_back(static_cast<std::uint8_t>(m_bits & 0xFFU));
        }
        m_bits = 0;
        m_count = 0;
    }

private:
    std::vector<std::uint8_t>& m_out;
    std::uint32_t m_bits = 0;
    int m_count = 0;
};

// The LZW dictionary: (prefix code, next index) to code, cleared by a new generation.
class Dictionary
{
public:
    Dictionary()
        : m_slots(kSize)
    {}

    void clear() { ++m_generation; }

    [[nodiscard]] int find(int prefix, int next) const
    {
        const std::uint32_t key = keyOf(prefix, next);
        for (std::size_t i = hash(key);; i = (i + 1) & (kSize - 1)) {
            const Slot& s = m_slots[i];
            if (s.generation != m_generation) {
                return -1;
            }
            if (s.key == key) {
                return s.code;
            }
        }
    }

    void add(int prefix, int next, int code)
    {
        const std::uint32_t key = keyOf(prefix, next);
        std::size_t i = hash(key);
        while (m_slots[i].generation == m_generation) {
            i = (i + 1) & (kSize - 1);
        }
        m_slots[i] = {key, code, m_generation};
    }

private:
    static constexpr std::size_t kSize = 16384; // over twice the 4096 codes, so probes stay short

    struct Slot
    {
        std::uint32_t key = 0;
        int code = 0;
        std::uint32_t generation = 0;
    };

    static std::uint32_t keyOf(int prefix, int next)
    {
        return (static_cast<std::uint32_t>(prefix) << 8U) | static_cast<std::uint32_t>(next);
    }

    static std::size_t hash(std::uint32_t key) { return (key * 2654435761U) >> 18U; }

    std::vector<Slot> m_slots;
    std::uint32_t m_generation = 1;
};

// GIF's LZW, as the GIF89a specification and the common decoders expect it.
std::vector<std::uint8_t> lzw(const std::vector<std::uint8_t>& indexes, int minCodeSize)
{
    const int clearCode = powerOfTwo(minCodeSize);
    std::vector<std::uint8_t> data;
    BitWriter bits(data);
    Dictionary dictionary;
    int codeSize = minCodeSize + 1;
    int maxCode = clearCode + 1;
    bits.put(clearCode, codeSize);
    int current = -1;
    for (const std::uint8_t value : indexes) {
        if (current < 0) {
            current = value;
            continue;
        }
        if (const int found = dictionary.find(current, value); found >= 0) {
            current = found;
            continue;
        }
        bits.put(current, codeSize);
        dictionary.add(current, value, ++maxCode);
        if (maxCode >= powerOfTwo(codeSize)) {
            ++codeSize;
        }
        if (maxCode == 4095) {
            bits.put(clearCode, codeSize);
            dictionary.clear();
            codeSize = minCodeSize + 1;
            maxCode = clearCode + 1;
        }
        current = value;
    }
    if (current >= 0) {
        bits.put(current, codeSize);
    }
    bits.put(clearCode, codeSize);
    bits.put(clearCode + 1, minCodeSize + 1);
    bits.flush();
    // Sub-blocks of at most 255 bytes, then an empty one.
    std::vector<std::uint8_t> blocks;
    for (std::size_t at = 0; at < data.size(); at += 255) {
        const std::size_t n = std::min<std::size_t>(255, data.size() - at);
        blocks.push_back(static_cast<std::uint8_t>(n));
        blocks.insert(blocks.end(), data.begin() + static_cast<std::ptrdiff_t>(at),
                      data.begin() + static_cast<std::ptrdiff_t>(at + n));
    }
    blocks.push_back(0);
    return blocks;
}

// One frame to store: a rectangle of colours (or kTransparent).
struct Frame
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::vector<std::int64_t> pixels; // RGB packed, or kTransparent
    int delay = 0;                    // centiseconds
    int disposal = 0;
};

Frame wholeFrame(const Rgba8Image& image, int disposal)
{
    Frame f;
    f.width = image.width;
    f.height = image.height;
    f.disposal = disposal;
    f.pixels.reserve(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));
    for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
        const std::uint8_t* p = image.pixels.data() + i;
        f.pixels.push_back(p[3] == 0 ? kTransparent : rgbOf(p));
    }
    return f;
}

bool samePixel(const std::uint8_t* a, const std::uint8_t* b)
{
    return (a[3] == 0 && b[3] == 0) || std::equal(a, a + 4, b);
}

// What changed from `before` to `after`, unchanged pixels transparent; empty when nothing did.
std::optional<Frame> changedFrame(const Rgba8Image& before, const Rgba8Image& after)
{
    int x0 = after.width;
    int y0 = after.height;
    int x1 = -1;
    int y1 = -1;
    const auto at = [&after](int x, int y) {
        return ((static_cast<std::size_t>(y) * static_cast<std::size_t>(after.width)) +
                static_cast<std::size_t>(x)) *
               4;
    };
    for (int y = 0; y < after.height; ++y) {
        for (int x = 0; x < after.width; ++x) {
            if (!samePixel(before.pixels.data() + at(x, y), after.pixels.data() + at(x, y))) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    }
    if (x1 < 0) {
        return std::nullopt;
    }
    Frame f;
    f.x = x0;
    f.y = y0;
    f.width = x1 - x0 + 1;
    f.height = y1 - y0 + 1;
    f.disposal = 1; // leave it: the next frame draws over it
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const std::uint8_t* a = before.pixels.data() + at(x, y);
            const std::uint8_t* b = after.pixels.data() + at(x, y);
            f.pixels.push_back(samePixel(a, b) ? kTransparent : rgbOf(b));
        }
    }
    return f;
}

// Whether frames can store only their changes: no pixel may turn transparent (a transparent
// pixel in a frame means "unchanged").
bool changesOnly(const GifAnimation& a)
{
    for (std::size_t i = 1; i < a.frames.size(); ++i) {
        const auto& before = a.frames[i - 1].pixels;
        const auto& after = a.frames[i].pixels;
        for (std::size_t p = 3; p < after.size(); p += 4) {
            if (before[p] != 0 && after[p] == 0) {
                return false;
            }
        }
    }
    return true;
}

struct Palette
{
    std::vector<std::uint32_t> colours;
    std::unordered_map<std::uint32_t, std::uint8_t> index;
    bool transparent = false;

    [[nodiscard]] int size() const { return static_cast<int>(colours.size()) + (transparent ? 1 : 0); }
    [[nodiscard]] int transparentIndex() const { return static_cast<int>(colours.size()); }
    // Bits for the colour table: at least 1, the table holds 2^bits entries.
    [[nodiscard]] int bits() const
    {
        int b = 1;
        while (powerOfTwo(b) < size()) {
            ++b;
        }
        return b;
    }
};

void collect(Palette& palette, const Frame& f)
{
    for (const std::int64_t p : f.pixels) {
        if (p == kTransparent) {
            palette.transparent = true;
        } else if (!palette.index.contains(static_cast<std::uint32_t>(p))) {
            palette.index.emplace(static_cast<std::uint32_t>(p),
                                  static_cast<std::uint8_t>(palette.colours.size()));
            palette.colours.push_back(static_cast<std::uint32_t>(p));
        }
        if (palette.size() > 257) {
            return; // already too many; the caller only needs to know that
        }
    }
}

void writeTable(std::vector<std::uint8_t>& out, const Palette& palette)
{
    const int entries = powerOfTwo(palette.bits());
    for (int i = 0; i < entries; ++i) {
        const std::uint32_t c =
            std::cmp_less(i, palette.colours.size()) ? palette.colours[static_cast<std::size_t>(i)] : 0U;
        out.insert(out.end(),
                   {static_cast<std::uint8_t>(c & 0xFFU), static_cast<std::uint8_t>((c >> 8U) & 0xFFU),
                    static_cast<std::uint8_t>((c >> 16U) & 0xFFU)});
    }
}

void writeFrame(std::vector<std::uint8_t>& out, const Frame& f, const Palette& palette, bool local,
                bool animated)
{
    if (animated || palette.transparent) {
        out.insert(out.end(), {0x21, 0xF9, 0x04});
        out.push_back(packed({f.disposal * 4, palette.transparent ? 1 : 0}));
        put16(out, f.delay);
        out.push_back(static_cast<std::uint8_t>(palette.transparent ? palette.transparentIndex() : 0));
        out.push_back(0);
    }
    out.push_back(0x2C);
    put16(out, f.x);
    put16(out, f.y);
    put16(out, f.width);
    put16(out, f.height);
    out.push_back(local ? packed({0x80, palette.bits() - 1}) : std::uint8_t{0});
    if (local) {
        writeTable(out, palette);
    }
    std::vector<std::uint8_t> indexes;
    indexes.reserve(f.pixels.size());
    for (const std::int64_t p : f.pixels) {
        indexes.push_back(p == kTransparent ? static_cast<std::uint8_t>(palette.transparentIndex())
                                            : palette.index.at(static_cast<std::uint32_t>(p)));
    }
    const int minCodeSize = std::max(2, palette.bits());
    out.push_back(static_cast<std::uint8_t>(minCodeSize));
    const std::vector<std::uint8_t> data = lzw(indexes, minCodeSize);
    out.insert(out.end(), data.begin(), data.end());
}

std::vector<Frame> framesOf(const GifAnimation& a)
{
    const bool animated = a.frames.size() > 1;
    const bool diff = animated && changesOnly(a);
    std::vector<Frame> frames;
    for (std::size_t i = 0; i < a.frames.size(); ++i) {
        const int delay = i < a.delays.size() ? (a.delays[i] + 5) / 10 : 10;
        if (i == 0 || !diff) {
            // Whole frames: left in place when the next stores its changes, else cleared.
            int disposal = animated ? 2 : 0;
            if (diff) {
                disposal = 1;
            }
            frames.push_back(wholeFrame(a.frames[i], disposal));
            frames.back().delay = delay;
            continue;
        }
        if (auto f = changedFrame(a.frames[i - 1], a.frames[i])) {
            f->delay = delay;
            frames.push_back(std::move(*f));
        } else {
            frames.back().delay += delay; // nothing changed: the previous frame shows longer
        }
    }
    return frames;
}

} // namespace

std::optional<std::vector<std::uint8_t>> writeGif(const GifAnimation& animation)
{
    if (animation.frames.empty()) {
        return std::nullopt;
    }
    for (const Rgba8Image& f : animation.frames) {
        for (std::size_t p = 3; p < f.pixels.size(); p += 4) {
            if (f.pixels[p] != 0 && f.pixels[p] != 255) {
                return std::nullopt; // GIF has no partial transparency
            }
        }
    }
    const std::vector<Frame> frames = framesOf(animation);
    Palette global;
    for (const Frame& f : frames) {
        collect(global, f);
    }
    const bool useGlobal = global.size() <= 256;
    std::vector<Palette> locals;
    if (!useGlobal) {
        for (const Frame& f : frames) {
            Palette p;
            collect(p, f);
            if (p.size() > 256) {
                return std::nullopt;
            }
            locals.push_back(std::move(p));
        }
    }

    std::vector<std::uint8_t> out = {'G', 'I', 'F', '8', '9', 'a'};
    put16(out, animation.width);
    put16(out, animation.height);
    out.push_back(useGlobal ? packed({0x80, 0x70, global.bits() - 1}) : std::uint8_t{0x70});
    out.insert(out.end(), {0, 0}); // background, aspect
    if (useGlobal) {
        writeTable(out, global);
    }
    const bool animated = animation.frames.size() > 1;
    if (animated && animation.loop >= 0) {
        const std::string app = "NETSCAPE2.0";
        out.insert(out.end(), {0x21, 0xFF, 0x0B});
        out.insert(out.end(), app.begin(), app.end());
        out.insert(out.end(), {0x03, 0x01});
        put16(out, animation.loop);
        out.push_back(0);
    }
    for (std::size_t i = 0; i < frames.size(); ++i) {
        writeFrame(out, frames[i], useGlobal ? global : locals[i], !useGlobal, animated);
    }
    out.push_back(0x3B);
    return out;
}

} // namespace pfx::detail
