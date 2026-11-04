// SPDX-License-Identifier: Apache-2.0
// Byte encodings for resources embedded in documents (presets.md section 3): base64 text and
// IEEE half floats.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pfx::detail {

[[nodiscard]] std::string base64Encode(const std::vector<std::uint8_t>& bytes);
// Standard alphabet, padding optional; whitespace and anything else fail.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> base64Decode(std::string_view text);

// Round to nearest even; out of range becomes infinity, NaN stays NaN.
[[nodiscard]] std::uint16_t toHalf(float value);
[[nodiscard]] float fromHalf(std::uint16_t half);

} // namespace pfx::detail
