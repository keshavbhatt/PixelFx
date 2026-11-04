// SPDX-License-Identifier: Apache-2.0
#include "pfx/lut.h"

#include "core/parallel.h"
#include "ops/value_table.h"
#include "pfx/color.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>

namespace pfx {

namespace {

constexpr std::size_t kMaxFileBytes =
    std::size_t{64} * 1024 * 1024; // a 256^3 cube is about 400 MB of text: refused

Error parseError(int line, const std::string& what)
{
    return {ErrorCode::ParseError, "Line " + std::to_string(line) + " of the LUT: " + what};
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

// The exponent after an "e": an optional sign and digits. The characters used, or 0.
std::size_t parseExponent(std::string_view s, int& out)
{
    std::size_t j = 0;
    const bool down = j < s.size() && s[j] == '-';
    if (j < s.size() && (s[j] == '-' || s[j] == '+')) {
        ++j;
    }
    const std::size_t start = j;
    int exponent = 0;
    for (; j < s.size() && s[j] >= '0' && s[j] <= '9'; ++j) {
        exponent = std::min(1000, (exponent * 10) + (s[j] - '0'));
    }
    out = down ? -exponent : exponent;
    return j == start ? 0 : j;
}

// One decimal number at the start of `s` ("-0.5", "1e-3", ".25"), whatever the locale: Apple's
// standard library has no floating-point from_chars. The characters used, or 0 if none.
std::size_t parseNumber(std::string_view s, double& out)
{
    std::size_t i = 0;
    const bool negative = i < s.size() && s[i] == '-';
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
        ++i;
    }
    double value = 0.0;
    int digits = 0;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i, ++digits) {
        value = (value * 10.0) + (s[i] - '0');
    }
    if (i < s.size() && s[i] == '.') {
        double scale = 0.1;
        for (++i; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i, ++digits, scale /= 10.0) {
            value += (s[i] - '0') * scale;
        }
    }
    if (digits == 0) {
        return 0;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        int exponent = 0;
        const std::size_t used = parseExponent(s.substr(i + 1), exponent);
        if (used == 0) {
            return 0;
        }
        value *= std::pow(10.0, exponent);
        i += 1 + used;
    }
    out = negative ? -value : value;
    return i;
}

// Up to `count` numbers separated by spaces or tabs; false if there are more or one is bad.
bool readNumbers(std::string_view s, float* out, int count)
{
    int n = 0;
    while (true) {
        s = trim(s);
        if (s.empty()) {
            return n == count;
        }
        if (n == count) {
            return false;
        }
        double value = 0.0;
        const std::size_t used = parseNumber(s, value);
        const bool separated = used == s.size() || s[used] == ' ' || s[used] == '\t';
        if (used == 0 || !separated || !std::isfinite(value) || std::abs(value) > 1e30) {
            return false;
        }
        out[n++] = static_cast<float>(value); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        s.remove_prefix(used);
    }
}

} // namespace

namespace {

std::array<float, 3> lookup1d(const Lut& lut, const std::array<float, 3>& in)
{
    std::array<float, 3> out{};
    const auto last = static_cast<std::size_t>(lut.size - 2);
    for (std::size_t c = 0; c < 3; ++c) {
        const auto i = std::min(static_cast<std::size_t>(in.at(c)), last);
        const float t = in.at(c) - static_cast<float>(i);
        const float a = lut.table[(i * 3) + c];
        const float z = lut.table[((i + 1) * 3) + c];
        out.at(c) = a + (t * (z - a));
    }
    return out;
}

// Tetrahedral: the cell's diagonal splits it into six tetrahedra; the ordering of the
// fractions picks one, and the colour is a blend of its four corners.
std::array<float, 3> lookup3d(const Lut& lut, const std::array<float, 3>& in)
{
    const auto n = static_cast<std::size_t>(lut.size);
    const auto cell = [n](float v) { return std::min(static_cast<std::size_t>(v), n - 2); };
    const std::size_t x = cell(in[0]);
    const std::size_t y = cell(in[1]);
    const std::size_t z = cell(in[2]);
    const float fx = in[0] - static_cast<float>(x);
    const float fy = in[1] - static_cast<float>(y);
    const float fz = in[2] - static_cast<float>(z);
    const auto at = [&](std::size_t dx, std::size_t dy, std::size_t dz) {
        return &lut.table[(((z + dz) * n * n) + ((y + dy) * n) + (x + dx)) * 3];
    };
    // The two middle corners and the weights, per ordering of fx, fy, fz.
    struct Tetra
    {
        const float* a;
        const float* b;
        std::array<float, 4> w;
    };
    Tetra t{};
    if (fx >= fy && fy >= fz) {
        t = {at(1, 0, 0), at(1, 1, 0), {1 - fx, fx - fy, fy - fz, fz}};
    } else if (fx >= fy && fx >= fz) {
        t = {at(1, 0, 0), at(1, 0, 1), {1 - fx, fx - fz, fz - fy, fy}};
    } else if (fx >= fy) {
        t = {at(0, 0, 1), at(1, 0, 1), {1 - fz, fz - fx, fx - fy, fy}};
    } else if (fz >= fy) {
        t = {at(0, 0, 1), at(0, 1, 1), {1 - fz, fz - fy, fy - fx, fx}};
    } else if (fz >= fx) {
        t = {at(0, 1, 0), at(0, 1, 1), {1 - fy, fy - fz, fz - fx, fx}};
    } else {
        t = {at(0, 1, 0), at(1, 1, 0), {1 - fy, fy - fx, fx - fz, fz}};
    }
    const float* c000 = at(0, 0, 0);
    const float* c111 = at(1, 1, 1);
    std::array<float, 3> out{};
    for (std::size_t c = 0; c < 3; ++c) {
        out.at(c) = (t.w[0] * c000[c]) + (t.w[1] * t.a[c]) + (t.w[2] * t.b[c]) + (t.w[3] * c111[c]);
    }
    return out;
}

} // namespace

std::array<float, 3> Lut::lookup(float r, float g, float b) const
{
    std::array<float, 3> in{r, g, b};
    const auto last = static_cast<float>(size - 1);
    for (std::size_t c = 0; c < 3; ++c) {
        const float span = domainMax.at(c) - domainMin.at(c);
        const float t = (in.at(c) - domainMin.at(c)) / span;
        in.at(c) = (std::isnan(t) ? 0.0F : std::clamp(t, 0.0F, 1.0F)) * last; // NaN must not reach a cast
    }
    return kind == Kind::OneD ? lookup1d(*this, in) : lookup3d(*this, in);
}

namespace {

// One .cube, line by line: keywords set the header, number rows fill the table.
class CubeParser
{
public:
    // An error for the line, or nothing.
    std::optional<std::string> line(std::string_view text)
    {
        const char first = text.front();
        if ((first >= '0' && first <= '9') || first == '-' || first == '.' || first == '+') {
            return row(first == '+' ? text.substr(1) : text);
        }
        if (text.starts_with("TITLE")) {
            std::string_view title = trim(text.substr(5));
            if (title.size() >= 2 && title.front() == '"' && title.back() == '"') {
                title = title.substr(1, title.size() - 2);
            }
            m_lut.title = std::string(title.substr(0, 256));
            return std::nullopt;
        }
        if (text.starts_with("LUT_3D_SIZE") || text.starts_with("LUT_1D_SIZE")) {
            return sizeLine(text.starts_with("LUT_3D_SIZE"), text.substr(11));
        }
        if (text.starts_with("DOMAIN_MIN") || text.starts_with("DOMAIN_MAX")) {
            auto& target = text.starts_with("DOMAIN_MIN") ? m_lut.domainMin : m_lut.domainMax;
            return readNumbers(text.substr(10), target.data(), 3)
                       ? std::nullopt
                       : std::optional<std::string>("expected three numbers.");
        }
        if (text.starts_with("LUT_1D_INPUT_RANGE") || text.starts_with("LUT_3D_INPUT_RANGE")) {
            std::array<float, 2> range{};
            if (!readNumbers(text.substr(18), range.data(), 2)) {
                return "expected two numbers.";
            }
            m_lut.domainMin.fill(range[0]);
            m_lut.domainMax.fill(range[1]);
            return std::nullopt;
        }
        return "an unknown keyword.";
    }

    Result<Lut> finish() &&
    {
        if (m_expected == 0) {
            return Error{ErrorCode::ParseError, "The LUT has no LUT_3D_SIZE or LUT_1D_SIZE."};
        }
        if (m_lut.table.size() != m_expected * 3) {
            return Error{ErrorCode::ParseError, "The LUT has fewer colours than its size says."};
        }
        for (std::size_t c = 0; c < 3; ++c) {
            if (!(m_lut.domainMax.at(c) > m_lut.domainMin.at(c))) {
                return Error{ErrorCode::ParseError, "The LUT's DOMAIN_MAX is not above its DOMAIN_MIN."};
            }
        }
        return std::move(m_lut);
    }

private:
    std::optional<std::string> row(std::string_view text)
    {
        if (m_expected == 0) {
            return "a colour before the LUT size.";
        }
        std::array<float, 3> rgb{};
        if (!readNumbers(text, rgb.data(), 3)) {
            return "expected three numbers.";
        }
        if (m_lut.table.size() >= m_expected * 3) {
            return "more colours than the size says.";
        }
        m_lut.table.insert(m_lut.table.end(), rgb.begin(), rgb.end());
        return std::nullopt;
    }

    std::optional<std::string> sizeLine(bool three, std::string_view text)
    {
        float size = 0;
        if (m_expected != 0 || !readNumbers(text, &size, 1)) {
            return "a bad or second size.";
        }
        const float limit = three ? 256.0F : 65536.0F;
        if (size < 2 || size > limit || size != std::floor(size)) {
            return "a size out of range.";
        }
        m_lut.kind = three ? Lut::Kind::ThreeD : Lut::Kind::OneD;
        m_lut.size = static_cast<int>(size);
        const auto n = static_cast<std::size_t>(m_lut.size);
        m_expected = three ? n * n * n : n;
        m_lut.table.reserve(m_expected * 3);
        return std::nullopt;
    }

    Lut m_lut;
    std::size_t m_expected = 0;
};

} // namespace

Result<Lut> parseCube(std::string_view text)
{
    if (text.size() > kMaxFileBytes) {
        return Error{ErrorCode::InvalidArgument, "The LUT file is too large."};
    }
    CubeParser parser;
    int lineNumber = 0;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = trim(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        ++lineNumber;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (auto error = parser.line(line)) {
            return parseError(lineNumber, *error);
        }
    }
    return std::move(parser).finish();
}

Result<Lut> loadCube(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        return Error{ErrorCode::NotFound, "The LUT file could not be read."};
    }
    if (bytes > kMaxFileBytes) {
        return Error{ErrorCode::InvalidArgument, "The LUT file is too large."};
    }
    const std::ifstream in(path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        return Error{ErrorCode::Io, "The LUT file could not be read."};
    }
    return parseCube(buffer.str());
}

Result<Lut> lutFromHald(const Image& hald)
{
    // Level L: an L^3 square, L^2 entries per edge.
    int level = 2;
    while (level * level * level < hald.width()) {
        ++level;
    }
    const int side = level * level * level;
    if (hald.width() != side || hald.height() != side || level > 16) {
        return Error{ErrorCode::InvalidArgument,
                     "This picture is not a HaldCLUT (a square of a cube's size)."};
    }
    Lut lut;
    lut.kind = Lut::Kind::ThreeD;
    lut.size = level * level;
    lut.table.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 3);
    const color::Mat3& m = color::kRec2020ToSrgb;
    // Row by row, red fastest: the same order as a .cube. Values back to the sRGB encoding the
    // Hald was made in.
    for (int y = 0; y < side; ++y) {
        const float* p = hald.row(y);
        for (int x = 0; x < side; ++x, p += 4) {
            for (std::size_t c = 0; c < 3; ++c) {
                const double linear = (m.at(c)[0] * static_cast<double>(p[0])) +
                                      (m.at(c)[1] * static_cast<double>(p[1])) +
                                      (m.at(c)[2] * static_cast<double>(p[2]));
                lut.table.push_back(static_cast<float>(color::srgbEncode(std::clamp(linear, 0.0, 1.0))));
            }
        }
    }
    return lut;
}

void applyLut(Image& image, const Lut& lut, float mix, int threads)
{
    mix = std::clamp(mix, 0.0F, 1.0F);
    if (mix <= 0.0F || lut.size < 2) {
        return;
    }
    static const detail::ValueTable encode([](double x) { return color::srgbEncode(x); });
    static const detail::ValueTable decode([](double x) { return color::srgbDecode(x); });
    const color::Mat3& toSrgb = color::kRec2020ToSrgb;
    const color::Mat3& toRec2020 = color::kSrgbToRec2020;
    detail::parallelFor(image.height(), threads, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* p = image.row(y);
            for (int x = 0; x < image.width(); ++x, p += 4) {
                std::array<float, 3> e{};
                for (std::size_t c = 0; c < 3; ++c) {
                    const double linear = (toSrgb.at(c)[0] * static_cast<double>(p[0])) +
                                          (toSrgb.at(c)[1] * static_cast<double>(p[1])) +
                                          (toSrgb.at(c)[2] * static_cast<double>(p[2]));
                    e.at(c) = static_cast<float>(encode(std::max(0.0, linear)));
                }
                const std::array<float, 3> out = lut.lookup(e[0], e[1], e[2]);
                std::array<double, 3> linear{};
                for (std::size_t c = 0; c < 3; ++c) {
                    linear.at(c) = decode(static_cast<double>(out.at(c)));
                }
                for (std::size_t c = 0; c < 3; ++c) {
                    const auto value = static_cast<float>((toRec2020.at(c)[0] * linear[0]) +
                                                          (toRec2020.at(c)[1] * linear[1]) +
                                                          (toRec2020.at(c)[2] * linear[2]));
                    p[c] += mix * (value - p[c]);
                }
            }
        }
    });
}

} // namespace pfx
