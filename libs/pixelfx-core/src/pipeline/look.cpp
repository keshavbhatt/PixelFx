// SPDX-License-Identifier: Apache-2.0
#include "pfx/look.h"

#include "core/parallel.h"
#include "ops/ops.h"
#include "pfx/color.h"
#include "pfx/registry.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace pfx {

void applyLook(Image& image, const Look& look, float strength, int threads)
{
    strength = std::clamp(strength, 0.0F, 1.0F);
    if (strength <= 0.0F || look.steps.empty()) {
        return;
    }
    const Image before = strength < 1.0F ? image : Image();
    const detail::OpContext ctx{threads};
    for (const LookStep& step : look.steps) {
        if (!step.enabled) {
            continue;
        }
        if (step.op == "lut") {
            const auto it = look.resources.find(step.params.text("resource", ""));
            if (it != look.resources.end()) {
                applyLut(image, it->second, static_cast<float>(step.params.number("mix", 100.0) / 100.0),
                         threads);
            }
            continue;
        }
        if (step.op != "geometry") {
            detail::applyOperation(step.op, image, step.params, ctx);
        }
    }
    if (strength < 1.0F) {
        // Linear light: a half-strength look is half the change in light, not in encoded values.
        const float* in = before.data();
        float* out = image.data();
        detail::parallelFor(image.height(), threads, [&](int y0, int y1) {
            const auto from = static_cast<std::size_t>(y0) * static_cast<std::size_t>(image.width()) * 4;
            const auto to = static_cast<std::size_t>(y1) * static_cast<std::size_t>(image.width()) * 4;
            for (std::size_t i = from; i < to; ++i) {
                out[i] =
                    in[i] +
                    (strength * (out[i] - in[i])); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            }
        });
    }
}

namespace {

// Every sRGB-encoded colour of an n-point grid, in linear Rec.2020: red along x within a block,
// blue picks the block, green along y.
Image gridImage(std::size_t n)
{
    Image grid(static_cast<int>(n * n), static_cast<int>(n));
    const color::Mat3& m = color::kSrgbToRec2020;
    const auto level = [n](std::size_t i) {
        return color::srgbDecode(static_cast<double>(i) / static_cast<double>(n - 1));
    };
    for (std::size_t b = 0; b < n; ++b) {
        for (std::size_t g = 0; g < n; ++g) {
            for (std::size_t r = 0; r < n; ++r) {
                const std::array<double, 3> lin = {level(r), level(g), level(b)};
                float* p = grid.pixel(static_cast<int>((b * n) + r), static_cast<int>(g));
                for (std::size_t c = 0; c < 3; ++c) {
                    p[c] = static_cast<float>((m.at(c)[0] * lin[0]) + (m.at(c)[1] * lin[1]) +
                                              (m.at(c)[2] * lin[2]));
                }
                p[3] = 1.0F;
            }
        }
    }
    return grid;
}

// The grid after the look, back in sRGB encoding, as a .cube table (red fastest).
std::vector<float> tableFrom(const Image& grid, std::size_t n)
{
    std::vector<float> table(n * n * n * 3);
    const color::Mat3& m = color::kRec2020ToSrgb;
    for (std::size_t b = 0; b < n; ++b) {
        for (std::size_t g = 0; g < n; ++g) {
            for (std::size_t r = 0; r < n; ++r) {
                const float* p = grid.pixel(static_cast<int>((b * n) + r), static_cast<int>(g));
                const std::size_t at = ((b * n * n) + (g * n) + r) * 3;
                for (std::size_t c = 0; c < 3; ++c) {
                    const double linear = (m.at(c)[0] * static_cast<double>(p[0])) +
                                          (m.at(c)[1] * static_cast<double>(p[1])) +
                                          (m.at(c)[2] * static_cast<double>(p[2]));
                    table[at + c] =
                        static_cast<float>(std::clamp(color::srgbEncode(std::max(0.0, linear)), 0.0, 1.0));
                }
            }
        }
    }
    return table;
}

} // namespace

BakedLook bakeLook(const Look& look, int size)
{
    BakedLook out;
    const auto n = static_cast<std::size_t>(std::clamp(size, 2, 129));
    // Only the colour-only steps go into the table.
    Look colourOnly;
    colourOnly.resources = look.resources;
    for (const LookStep& step : look.steps) {
        const OpSpec* spec = registry().op(step.op);
        if (!step.enabled) {
            continue;
        }
        if (step.op == "lut" || (spec != nullptr && spec->pointwise)) {
            colourOnly.steps.push_back(step);
        } else {
            out.leftOut.push_back(step.op);
        }
    }
    Image grid = gridImage(n);
    applyLook(grid, colourOnly, 1.0F);
    out.lut.kind = Lut::Kind::ThreeD;
    out.lut.size = static_cast<int>(n);
    out.lut.table = tableFrom(grid, n);
    return out;
}

std::string cubeText(const Lut& lut)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (!lut.title.empty()) {
        std::string title = lut.title;
        std::ranges::replace(title, '"', '\'');
        out << "TITLE \"" << title << "\"\n";
    }
    out << (lut.kind == Lut::Kind::ThreeD ? "LUT_3D_SIZE " : "LUT_1D_SIZE ") << lut.size << "\n";
    out << "DOMAIN_MIN " << lut.domainMin[0] << ' ' << lut.domainMin[1] << ' ' << lut.domainMin[2] << "\n";
    out << "DOMAIN_MAX " << lut.domainMax[0] << ' ' << lut.domainMax[1] << ' ' << lut.domainMax[2] << "\n";
    out.setf(std::ios::fixed);
    out.precision(6);
    for (std::size_t i = 0; i + 2 < lut.table.size(); i += 3) {
        out << lut.table[i] << ' ' << lut.table[i + 1] << ' ' << lut.table[i + 2] << '\n';
    }
    return out.str();
}

} // namespace pfx
