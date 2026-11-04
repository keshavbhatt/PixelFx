// SPDX-License-Identifier: Apache-2.0
#include "fixtures.h"
#include "pfx/color.h"
#include "pfx/lut.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <sstream>
#include <string>

namespace {

// A .cube of size n whose colours are f(r, g, b) at the lattice points, red fastest.
template <typename F>
std::string cube(int n, F f, const std::string& header = {})
{
    std::ostringstream out;
    out << "# made by the test\nTITLE \"test\"\n" << header << "LUT_3D_SIZE " << n << "\n";
    for (int b = 0; b < n; ++b) {
        for (int g = 0; g < n; ++g) {
            for (int r = 0; r < n; ++r) {
                const auto c = f(r / double(n - 1), g / double(n - 1), b / double(n - 1));
                out << c[0] << ' ' << c[1] << ' ' << c[2] << '\n';
            }
        }
    }
    return out.str();
}

std::array<double, 3> identity(double r, double g, double b)
{
    return {r, g, b};
}

} // namespace

TEST_CASE("an identity cube leaves the photo as it was")
{
    const auto lut = pfx::parseCube(cube(17, identity));
    REQUIRE(lut.ok());
    CHECK(lut.value().title == "test");
    CHECK(lut.value().size == 17);
    // Colours inside sRGB (a LUT cannot hold what lies outside it) and below white.
    pfx::Image image(64, 48);
    const pfx::color::Mat3& m = pfx::color::kSrgbToRec2020;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::array<double, 3> srgb = {0.9 * x / 63.0, 0.9 * y / 47.0, 0.45 * (x + y) / 110.0};
            float* p = image.pixel(x, y);
            for (std::size_t c = 0; c < 3; ++c) {
                p[c] = static_cast<float>((m.at(c)[0] * srgb[0]) + (m.at(c)[1] * srgb[1]) +
                                          (m.at(c)[2] * srgb[2]));
            }
            p[3] = 1.0F;
        }
    }
    const pfx::Image before = image;
    pfx::applyLut(image, lut.value());
    for (int y = 0; y < image.height(); y += 7) {
        for (int x = 0; x < image.width(); x += 5) {
            for (int c = 0; c < 3; ++c) {
                CHECK(image.pixel(x, y)[c] ==
                      doctest::Approx(before.pixel(x, y)[c]).epsilon(2e-3).scale(0.01));
            }
        }
    }
}

TEST_CASE("tetrahedral lookup is exact on the lattice and on a linear cube anywhere")
{
    const auto f = [](double r, double g, double b) {
        return std::array<double, 3>{1 - r, (g + b) / 2, r * 0.5};
    };
    const auto lut = pfx::parseCube(cube(5, f));
    REQUIRE(lut.ok());
    for (const auto& in :
         {std::array<float, 3>{0.25F, 0.5F, 1.0F}, {0.1F, 0.73F, 0.42F}, {0.9F, 0.05F, 0.6F}}) {
        const auto out = lut.value().lookup(in[0], in[1], in[2]);
        const auto want = f(in[0], in[1], in[2]);
        for (std::size_t c = 0; c < 3; ++c) {
            CHECK(out.at(c) == doctest::Approx(want.at(c)).epsilon(1e-5));
        }
    }
}

TEST_CASE("the domain maps inputs onto the table, and mix blends in linear light")
{
    // A domain of 0 to 2: an input of 1 lands on the middle of the table.
    const auto lut = pfx::parseCube(cube(3, identity, "DOMAIN_MIN 0 0 0\nDOMAIN_MAX 2 2 2\n"));
    REQUIRE(lut.ok());
    CHECK(lut.value().lookup(1.0F, 1.0F, 1.0F)[0] == doctest::Approx(0.5F));
    CHECK(lut.value().lookup(3.0F, -1.0F, 0.0F)[0] == doctest::Approx(1.0F)); // clamped
    CHECK(lut.value().lookup(3.0F, -1.0F, 0.0F)[1] == doctest::Approx(0.0F));

    // Black for everything at mix 0.5: half the light.
    const auto black =
        pfx::parseCube(cube(2, [](double, double, double) { return std::array<double, 3>{}; }));
    REQUIRE(black.ok());
    pfx::Image image(2, 1);
    image.fill(0.5F, 0.5F, 0.5F, 0.7F);
    pfx::applyLut(image, black.value(), 0.5F);
    CHECK(image.pixel(0, 0)[1] == doctest::Approx(0.25F).epsilon(1e-3));
    CHECK(image.pixel(0, 0)[3] == doctest::Approx(0.7F)); // alpha kept
}

TEST_CASE("a 1D table is a curve per channel")
{
    const auto lut = pfx::parseCube("LUT_1D_SIZE 3\n0 1 0\n0.5 0.5 0.25\n1 0 1\n");
    REQUIRE(lut.ok());
    CHECK(lut.value().kind == pfx::Lut::Kind::OneD);
    const auto out = lut.value().lookup(0.75F, 0.25F, 0.5F);
    CHECK(out[0] == doctest::Approx(0.75F)); // identity in red
    CHECK(out[1] == doctest::Approx(0.75F)); // inverted in green
    CHECK(out[2] == doctest::Approx(0.25F)); // the middle of blue's curve
}

TEST_CASE("bad cubes fail with the line")
{
    CHECK_FALSE(pfx::parseCube("").ok());
    CHECK_FALSE(pfx::parseCube("LUT_3D_SIZE 2\n0 0 0\n").ok()); // too few
    CHECK_FALSE(pfx::parseCube("LUT_3D_SIZE 1\n0 0 0\n").ok());
    CHECK_FALSE(pfx::parseCube("LUT_3D_SIZE 300\n").ok());
    CHECK_FALSE(pfx::parseCube("0 0 0\nLUT_3D_SIZE 2\n").ok()); // a colour before the size
    const auto bad = pfx::parseCube("LUT_1D_SIZE 2\n0 0 0\n0 0 x\n");
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().message.find("Line 3") != std::string::npos);
    CHECK_FALSE(pfx::parseCube("LUT_1D_SIZE 2\n0 0 0\n1 1 1\n1 1 1\n").ok()); // too many
    CHECK_FALSE(pfx::parseCube("LUT_1D_SIZE 2\nDOMAIN_MIN 1 1 1\nDOMAIN_MAX 1 1 1\n0 0 0\n1 1 1\n").ok());
    CHECK_FALSE(pfx::parseCube("LUT_1D_SIZE 2\nFOO 1\n0 0 0\n1 1 1\n").ok());
    CHECK_FALSE(pfx::parseCube("LUT_1D_SIZE 2\n0 0 nan\n1 1 1\n").ok());
    CHECK(pfx::parseCube("LUT_1D_SIZE 2\nLUT_1D_INPUT_RANGE 0 4\n0 0 0\n1 1 1\n").value().domainMax[2] ==
          4.0F);
}

TEST_CASE("a HaldCLUT picture becomes a 3D table")
{
    // An identity Hald of level 2: 8 by 8 pixels, a cube of 4 per edge, red fastest.
    pfx::Image hald(8, 8);
    const pfx::color::Mat3& m = pfx::color::kSrgbToRec2020;
    for (int i = 0; i < 64; ++i) {
        const std::array<double, 3> e = {(i % 4) / 3.0, ((i / 4) % 4) / 3.0, (i / 16) / 3.0};
        std::array<double, 3> lin{};
        for (std::size_t c = 0; c < 3; ++c) {
            lin.at(c) = pfx::color::srgbDecode(e.at(c));
        }
        float* p = hald.pixel(i % 8, i / 8);
        for (std::size_t c = 0; c < 3; ++c) {
            p[c] = static_cast<float>((m.at(c)[0] * lin[0]) + (m.at(c)[1] * lin[1]) + (m.at(c)[2] * lin[2]));
        }
        p[3] = 1.0F;
    }
    const auto lut = pfx::lutFromHald(hald);
    REQUIRE(lut.ok());
    CHECK(lut.value().size == 4);
    const auto out = lut.value().lookup(0.2F, 0.6F, 0.9F);
    CHECK(out[0] == doctest::Approx(0.2F).epsilon(1e-3));
    CHECK(out[1] == doctest::Approx(0.6F).epsilon(1e-3));
    CHECK(out[2] == doctest::Approx(0.9F).epsilon(1e-3));
    CHECK_FALSE(pfx::lutFromHald(pfx::Image(10, 10)).ok());
}
