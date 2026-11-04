// SPDX-License-Identifier: Apache-2.0
#include "recipe/encoding.h"

#include <array>
#include <bit>
#include <cmath>

namespace pfx::detail {

namespace {

constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int sextet(char c)
{
    const auto at = kAlphabet.find(c);
    return at == std::string_view::npos ? -1 : static_cast<int>(at);
}

} // namespace

std::string base64Encode(const std::vector<std::uint8_t>& bytes)
{
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t a = bytes[i];
        const std::uint32_t b = i + 1 < bytes.size() ? bytes[i + 1] : 0U;
        const std::uint32_t c = i + 2 < bytes.size() ? bytes[i + 2] : 0U;
        const std::uint32_t v = (a << 16U) | (b << 8U) | c;
        out.push_back(kAlphabet[(v >> 18U) & 63U]);
        out.push_back(kAlphabet[(v >> 12U) & 63U]);
        out.push_back(i + 1 < bytes.size() ? kAlphabet[(v >> 6U) & 63U] : '=');
        out.push_back(i + 2 < bytes.size() ? kAlphabet[v & 63U] : '=');
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> base64Decode(std::string_view text)
{
    while (!text.empty() && text.back() == '=') {
        text.remove_suffix(1);
    }
    if (text.size() % 4 == 1) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> out;
    out.reserve((text.size() * 3) / 4);
    std::uint32_t bits = 0;
    int count = 0;
    for (const char c : text) {
        const int v = sextet(c);
        if (v < 0) {
            return std::nullopt;
        }
        bits = (bits << 6U) | static_cast<std::uint32_t>(v);
        count += 6;
        if (count >= 8) {
            count -= 8;
            out.push_back(static_cast<std::uint8_t>((bits >> static_cast<unsigned>(count)) & 0xFFU));
        }
    }
    return out;
}

std::uint16_t toHalf(float value)
{
    const auto f = std::bit_cast<std::uint32_t>(value);
    const auto sign = static_cast<std::uint16_t>((f >> 16U) & 0x8000U);
    const std::uint32_t exponent = (f >> 23U) & 0xFFU;
    std::uint32_t mantissa = f & 0x7FFFFFU;
    if (exponent == 0xFFU) { // infinity or NaN
        return static_cast<std::uint16_t>(sign | 0x7C00U | (mantissa != 0 ? 0x200U : 0U));
    }
    const int e = static_cast<int>(exponent) - 127 + 15;
    if (e >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7C00U);
    }
    if (e <= 0) { // subnormal or zero
        if (e < -10) {
            return sign;
        }
        mantissa |= 0x800000U;
        const auto shift = static_cast<unsigned>(14 - e);
        std::uint32_t half = mantissa >> shift;
        const std::uint32_t rest = mantissa & ((1U << shift) - 1U);
        const std::uint32_t halfway = 1U << (shift - 1U);
        if (rest > halfway || (rest == halfway && (half & 1U) != 0)) {
            ++half;
        }
        return static_cast<std::uint16_t>(sign | half);
    }
    std::uint32_t half = (static_cast<std::uint32_t>(e) << 10U) | (mantissa >> 13U);
    const std::uint32_t rest = mantissa & 0x1FFFU;
    if (rest > 0x1000U || (rest == 0x1000U && (half & 1U) != 0)) {
        ++half; // may carry into the exponent, which is still correct
    }
    return static_cast<std::uint16_t>(sign | half);
}

float fromHalf(std::uint16_t half)
{
    const std::uint32_t sign = (static_cast<std::uint32_t>(half) & 0x8000U) << 16U;
    const auto bits = static_cast<std::uint32_t>(half);
    const std::uint32_t exponent = (bits >> 10U) & 0x1FU;
    const std::uint32_t mantissa = bits & 0x3FFU;
    if (exponent == 0) {
        const float value = std::ldexp(static_cast<float>(mantissa), -24);
        return sign != 0 ? -value : value;
    }
    if (exponent == 31) {
        return std::bit_cast<float>(sign | 0x7F800000U | (mantissa << 13U));
    }
    return std::bit_cast<float>(sign | ((exponent + 112U) << 23U) | (mantissa << 13U));
}

} // namespace pfx::detail
