// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pfx/image.h"
#include "pfx/result.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pfx {

// A colour lookup table as a .cube file holds it (FEATURES L5, presets.md section 3): 1D (one
// curve per channel) or 3D (a cube of colours). Inputs are mapped from [domainMin, domainMax]
// onto the table; entries are R, G, B triples, red changing fastest.
struct Lut
{
    enum class Kind
    {
        OneD,
        ThreeD,
    };
    Kind kind = Kind::ThreeD;
    int size = 0; // entries per channel (1D) or per edge (3D)
    std::array<float, 3> domainMin{0.0F, 0.0F, 0.0F};
    std::array<float, 3> domainMax{1.0F, 1.0F, 1.0F};
    std::vector<float> table; // size (1D) or size^3 (3D) triples
    std::string title;
    bool operator==(const Lut&) const = default;

    // The table's colour for an input already in its encoding, interpolated (tetrahedral in
    // 3D, linear in 1D); inputs outside the domain are clamped to it.
    [[nodiscard]] std::array<float, 3> lookup(float r, float g, float b) const;
};

// Reads the text of a .cube file (Resolve / Adobe flavour): TITLE, LUT_1D_SIZE or LUT_3D_SIZE
// (3D: 2 to 256 per edge; 1D: 2 to 65536), DOMAIN_MIN, DOMAIN_MAX, # comments, then the rows.
// A wrong row count, a stray keyword or a number that does not parse fails with the line.
[[nodiscard]] Result<Lut> parseCube(std::string_view text);
[[nodiscard]] Result<Lut> loadCube(const std::filesystem::path& path);

// A HaldCLUT picture (an identity Hald of level L is L^3 by L^3 pixels, a cube of L^2 per
// edge, read as 8-bit sRGB values) as a 3D table.
[[nodiscard]] Result<Lut> lutFromHald(const Image& hald);

// Applies the table to the image as display LUTs expect: the colour is taken to sRGB
// primaries with the sRGB curve, looked up, and brought back to linear Rec.2020; `mix` (0 to
// 1) blends the result with the input in linear light. Highlights above white are clamped to
// the table's domain, as in every LUT. Alpha is kept.
void applyLut(Image& image, const Lut& lut, float mix = 1.0F, int threads = 0);

} // namespace pfx
