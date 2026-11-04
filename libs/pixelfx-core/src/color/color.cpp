// SPDX-License-Identifier: Apache-2.0
#include "pfx/color.h"

#include <algorithm>
#include <cmath>

namespace pfx::color {

const Mat3 kRec2020ToXyz = {{
    {0.636958048301291, 0.144616903586208, 0.168880975164172},
    {0.262700212011267, 0.677998071518871, 0.059301716469862},
    {0.000000000000000, 0.028072693049087, 1.060985057710791},
}};
const Mat3 kXyzToRec2020 = inverse(kRec2020ToXyz);

const Mat3 kSrgbToRec2020 = {{
    {0.627403895934699, 0.329283038377884, 0.043313065687417},
    {0.069097289358232, 0.919540395075459, 0.011362315566309},
    {0.016391438875150, 0.088013307877226, 0.895595253247624},
}};
const Mat3 kRec2020ToSrgb = inverse(kSrgbToRec2020);

double luminance(double r, double g, double b)
{
    return kRec2020Luma[0] * r + kRec2020Luma[1] * g + kRec2020Luma[2] * b;
}

double luminance(float r, float g, float b)
{
    return luminance(double{r}, double{g}, double{b});
}

double srgbEncode(double linear)
{
    if (linear <= 0.0031308) {
        return 12.92 * linear;
    }
    if (linear > 1.0) {
        // Continue with the slope at 1.0 so values with headroom stay ordered.
        constexpr double kSlopeAtOne = 1.055 / 2.4;
        return 1.0 + (linear - 1.0) * kSlopeAtOne;
    }
    return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

double srgbDecode(double encoded)
{
    if (encoded <= 0.04045) {
        return encoded / 12.92;
    }
    if (encoded > 1.0) {
        constexpr double kSlopeAtOne = 2.4 / 1.055;
        return 1.0 + (encoded - 1.0) * kSlopeAtOne;
    }
    return std::pow((encoded + 0.055) / 1.055, 2.4);
}

double perceptualEncode(double linear)
{
    if (linear <= 0.0) {
        return 0.0;
    }
    if (linear > 1.0) {
        return 1.0 + (linear - 1.0) / 2.2;
    }
    return std::pow(linear, 1.0 / 2.2);
}

double perceptualDecode(double encoded)
{
    if (encoded <= 0.0) {
        return 0.0;
    }
    if (encoded > 1.0) {
        return 1.0 + (encoded - 1.0) * 2.2;
    }
    return std::pow(encoded, 2.2);
}

Vec3 transform(const Mat3& m, const Vec3& v) noexcept
{
    return {m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
            m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]};
}

Mat3 multiply(const Mat3& a, const Mat3& b) noexcept
{
    Mat3 out{};
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
    return out;
}

Mat3 inverse(const Mat3& m) noexcept
{
    const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                       m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                       m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    const double inv = 1.0 / det;
    return {{
        {(m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv, (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv,
         (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv},
        {(m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv, (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv,
         (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv},
        {(m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv, (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv,
         (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv},
    }};
}

std::array<double, 2> planckianXy(double kelvin)
{
    const double t = std::clamp(kelvin, 1667.0, 25000.0);
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double x = t <= 4000.0 ? -0.2661239e9 / t3 - 0.2343589e6 / t2 + 0.8776956e3 / t + 0.179910
                                 : -3.0258469e9 / t3 + 2.1070379e6 / t2 + 0.2226347e3 / t + 0.240390;
    const double x2 = x * x;
    const double x3 = x2 * x;
    double y = 0.0;
    if (t <= 2222.0) {
        y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    } else if (t <= 4000.0) {
        y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    } else {
        y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
    }
    return {x, y};
}

Mat3 bradford(const std::array<double, 2>& fromXy, const std::array<double, 2>& toXy)
{
    static const Mat3 kBradford = {{
        {0.8951, 0.2664, -0.1614},
        {-0.7502, 1.7135, 0.0367},
        {0.0389, -0.0685, 1.0296},
    }};
    static const Mat3 kBradfordInverse = inverse(kBradford);
    const auto toXyz = [](const std::array<double, 2>& xy) {
        return Vec3{xy[0] / xy[1], 1.0, (1.0 - xy[0] - xy[1]) / xy[1]};
    };
    const Vec3 from = transform(kBradford, toXyz(fromXy));
    const Vec3 to = transform(kBradford, toXyz(toXy));
    const Mat3 scale = {
        {{to[0] / from[0], 0.0, 0.0}, {0.0, to[1] / from[1], 0.0}, {0.0, 0.0, to[2] / from[2]}}};
    return multiply(kBradfordInverse, multiply(scale, kBradford));
}

} // namespace pfx::color
