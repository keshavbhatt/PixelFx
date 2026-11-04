// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>

namespace pfx::color {

using Mat3 = std::array<std::array<double, 3>, 3>;
using Vec3 = std::array<double, 3>;

// Luminance weights of Rec.2020 (the working space).
inline constexpr Vec3 kRec2020Luma = {0.2627, 0.6780, 0.0593};

// Rec.2020 linear RGB <-> CIE XYZ, D65.
extern const Mat3 kRec2020ToXyz;
extern const Mat3 kXyzToRec2020;
// Linear sRGB / Rec.709 -> linear Rec.2020, and back.
extern const Mat3 kSrgbToRec2020;
extern const Mat3 kRec2020ToSrgb;

[[nodiscard]] double luminance(double r, double g, double b);
// Pixels are stored as float; this overload widens them explicitly.
[[nodiscard]] double luminance(float r, float g, float b);

// sRGB transfer function, extended linearly beyond [0, 1] so values with headroom survive.
[[nodiscard]] double srgbEncode(double linear);
[[nodiscard]] double srgbDecode(double encoded);

// The perceptual encoding tone operations work in: a pure power of 1/2.2, extended for x > 1.
[[nodiscard]] double perceptualEncode(double linear);
[[nodiscard]] double perceptualDecode(double encoded);

[[nodiscard]] Vec3 transform(const Mat3& m, const Vec3& v) noexcept;
[[nodiscard]] Mat3 multiply(const Mat3& a, const Mat3& b) noexcept;
[[nodiscard]] Mat3 inverse(const Mat3& m) noexcept;

// CIE xy chromaticity of a Planckian radiator, 1667 K to 25000 K (Kang et al. 2002).
[[nodiscard]] std::array<double, 2> planckianXy(double kelvin);
// Bradford chromatic adaptation in XYZ from one white (xy) to another.
[[nodiscard]] Mat3 bradford(const std::array<double, 2>& fromXy, const std::array<double, 2>& toXy);

} // namespace pfx::color
