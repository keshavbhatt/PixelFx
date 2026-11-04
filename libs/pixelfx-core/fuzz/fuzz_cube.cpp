// SPDX-License-Identifier: Apache-2.0
// libFuzzer target: any text given to the .cube reader fails with a reason or gives a table
// whose lookups stay in bounds.
#include "pfx/lut.h"

#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view text(
        reinterpret_cast<const char*>(data), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        size);
    const auto lut = pfx::parseCube(text);
    if (lut) {
        for (const float v : {-1.0F, 0.0F, 0.37F, 1.0F, 2.0F}) {
            [[maybe_unused]] const auto out = lut.value().lookup(v, 1.0F - v, v * 0.5F);
        }
    }
    return 0;
}
