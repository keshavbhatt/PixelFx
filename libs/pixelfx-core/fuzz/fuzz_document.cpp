// SPDX-License-Identifier: Apache-2.0
// libFuzzer target: any text given to parseDocument either fails with a reason or gives a
// document that serializes and parses back to the same settings.
#include "pfx/document.h"

#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data),
                                size); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    pfx::ParseReport report;
    const auto parsed = pfx::parseDocument(text, &report);
    if (!parsed) {
        return 0;
    }
    const std::string again = pfx::serializeDocument(parsed.value());
    const auto reparsed = pfx::parseDocument(again);
    if (!reparsed || !(reparsed.value().settings == parsed.value().settings)) {
        std::abort(); // a document PixelFx wrote must read back the same
    }
    return 0;
}
